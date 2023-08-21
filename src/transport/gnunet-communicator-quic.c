#include "gnunet_common.h"
#include "gnunet_util_lib.h"
#include "gnunet_core_service.h"
#include "quiche.h"
#include "platform.h"
#include "gnunet_protocols.h"
#include "gnunet_signatures.h"
#include "gnunet_constants.h"
#include "gnunet_statistics_service.h"
#include "gnunet_transport_application_service.h"
#include "gnunet_transport_communication_service.h"
#include "gnunet_nt_lib.h"
#include "gnunet_nat_service.h"
#include "stdint.h"
#include "inttypes.h"

#define COMMUNICATOR_CONFIG_SECTION "communicator-quic"
#define COMMUNICATOR_ADDRESS_PREFIX "quic"
#define MAX_DATAGRAM_SIZE 1350

/* Currently equivalent to QUICHE_MAX_CONN_ID_LEN */
#define LOCAL_CONN_ID_LEN 20
#define MAX_TOKEN_LEN \
        sizeof("quiche") - 1   \
        + sizeof(struct sockaddr_storage)   \
        + QUICHE_MAX_CONN_ID_LEN
#define CID_LEN sizeof(uint8_t) * QUICHE_MAX_CONN_ID_LEN
#define TOKEN_LEN sizeof(uint8_t) * MAX_TOKEN_LEN
/* Generic, bidirectional, client-initiated quic stream id */
#define STREAMID_BI 4
#define PEERID_LEN sizeof(struct GNUNET_PeerIdentity)
/**
 * How long do we believe our addresses to remain up (before
 * the other peer should revalidate).
 */
#define ADDRESS_VALIDITY_PERIOD GNUNET_TIME_UNIT_HOURS
/**
 * Map of DCID (uint8_t) -> quic_conn for quickly retrieving connections to other peers.
 */
struct GNUNET_CONTAINER_MultiHashMap *conn_map;
/**
 * Map of sockaddr -> struct PeerAddress
*/
struct GNUNET_CONTAINER_MultiHashMap *addr_map;
static const struct GNUNET_CONFIGURATION_Handle *cfg;
static struct GNUNET_TIME_Relative rekey_interval;
static struct GNUNET_NETWORK_Handle *udp_sock;
static struct GNUNET_SCHEDULER_Task *read_task;
static struct GNUNET_TRANSPORT_CommunicatorHandle *ch;
static struct GNUNET_TRANSPORT_ApplicationHandle *ah;
static int have_v6_socket;
static uint16_t my_port;
static unsigned long long rekey_max_bytes;
static quiche_config *config = NULL;

/**
 * Our peer identity
*/
struct GNUNET_PeerIdentity my_identity;

/**
 * Our private key.
 */
static struct GNUNET_CRYPTO_EddsaPrivateKey *my_private_key;

/**
 * Connection to NAT service.
 */
static struct GNUNET_NAT_Handle *nat;

/**
 * Information we track per peer we have recently been in contact with.
 *
 * (Since quiche handles crypto, handshakes, etc. we don't differentiate
 *  between SenderAddress and ReceiverAddress)
 */
struct PeerAddress
{
  /**
   * To whom are we talking to.
   */
  struct GNUNET_PeerIdentity target;

  /**
   * Flag to indicate whether we know the PeerIdentity (target) yet
  */
  int id_rcvd;

  /**
   * Address of the receiver in the human-readable format
   * with the #COMMUNICATOR_ADDRESS_PREFIX.
   */
  char *foreign_addr;

  /**
   * Address of the other peer.
   */
  struct sockaddr *address;

  /**
   * Length of the address.
   */
  socklen_t address_len;

  /**
   * The QUIC connection associated with this peer
  */
  struct quic_conn *conn;

  /**
   * Default message queue we are providing for the #ch.
   */
  struct GNUNET_MQ_Handle *d_mq;

  /**
   * handle for default queue with the #ch.
   */
  struct GNUNET_TRANSPORT_QueueHandle *d_qh;

  /**
   * Timeout for this peer address.
   */
  struct GNUNET_TIME_Absolute timeout;

  /**
   * MTU we allowed transport for this peer's default queue.
   * FIXME: MTU from quiche
   */
  size_t d_mtu;

  /**
   * Which network type does this queue use?
   */
  enum GNUNET_NetworkType nt;

  /**
   * receiver_destroy already called on receiver.
   */
  int peer_destroy_called;

  /**
   * Entry in sender expiration heap.
   */
  // struct GNUNET_CONTAINER_HeapNode *hn;
};

// /**
//  * Expiration heap for peers (contains `struct PeerAddress`)
//  */
// static struct GNUNET_CONTAINER_Heap *peers_heap;

/**
 * ID of timeout task
 */
static struct GNUNET_SCHEDULER_Task *timeout_task;

/**
 * Network scanner to determine network types.
 */
static struct GNUNET_NT_InterfaceScanner *is;

/**
 * For logging statistics.
 */
static struct GNUNET_STATISTICS_Handle *stats;

/**
 * QUIC connection object. A connection has a unique SCID/DCID pair. Here we store our SCID
 * (incoming packet DCID field == outgoing packet SCID field) for a given connection. This
 * is hashed for each unique quic_conn.
*/
struct quic_conn
{
  uint8_t cid[LOCAL_CONN_ID_LEN];

  quiche_conn *conn;
};

/**
 * QUIC_header is used to store information received from an incoming QUIC packet
*/
struct QUIC_header
{
  uint8_t type;
  uint32_t version;

  uint8_t scid[QUICHE_MAX_CONN_ID_LEN];
  size_t scid_len;

  uint8_t dcid[QUICHE_MAX_CONN_ID_LEN];
  size_t dcid_len;

  uint8_t odcid[QUICHE_MAX_CONN_ID_LEN];
  size_t odcid_len;

  uint8_t token[MAX_TOKEN_LEN];
  size_t token_len;
};

/**
 * TODOS:
 *  1. Mandate timeouts
 *  2. Setup stats handler properly
*/

/**
 * Given a PeerAddress, receive data from streams after doing connection logic.
 * ASSUMES: connection is established to peer
*/
static void
recv_from_streams (struct PeerAddress *peer)
{
  char stream_buf[UINT16_MAX];
  size_t buf_size = UINT16_MAX;
  char *buf_ptr = stream_buf;
  struct GNUNET_MessageHeader *hdr;

  uint64_t s = 0;
  quiche_stream_iter *readable;
  bool fin;
  ssize_t recv_len;

  readable = quiche_conn_readable (peer->conn->conn);
  while (quiche_stream_iter_next (readable, &s))
  {
    GNUNET_log (GNUNET_ERROR_TYPE_DEBUG,  "stream %" PRIu64 " is readable\n",
                s);
    fin = false;
    recv_len = quiche_conn_stream_recv (peer->conn->conn, s,
                                        (uint8_t *) stream_buf, buf_size,
                                        &fin);
    if (recv_len < 0)
    {
      break;
    }
    /**
     * Initial packet should contain peerid
    */
    if ((GNUNET_YES == quiche_conn_is_established (peer->conn->conn)) &&
        (GNUNET_NO == peer->id_rcvd))
    {
      if (recv_len < sizeof(struct GNUNET_PeerIdentity))
      {
        GNUNET_log (GNUNET_ERROR_TYPE_ERROR,
                    "message recv len of %zd less than length of peer identity\n",
                    recv_len);
        return;
      }
      GNUNET_log (GNUNET_ERROR_TYPE_DEBUG,
                  "receiving peer identity\n");
      struct GNUNET_PeerIdentity *pid = (struct
                                         GNUNET_PeerIdentity *) stream_buf;
      peer->target = *pid;
      peer->id_rcvd = GNUNET_YES;
      buf_ptr += sizeof(struct GNUNET_PeerIdentity);
      recv_len -= sizeof(struct GNUNET_PeerIdentity);
    }
    /**
     * Parse messages to pass to communicator
    */
    while (recv_len >= sizeof(struct GNUNET_MessageHeader))
    {
      hdr = (struct GNUNET_MessageHeader *) buf_ptr;
      if (ntohs (hdr->size) > recv_len)
      {
        GNUNET_log (GNUNET_ERROR_TYPE_ERROR,
                    "message size stated is greater than length of rcvd data!\n");
        return;
      }
      GNUNET_log (GNUNET_ERROR_TYPE_DEBUG, "passing %zd bytes to core\n",
                  recv_len);
      GNUNET_TRANSPORT_communicator_receive (ch, &peer->target, hdr,
                                             ADDRESS_VALIDITY_PERIOD, NULL,
                                             NULL);
      recv_len -= ntohs (hdr->size);
      buf_ptr += ntohs (hdr->size);
    }
    /**
     * Check for leftover bytes
    */
    if (0 != recv_len)
    {
      GNUNET_log (GNUNET_ERROR_TYPE_ERROR,
                  "message recv len of %zd less than length of message header\n",
                  recv_len);
    }
    /**
     * fin
    */
    if (fin)
    {
      GNUNET_log (GNUNET_ERROR_TYPE_DEBUG,
                  "fin received, closing connection\n");
      if (0 > quiche_conn_close (peer->conn->conn, true, 0, NULL, 0))
      {
        GNUNET_log (GNUNET_ERROR_TYPE_ERROR,
                    "quiche failed to close connection to peer\n");
      }
    }
  }
  quiche_stream_iter_free (readable);
}


/**
 * TODO: review token generation, assure tokens are generated properly
*/
static void
mint_token (const uint8_t *dcid, size_t dcid_len,
            struct sockaddr_storage *addr, socklen_t addr_len,
            uint8_t *token, size_t *token_len)
{
  GNUNET_memcpy (token, "quiche", sizeof("quiche") - 1);
  GNUNET_memcpy (token + sizeof("quiche") - 1, addr, addr_len);
  GNUNET_memcpy (token + sizeof("quiche") - 1 + addr_len, dcid, dcid_len);

  *token_len = sizeof("quiche") - 1 + addr_len + dcid_len;
}


static enum GNUNET_GenericReturnValue
validate_token (const uint8_t *token, size_t token_len,
                struct sockaddr_storage *addr, socklen_t addr_len,
                uint8_t *odcid, size_t *odcid_len)
{
  if ((token_len < sizeof("quiche") - 1) ||
      memcmp (token, "quiche", sizeof("quiche") - 1))
  {
    return GNUNET_NO;
  }

  token += sizeof("quiche") - 1;
  token_len -= sizeof("quiche") - 1;

  if ((token_len < addr_len) || memcmp (token, addr, addr_len))
  {
    return GNUNET_NO;
  }

  token += addr_len;
  token_len -= addr_len;

  if (*odcid_len < token_len)
  {
    return GNUNET_NO;
  }

  memcpy (odcid, token, token_len);
  *odcid_len = token_len;

  return GNUNET_OK;
}


static struct quic_conn*
create_conn (uint8_t *scid, size_t scid_len,
             uint8_t *odcid, size_t odcid_len,
             struct sockaddr *local_addr,
             socklen_t local_addr_len,
             struct sockaddr_storage *peer_addr,
             socklen_t peer_addr_len)
{
  struct quic_conn *conn;
  quiche_conn *q_conn;
  conn = GNUNET_new (struct quic_conn);
  if (scid_len != LOCAL_CONN_ID_LEN)
  {
    GNUNET_log (GNUNET_ERROR_TYPE_DEBUG,
                "error while creating connection, scid length too short\n");
    return NULL;
  }

  GNUNET_memcpy (conn->cid, scid, LOCAL_CONN_ID_LEN);
  q_conn = quiche_accept (conn->cid, LOCAL_CONN_ID_LEN,
                          odcid, odcid_len,
                          local_addr,
                          local_addr_len,
                          (struct sockaddr *) peer_addr,
                          peer_addr_len,
                          config);
  if (NULL == q_conn)
  {
    GNUNET_log (GNUNET_ERROR_TYPE_ERROR,
                "quiche failed to create connection after call to quiche_accept\n");
    return NULL;
  }
  conn->conn = q_conn;
  GNUNET_log (GNUNET_ERROR_TYPE_DEBUG, "new quic connection created\n");
  return conn;
}


static void
flush_egress (struct quic_conn *conn)
{
  static uint8_t out[MAX_DATAGRAM_SIZE];
  quiche_send_info send_info;

  ssize_t written;
  ssize_t sent;

  while (1)
  {
    written = quiche_conn_send (conn->conn, out, sizeof(out), &send_info);

    if (QUICHE_ERR_DONE == written)
    {
      GNUNET_log (GNUNET_ERROR_TYPE_DEBUG, "done writing quic packets\n");
      break;
    }
    if (0 > written)
    {
      GNUNET_log (GNUNET_ERROR_TYPE_ERROR, "quiche failed to create packet\n");
      return;
    }
    sent = GNUNET_NETWORK_socket_sendto (udp_sock, out, written,
                                         (struct sockaddr *) &send_info.to,
                                         send_info.to_len);
    if (sent != written)
    {
      GNUNET_log (GNUNET_ERROR_TYPE_ERROR,
                  "quiche failed to send data to peer\n");
      return;
    }
    GNUNET_log (GNUNET_ERROR_TYPE_DEBUG, "sent %zd bytes\n", sent);
  }
}


/**
 * Increment receiver timeout due to activity.
 *
 * @param receiver address for which the timeout should be rescheduled
 */
static void
reschedule_peer_timeout (struct PeerAddress *peer)
{
  peer->timeout =
    GNUNET_TIME_relative_to_absolute (GNUNET_CONSTANTS_IDLE_CONNECTION_TIMEOUT);
  // GNUNET_CONTAINER_heap_update_cost (peer->hn,
  //                                    peer->timeout.abs_value_us);
}


/**
 * Destroys a receiving state due to timeout or shutdown.
 *
 * @param receiver entity to close down
 */
static void
peer_destroy (struct PeerAddress *peer)
{
  struct GNUNET_HashCode addr_key;

  peer->peer_destroy_called = GNUNET_YES;
  GNUNET_log (GNUNET_ERROR_TYPE_DEBUG,
              "Disconnecting peer for peer `%s'\n",
              GNUNET_i2s (&peer->target));
  if (NULL != peer->d_qh)
  {
    GNUNET_TRANSPORT_communicator_mq_del (peer->d_qh);
    peer->d_qh = NULL;
  }
  // GNUNET_assert (peer == GNUNET_CONTAINER_heap_remove_node (peer->hn));
  /**
   * Remove peer from hashmap
  */
  GNUNET_CRYPTO_hash (peer->address, peer->address_len, &addr_key);
  if (GNUNET_NO == GNUNET_CONTAINER_multihashmap_remove (addr_map, &addr_key,
                                                         peer))
  {
    GNUNET_log (GNUNET_ERROR_TYPE_ERROR,
                "tried to remove non-existent peer from addr map\n");
    return;
  }
  GNUNET_STATISTICS_set (stats,
                         "# peers active",
                         GNUNET_CONTAINER_multihashmap_size (addr_map),
                         GNUNET_NO);
  quiche_conn_free (peer->conn->conn);
  GNUNET_free (peer->address);
  GNUNET_free (peer->foreign_addr);
  GNUNET_free (peer->conn);
  GNUNET_free (peer);
}


/**
 * Iterator over all peers to clean up.
 *
 * @param cls NULL
 * @param key peer->address
 * @param value the peer to destroy
 * @return #GNUNET_OK to continue to iterate
 */
static int
get_peer_delete_it (void *cls,
                    const struct GNUNET_Hashcode *key,
                    void *value)
{
  struct PeerAddress *peer = value;
  (void) cls;
  (void) key;
  peer_destroy (peer);
  return GNUNET_OK;
}


/**
 * Signature of functions implementing the sending functionality of a
 * message queue.
 *
 * @param mq the message queue
 * @param msg the message to send
 * @param impl_state our `struct PeerAddress`
 */
static void
mq_send_d (struct GNUNET_MQ_Handle *mq,
           const struct GNUNET_MessageHeader *msg,
           void *impl_state)
{
  struct PeerAddress *peer = impl_state;
  uint16_t msize = ntohs (msg->size);
  struct quic_conn *q_conn = peer->conn;

  if (NULL == q_conn->conn)
  {
    GNUNET_log (GNUNET_ERROR_TYPE_ERROR,
                "peer never established quic connection\n");
    return;
  }

  GNUNET_assert (mq == peer->d_mq);
  if (msize > peer->d_mtu)
  {
    GNUNET_log (GNUNET_ERROR_TYPE_DEBUG,
                "msize: %u, mtu: %lu\n",
                msize,
                peer->d_mtu);
    GNUNET_break (0);
    if (GNUNET_YES != peer->peer_destroy_called)
      peer_destroy (peer);
    return;
  }
  reschedule_peer_timeout (peer);

  // quiche_conn_stream_send (peer->conn->conn, s, (uint8_t *) resp,
  //                              5, true);
  // if (-1 == GNUNET_NETWORK_socket_sendto (udp_sock,
  //                                         dgram,
  //                                         sizeof(dgram),
  //                                         peer->address,
  //                                         peer->address_len))
  //   GNUNET_log_strerror (GNUNET_ERROR_TYPE_WARNING, "send");
  // GNUNET_log (GNUNET_ERROR_TYPE_DEBUG,
  //             "Sending UDPBox with payload size %u, %u acks left\n",
  //             msize,
  //             peer->acks_available);
  // GNUNET_MQ_impl_send_continue (mq);
  // return;
}


/**
 * Signature of functions implementing the destruction of a message
 * queue.  Implementations must not free @a mq, but should take care
 * of @a impl_state.
 *
 * @param mq the message queue to destroy
 * @param impl_state our `struct PeerAddress`
 */
static void
mq_destroy_d (struct GNUNET_MQ_Handle *mq, void *impl_state)
{
  struct PeerAddress *peer = impl_state;
  GNUNET_log (GNUNET_ERROR_TYPE_DEBUG,
              "Default MQ destroyed\n");
  if (mq == peer->d_mq)
  {
    peer->d_mq = NULL;
    if (GNUNET_YES != peer->peer_destroy_called)
      peer_destroy (peer);
  }
}


/**
 * Implementation function that cancels the currently sent message.
 *
 * @param mq message queue
 * @param impl_state our `struct PeerAddress`
 */
static void
mq_cancel (struct GNUNET_MQ_Handle *mq, void *impl_state)
{
  /* Cancellation is impossible with QUIC; bail */
  GNUNET_assert (0);
}


/**
 * Generic error handler, called with the appropriate
 * error code and the same closure specified at the creation of
 * the message queue.
 * Not every message queue implementation supports an error handler.
 *
 * @param cls our `struct ReceiverAddress`
 * @param error error code
 */
static void
mq_error (void *cls, enum GNUNET_MQ_Error error)
{
  struct PeerAddress *peer = cls;

  GNUNET_log (GNUNET_ERROR_TYPE_ERROR,
              "MQ error in queue to %s: %d\n",
              GNUNET_i2s (&peer->target),
              (int) error);
  peer_destroy (peer);
}


/**
 * Convert UDP bind specification to a `struct sockaddr *`
 *
 * @param bindto bind specification to convert
 * @param[out] sock_len set to the length of the address
 * @return converted bindto specification
 */
static struct sockaddr *
udp_address_to_sockaddr (const char *bindto, socklen_t *sock_len)
{
  struct sockaddr *in;
  unsigned int port;
  char dummy[2];
  char *colon;
  char *cp;

  if (1 == sscanf (bindto, "%u%1s", &port, dummy))
  {
    /* interpreting value as just a PORT number */
    if (port > UINT16_MAX)
    {
      GNUNET_log (GNUNET_ERROR_TYPE_ERROR,
                  "BINDTO specification `%s' invalid: value too large for port\n",
                  bindto);
      return NULL;
    }
    if ((GNUNET_NO == GNUNET_NETWORK_test_pf (PF_INET6)) ||
        (GNUNET_YES ==
         GNUNET_CONFIGURATION_get_value_yesno (cfg,
                                               COMMUNICATOR_CONFIG_SECTION,
                                               "DISABLE_V6")))
    {
      struct sockaddr_in *i4;

      i4 = GNUNET_malloc (sizeof(struct sockaddr_in));
      i4->sin_family = AF_INET;
      i4->sin_port = htons ((uint16_t) port);
      *sock_len = sizeof(struct sockaddr_in);
      in = (struct sockaddr *) i4;
    }
    else
    {
      struct sockaddr_in6 *i6;

      i6 = GNUNET_malloc (sizeof(struct sockaddr_in6));
      i6->sin6_family = AF_INET6;
      i6->sin6_port = htons ((uint16_t) port);
      *sock_len = sizeof(struct sockaddr_in6);
      in = (struct sockaddr *) i6;
    }
    return in;
  }
  cp = GNUNET_strdup (bindto);
  colon = strrchr (cp, ':');
  if (NULL != colon)
  {
    /* interpret value after colon as port */
    *colon = '\0';
    colon++;
    if (1 == sscanf (colon, "%u%1s", &port, dummy))
    {
      /* interpreting value as just a PORT number */
      if (port > UINT16_MAX)
      {
        GNUNET_log (GNUNET_ERROR_TYPE_ERROR,
                    "BINDTO specification `%s' invalid: value too large for port\n",
                    bindto);
        GNUNET_free (cp);
        return NULL;
      }
    }
    else
    {
      GNUNET_log (
        GNUNET_ERROR_TYPE_ERROR,
        "BINDTO specification `%s' invalid: last ':' not followed by number\n",
        bindto);
      GNUNET_free (cp);
      return NULL;
    }
  }
  else
  {
    /* interpret missing port as 0, aka pick any free one */
    port = 0;
  }
  {
    /* try IPv4 */
    struct sockaddr_in v4;

    memset (&v4, 0, sizeof(v4));
    if (1 == inet_pton (AF_INET, cp, &v4.sin_addr))
    {
      v4.sin_family = AF_INET;
      v4.sin_port = htons ((uint16_t) port);
#if HAVE_SOCKADDR_IN_SIN_LEN
      v4.sin_len = sizeof(struct sockaddr_in);
#endif
      in = GNUNET_memdup (&v4, sizeof(struct sockaddr_in));
      *sock_len = sizeof(struct sockaddr_in);
      GNUNET_free (cp);
      return in;
    }
  }
  {
    /* try IPv6 */
    struct sockaddr_in6 v6;
    const char *start;

    memset (&v6, 0, sizeof(v6));
    start = cp;
    if (('[' == *cp) && (']' == cp[strlen (cp) - 1]))
    {
      start++;   /* skip over '[' */
      cp[strlen (cp) - 1] = '\0';  /* eat ']' */
    }
    if (1 == inet_pton (AF_INET6, start, &v6.sin6_addr))
    {
      v6.sin6_family = AF_INET6;
      v6.sin6_port = htons ((uint16_t) port);
#if HAVE_SOCKADDR_IN_SIN_LEN
      v6.sin6_len = sizeof(sizeof(struct sockaddr_in6));
#endif
      in = GNUNET_memdup (&v6, sizeof(v6));
      *sock_len = sizeof(v6);
      GNUNET_free (cp);
      return in;
    }
  }
  /* #5528 FIXME (feature!): maybe also try getnameinfo()? */
  GNUNET_free (cp);
  return NULL;
}


/**
 * Setup the MQ for the @a peer.  If a queue exists,
 * the existing one is destroyed.  Then the MTU is
 * recalculated and a fresh queue is initialized.
 *
 * @param peer peer to setup MQ for
 */
static void
setup_peer_mq (struct PeerAddress *peer)
{
  size_t base_mtu;

  switch (peer->address->sa_family)
  {
  case AF_INET:
    base_mtu = 1480     /* Ethernet MTU, 1500 - Ethernet header - VLAN tag */
               - sizeof(struct GNUNET_TUN_IPv4Header) /* 20 */
               - sizeof(struct GNUNET_TUN_UdpHeader) /* 8 */;
    break;

  case AF_INET6:
    base_mtu = 1280     /* Minimum MTU required by IPv6 */
               - sizeof(struct GNUNET_TUN_IPv6Header) /* 40 */
               - sizeof(struct GNUNET_TUN_UdpHeader) /* 8 */;
    break;

  default:
    GNUNET_assert (0);
    break;
  }
  /* MTU == base_mtu */
  peer->d_mtu = base_mtu;

  GNUNET_log (GNUNET_ERROR_TYPE_DEBUG,
              "Setting up MQs and QHs\n");
  /* => Effective MTU for CORE will range from 1080 (IPv6 + KX) to
     1404 (IPv4 + Box) bytes, depending on circumstances... */

  if (NULL == peer->d_mq)
    peer->d_mq = GNUNET_MQ_queue_for_callbacks (&mq_send_d,
                                                &mq_destroy_d,
                                                &mq_cancel,
                                                peer,
                                                NULL,
                                                &mq_error,
                                                peer);
  peer->d_qh =
    GNUNET_TRANSPORT_communicator_mq_add (ch,
                                          &peer->target,
                                          peer->foreign_addr,
                                          peer->d_mtu,
                                          GNUNET_TRANSPORT_QUEUE_LENGTH_UNLIMITED,
                                          0, /* Priority */
                                          peer->nt,
                                          GNUNET_TRANSPORT_CS_OUTBOUND,
                                          peer->d_mq);
}


/**
 * Taken from: UDP communicator
 * Converts @a address to the address string format used by this
 * communicator in HELLOs.
 *
 * @param address the address to convert, must be AF_INET or AF_INET6.
 * @param address_len number of bytes in @a address
 * @return string representation of @a address
 */
static char *
sockaddr_to_udpaddr_string (const struct sockaddr *address,
                            socklen_t address_len)
{
  char *ret;

  switch (address->sa_family)
  {
  case AF_INET:
    GNUNET_asprintf (&ret,
                     "%s-%s",
                     COMMUNICATOR_ADDRESS_PREFIX,
                     GNUNET_a2s (address, address_len));
    break;

  case AF_INET6:
    GNUNET_asprintf (&ret,
                     "%s-%s",
                     COMMUNICATOR_ADDRESS_PREFIX,
                     GNUNET_a2s (address, address_len));
    break;

  default:
    GNUNET_assert (0);
  }
  return ret;
}


/**
 * Function called when the transport service has received a
 * backchannel message for this communicator (!) via a different return
 * path. Should be an acknowledgement.
 *
 * @param cls closure, NULL
 * @param sender which peer sent the notification
 * @param msg payload
 */
static void
notify_cb (void *cls,
           const struct GNUNET_PeerIdentity *sender,
           const struct GNUNET_MessageHeader *msg)
{
  // const struct UDPAck *ack;

  // (void) cls;
  // GNUNET_log (GNUNET_ERROR_TYPE_DEBUG,
  //             "Storing UDPAck received from backchannel from %s\n",
  //             GNUNET_i2s_full (sender));
  // if ((ntohs (msg->type) != GNUNET_MESSAGE_TYPE_COMMUNICATOR_UDP_ACK) ||
  //     (ntohs (msg->size) != sizeof(struct UDPAck)))
  // {
  //   GNUNET_break_op (0);
  //   return;
  // }
  // ack = (const struct UDPAck *) msg;
  // GNUNET_CONTAINER_multipeermap_get_multiple (receivers,
  //                                             sender,
  //                                             &handle_ack,
  //                                             (void *) ack);
}


/**
 * Task run to check #receiver_heap and #sender_heap for timeouts.
 *
 * @param cls unused, NULL
 */
static void
check_timeouts (void *cls)
{
  // struct GNUNET_TIME_Relative st;
  // struct GNUNET_TIME_Relative rt;
  // struct GNUNET_TIME_Relative delay;
  // struct ReceiverAddress *receiver;
  // struct SenderAddress *sender;

  // (void) cls;
  // timeout_task = NULL;
  // rt = GNUNET_TIME_UNIT_FOREVER_REL;
  // while (NULL != (receiver = GNUNET_CONTAINER_heap_peek (receivers_heap)))
  // {
  //   /* if (GNUNET_YES != receiver->receiver_destroy_called) */
  //   /* { */
  //   rt = GNUNET_TIME_absolute_get_remaining (receiver->timeout);
  //   if (0 != rt.rel_value_us)
  //     break;
  //   GNUNET_log (GNUNET_ERROR_TYPE_DEBUG,
  //               "Receiver timed out\n");
  //   receiver_destroy (receiver);
  //   // }
  // }
  // st = GNUNET_TIME_UNIT_FOREVER_REL;
  // while (NULL != (sender = GNUNET_CONTAINER_heap_peek (senders_heap)))
  // {
  //   if (GNUNET_YES != sender->sender_destroy_called)
  //   {
  //     st = GNUNET_TIME_absolute_get_remaining (sender->timeout);
  //     if (0 != st.rel_value_us)
  //       break;
  //     sender_destroy (sender);
  //   }
  // }
  // delay = GNUNET_TIME_relative_min (rt, st);
  // if (delay.rel_value_us < GNUNET_TIME_UNIT_FOREVER_REL.rel_value_us)
  //   timeout_task = GNUNET_SCHEDULER_add_delayed (delay, &check_timeouts, NULL);
}


/**
 * Function called by the transport service to initialize a
 * message queue given address information about another peer.
 * If and when the communication channel is established, the
 * communicator must call #GNUNET_TRANSPORT_communicator_mq_add()
 * to notify the service that the channel is now up.  It is
 * the responsibility of the communicator to manage sane
 * retries and timeouts for any @a peer/@a address combination
 * provided by the transport service.  Timeouts and retries
 * do not need to be signalled to the transport service.
 *
 * @param cls closure
 * @param peer identity of the other peer
 * @param address where to send the message, human-readable
 *        communicator-specific format, 0-terminated, UTF-8
 * @return #GNUNET_OK on success, #GNUNET_SYSERR if the provided address is
 * invalid
 */
static int
mq_init (void *cls, const struct GNUNET_PeerIdentity *peer_id, const
         char *address)
{
  struct PeerAddress *peer;
  const char *path;
  struct sockaddr *in;
  socklen_t in_len;
  struct GNUNET_HashCode addr_key;
  uint8_t scid[LOCAL_CONN_ID_LEN];

  ssize_t send_len;
  char my_pid[PEERID_LEN];

  struct quic_conn *q_conn;
  char *bindto;
  socklen_t local_in_len;
  struct sockaddr *local_addr;

  if (GNUNET_OK !=
      GNUNET_CONFIGURATION_get_value_string (cfg,
                                             COMMUNICATOR_CONFIG_SECTION,
                                             "BINDTO",
                                             &bindto))
  {
    GNUNET_log_config_missing (GNUNET_ERROR_TYPE_ERROR,
                               COMMUNICATOR_CONFIG_SECTION,
                               "BINDTO");
    return GNUNET_SYSERR;
  }
  local_addr = udp_address_to_sockaddr (bindto, &local_in_len);

  if (0 != strncmp (address,
                    COMMUNICATOR_ADDRESS_PREFIX "-",
                    strlen (COMMUNICATOR_ADDRESS_PREFIX "-")))
  {
    GNUNET_break_op (0);
    return GNUNET_SYSERR;
  }
  path = &address[strlen (COMMUNICATOR_ADDRESS_PREFIX "-")];
  in = udp_address_to_sockaddr (path, &in_len);

  /**
   * If we already have a queue with this peer, ignore
  */
  GNUNET_CRYPTO_hash (&in, in_len, &addr_key);
  peer = GNUNET_CONTAINER_multihashmap_get (addr_map, &addr_key);
  if (NULL != peer)
  {
    GNUNET_log (GNUNET_ERROR_TYPE_DEBUG,
                "ignoring transport service mq request, we already have an mq with this peer (address)\n");
    return GNUNET_SYSERR;
  }
  peer = GNUNET_new (struct PeerAddress);
  peer->address = in;
  peer->address_len = in_len;
  peer->target = *peer_id;
  peer->nt = GNUNET_NT_scanner_get_type (is, in, in_len);
  peer->timeout =
    GNUNET_TIME_relative_to_absolute (GNUNET_CONSTANTS_IDLE_CONNECTION_TIMEOUT);
  GNUNET_STATISTICS_set (stats,
                         "# peers active",
                         GNUNET_CONTAINER_multihashmap_size (addr_map),
                         GNUNET_NO);
  peer->foreign_addr =
    sockaddr_to_udpaddr_string (peer->address, peer->address_len);
  /**
   * Before setting up peer mq, initiate a quic connection to the target (perform handshake w/ quiche)
  */
  GNUNET_CRYPTO_random_block (GNUNET_CRYPTO_QUALITY_STRONG, scid,
                              LOCAL_CONN_ID_LEN);
  q_conn = GNUNET_new (struct quic_conn);
  GNUNET_memcpy (q_conn->cid, scid, LOCAL_CONN_ID_LEN);
  GNUNET_log (GNUNET_ERROR_TYPE_DEBUG,
              "Attempting to perform QUIC handshake with peer\n");
  q_conn->conn = quiche_connect (peer->foreign_addr, scid, LOCAL_CONN_ID_LEN,
                                 local_addr,
                                 local_in_len, peer->address, peer->address_len,
                                 config);
  flush_egress (peer->conn);
  if (GNUNET_NO == quiche_conn_is_established (q_conn->conn))
  {
    GNUNET_log (GNUNET_ERROR_TYPE_ERROR,
                "Failed to form connection to peer. mq not created\n");
    return GNUNET_NO;
  }
  GNUNET_log (GNUNET_ERROR_TYPE_DEBUG,
              "Handshake established with peer, sending our peer id\n");
  peer->conn = q_conn;
  /**
   * Send our pid
  */
  GNUNET_memcpy (my_pid, &my_identity, PEERID_LEN);
  send_len = quiche_conn_stream_send (peer->conn->conn, STREAMID_BI, my_pid,
                                      PEERID_LEN,
                                      false);
  if (0 > send_len)
  {
    GNUNET_log (GNUNET_ERROR_TYPE_ERROR,
                "Failed to write peer identity packet\n");
    return GNUNET_NO;
  }
  flush_egress (peer->conn);
  GNUNET_log (GNUNET_ERROR_TYPE_DEBUG, "Peer identity sent to peer\n");
  /**
   * Insert peer into hashmap
  */
  GNUNET_CONTAINER_multihashmap_put (addr_map, &addr_key,
                                     peer,
                                     GNUNET_CONTAINER_MULTIHASHMAPOPTION_UNIQUE_ONLY);
  GNUNET_log (GNUNET_ERROR_TYPE_DEBUG, "Added new peer to the addr map\n");
  setup_peer_mq (peer);
  if (NULL == timeout_task)
    timeout_task = GNUNET_SCHEDULER_add_now (&check_timeouts, NULL);
  GNUNET_free (local_addr);
  return GNUNET_OK;
}


static void
try_connection_reversal (void *cls,
                         const struct sockaddr *addr,
                         socklen_t addrlen)
{
  /* FIXME: support reversal: #5529 */
  GNUNET_log (GNUNET_ERROR_TYPE_INFO,
              "No connection reversal implemented!");
}


/**
 * Signature of the callback passed to #GNUNET_NAT_register() for
 * a function to call whenever our set of 'valid' addresses changes.
 *
 * @param cls closure
 * @param app_ctx[in,out] location where the app can store stuff
 *                  on add and retrieve it on remove
 * @param add_remove #GNUNET_YES to add a new public IP address,
 *                   #GNUNET_NO to remove a previous (now invalid) one
 * @param ac address class the address belongs to
 * @param addr either the previous or the new public IP address
 * @param addrlen actual length of the @a addr
 */
static void
nat_address_cb (void *cls,
                void **app_ctx,
                int add_remove,
                enum GNUNET_NAT_AddressClass ac,
                const struct sockaddr *addr,
                socklen_t addrlen)
{
  char *my_addr;
  struct GNUNET_TRANSPORT_AddressIdentifier *ai;

  if (GNUNET_YES == add_remove)
  {
    enum GNUNET_NetworkType nt;

    GNUNET_asprintf (&my_addr,
                     "%s-%s",
                     COMMUNICATOR_ADDRESS_PREFIX,
                     GNUNET_a2s (addr, addrlen));
    nt = GNUNET_NT_scanner_get_type (is, addr, addrlen);
    ai =
      GNUNET_TRANSPORT_communicator_address_add (ch,
                                                 my_addr,
                                                 nt,
                                                 GNUNET_TIME_UNIT_FOREVER_REL);
    GNUNET_free (my_addr);
    *app_ctx = ai;
  }
  else
  {
    ai = *app_ctx;
    GNUNET_TRANSPORT_communicator_address_remove (ai);
    *app_ctx = NULL;
  }
}


/**
 * Shutdown the QUIC communicator.
 *
 * @param cls NULL (always)
 */
static void
do_shutdown (void *cls)
{
  GNUNET_log (GNUNET_ERROR_TYPE_DEBUG,
              "do_shutdown\n");
  GNUNET_CONTAINER_multihashmap_iterate (addr_map, &get_peer_delete_it, NULL);
  GNUNET_CONTAINER_multihashmap_destroy (addr_map);
  quiche_config_free (config);

  if (NULL != timeout_task)
  {
    GNUNET_SCHEDULER_cancel (timeout_task);
    timeout_task = NULL;
  }
  if (NULL != read_task)
  {
    GNUNET_SCHEDULER_cancel (read_task);
    read_task = NULL;
  }
  if (NULL != udp_sock)
  {
    GNUNET_break (GNUNET_OK ==
                  GNUNET_NETWORK_socket_close (udp_sock));
    udp_sock = NULL;
  }
  if (NULL != ch)
  {
    GNUNET_TRANSPORT_communicator_disconnect (ch);
    ch = NULL;
  }
  if (NULL != ah)
  {
    GNUNET_TRANSPORT_application_done (ah);
    ah = NULL;
  }
  if (NULL != my_private_key)
  {
    GNUNET_free (my_private_key);
    my_private_key = NULL;
  }
  GNUNET_log (GNUNET_ERROR_TYPE_DEBUG,
              "do_shutdown finished\n");
}


static void
sock_read (void *cls)
{
  struct sockaddr_storage sa;
  struct sockaddr_in *addr_verify;
  socklen_t salen = sizeof(sa);
  uint8_t buf[UINT16_MAX];
  uint8_t out[MAX_DATAGRAM_SIZE];
  ssize_t rcvd;

  ssize_t process_pkt;
  struct QUIC_header quic_header;
  uint8_t new_cid[LOCAL_CONN_ID_LEN];

  struct PeerAddress *peer;
  struct GNUNET_HashCode addr_key;

  (void) cls;
  quic_header.scid_len = sizeof(quic_header.scid);
  quic_header.dcid_len = sizeof(quic_header.dcid);
  quic_header.odcid_len = sizeof(quic_header.odcid);
  quic_header.token_len = sizeof(quic_header.token);
  /**
   * Get local_addr, in_len for quiche
  */
  char *bindto;
  socklen_t in_len;
  if (GNUNET_OK !=
      GNUNET_CONFIGURATION_get_value_string (cfg,
                                             COMMUNICATOR_CONFIG_SECTION,
                                             "BINDTO",
                                             &bindto))
  {
    GNUNET_log_config_missing (GNUNET_ERROR_TYPE_ERROR,
                               COMMUNICATOR_CONFIG_SECTION,
                               "BINDTO");
    return;
  }
  struct sockaddr *local_addr = udp_address_to_sockaddr (bindto, &in_len);

  read_task = GNUNET_SCHEDULER_add_read_net (GNUNET_TIME_UNIT_FOREVER_REL,
                                             udp_sock,
                                             &sock_read,
                                             NULL);
  while (1)
  {
    rcvd = GNUNET_NETWORK_socket_recvfrom (udp_sock,
                                           buf,
                                           sizeof(buf),
                                           (struct sockaddr *) &sa,
                                           &salen);
    if (-1 == rcvd)
    {
      if (EAGAIN == errno)
        break; // We are done reading data
      GNUNET_log_strerror (GNUNET_ERROR_TYPE_DEBUG, "recv");
      return;
    }

    GNUNET_log (GNUNET_ERROR_TYPE_DEBUG,
                "Read %lu bytes\n", rcvd);

    if (-1 == rcvd)
    {
      GNUNET_log_strerror (GNUNET_ERROR_TYPE_DEBUG, "recv");
      return;
    }
    GNUNET_CRYPTO_hash ((struct sockaddr *) &sa, salen,
                        &addr_key);
    peer = GNUNET_CONTAINER_multihashmap_get (addr_map, &addr_key);

    if (NULL == peer)
    {
      /**
       * Create new PeerAddress (receiver) with id_rcvd = false
      */
      peer = GNUNET_new (struct PeerAddress);
      peer->address = GNUNET_memdup (&sa, salen);
      peer->address_len = salen;
      peer->id_rcvd = GNUNET_NO;
      peer->conn = NULL;
      peer->foreign_addr = sockaddr_to_udpaddr_string (peer->address,
                                                       peer->address_len);
      setup_peer_mq (peer);
      if (GNUNET_SYSERR == GNUNET_CONTAINER_multihashmap_put (addr_map,
                                                              &addr_key,
                                                              peer,
                                                              GNUNET_CONTAINER_MULTIHASHMAPOPTION_UNIQUE_ONLY))
      {
        GNUNET_log (GNUNET_ERROR_TYPE_ERROR,
                    "tried to add duplicate address into address map\n");
        return;
      }
      GNUNET_log (GNUNET_ERROR_TYPE_DEBUG, "added new peer to address map\n");
    }

    /**
     * Parse QUIC info
    */
    int rc = quiche_header_info (buf, rcvd, LOCAL_CONN_ID_LEN,
                                 &quic_header.version,
                                 &quic_header.type, quic_header.scid,
                                 &quic_header.scid_len, quic_header.dcid,
                                 &quic_header.dcid_len,
                                 quic_header.token, &quic_header.token_len);
    if (0 > rc)
    {
      GNUNET_log (GNUNET_ERROR_TYPE_ERROR,
                  "failed to parse quic header: %d\n",
                  rc);
      return;
    }

    /**
     * New QUIC connection with peer
    */
    if (NULL == peer->conn)
    {
      GNUNET_log (GNUNET_ERROR_TYPE_DEBUG,
                  "attempting to create new connection\n");
      if (0 == quiche_version_is_supported (quic_header.version))
      {
        GNUNET_log (GNUNET_ERROR_TYPE_DEBUG,
                    "quic version negotiation initiated\n");
        /**
         * Write a version negotiation packet to "out"
        */
        ssize_t written = quiche_negotiate_version (quic_header.scid,
                                                    quic_header.scid_len,
                                                    quic_header.dcid,
                                                    quic_header.dcid_len,
                                                    out, sizeof(out));
        if (0 > written)
        {
          GNUNET_log (GNUNET_ERROR_TYPE_ERROR,
                      "quiche failed to generate version negotiation packet\n");
          return;
        }
        ssize_t sent = GNUNET_NETWORK_socket_sendto (udp_sock,
                                                     out,
                                                     written,
                                                     (struct sockaddr*) &sa,
                                                     salen);
        if (sent != written)
        {
          GNUNET_log (GNUNET_ERROR_TYPE_ERROR,
                      "failed to send version negotiation packet to peer\n");
          return;
        }
        GNUNET_log (GNUNET_ERROR_TYPE_DEBUG,
                    "sent %zd bytes to peer during version negotiation\n",
                    sent);
        return;
      }

      if (0 == quic_header.token_len)
      {
        GNUNET_log (GNUNET_ERROR_TYPE_DEBUG, "quic stateless retry\n");
        mint_token (quic_header.dcid, quic_header.dcid_len, &sa, salen,
                    quic_header.token, &quic_header.token_len);

        uint8_t new_cid[LOCAL_CONN_ID_LEN];
        GNUNET_CRYPTO_random_block (GNUNET_CRYPTO_QUALITY_STRONG, new_cid,
                                    LOCAL_CONN_ID_LEN);

        ssize_t written = quiche_retry (quic_header.scid, quic_header.scid_len,
                                        quic_header.dcid, quic_header.dcid_len,
                                        new_cid, LOCAL_CONN_ID_LEN,
                                        quic_header.token,
                                        quic_header.token_len,
                                        quic_header.version, out, sizeof(out));
        if (0 > written)
        {
          GNUNET_log (GNUNET_ERROR_TYPE_ERROR,
                      "quiche failed to write retry packet\n");
          return;
        }
        ssize_t sent = GNUNET_NETWORK_socket_sendto (udp_sock,
                                                     out,
                                                     written,
                                                     (struct sockaddr*) &sa,
                                                     salen);
        if (written != sent)
        {
          GNUNET_log (GNUNET_ERROR_TYPE_ERROR, "failed to send retry packet\n");
          return;
        }

        GNUNET_log (GNUNET_ERROR_TYPE_DEBUG, "sent %zd bytes\n", sent);
      }

      if (GNUNET_OK != validate_token (quic_header.token, quic_header.token_len,
                                       &sa, salen,
                                       quic_header.odcid,
                                       &quic_header.odcid_len))
      {
        GNUNET_log (GNUNET_ERROR_TYPE_ERROR,
                    "invalid address validation token created\n");
        return;
      }
      peer->conn = create_conn (quic_header.dcid, quic_header.dcid_len,
                                quic_header.odcid, quic_header.odcid_len,
                                local_addr, in_len,
                                &sa, salen);
      if (NULL == peer->conn)
      {
        GNUNET_log (GNUNET_ERROR_TYPE_ERROR,
                    "failed to create quic connection with peer\n");
        return;
      }
    } // null connection

    quiche_recv_info recv_info = {
      (struct sockaddr *) &sa,
      salen,

      local_addr,
      in_len,
    };
    process_pkt = quiche_conn_recv (peer->conn->conn, buf, rcvd, &recv_info);
    if (0 > process_pkt)
    {
      GNUNET_log (GNUNET_ERROR_TYPE_ERROR,
                  "quiche failed to process received packet: %zd\n",
                  process_pkt);
      return;
    }
    GNUNET_log (GNUNET_ERROR_TYPE_DEBUG,
                "quiche processed %zd bytes\n", process_pkt);
    // Check for data on all available streams
    recv_from_streams (peer);
    /**
     * TODO: Should we use a list instead of hashmap?
     * Overhead for hashing function, O(1) retrieval vs O(n) iteration with n=30?
     *
     * TODO: Is iteration necessary as in the quiche server example?
    */
    quiche_stats stats;
    quiche_path_stats path_stats;

    flush_egress (peer->conn);

    if (quiche_conn_is_closed (peer->conn->conn))
    {
      quiche_conn_stats (peer->conn->conn, &stats);
      quiche_conn_path_stats (peer->conn->conn, 0, &path_stats);

      GNUNET_log (GNUNET_ERROR_TYPE_DEBUG,
                  "connection closed. quiche stats: sent=%zu, recv=%zu\n",
                  stats.sent, stats.recv);
      peer_destroy (peer);
    }
  }
  GNUNET_free (local_addr);
}


/**
 * Setup communicator and launch network interactions.
 *
 * @param cls NULL (always)
 * @param args remaining command-line arguments
 * @param cfgfile name of the configuration file used (for saving, can be NULL!)
 * @param c configuration
 */
static void
run (void *cls,
     char *const *args,
     const char *cfgfile,
     const struct GNUNET_CONFIGURATION_Handle *c)
{
  char *bindto;
  struct sockaddr *in;
  socklen_t in_len;
  struct sockaddr_storage in_sto;
  socklen_t sto_len;

  (void) cls;
  cfg = c;

  if (GNUNET_OK !=
      GNUNET_CONFIGURATION_get_value_string (cfg,
                                             COMMUNICATOR_CONFIG_SECTION,
                                             "BINDTO",
                                             &bindto))
  {
    GNUNET_log_config_missing (GNUNET_ERROR_TYPE_ERROR,
                               COMMUNICATOR_CONFIG_SECTION,
                               "BINDTO");
    return;
  }

  // if (GNUNET_OK !=
  //     GNUNET_CONFIGURATION_get_value_time (cfg,
  //                                          COMMUNICATOR_CONFIG_SECTION,
  //                                          "REKEY_INTERVAL",
  //                                          &rekey_interval))
  //   rekey_interval = DEFAULT_REKEY_TIME_INTERVAL;

  // if (GNUNET_OK !=
  //     GNUNET_CONFIGURATION_get_value_size (cfg,
  //                                          COMMUNICATOR_CONFIG_SECTION,
  //                                          "REKEY_MAX_BYTES",
  //                                          &rekey_max_bytes))
  //   rekey_max_bytes = DEFAULT_REKEY_MAX_BYTES;

  in = udp_address_to_sockaddr (bindto, &in_len);

  if (NULL == in)
  {
    GNUNET_log (GNUNET_ERROR_TYPE_ERROR,
                "Failed to setup UDP socket address with path `%s'\n",
                bindto);
    GNUNET_free (bindto);
    return;
  }
  udp_sock =
    GNUNET_NETWORK_socket_create (in->sa_family,
                                  SOCK_DGRAM,
                                  IPPROTO_UDP);
  if (NULL == udp_sock)
  {
    GNUNET_log_strerror (GNUNET_ERROR_TYPE_ERROR, "socket");
    GNUNET_free (in);
    GNUNET_free (bindto);
    return;
  }
  if (AF_INET6 == in->sa_family)
    have_v6_socket = GNUNET_YES;
  if (GNUNET_OK !=
      GNUNET_NETWORK_socket_bind (udp_sock,
                                  in,
                                  in_len))
  {
    GNUNET_log_strerror_file (GNUNET_ERROR_TYPE_ERROR,
                              "bind",
                              bindto);
    GNUNET_NETWORK_socket_close (udp_sock);
    udp_sock = NULL;
    GNUNET_free (in);
    GNUNET_free (bindto);
    return;
  }
  sto_len = sizeof(in_sto);
  if (0 != getsockname (GNUNET_NETWORK_get_fd (udp_sock),
                        (struct sockaddr *) &in_sto,
                        &sto_len))
  {
    memcpy (&in_sto, in, in_len);
    sto_len = in_len;
  }
  GNUNET_free (in);
  GNUNET_free (bindto);
  in = (struct sockaddr *) &in_sto;
  in_len = sto_len;
  GNUNET_log_from_nocheck (GNUNET_ERROR_TYPE_DEBUG,
                           "transport",
                           "Bound to `%s'\n",
                           GNUNET_a2s ((const struct sockaddr *) &in_sto,
                                       sto_len));
  switch (in->sa_family)
  {
  case AF_INET:
    my_port = ntohs (((struct sockaddr_in *) in)->sin_port);
    break;

  case AF_INET6:
    my_port = ntohs (((struct sockaddr_in6 *) in)->sin6_port);
    break;

  default:
    GNUNET_break (0);
    my_port = 0;
  }
  GNUNET_SCHEDULER_add_shutdown (&do_shutdown, NULL);
  /**
   * Setup QUICHE configuration
  */
  config = quiche_config_new (QUICHE_PROTOCOL_VERSION);
  quiche_config_verify_peer (config, false);
  // conn_map = GNUNET_CONTAINER_multihashmap_create (2, GNUNET_NO);
  addr_map = GNUNET_CONTAINER_multihashmap_create (2, GNUNET_NO);
  /**
   * Get our public key for initial packet
  */
  my_private_key = GNUNET_CRYPTO_eddsa_key_create_from_configuration (cfg);
  if (NULL == my_private_key)
  {
    GNUNET_log (
      GNUNET_ERROR_TYPE_ERROR,
      _ (
        "Transport service is lacking key configuration settings. Exiting.\n"));
    GNUNET_SCHEDULER_shutdown ();
    return;
  }
  GNUNET_CRYPTO_eddsa_key_get_public (my_private_key, &my_identity.public_key);
  /* start reading */
  read_task = GNUNET_SCHEDULER_add_read_net (GNUNET_TIME_UNIT_FOREVER_REL,
                                             udp_sock,
                                             &sock_read,
                                             NULL);
  ch = GNUNET_TRANSPORT_communicator_connect (cfg,
                                              COMMUNICATOR_CONFIG_SECTION,
                                              COMMUNICATOR_ADDRESS_PREFIX,
                                              GNUNET_TRANSPORT_CC_RELIABLE,
                                              &mq_init,
                                              NULL,
                                              &notify_cb,
                                              NULL);
  is = GNUNET_NT_scanner_init ();
  nat = GNUNET_NAT_register (cfg,
                             COMMUNICATOR_CONFIG_SECTION,
                             IPPROTO_UDP,
                             1 /* one address */,
                             (const struct sockaddr **) &in,
                             &in_len,
                             &nat_address_cb,
                             try_connection_reversal,
                             NULL /* closure */);
  if (NULL == ch)
  {
    GNUNET_break (0);
    GNUNET_SCHEDULER_shutdown ();
    return;
  }
  ah = GNUNET_TRANSPORT_application_init (cfg);
  if (NULL == ah)
  {
    GNUNET_break (0);
    GNUNET_SCHEDULER_shutdown ();
    return;
  }

  /* start broadcasting */
  // if (GNUNET_YES !=
  //     GNUNET_CONFIGURATION_get_value_yesno (cfg,
  //                                           COMMUNICATOR_CONFIG_SECTION,
  //                                           "DISABLE_BROADCAST"))
  // {
  //   broadcast_task = GNUNET_SCHEDULER_add_now (&do_broadcast, NULL);
  // }
}


int
main (int argc, char *const *argv)
{
  static const struct GNUNET_GETOPT_CommandLineOption options[] = {
    GNUNET_GETOPT_OPTION_END
  };
  int ret;

  GNUNET_log_from_nocheck (GNUNET_ERROR_TYPE_DEBUG,
                           "transport",
                           "Starting quic communicator\n");
  if (GNUNET_OK != GNUNET_STRINGS_get_utf8_args (argc, argv, &argc, &argv))
    return 2;

  ret = (GNUNET_OK == GNUNET_PROGRAM_run (argc,
                                          argv,
                                          "gnunet-communicator-quic",
                                          _ ("GNUnet QUIC communicator"),
                                          options,
                                          &run,
                                          NULL))
          ? 0
          : 1;
  GNUNET_free_nz ((void *) argv);
  return ret;
}
