/*
     This file is part of GNUnet.
     Copyright (C) 2009-2013, 2016 GNUnet e.V.

     GNUnet is free software: you can redistribute it and/or modify it
     under the terms of the GNU Affero General Public License as published
     by the Free Software Foundation, either version 3 of the License,
     or (at your option) any later version.

     GNUnet is distributed in the hope that it will be useful, but
     WITHOUT ANY WARRANTY; without even the implied warranty of
     MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
     Affero General Public License for more details.

     You should have received a copy of the GNU Affero General Public License
     along with this program.  If not, see <http://www.gnu.org/licenses/>.

     SPDX-License-Identifier: AGPL3.0-or-later
 */

/**
 * @file core/gnunet-service-core_kx.c
 * @brief code for managing the key exchange (SET_KEY, PING, PONG) with other
 * peers
 * @author Christian Grothoff
 */
#include "platform.h"
#include "gnunet-service-core_kx.h"
#include "gnunet_transport_core_service.h"
#include "gnunet-service-core_sessions.h"
#include "gnunet-service-core.h"
#include "gnunet_constants.h"
#include "gnunet_signatures.h"
#include "gnunet_protocols.h"

/**
 * Enable expensive (and possibly problematic for privacy!) logging of KX.
 */
#define DEBUG_KX 0


/**
 * How long do we wait for SET_KEY confirmation initially?
 */
#define INITIAL_SET_KEY_RETRY_FREQUENCY \
        GNUNET_TIME_relative_multiply (GNUNET_TIME_UNIT_SECONDS, 10)

/**
 * What is the minimum frequency for a PING message?
 */
#define MIN_PING_FREQUENCY \
        GNUNET_TIME_relative_multiply (GNUNET_TIME_UNIT_SECONDS, 5)

/**
 * How often do we rekey?
 */
#define REKEY_FREQUENCY \
        GNUNET_TIME_relative_multiply (GNUNET_TIME_UNIT_HOURS, 12)

/**
 * What time difference do we tolerate?
 */
#define REKEY_TOLERANCE \
        GNUNET_TIME_relative_multiply (GNUNET_TIME_UNIT_MINUTES, 5)

/**
 * What is the maximum age of a message for us to consider processing
 * it?  Note that this looks at the timestamp used by the other peer,
 * so clock skew between machines does come into play here.  So this
 * should be picked high enough so that a little bit of clock skew
 * does not prevent peers from connecting to us.
 */
#define MAX_MESSAGE_AGE GNUNET_TIME_UNIT_DAYS


#if CONG_CRYPTO_ENABLED
GNUNET_NETWORK_STRUCT_BEGIN

/**
 * Encapsulation for encrypted messages exchanged between
 * peers.  Followed by the actual encrypted data.
 */
struct EncryptedMessage
{
  /**
   * Message type is #GNUNET_MESSAGE_TYPE_CORE_ENCRYPTED_MESSAGE.
   */
  struct GNUNET_MessageHeader header;

  /**
   * The nonce.
   */
  unsigned char nonce[crypto_aead_xchacha20poly1305_ietf_NPUBBYTES];

  /**
   * The Poly1305 tag of the encrypted message
   * (which is starting at @e sequence_number),
   * used to verify message integrity. Everything after this value
   * (excluding this value itself) will be encrypted and
   * authenticated.  #ENCRYPTED_HEADER_SIZE must be set to the offset
   * of the *next* field.
   */
  unsigned char tag[crypto_aead_xchacha20poly1305_ietf_ABYTES];

  /**
   * Sequence number, in network byte order.  This field
   * must be the first encrypted/decrypted field
   */
  uint32_t sequence_number GNUNET_PACKED;

  /**
   * Reserved, always zero.
   */
  uint32_t reserved GNUNET_PACKED;

  /**
   * Timestamp.  Used to prevent replay of ancient messages
   * (recent messages are caught with the sequence number).
   */
  struct GNUNET_TIME_AbsoluteNBO timestamp;
};
GNUNET_NETWORK_STRUCT_END

#else

GNUNET_NETWORK_STRUCT_BEGIN

/**
 * Encapsulation for encrypted messages exchanged between
 * peers.  Followed by the actual encrypted data.
 */
struct EncryptedMessage
{
  /**
   * Message type is #GNUNET_MESSAGE_TYPE_CORE_ENCRYPTED_MESSAGE.
   */
  struct GNUNET_MessageHeader header;

  /**
   * Random value used for IV generation.
   */
  uint32_t iv_seed GNUNET_PACKED;

  /**
   * MAC of the encrypted message (starting at @e sequence_number),
   * used to verify message integrity. Everything after this value
   * (excluding this value itself) will be encrypted and
   * authenticated.  #ENCRYPTED_HEADER_SIZE must be set to the offset
   * of the *next* field.
   */
  struct GNUNET_HashCode hmac;

  /**
   * Sequence number, in network byte order.  This field
   * must be the first encrypted/decrypted field
   */
  uint32_t sequence_number GNUNET_PACKED;

  /**
   * Reserved, always zero.
   */
  uint32_t reserved GNUNET_PACKED;

  /**
   * Timestamp.  Used to prevent replay of ancient messages
   * (recent messages are caught with the sequence number).
   */
  struct GNUNET_TIME_AbsoluteNBO timestamp;
};
GNUNET_NETWORK_STRUCT_END

#endif

/**
 * Number of bytes (at the beginning) of `struct EncryptedMessage`
 * that are NOT encrypted.
 */
#define ENCRYPTED_HEADER_SIZE \
        (offsetof (struct EncryptedMessage, sequence_number))


/**
 * Information about the status of a key exchange with another peer.
 */
struct GSC_KeyExchangeInfo
{
  /**
   * DLL.
   */
  struct GSC_KeyExchangeInfo *next;

  /**
   * DLL.
   */
  struct GSC_KeyExchangeInfo *prev;

  /**
   * Identity of the peer.
   */
  const struct GNUNET_PeerIdentity *peer;

  /**
   * Message queue for sending messages to @a peer.
   */
  struct GNUNET_MQ_Handle *mq;

  /**
   * Our message stream tokenizer (for encrypted payload).
   */
  struct GNUNET_MessageStreamTokenizer *mst;

  /**
   * PING message we transmit to the other peer.
   */
  struct PingMessage ping;

  /**
   * Ephemeral public ECC key of the other peer.
   */
  struct GNUNET_CRYPTO_EcdhePublicKey other_ephemeral_key;

#if CONG_CRYPTO_ENABLED
  /**
   * Key we use to encrypt our messages for the other peer
   * (initialized by us when we do the handshake).
   */
  unsigned char encrypt_key[crypto_aead_xchacha20poly1305_ietf_KEYBYTES];

  /**
   * Key we use to decrypt messages from the other peer
   * (given to us by the other peer during the handshake).
   */
  unsigned char decrypt_key[crypto_aead_xchacha20poly1305_ietf_KEYBYTES];
#else
  /**
   * Key we use to encrypt our messages for the other peer
   * (initialized by us when we do the handshake).
   */
  struct GNUNET_CRYPTO_SymmetricSessionKey encrypt_key;

  /**
   * Key we use to decrypt messages from the other peer
   * (given to us by the other peer during the handshake).
   */
  struct GNUNET_CRYPTO_SymmetricSessionKey decrypt_key;
#endif

  /**
   * At what time did the other peer generate the decryption key?
   */
  struct GNUNET_TIME_Absolute foreign_key_expires;

  /**
   * When should the session time out (if there are no PONGs)?
   */
  struct GNUNET_TIME_Absolute timeout;

  /**
   * What was the last timeout we informed our monitors about?
   */
  struct GNUNET_TIME_Absolute last_notify_timeout;

  /**
   * At what frequency are we currently re-trying SET_KEY messages?
   */
  struct GNUNET_TIME_Relative set_key_retry_frequency;

  /**
   * ID of task used for re-trying SET_KEY and PING message.
   */
  struct GNUNET_SCHEDULER_Task *retry_set_key_task;

  /**
   * ID of task used for sending keep-alive pings.
   */
  struct GNUNET_SCHEDULER_Task *keep_alive_task;

  /**
   * Bit map indicating which of the 32 sequence numbers before the
   * last were received (good for accepting out-of-order packets and
   * estimating reliability of the connection)
   */
  uint32_t last_packets_bitmap;

  /**
   * last sequence number received on this connection (highest)
   */
  uint32_t last_sequence_number_received;

  /**
   * last sequence number transmitted
   */
  uint32_t last_sequence_number_sent;

  /**
   * What was our PING challenge number (for this peer)?
   */
  uint32_t ping_challenge;

  /**
   * #GNUNET_YES if this peer currently has excess bandwidth.
   */
  int has_excess_bandwidth;

  /**
   * What is our connection status?
   */
  enum GNUNET_CORE_KxState status;
};


/**
 * Transport service.
 */
static struct GNUNET_TRANSPORT_CoreHandle *transport;

/**
 * Our private key.
 */
static struct GNUNET_CRYPTO_EddsaPrivateKey my_private_key;

/**
 * Our ephemeral private key.
 */
static struct GNUNET_CRYPTO_EcdhePrivateKey my_ephemeral_key;

/**
 * Current message we send for a key exchange.
 */
static struct EphemeralKeyMessage current_ekm;

/**
 * DLL head.
 */
static struct GSC_KeyExchangeInfo *kx_head;

/**
 * DLL tail.
 */
static struct GSC_KeyExchangeInfo *kx_tail;

/**
 * Task scheduled for periodic re-generation (and thus rekeying) of our
 * ephemeral key.
 */
static struct GNUNET_SCHEDULER_Task *rekey_task;

/**
 * Notification context for broadcasting to monitors.
 */
static struct GNUNET_NotificationContext *nc;

#if ! CONG_CRYPTO_ENABLED
/**
 * Calculate seed value we should use for a message.
 *
 * @param kx key exchange context
 */
static uint32_t
calculate_seed (struct GSC_KeyExchangeInfo *kx)
{
  /* Note: may want to make this non-random and instead
     derive from key material to avoid having an undetectable
     side-channel */
  return htonl (
    GNUNET_CRYPTO_random_u32 (GNUNET_CRYPTO_QUALITY_NONCE, UINT32_MAX));
}


#endif

/**
 * Inform all monitors about the KX state of the given peer.
 *
 * @param kx key exchange state to inform about
 */
static void
monitor_notify_all (struct GSC_KeyExchangeInfo *kx)
{
  struct MonitorNotifyMessage msg;

  msg.header.type = htons (GNUNET_MESSAGE_TYPE_CORE_MONITOR_NOTIFY);
  msg.header.size = htons (sizeof(msg));
  msg.state = htonl ((uint32_t) kx->status);
  msg.peer = *kx->peer;
  msg.timeout = GNUNET_TIME_absolute_hton (kx->timeout);
  GNUNET_notification_context_broadcast (nc, &msg.header, GNUNET_NO);
  kx->last_notify_timeout = kx->timeout;
}


#if ! CONG_CRYPTO_ENABLED
/**
 * Derive an authentication key from "set key" information
 *
 * @param akey authentication key to derive
 * @param skey session key to use
 * @param seed seed to use
 */
static void
derive_auth_key (struct GNUNET_CRYPTO_AuthKey *akey,
                 const struct GNUNET_CRYPTO_SymmetricSessionKey *skey,
                 uint32_t seed)
{
  static const char ctx[] = "authentication key";

#if DEBUG_KX
  struct GNUNET_HashCode sh;

  GNUNET_CRYPTO_hash (skey, sizeof(*skey), &sh);
  GNUNET_log (GNUNET_ERROR_TYPE_DEBUG,
              "Deriving Auth key from SKEY %s and seed %u\n",
              GNUNET_h2s (&sh),
              (unsigned int) seed);
#endif
  GNUNET_CRYPTO_hmac_derive_key (akey,
                                 skey,
                                 &seed,
                                 sizeof(seed),
                                 skey,
                                 sizeof(
                                   struct GNUNET_CRYPTO_SymmetricSessionKey),
                                 ctx,
                                 sizeof(ctx),
                                 NULL);
}


/**
 * Derive an IV from packet information
 *
 * @param iv initialization vector to initialize
 * @param skey session key to use
 * @param seed seed to use
 * @param identity identity of the other peer to use
 */
static void
derive_iv (struct GNUNET_CRYPTO_SymmetricInitializationVector *iv,
           const struct GNUNET_CRYPTO_SymmetricSessionKey *skey,
           uint32_t seed,
           const struct GNUNET_PeerIdentity *identity)
{
  static const char ctx[] = "initialization vector";

#if DEBUG_KX
  struct GNUNET_HashCode sh;

  GNUNET_CRYPTO_hash (skey, sizeof(*skey), &sh);
  GNUNET_log (GNUNET_ERROR_TYPE_DEBUG,
              "Deriving IV from SKEY %s and seed %u for peer %s\n",
              GNUNET_h2s (&sh),
              (unsigned int) seed,
              GNUNET_i2s (identity));
#endif
  GNUNET_CRYPTO_symmetric_derive_iv (iv,
                                     skey,
                                     &seed,
                                     sizeof(seed),
                                     identity,
                                     sizeof(struct GNUNET_PeerIdentity),
                                     ctx,
                                     sizeof(ctx),
                                     NULL);
}


/**
 * Derive an IV from pong packet information
 *
 * @param iv initialization vector to initialize
 * @param skey session key to use
 * @param seed seed to use
 * @param challenge nonce to use
 * @param identity identity of the other peer to use
 */
static void
derive_pong_iv (struct GNUNET_CRYPTO_SymmetricInitializationVector *iv,
                const struct GNUNET_CRYPTO_SymmetricSessionKey *skey,
                uint32_t seed,
                uint32_t challenge,
                const struct GNUNET_PeerIdentity *identity)
{
  static const char ctx[] = "pong initialization vector";

#if DEBUG_KX
  struct GNUNET_HashCode sh;

  GNUNET_CRYPTO_hash (skey, sizeof(*skey), &sh);
  GNUNET_log (GNUNET_ERROR_TYPE_DEBUG,
              "Deriving PONG IV from SKEY %s and seed %u/%u for %s\n",
              GNUNET_h2s (&sh),
              (unsigned int) seed,
              (unsigned int) challenge,
              GNUNET_i2s (identity));
#endif
  GNUNET_CRYPTO_symmetric_derive_iv (iv,
                                     skey,
                                     &seed,
                                     sizeof(seed),
                                     identity,
                                     sizeof(struct GNUNET_PeerIdentity),
                                     &challenge,
                                     sizeof(challenge),
                                     ctx,
                                     sizeof(ctx),
                                     NULL);
}


#endif


#if CONG_CRYPTO_ENABLED
/**
 * Derive an XChacha20 key from key material
 *
 * @param sender peer identity of the sender
 * @param receiver peer identity of the sender
 * @param key_material high entropy key material to use
 * @param skey set to derived session key
 */
static void
derive_symmetric_key (const struct GNUNET_PeerIdentity *sender,
                      const struct GNUNET_PeerIdentity *receiver,
                      const struct GNUNET_HashCode *key_material,
                      unsigned char *skey)
{
  static const char ctx[] = "xchacha20 key generation vector";

#if DEBUG_KX
  GNUNET_log (GNUNET_ERROR_TYPE_DEBUG,
              "Deriving XChaCha20 Key for %s to %s from %s\n",
              GNUNET_i2s (sender),
              GNUNET_i2s2 (receiver),
              GNUNET_h2s (key_material));
#endif
  GNUNET_CRYPTO_kdf (skey,
                     crypto_aead_xchacha20poly1305_ietf_KEYBYTES,
                     ctx,
                     sizeof(ctx),
                     key_material,
                     sizeof(struct GNUNET_HashCode),
                     sender,
                     sizeof(struct GNUNET_PeerIdentity),
                     receiver,
                     sizeof(struct GNUNET_PeerIdentity),
                     NULL);
}


#else

/**
 * Derive an AES key from key material
 *
 * @param sender peer identity of the sender
 * @param receiver peer identity of the sender
 * @param key_material high entropy key material to use
 * @param skey set to derived session key
 */
static void
derive_aes_key (const struct GNUNET_PeerIdentity *sender,
                const struct GNUNET_PeerIdentity *receiver,
                const struct GNUNET_HashCode *key_material,
                struct GNUNET_CRYPTO_SymmetricSessionKey *skey)
{
  static const char ctx[] = "aes key generation vector";

#if DEBUG_KX
  struct GNUNET_HashCode sh;

  GNUNET_CRYPTO_hash (skey, sizeof(*skey), &sh);
  GNUNET_log (GNUNET_ERROR_TYPE_DEBUG,
              "Deriving AES Keys for %s to %s from %s\n",
              GNUNET_i2s (sender),
              GNUNET_i2s2 (receiver),
              GNUNET_h2s (key_material));
#endif
  GNUNET_CRYPTO_kdf (skey,
                     sizeof(struct GNUNET_CRYPTO_SymmetricSessionKey),
                     ctx,
                     sizeof(ctx),
                     key_material,
                     sizeof(struct GNUNET_HashCode),
                     sender,
                     sizeof(struct GNUNET_PeerIdentity),
                     receiver,
                     sizeof(struct GNUNET_PeerIdentity),
                     NULL);
}


#endif

#if ! CONG_CRYPTO_ENABLED
/**
 * Encrypt size bytes from @a in and write the result to @a out.  Use the
 * @a kx key for outbound traffic of the given neighbour.
 *
 * @param kx key information context
 * @param iv initialization vector to use
 * @param in ciphertext
 * @param out plaintext
 * @param size size of @a in / @a out
 *
 * @return #GNUNET_OK on success
 */
static int
do_encrypt (struct GSC_KeyExchangeInfo *kx,
            const struct GNUNET_CRYPTO_SymmetricInitializationVector *iv,
            const void *in,
            void *out,
            size_t size)
{
  if (size != (uint16_t) size)
  {
    GNUNET_break (0);
    return GNUNET_NO;
  }
  GNUNET_assert (size == GNUNET_CRYPTO_symmetric_encrypt (in,
                                                          (uint16_t) size,
                                                          &kx->encrypt_key,
                                                          iv,
                                                          out));
  GNUNET_STATISTICS_update (GSC_stats,
                            gettext_noop ("# bytes encrypted"),
                            size,
                            GNUNET_NO);
  /* the following is too sensitive to write to log files by accident,
     so we require manual intervention to get this one... */
#if DEBUG_KX
  GNUNET_log (GNUNET_ERROR_TYPE_DEBUG,
              "Encrypted %u bytes for `%s' using key %s, IV %u\n",
              (unsigned int) size,
              GNUNET_i2s (kx->peer),
              kx->encrypt_key.aes_key,
              GNUNET_CRYPTO_crc32_n (iv, sizeof(iv)));
#endif
  return GNUNET_OK;
}


#endif

#if ! CONG_CRYPTO_ENABLED
/**
 * Decrypt size bytes from @a in and write the result to @a out.  Use
 * the @a kx key for inbound traffic of the given neighbour.  This
 * function does NOT do any integrity-checks on the result.
 *
 * @param kx key information context
 * @param iv initialization vector to use
 * @param in ciphertext
 * @param out plaintext
 * @param size size of @a in / @a out
 * @return #GNUNET_OK on success
 */
static int
do_decrypt (struct GSC_KeyExchangeInfo *kx,
            const struct GNUNET_CRYPTO_SymmetricInitializationVector *iv,
            const void *in,
            void *out,
            size_t size)
{
  if (size != (uint16_t) size)
  {
    GNUNET_break (0);
    return GNUNET_NO;
  }
  if ((kx->status != GNUNET_CORE_KX_STATE_KEY_RECEIVED) &&
      (kx->status != GNUNET_CORE_KX_STATE_UP) &&
      (kx->status != GNUNET_CORE_KX_STATE_REKEY_SENT))
  {
    GNUNET_break_op (0);
    return GNUNET_SYSERR;
  }
  if (size != GNUNET_CRYPTO_symmetric_decrypt (in,
                                               (uint16_t) size,
                                               &kx->decrypt_key,
                                               iv,
                                               out))
  {
    GNUNET_break (0);
    return GNUNET_SYSERR;
  }
  GNUNET_STATISTICS_update (GSC_stats,
                            gettext_noop ("# bytes decrypted"),
                            size,
                            GNUNET_NO);
  /* the following is too sensitive to write to log files by accident,
     so we require manual intervention to get this one... */
#if DEBUG_KX
  GNUNET_log (GNUNET_ERROR_TYPE_DEBUG,
              "Decrypted %u bytes from `%s' using key %s, IV %u\n",
              (unsigned int) size,
              GNUNET_i2s (kx->peer),
              kx->decrypt_key.aes_key,
              GNUNET_CRYPTO_crc32_n (iv, sizeof(*iv)));
#endif
  return GNUNET_OK;
}


#endif

/**
 * Send our key (and encrypted PING) to the other peer.
 *
 * @param kx key exchange context
 */
static void
send_key (struct GSC_KeyExchangeInfo *kx);


/**
 * Task that will retry #send_key() if our previous attempt failed.
 *
 * @param cls our `struct GSC_KeyExchangeInfo`
 */
static void
set_key_retry_task (void *cls)
{
  struct GSC_KeyExchangeInfo *kx = cls;

  kx->retry_set_key_task = NULL;
  kx->set_key_retry_frequency =
    GNUNET_TIME_STD_BACKOFF (kx->set_key_retry_frequency);
  GNUNET_assert (GNUNET_CORE_KX_STATE_DOWN != kx->status);
  send_key (kx);
}


/**
 * Create a fresh PING message for transmission to the other peer.
 *
 * @param kx key exchange context to create PING for
 */
static void
setup_fresh_ping (struct GSC_KeyExchangeInfo *kx)
{
  struct PingMessage pp;
  struct PingMessage *pm;

  pm = &kx->ping;
  kx->ping_challenge =
    GNUNET_CRYPTO_random_u32 (GNUNET_CRYPTO_QUALITY_WEAK, UINT32_MAX);
  pm->header.size = htons (sizeof(struct PingMessage));
  pm->header.type = htons (GNUNET_MESSAGE_TYPE_CORE_PING);
  pp.challenge = kx->ping_challenge;
  pp.target = *kx->peer;
#if CONG_CRYPTO_ENABLED
  randombytes_buf (pm->nonce, sizeof (pm->nonce));
  GNUNET_assert (0 ==
                 crypto_aead_xchacha20poly1305_ietf_encrypt_detached (
                   (unsigned char*) &pm->target,
                   pm->tag,
                   NULL,
                   (unsigned char*) &pp.target,
                   sizeof(pp.target) + sizeof (pp.challenge),
                   NULL, 0,
                   NULL,
                   pm->nonce,
                   kx->encrypt_key));
#else
  {
    struct GNUNET_CRYPTO_SymmetricInitializationVector iv;
    pm->iv_seed = calculate_seed (kx);
    derive_iv (&iv, &kx->encrypt_key, pm->iv_seed, kx->peer);
    do_encrypt (kx,
                &iv,
                &pp.target,
                &pm->target,
                sizeof(struct PingMessage)
                - ((void *) &pm->target - (void *) pm));
  }
#endif
}


/**
 * Deliver P2P message to interested clients.  Invokes send twice,
 * once for clients that want the full message, and once for clients
 * that only want the header
 *
 * @param cls the `struct GSC_KeyExchangeInfo`
 * @param m the message
 * @return #GNUNET_OK on success,
 *    #GNUNET_NO to stop further processing (no error)
 *    #GNUNET_SYSERR to stop further processing with error
 */
static int
deliver_message (void *cls, const struct GNUNET_MessageHeader *m)
{
  struct GSC_KeyExchangeInfo *kx = cls;

  GNUNET_log (GNUNET_ERROR_TYPE_DEBUG,
              "Decrypted message of type %d from %s\n",
              ntohs (m->type),
              GNUNET_i2s (kx->peer));
  if (GNUNET_CORE_KX_STATE_UP != kx->status)
  {
    GNUNET_STATISTICS_update (GSC_stats,
                              gettext_noop ("# PAYLOAD dropped (out of order)"),
                              1,
                              GNUNET_NO);
    return GNUNET_OK;
  }
  switch (ntohs (m->type))
  {
  case GNUNET_MESSAGE_TYPE_CORE_BINARY_TYPE_MAP:
  case GNUNET_MESSAGE_TYPE_CORE_COMPRESSED_TYPE_MAP:
    GSC_SESSIONS_set_typemap (kx->peer, m);
    return GNUNET_OK;

  case GNUNET_MESSAGE_TYPE_CORE_CONFIRM_TYPE_MAP:
    GSC_SESSIONS_confirm_typemap (kx->peer, m);
    return GNUNET_OK;

  default:
    GSC_CLIENTS_deliver_message (kx->peer,
                                 m,
                                 ntohs (m->size),
                                 GNUNET_CORE_OPTION_SEND_FULL_INBOUND);
    GSC_CLIENTS_deliver_message (kx->peer,
                                 m,
                                 sizeof(struct GNUNET_MessageHeader),
                                 GNUNET_CORE_OPTION_SEND_HDR_INBOUND);
  }
  return GNUNET_OK;
}


/**
 * Function called by transport to notify us that
 * a peer connected to us (on the network level).
 * Starts the key exchange with the given peer.
 *
 * @param cls closure (NULL)
 * @param pid identity of the peer to do a key exchange with
 * @return key exchange information context
 */
static void *
handle_transport_notify_connect (void *cls,
                                 const struct GNUNET_PeerIdentity *pid,
                                 struct GNUNET_MQ_Handle *mq)
{
  struct GSC_KeyExchangeInfo *kx;
  struct GNUNET_HashCode h1;
  struct GNUNET_HashCode h2;

  GNUNET_log (GNUNET_ERROR_TYPE_DEBUG,
              "Initiating key exchange with `%s'\n",
              GNUNET_i2s (pid));
  GNUNET_STATISTICS_update (GSC_stats,
                            gettext_noop ("# key exchanges initiated"),
                            1,
                            GNUNET_NO);

  kx = GNUNET_new (struct GSC_KeyExchangeInfo);
  kx->mst = GNUNET_MST_create (&deliver_message, kx);
  kx->mq = mq;
  kx->peer = pid;
  kx->set_key_retry_frequency = INITIAL_SET_KEY_RETRY_FREQUENCY;
  GNUNET_CONTAINER_DLL_insert (kx_head, kx_tail, kx);
  kx->status = GNUNET_CORE_KX_STATE_KEY_SENT;
  monitor_notify_all (kx);
  GNUNET_CRYPTO_hash (pid, sizeof(struct GNUNET_PeerIdentity), &h1);
  GNUNET_CRYPTO_hash (&GSC_my_identity,
                      sizeof(struct GNUNET_PeerIdentity),
                      &h2);
  if (0 < GNUNET_CRYPTO_hash_cmp (&h1, &h2))
  {
    /* peer with "lower" identity starts KX, otherwise we typically end up
       with both peers starting the exchange and transmit the 'set key'
       message twice */
    send_key (kx);
  }
  else
  {
    /* peer with "higher" identity starts a delayed KX, if the "lower" peer
     * does not start a KX since it sees no reasons to do so  */
    kx->retry_set_key_task =
      GNUNET_SCHEDULER_add_delayed (GNUNET_TIME_UNIT_SECONDS,
                                    &set_key_retry_task,
                                    kx);
  }
  return kx;
}


/**
 * Function called by transport telling us that a peer
 * disconnected.
 * Stop key exchange with the given peer.  Clean up key material.
 *
 * @param cls closure
 * @param peer the peer that disconnected
 * @param handler_cls the `struct GSC_KeyExchangeInfo` of the peer
 */
static void
handle_transport_notify_disconnect (void *cls,
                                    const struct GNUNET_PeerIdentity *peer,
                                    void *handler_cls)
{
  struct GSC_KeyExchangeInfo *kx = handler_cls;

  GNUNET_log (GNUNET_ERROR_TYPE_DEBUG,
              "Peer `%s' disconnected from us.\n",
              GNUNET_i2s (peer));
  GSC_SESSIONS_end (kx->peer);
  GNUNET_STATISTICS_update (GSC_stats,
                            gettext_noop ("# key exchanges stopped"),
                            1,
                            GNUNET_NO);
  if (NULL != kx->retry_set_key_task)
  {
    GNUNET_SCHEDULER_cancel (kx->retry_set_key_task);
    kx->retry_set_key_task = NULL;
  }
  if (NULL != kx->keep_alive_task)
  {
    GNUNET_SCHEDULER_cancel (kx->keep_alive_task);
    kx->keep_alive_task = NULL;
  }
  kx->status = GNUNET_CORE_KX_PEER_DISCONNECT;
  monitor_notify_all (kx);
  GNUNET_CONTAINER_DLL_remove (kx_head, kx_tail, kx);
  GNUNET_MST_destroy (kx->mst);
  GNUNET_free (kx);
}


/**
 * Send our PING to the other peer.
 *
 * @param kx key exchange context
 */
static void
send_ping (struct GSC_KeyExchangeInfo *kx)
{
  struct GNUNET_MQ_Envelope *env;

  GNUNET_STATISTICS_update (GSC_stats,
                            gettext_noop ("# PING messages transmitted"),
                            1,
                            GNUNET_NO);
  env = GNUNET_MQ_msg_copy (&kx->ping.header);
  GNUNET_MQ_send (kx->mq, env);
}


/**
 * Derive fresh session keys from the current ephemeral keys.
 *
 * @param kx session to derive keys for
 */
static void
derive_session_keys (struct GSC_KeyExchangeInfo *kx)
{
  struct GNUNET_HashCode key_material;

  if (GNUNET_OK !=
      GNUNET_CRYPTO_ecc_ecdh (&my_ephemeral_key,
                              &kx->other_ephemeral_key,
                              &key_material))
  {
    GNUNET_break (0);
    return;
  }
#if CONG_CRYPTO_ENABLED
  derive_symmetric_key (&GSC_my_identity, kx->peer, &key_material, kx->
                        encrypt_key);
  derive_symmetric_key (kx->peer, &GSC_my_identity, &key_material, kx->
                        decrypt_key);
#else
  derive_aes_key (&GSC_my_identity, kx->peer, &key_material, &kx->
                  encrypt_key);
  derive_aes_key (kx->peer, &GSC_my_identity, &key_material, &kx->
                  decrypt_key);
#endif
  memset (&key_material, 0, sizeof(key_material));
  /* fresh key, reset sequence numbers */
  kx->last_sequence_number_received = 0;
  kx->last_packets_bitmap = 0;
  setup_fresh_ping (kx);
}


/**
 * We received a #GNUNET_MESSAGE_TYPE_CORE_EPHEMERAL_KEY message.
 * Validate and update our key material and status.
 *
 * @param cls key exchange status for the corresponding peer
 * @param m the set key message we received
 */
static void
handle_ephemeral_key (void *cls, const struct EphemeralKeyMessage *m)
{
  struct GSC_KeyExchangeInfo *kx = cls;
  struct GNUNET_TIME_Absolute start_t;
  struct GNUNET_TIME_Absolute end_t;
  struct GNUNET_TIME_Absolute now;
  enum GNUNET_CORE_KxState sender_status;
  enum GNUNET_GenericReturnValue do_verify = GNUNET_YES;

  end_t = GNUNET_TIME_absolute_ntoh (m->expiration_time);
  if (((GNUNET_CORE_KX_STATE_KEY_RECEIVED == kx->status) ||
       (GNUNET_CORE_KX_STATE_UP == kx->status) ||
       (GNUNET_CORE_KX_STATE_REKEY_SENT == kx->status)) &&
      (end_t.abs_value_us < kx->foreign_key_expires.abs_value_us))
  {
    GNUNET_STATISTICS_update (GSC_stats,
                              gettext_noop ("# old ephemeral keys ignored"),
                              1,
                              GNUNET_NO);
    GNUNET_log (GNUNET_ERROR_TYPE_WARNING,
                "Received expired EPHEMERAL_KEY from %s\n",
                GNUNET_i2s (&m->origin_identity));
    GNUNET_TRANSPORT_core_receive_continue (transport, kx->peer);
    return;
  }
  if (0 == memcmp (&m->ephemeral_key,
                   &kx->other_ephemeral_key,
                   sizeof(m->ephemeral_key)))
  {
    GNUNET_STATISTICS_update (GSC_stats,
                              gettext_noop (
                                "# duplicate ephemeral keys. Not verifying."),
                              1,
                              GNUNET_NO);
    GNUNET_log (GNUNET_ERROR_TYPE_WARNING,
                "Duplicate EPHEMERAL_KEY from %s, do not verify\n",
                GNUNET_i2s (&m->origin_identity));
    do_verify = GNUNET_NO;
  }
  if (0 != memcmp (&m->origin_identity,
                   kx->peer,
                   sizeof(struct GNUNET_PeerIdentity)))
  {
    GNUNET_log (GNUNET_ERROR_TYPE_WARNING,
                "Received EPHEMERAL_KEY from %s, but expected %s\n",
                GNUNET_i2s (&m->origin_identity),
                GNUNET_i2s_full (kx->peer));
    GNUNET_break_op (0);
    GNUNET_TRANSPORT_core_receive_continue (transport, kx->peer);
    return;
  }
  if (do_verify && ((ntohl (m->purpose.size) !=
                     sizeof(struct GNUNET_CRYPTO_EccSignaturePurpose)
                     + sizeof(struct GNUNET_TIME_AbsoluteNBO)
                     + sizeof(struct GNUNET_TIME_AbsoluteNBO)
                     + sizeof(struct GNUNET_CRYPTO_EddsaPublicKey)
                     + sizeof(struct GNUNET_CRYPTO_EddsaPublicKey)) ||
                    (GNUNET_OK !=
                     GNUNET_CRYPTO_eddsa_verify_ (
                       GNUNET_SIGNATURE_PURPOSE_SET_ECC_KEY,
                       &m->purpose,
                       &m->signature,
                       &m->origin_identity.public_key
                       ))))
  {
    /* invalid signature */
    GNUNET_break_op (0);
    GNUNET_STATISTICS_update (GSC_stats,
                              gettext_noop (
                                "# EPHEMERAL_KEYs rejected (bad signature)"),
                              1,
                              GNUNET_NO);
    GNUNET_log (GNUNET_ERROR_TYPE_WARNING,
                "Received EPHEMERAL_KEY from %s with bad signature\n",
                GNUNET_i2s (&m->origin_identity));
    GNUNET_TRANSPORT_core_receive_continue (transport, kx->peer);
    return;
  }
  now = GNUNET_TIME_absolute_get ();
  start_t = GNUNET_TIME_absolute_ntoh (m->creation_time);
  if ((end_t.abs_value_us <
       GNUNET_TIME_absolute_subtract (now, REKEY_TOLERANCE).abs_value_us) ||
      (start_t.abs_value_us >
       GNUNET_TIME_absolute_add (now, REKEY_TOLERANCE).abs_value_us))
  {
    GNUNET_log (
      GNUNET_ERROR_TYPE_WARNING,
      _ (
        "EPHEMERAL_KEY from peer `%s' rejected as its validity range does not match our system time (%llu not in [%llu,%llu]).\n"),
      GNUNET_i2s (kx->peer),
      (unsigned long long) now.abs_value_us,
      (unsigned long long) start_t.abs_value_us,
      (unsigned long long) end_t.abs_value_us);
    GNUNET_STATISTICS_update (GSC_stats,
                              gettext_noop (
                                "# EPHEMERAL_KEY messages rejected due to time")
                              ,
                              1,
                              GNUNET_NO);
    GNUNET_TRANSPORT_core_receive_continue (transport, kx->peer);
    return;
  }
#if DEBUG_KX
  {
    struct GNUNET_HashCode eh;

    GNUNET_CRYPTO_hash (&m->ephemeral_key, sizeof(m->ephemeral_key), &eh);
    GNUNET_log (GNUNET_ERROR_TYPE_DEBUG,
                "Received valid EPHEMERAL_KEY `%s' from `%s' in state %d.\n",
                GNUNET_h2s (&eh),
                GNUNET_i2s (kx->peer),
                kx->status);
  }
#endif
  GNUNET_STATISTICS_update (GSC_stats,
                            gettext_noop ("# valid ephemeral keys received"),
                            1,
                            GNUNET_NO);
  kx->other_ephemeral_key = m->ephemeral_key;
  kx->foreign_key_expires = end_t;
  derive_session_keys (kx);

  /* check if we still need to send the sender our key */
  sender_status = ntohl (m->sender_status);
  switch (sender_status)
  {
  case GNUNET_CORE_KX_STATE_DOWN:
    GNUNET_break_op (0);
    break;

  case GNUNET_CORE_KX_STATE_KEY_SENT:
    /* fine, need to send our key after updating our status, see below */
    GSC_SESSIONS_reinit (kx->peer);
    break;

  case GNUNET_CORE_KX_STATE_KEY_RECEIVED:
    /* other peer already got our key, but typemap did go down */
    GSC_SESSIONS_reinit (kx->peer);
    break;

  case GNUNET_CORE_KX_STATE_UP:
    /* other peer already got our key, typemap NOT down */
    break;

  case GNUNET_CORE_KX_STATE_REKEY_SENT:
    /* other peer already got our key, typemap NOT down */
    break;

  default:
    GNUNET_break (0);
    break;
  }
  /* check if we need to confirm everything is fine via PING + PONG */
  switch (kx->status)
  {
  case GNUNET_CORE_KX_STATE_DOWN:
    GNUNET_assert (NULL == kx->keep_alive_task);
    kx->status = GNUNET_CORE_KX_STATE_KEY_RECEIVED;
    monitor_notify_all (kx);
    if (GNUNET_CORE_KX_STATE_KEY_SENT == sender_status)
      send_key (kx);
    else
      send_ping (kx);
    break;

  case GNUNET_CORE_KX_STATE_KEY_SENT:
    GNUNET_assert (NULL == kx->keep_alive_task);
    kx->status = GNUNET_CORE_KX_STATE_KEY_RECEIVED;
    monitor_notify_all (kx);
    if (GNUNET_CORE_KX_STATE_KEY_SENT == sender_status)
      send_key (kx);
    else
      send_ping (kx);
    break;

  case GNUNET_CORE_KX_STATE_KEY_RECEIVED:
    GNUNET_assert (NULL == kx->keep_alive_task);
    if (GNUNET_CORE_KX_STATE_KEY_SENT == sender_status)
      send_key (kx);
    else
      send_ping (kx);
    break;

  case GNUNET_CORE_KX_STATE_UP:
    kx->status = GNUNET_CORE_KX_STATE_REKEY_SENT;
    monitor_notify_all (kx);
    if (GNUNET_CORE_KX_STATE_KEY_SENT == sender_status)
      send_key (kx);
    else
      send_ping (kx);
    break;

  case GNUNET_CORE_KX_STATE_REKEY_SENT:
    if (GNUNET_CORE_KX_STATE_KEY_SENT == sender_status)
      send_key (kx);
    else
      send_ping (kx);
    break;

  default:
    GNUNET_break (0);
    break;
  }
  GNUNET_TRANSPORT_core_receive_continue (transport, kx->peer);
}


static void
send_keep_alive (void *cls);


/**
 * We received a PING message.  Validate and transmit
 * a PONG message.
 *
 * @param cls key exchange status for the corresponding peer
 * @param m the encrypted PING message itself
 */
static void
handle_ping (void *cls, const struct PingMessage *m)
{
  struct GSC_KeyExchangeInfo *kx = cls;
  struct PingMessage t;
  struct PongMessage tx;
  struct PongMessage *tp;
  struct GNUNET_MQ_Envelope *env;
#if CONG_CRYPTO_ENABLED
#else
  struct GNUNET_CRYPTO_SymmetricInitializationVector iv;
#endif

  GNUNET_STATISTICS_update (GSC_stats,
                            gettext_noop ("# PING messages received"),
                            1,
                            GNUNET_NO);
  if ((kx->status != GNUNET_CORE_KX_STATE_KEY_RECEIVED) &&
      (kx->status != GNUNET_CORE_KX_STATE_UP) &&
      (kx->status != GNUNET_CORE_KX_STATE_REKEY_SENT))
  {
    /* ignore */
    GNUNET_STATISTICS_update (GSC_stats,
                              gettext_noop (
                                "# PING messages dropped (out of order)"),
                              1,
                              GNUNET_NO);
    GNUNET_TRANSPORT_core_receive_continue (transport, kx->peer);
    return;
  }
  GNUNET_log (GNUNET_ERROR_TYPE_DEBUG,
              "Core service receives PING request from `%s'.\n",
              GNUNET_i2s (kx->peer));
#if CONG_CRYPTO_ENABLED
  if (0 != crypto_aead_xchacha20poly1305_ietf_decrypt_detached (
        (unsigned char*) &t.target,
        NULL,
        (unsigned char*) &m->target,
        sizeof (m->target) + sizeof (m->challenge),
        m->tag,
        NULL, 0,
        m->nonce,
        kx->decrypt_key))
  {
    GNUNET_break_op (0);
    GNUNET_TRANSPORT_core_receive_continue (transport, kx->peer);
    return;
  }
#else
  derive_iv (&iv, &kx->decrypt_key, m->iv_seed, &GSC_my_identity);
  if (GNUNET_OK != do_decrypt (kx,
                               &iv,
                               &m->target,
                               &t.target,
                               sizeof(struct PingMessage)
                               - ((void *) &m->target - (void *) m)))
  {
    GNUNET_break_op (0);
    GNUNET_TRANSPORT_core_receive_continue (transport, kx->peer);
    return;
  }
#endif
  if (0 !=
      memcmp (&t.target, &GSC_my_identity, sizeof(struct GNUNET_PeerIdentity)))
  {
    if (GNUNET_CORE_KX_STATE_REKEY_SENT != kx->status)
      GNUNET_log (GNUNET_ERROR_TYPE_WARNING,
                  "Decryption of PING from peer `%s' failed, PING for `%s'?\n",
                  GNUNET_i2s (kx->peer),
                  GNUNET_i2s2 (&t.target));
    else
      GNUNET_log (
        GNUNET_ERROR_TYPE_DEBUG,
        "Decryption of PING from peer `%s' failed after rekey (harmless)\n",
        GNUNET_i2s (kx->peer));
    GNUNET_break_op (0);
    GNUNET_TRANSPORT_core_receive_continue (transport, kx->peer);
    return;
  }
  /* construct PONG */
  tx.reserved = 0;
  tx.challenge = t.challenge;
  tx.target = t.target;
  env = GNUNET_MQ_msg (tp, GNUNET_MESSAGE_TYPE_CORE_PONG);
#if CONG_CRYPTO_ENABLED
  randombytes_buf (tp->nonce, sizeof (tp->nonce));
  GNUNET_assert (0 ==
                 crypto_aead_xchacha20poly1305_ietf_encrypt_detached (
                   (unsigned char*) &tp->challenge,
                   tp->tag,
                   NULL,
                   (unsigned char*) &tx.challenge,
                   sizeof(struct PongMessage)
                   - ((void *) &tp->challenge - (void *) tp),
                   NULL, 0,
                   NULL,
                   tp->nonce,
                   kx->encrypt_key));
#else
  tp->iv_seed = calculate_seed (kx);
  derive_pong_iv (&iv, &kx->encrypt_key, tp->iv_seed, t.challenge, kx->peer);
  do_encrypt (kx,
              &iv,
              &tx.challenge,
              &tp->challenge,
              sizeof(struct PongMessage)
              - ((void *) &tp->challenge - (void *) tp));
#endif
  GNUNET_STATISTICS_update (GSC_stats,
                            gettext_noop ("# PONG messages created"),
                            1,
                            GNUNET_NO);
  GNUNET_MQ_send (kx->mq, env);
  if (NULL != kx->keep_alive_task)
  {
    GNUNET_SCHEDULER_cancel (kx->keep_alive_task);
    kx->keep_alive_task = GNUNET_SCHEDULER_add_delayed (MIN_PING_FREQUENCY, &
                                                        send_keep_alive, kx);
  }
  GNUNET_TRANSPORT_core_receive_continue (transport, kx->peer);
}


/**
 * Task triggered when a neighbour entry is about to time out
 * (and we should prevent this by sending a PING).
 *
 * @param cls the `struct GSC_KeyExchangeInfo`
 */
static void
send_keep_alive (void *cls)
{
  struct GSC_KeyExchangeInfo *kx = cls;
  struct GNUNET_TIME_Relative retry;
  struct GNUNET_TIME_Relative left;

  kx->keep_alive_task = NULL;
  left = GNUNET_TIME_absolute_get_remaining (kx->timeout);
  if (0 == left.rel_value_us)
  {
    GNUNET_STATISTICS_update (GSC_stats,
                              gettext_noop ("# sessions terminated by timeout"),
                              1,
                              GNUNET_NO);
    GSC_SESSIONS_end (kx->peer);
    kx->status = GNUNET_CORE_KX_STATE_KEY_SENT;
    monitor_notify_all (kx);
    send_key (kx);
    return;
  }
  GNUNET_log (GNUNET_ERROR_TYPE_DEBUG,
              "Sending KEEPALIVE to `%s'\n",
              GNUNET_i2s (kx->peer));
  GNUNET_STATISTICS_update (GSC_stats,
                            gettext_noop ("# keepalive messages sent"),
                            1,
                            GNUNET_NO);
  setup_fresh_ping (kx);
  send_ping (kx);
  retry = GNUNET_TIME_relative_max (GNUNET_TIME_relative_divide (left, 2),
                                    MIN_PING_FREQUENCY);
  kx->keep_alive_task =
    GNUNET_SCHEDULER_add_delayed (retry, &send_keep_alive, kx);
}


/**
 * We've seen a valid message from the other peer.
 * Update the time when the session would time out
 * and delay sending our keep alive message further.
 *
 * @param kx key exchange where we saw activity
 */
static void
update_timeout (struct GSC_KeyExchangeInfo *kx)
{
  struct GNUNET_TIME_Relative delta;

  kx->timeout =
    GNUNET_TIME_relative_to_absolute (GNUNET_CONSTANTS_IDLE_CONNECTION_TIMEOUT);
  delta =
    GNUNET_TIME_absolute_get_difference (kx->last_notify_timeout, kx->timeout);
  if (delta.rel_value_us > 5LL * 1000LL * 1000LL)
  {
    /* we only notify monitors about timeout changes if those
       are bigger than the threshold (5s) */
    monitor_notify_all (kx);
  }
  if (NULL != kx->keep_alive_task)
    GNUNET_SCHEDULER_cancel (kx->keep_alive_task);
  kx->keep_alive_task = GNUNET_SCHEDULER_add_delayed (
    GNUNET_TIME_relative_divide (GNUNET_CONSTANTS_IDLE_CONNECTION_TIMEOUT, 2),
    &send_keep_alive,
    kx);
}


/**
 * We received a PONG message.  Validate and update our status.
 *
 * @param kx key exchange context for the the PONG
 * @param m the encrypted PONG message itself
 */
static void
handle_pong (void *cls, const struct PongMessage *m)
{
  struct GSC_KeyExchangeInfo *kx = cls;
  struct PongMessage t;
#if CONG_CRYPTO_ENABLED
#else
  struct GNUNET_CRYPTO_SymmetricInitializationVector iv;
#endif

  GNUNET_STATISTICS_update (GSC_stats,
                            gettext_noop ("# PONG messages received"),
                            1,
                            GNUNET_NO);
  switch (kx->status)
  {
  case GNUNET_CORE_KX_STATE_DOWN:
    GNUNET_STATISTICS_update (GSC_stats,
                              gettext_noop (
                                "# PONG messages dropped (connection down)"),
                              1,
                              GNUNET_NO);
    GNUNET_TRANSPORT_core_receive_continue (transport, kx->peer);
    return;

  case GNUNET_CORE_KX_STATE_KEY_SENT:
    GNUNET_STATISTICS_update (GSC_stats,
                              gettext_noop (
                                "# PONG messages dropped (out of order)"),
                              1,
                              GNUNET_NO);
    GNUNET_TRANSPORT_core_receive_continue (transport, kx->peer);
    return;

  case GNUNET_CORE_KX_STATE_KEY_RECEIVED:
    break;

  case GNUNET_CORE_KX_STATE_UP:
    break;

  case GNUNET_CORE_KX_STATE_REKEY_SENT:
    break;

  default:
    GNUNET_break (0);
    GNUNET_TRANSPORT_core_receive_continue (transport, kx->peer);
    return;
  }
  GNUNET_log (GNUNET_ERROR_TYPE_DEBUG,
              "Core service receives PONG response from `%s'.\n",
              GNUNET_i2s (kx->peer));
  /* mark as garbage, just to be sure */
  memset (&t, 255, sizeof(t));
#if CONG_CRYPTO_ENABLED
  if (0 != crypto_aead_xchacha20poly1305_ietf_decrypt_detached (
        (unsigned char*) &t.challenge,
        NULL,
        (unsigned char*) &m->challenge,
        sizeof(struct PongMessage)
        - ((void *) &m->challenge - (void *) m),
        m->tag,
        NULL, 0,
        m->nonce,
        kx->decrypt_key))
  {
    GNUNET_break_op (0);
    GNUNET_TRANSPORT_core_receive_continue (transport, kx->peer);
    return;
  }
#else
  derive_pong_iv (&iv,
                  &kx->decrypt_key,
                  m->iv_seed,
                  kx->ping_challenge,
                  &GSC_my_identity);
  if (GNUNET_OK != do_decrypt (kx,
                               &iv,
                               &m->challenge,
                               &t.challenge,
                               sizeof(struct PongMessage)
                               - ((void *) &m->challenge - (void *) m)))
  {
    GNUNET_break_op (0);
    GNUNET_TRANSPORT_core_receive_continue (transport, kx->peer);
    return;
  }
#endif
  GNUNET_STATISTICS_update (GSC_stats,
                            gettext_noop ("# PONG messages decrypted"),
                            1,
                            GNUNET_NO);
  if ((0 !=
       memcmp (&t.target, kx->peer, sizeof(struct GNUNET_PeerIdentity))) ||
      (kx->ping_challenge != t.challenge))
  {
    /* PONG malformed */
    GNUNET_log (GNUNET_ERROR_TYPE_DEBUG,
                "Received malformed PONG wanted sender `%s' with challenge %u\n",
                GNUNET_i2s (kx->peer),
                (unsigned int) kx->ping_challenge);
    GNUNET_log (GNUNET_ERROR_TYPE_DEBUG,
                "Received malformed PONG received from `%s' with challenge %u\n",
                GNUNET_i2s (&t.target),
                (unsigned int) t.challenge);
    GNUNET_TRANSPORT_core_receive_continue (transport, kx->peer);
    return;
  }
  GNUNET_log (GNUNET_ERROR_TYPE_DEBUG,
              "Received valid PONG from `%s'\n",
              GNUNET_i2s (kx->peer));
  /* no need to resend key any longer */
  if (NULL != kx->retry_set_key_task)
  {
    GNUNET_SCHEDULER_cancel (kx->retry_set_key_task);
    kx->retry_set_key_task = NULL;
  }
  switch (kx->status)
  {
  case GNUNET_CORE_KX_STATE_DOWN:
    GNUNET_assert (0);  /* should be impossible */
    GNUNET_TRANSPORT_core_receive_continue (transport, kx->peer);
    return;

  case GNUNET_CORE_KX_STATE_KEY_SENT:
    GNUNET_assert (0);  /* should be impossible */
    GNUNET_TRANSPORT_core_receive_continue (transport, kx->peer);
    return;

  case GNUNET_CORE_KX_STATE_KEY_RECEIVED:
    GNUNET_STATISTICS_update (GSC_stats,
                              gettext_noop (
                                "# session keys confirmed via PONG"),
                              1,
                              GNUNET_NO);
    kx->status = GNUNET_CORE_KX_STATE_UP;
    monitor_notify_all (kx);
    GSC_SESSIONS_create (kx->peer, kx);
    GNUNET_assert (NULL == kx->keep_alive_task);
    update_timeout (kx);
    break;

  case GNUNET_CORE_KX_STATE_UP:
    GNUNET_STATISTICS_update (GSC_stats,
                              gettext_noop ("# timeouts prevented via PONG"),
                              1,
                              GNUNET_NO);
    update_timeout (kx);
    break;

  case GNUNET_CORE_KX_STATE_REKEY_SENT:
    GNUNET_STATISTICS_update (GSC_stats,
                              gettext_noop (
                                "# rekey operations confirmed via PONG"),
                              1,
                              GNUNET_NO);
    kx->status = GNUNET_CORE_KX_STATE_UP;
    monitor_notify_all (kx);
    update_timeout (kx);
    break;

  default:
    GNUNET_break (0);
    break;
  }
  GNUNET_TRANSPORT_core_receive_continue (transport, kx->peer);
}


/**
 * Send our key to the other peer.
 *
 * @param kx key exchange context
 */
static void
send_key (struct GSC_KeyExchangeInfo *kx)
{
  struct GNUNET_MQ_Envelope *env;

  GNUNET_assert (GNUNET_CORE_KX_STATE_DOWN != kx->status);
  if (NULL != kx->retry_set_key_task)
  {
    GNUNET_SCHEDULER_cancel (kx->retry_set_key_task);
    kx->retry_set_key_task = NULL;
  }
  /* always update sender status in SET KEY message */
#if DEBUG_KX
  {
    struct GNUNET_HashCode hc;

    GNUNET_CRYPTO_hash (&current_ekm.ephemeral_key,
                        sizeof(current_ekm.ephemeral_key),
                        &hc);
    GNUNET_log (GNUNET_ERROR_TYPE_DEBUG,
                "Sending EPHEMERAL_KEY %s to `%s' (my status: %d)\n",
                GNUNET_h2s (&hc),
                GNUNET_i2s (kx->peer),
                kx->status);
  }
#endif
  current_ekm.sender_status = htonl ((int32_t) (kx->status));
  env = GNUNET_MQ_msg_copy (&current_ekm.header);
  GNUNET_MQ_send (kx->mq, env);
  if (GNUNET_CORE_KX_STATE_KEY_SENT != kx->status)
    send_ping (kx);
  kx->retry_set_key_task =
    GNUNET_SCHEDULER_add_delayed (kx->set_key_retry_frequency,
                                  &set_key_retry_task,
                                  kx);
}


void
GSC_KX_encrypt_and_transmit (struct GSC_KeyExchangeInfo *kx,
                             const void *payload,
                             size_t payload_size)
{
  size_t used = payload_size + sizeof(struct EncryptedMessage);
  char pbuf[used]; /* plaintext */
  struct EncryptedMessage *em; /* encrypted message */
  struct EncryptedMessage *ph; /* plaintext header */
  struct GNUNET_MQ_Envelope *env;
#if CONG_CRYPTO_ENABLED
#else
  struct GNUNET_CRYPTO_SymmetricInitializationVector iv;
  struct GNUNET_CRYPTO_AuthKey auth_key;
#endif

  ph = (struct EncryptedMessage *) pbuf;
  ph->sequence_number = htonl (++kx->last_sequence_number_sent);
  ph->reserved = 0;
  ph->timestamp = GNUNET_TIME_absolute_hton (GNUNET_TIME_absolute_get ());
  GNUNET_memcpy (&ph[1], payload, payload_size);
  env = GNUNET_MQ_msg_extra (em,
                             payload_size,
                             GNUNET_MESSAGE_TYPE_CORE_ENCRYPTED_MESSAGE);
#if CONG_CRYPTO_ENABLED
  randombytes_buf (ph->nonce, sizeof (ph->nonce));
  GNUNET_memcpy (em->nonce, ph->nonce, sizeof (ph->nonce));
  GNUNET_assert (0 ==
                 crypto_aead_xchacha20poly1305_ietf_encrypt_detached (
                   (unsigned char*) &em->sequence_number,
                   em->tag,
                   NULL,
                   (unsigned char*) &ph->sequence_number,
                   used - ENCRYPTED_HEADER_SIZE,
                   NULL, 0,
                   NULL,
                   ph->nonce,
                   kx->encrypt_key));
#else
  ph->iv_seed = calculate_seed (kx);
  em->iv_seed = ph->iv_seed;
  derive_iv (&iv, &kx->encrypt_key, ph->iv_seed, kx->peer);
  GNUNET_assert (GNUNET_OK == do_encrypt (kx,
                                          &iv,
                                          &ph->sequence_number,
                                          &em->sequence_number,
                                          used - ENCRYPTED_HEADER_SIZE));
#if DEBUG_KX
  {
    struct GNUNET_HashCode hc;

    GNUNET_CRYPTO_hash (&ph->sequence_number,
                        used - ENCRYPTED_HEADER_SIZE,
                        &hc);
    GNUNET_log (GNUNET_ERROR_TYPE_DEBUG,
                "Encrypted payload `%s' of %u bytes for %s\n",
                GNUNET_h2s (&hc),
                (unsigned int) (used - ENCRYPTED_HEADER_SIZE),
                GNUNET_i2s (kx->peer));
  }
#endif
  derive_auth_key (&auth_key, &kx->encrypt_key, ph->iv_seed);
  GNUNET_CRYPTO_hmac (&auth_key,
                      &em->sequence_number,
                      used - ENCRYPTED_HEADER_SIZE,
                      &em->hmac);
#if DEBUG_KX
  {
    struct GNUNET_HashCode hc;

    GNUNET_CRYPTO_hash (&auth_key, sizeof(auth_key), &hc);
    GNUNET_log (GNUNET_ERROR_TYPE_DEBUG,
                "For peer %s, used AC %s to create hmac %s\n",
                GNUNET_i2s (kx->peer),
                GNUNET_h2s (&hc),
                GNUNET_h2s2 (&em->hmac));
  }
#endif
#endif
  kx->has_excess_bandwidth = GNUNET_NO;
  GNUNET_MQ_send (kx->mq, env);
}


/**
 * We received an encrypted message.  Check that it is
 * well-formed (size-wise).
 *
 * @param cls key exchange context for encrypting the message
 * @param m encrypted message
 * @return #GNUNET_OK if @a msg is well-formed (size-wise)
 */
static int
check_encrypted (void *cls, const struct EncryptedMessage *m)
{
  uint16_t size = ntohs (m->header.size) - sizeof(*m);

  if (size < sizeof(struct GNUNET_MessageHeader))
  {
    GNUNET_break_op (0);
    return GNUNET_SYSERR;
  }
  return GNUNET_OK;
}


/**
 * We received an encrypted message.  Decrypt, validate and
 * pass on to the appropriate clients.
 *
 * @param cls key exchange context for encrypting the message
 * @param m encrypted message
 */
static void
handle_encrypted (void *cls, const struct EncryptedMessage *m)
{
  struct GSC_KeyExchangeInfo *kx = cls;
  struct EncryptedMessage *pt; /* plaintext */
  uint32_t snum;
  struct GNUNET_TIME_Absolute t;
  uint16_t size = ntohs (m->header.size);
  char buf[size] GNUNET_ALIGN;

  if (GNUNET_CORE_KX_STATE_UP != kx->status)
  {
    GNUNET_STATISTICS_update (GSC_stats,
                              gettext_noop (
                                "# DATA message dropped (out of order)"),
                              1,
                              GNUNET_NO);
    GNUNET_TRANSPORT_core_receive_continue (transport, kx->peer);
    return;
  }
  if (0 ==
      GNUNET_TIME_absolute_get_remaining (kx->foreign_key_expires).rel_value_us)
  {
    GNUNET_log (
      GNUNET_ERROR_TYPE_WARNING,
      _ (
        "Session to peer `%s' went down due to key expiration (should not happen)\n"),
      GNUNET_i2s (kx->peer));
    GNUNET_STATISTICS_update (GSC_stats,
                              gettext_noop (
                                "# sessions terminated by key expiration"),
                              1,
                              GNUNET_NO);
    GSC_SESSIONS_end (kx->peer);
    if (NULL != kx->keep_alive_task)
    {
      GNUNET_SCHEDULER_cancel (kx->keep_alive_task);
      kx->keep_alive_task = NULL;
    }
    kx->status = GNUNET_CORE_KX_STATE_KEY_SENT;
    monitor_notify_all (kx);
    send_key (kx);
    GNUNET_TRANSPORT_core_receive_continue (transport, kx->peer);
    return;
  }

  /* validate hash */
#if DEBUG_KX
  {
    struct GNUNET_HashCode hc;

    GNUNET_CRYPTO_hash (&m->sequence_number, size - ENCRYPTED_HEADER_SIZE, &hc);
    GNUNET_log (GNUNET_ERROR_TYPE_DEBUG,
                "Received encrypted payload `%s' of %u bytes from %s\n",
                GNUNET_h2s (&hc),
                (unsigned int) (size - ENCRYPTED_HEADER_SIZE),
                GNUNET_i2s (kx->peer));
  }
#endif

#if CONG_CRYPTO_ENABLED
  if (0 != crypto_aead_xchacha20poly1305_ietf_decrypt_detached (
        (unsigned char*) &buf[ENCRYPTED_HEADER_SIZE],
        NULL,
        (unsigned char*) &m->sequence_number,
        size - ENCRYPTED_HEADER_SIZE,
        m->tag,
        NULL, 0,
        m->nonce,
        kx->decrypt_key))
  {
    GNUNET_break_op (0);
    GNUNET_TRANSPORT_core_receive_continue (transport, kx->peer);
    return;
  }
#else
  {
    struct GNUNET_CRYPTO_SymmetricInitializationVector iv;
    struct GNUNET_CRYPTO_AuthKey auth_key;
    struct GNUNET_HashCode ph;
    derive_auth_key (&auth_key, &kx->decrypt_key, m->iv_seed);
    GNUNET_CRYPTO_hmac (&auth_key,
                        &m->sequence_number,
                        size - ENCRYPTED_HEADER_SIZE,
                        &ph);
#if DEBUG_KX
    {
      struct GNUNET_HashCode hc;

      GNUNET_CRYPTO_hash (&auth_key, sizeof(auth_key), &hc);
      GNUNET_log (GNUNET_ERROR_TYPE_DEBUG,
                  "For peer %s, used AC %s to verify hmac %s\n",
                  GNUNET_i2s (kx->peer),
                  GNUNET_h2s (&hc),
                  GNUNET_h2s2 (&m->hmac));
    }
#endif
    if (0 != memcmp (&ph, &m->hmac, sizeof(struct GNUNET_HashCode)))
    {
      /* checksum failed */
      GNUNET_log (GNUNET_ERROR_TYPE_WARNING,
                  "Failed checksum validation for a message from `%s'\n",
                  GNUNET_i2s (kx->peer));
      GNUNET_TRANSPORT_core_receive_continue (transport, kx->peer);
      return;
    }
    derive_iv (&iv, &kx->decrypt_key, m->iv_seed, &GSC_my_identity);
    /* decrypt */
    if (GNUNET_OK != do_decrypt (kx,
                                 &iv,
                                 &m->sequence_number,
                                 &buf[ENCRYPTED_HEADER_SIZE],
                                 size - ENCRYPTED_HEADER_SIZE))
    {
      GNUNET_break_op (0);
      GNUNET_TRANSPORT_core_receive_continue (transport, kx->peer);
      return;
    }
  }
#endif
  GNUNET_log (GNUNET_ERROR_TYPE_DEBUG,
              "Decrypted %u bytes from %s\n",
              (unsigned int) (size - ENCRYPTED_HEADER_SIZE),
              GNUNET_i2s (kx->peer));
  pt = (struct EncryptedMessage *) buf;

  /* validate sequence number */
  snum = ntohl (pt->sequence_number);
  if (kx->last_sequence_number_received == snum)
  {
    GNUNET_log (GNUNET_ERROR_TYPE_DEBUG,
                "Received duplicate message, ignoring.\n");
    /* duplicate, ignore */
    GNUNET_STATISTICS_update (GSC_stats,
                              gettext_noop ("# bytes dropped (duplicates)"),
                              size,
                              GNUNET_NO);
    GNUNET_TRANSPORT_core_receive_continue (transport, kx->peer);
    return;
  }
  if ((kx->last_sequence_number_received > snum) &&
      (kx->last_sequence_number_received - snum > 32))
  {
    GNUNET_log (GNUNET_ERROR_TYPE_DEBUG,
                "Received ancient out of sequence message, ignoring.\n");
    /* ancient out of sequence, ignore */
    GNUNET_STATISTICS_update (GSC_stats,
                              gettext_noop (
                                "# bytes dropped (out of sequence)"),
                              size,
                              GNUNET_NO);
    GNUNET_TRANSPORT_core_receive_continue (transport, kx->peer);
    return;
  }
  if (kx->last_sequence_number_received > snum)
  {
    uint32_t rotbit = 1U << (kx->last_sequence_number_received - snum - 1);

    if ((kx->last_packets_bitmap & rotbit) != 0)
    {
      GNUNET_log (GNUNET_ERROR_TYPE_DEBUG,
                  "Received duplicate message, ignoring.\n");
      GNUNET_STATISTICS_update (GSC_stats,
                                gettext_noop ("# bytes dropped (duplicates)"),
                                size,
                                GNUNET_NO);
      /* duplicate, ignore */
      GNUNET_TRANSPORT_core_receive_continue (transport, kx->peer);
      return;
    }
    kx->last_packets_bitmap |= rotbit;
  }
  if (kx->last_sequence_number_received < snum)
  {
    unsigned int shift = (snum - kx->last_sequence_number_received);

    if (shift >= 8 * sizeof(kx->last_packets_bitmap))
      kx->last_packets_bitmap = 0;
    else
      kx->last_packets_bitmap <<= shift;
    kx->last_sequence_number_received = snum;
  }

  /* check timestamp */
  t = GNUNET_TIME_absolute_ntoh (pt->timestamp);
  if (GNUNET_TIME_absolute_get_duration (t).rel_value_us >
      MAX_MESSAGE_AGE.rel_value_us)
  {
    GNUNET_log (GNUNET_ERROR_TYPE_DEBUG,
                "Message received far too old (%s). Content ignored.\n",
                GNUNET_STRINGS_relative_time_to_string (
                  GNUNET_TIME_absolute_get_duration (t),
                  GNUNET_YES));
    GNUNET_STATISTICS_update (GSC_stats,
                              gettext_noop (
                                "# bytes dropped (ancient message)"),
                              size,
                              GNUNET_NO);
    GNUNET_TRANSPORT_core_receive_continue (transport, kx->peer);
    return;
  }

  /* process decrypted message(s) */
  update_timeout (kx);
  GNUNET_STATISTICS_update (GSC_stats,
                            gettext_noop ("# bytes of payload decrypted"),
                            size - sizeof(struct EncryptedMessage),
                            GNUNET_NO);
  if (GNUNET_OK !=
      GNUNET_MST_from_buffer (kx->mst,
                              &buf[sizeof(struct EncryptedMessage)],
                              size - sizeof(struct EncryptedMessage),
                              GNUNET_YES,
                              GNUNET_NO))
    GNUNET_break_op (0);

  GNUNET_TRANSPORT_core_receive_continue (transport, kx->peer);
}


/**
 * Setup the message that links the ephemeral key to our persistent
 * public key and generate the appropriate signature.
 */
static void
sign_ephemeral_key ()
{
  current_ekm.header.size = htons (sizeof(struct EphemeralKeyMessage));
  current_ekm.header.type = htons (GNUNET_MESSAGE_TYPE_CORE_EPHEMERAL_KEY);
  current_ekm.sender_status = 0; /* to be set later */
  current_ekm.purpose.purpose = htonl (GNUNET_SIGNATURE_PURPOSE_SET_ECC_KEY);
  current_ekm.purpose.size =
    htonl (sizeof(struct GNUNET_CRYPTO_EccSignaturePurpose)
           + sizeof(struct GNUNET_TIME_AbsoluteNBO)
           + sizeof(struct GNUNET_TIME_AbsoluteNBO)
           + sizeof(struct GNUNET_CRYPTO_EcdhePublicKey)
           + sizeof(struct GNUNET_PeerIdentity));
  current_ekm.creation_time =
    GNUNET_TIME_absolute_hton (GNUNET_TIME_absolute_get ());
  if (GNUNET_YES == GNUNET_CONFIGURATION_get_value_yesno (GSC_cfg,
                                                          "core",
                                                          "USE_EPHEMERAL_KEYS"))
  {
    current_ekm.expiration_time =
      GNUNET_TIME_absolute_hton (GNUNET_TIME_relative_to_absolute (
                                   GNUNET_TIME_relative_add (REKEY_FREQUENCY,
                                                             REKEY_TOLERANCE)));
  }
  else
  {
    current_ekm.expiration_time =
      GNUNET_TIME_absolute_hton (GNUNET_TIME_UNIT_FOREVER_ABS);
  }
  GNUNET_CRYPTO_ecdhe_key_get_public (&my_ephemeral_key,
                                      &current_ekm.ephemeral_key);
  current_ekm.origin_identity = GSC_my_identity;
  GNUNET_assert (GNUNET_OK ==
                 GNUNET_CRYPTO_eddsa_sign_ (&my_private_key,
                                            &current_ekm.purpose,
                                            &current_ekm.signature));
}


/**
 * Task run to trigger rekeying.
 *
 * @param cls closure, NULL
 */
static void
do_rekey (void *cls)
{
  struct GSC_KeyExchangeInfo *pos;

  (void) cls;
  rekey_task = GNUNET_SCHEDULER_add_delayed (REKEY_FREQUENCY, &do_rekey, NULL);
  GNUNET_CRYPTO_ecdhe_key_create (&my_ephemeral_key);
  sign_ephemeral_key ();
  {
    struct GNUNET_HashCode eh;

    GNUNET_CRYPTO_hash (&current_ekm.ephemeral_key,
                        sizeof(current_ekm.ephemeral_key),
                        &eh);
    GNUNET_log (GNUNET_ERROR_TYPE_INFO, "Rekeying to %s\n", GNUNET_h2s (&eh));
  }
  for (pos = kx_head; NULL != pos; pos = pos->next)
  {
    if (GNUNET_CORE_KX_STATE_UP == pos->status)
    {
      pos->status = GNUNET_CORE_KX_STATE_REKEY_SENT;
      derive_session_keys (pos);
    }
    else if (GNUNET_CORE_KX_STATE_DOWN == pos->status)
    {
      pos->status = GNUNET_CORE_KX_STATE_KEY_SENT;
    }
    monitor_notify_all (pos);
    send_key (pos);
  }
}


/**
 * Initialize KX subsystem.
 *
 * @param pk private key to use for the peer
 * @return #GNUNET_OK on success, #GNUNET_SYSERR on failure
 */
int
GSC_KX_init (struct GNUNET_CRYPTO_EddsaPrivateKey *pk)
{
  struct GNUNET_MQ_MessageHandler handlers[] = {
    GNUNET_MQ_hd_fixed_size (ephemeral_key,
                             GNUNET_MESSAGE_TYPE_CORE_EPHEMERAL_KEY,
                             struct EphemeralKeyMessage,
                             NULL),
    GNUNET_MQ_hd_fixed_size (ping,
                             GNUNET_MESSAGE_TYPE_CORE_PING,
                             struct PingMessage,
                             NULL),
    GNUNET_MQ_hd_fixed_size (pong,
                             GNUNET_MESSAGE_TYPE_CORE_PONG,
                             struct PongMessage,
                             NULL),
    GNUNET_MQ_hd_var_size (encrypted,
                           GNUNET_MESSAGE_TYPE_CORE_ENCRYPTED_MESSAGE,
                           struct EncryptedMessage,
                           NULL),
    GNUNET_MQ_handler_end ()
  };

  my_private_key = *pk;
  GNUNET_CRYPTO_eddsa_key_get_public (&my_private_key,
                                      &GSC_my_identity.public_key);
  GNUNET_CRYPTO_ecdhe_key_create (&my_ephemeral_key);
  sign_ephemeral_key ();
  {
    struct GNUNET_HashCode eh;

    GNUNET_CRYPTO_hash (&current_ekm.ephemeral_key,
                        sizeof(current_ekm.ephemeral_key),
                        &eh);
    GNUNET_log (GNUNET_ERROR_TYPE_INFO,
                "Starting with ephemeral key %s\n",
                GNUNET_h2s (&eh));
  }

  nc = GNUNET_notification_context_create (1);
  rekey_task = GNUNET_SCHEDULER_add_delayed (REKEY_FREQUENCY, &do_rekey, NULL);
  transport =
    GNUNET_TRANSPORT_core_connect (GSC_cfg,
                                   &GSC_my_identity,
                                   handlers,
                                   NULL,
                                   &handle_transport_notify_connect,
                                   &handle_transport_notify_disconnect);
  if (NULL == transport)
  {
    GSC_KX_done ();
    return GNUNET_SYSERR;
  }
  return GNUNET_OK;
}


/**
 * Shutdown KX subsystem.
 */
void
GSC_KX_done ()
{
  if (NULL != transport)
  {
    GNUNET_TRANSPORT_core_disconnect (transport);
    transport = NULL;
  }
  if (NULL != rekey_task)
  {
    GNUNET_SCHEDULER_cancel (rekey_task);
    rekey_task = NULL;
  }
  memset (&my_ephemeral_key,
          0,
          sizeof (my_ephemeral_key));
  memset (&my_private_key,
          0,
          sizeof (my_private_key));
  if (NULL != nc)
  {
    GNUNET_notification_context_destroy (nc);
    nc = NULL;
  }
}


/**
 * Check how many messages are queued for the given neighbour.
 *
 * @param kxinfo data about neighbour to check
 * @return number of items in the message queue
 */
unsigned int
GSC_NEIGHBOURS_get_queue_length (const struct GSC_KeyExchangeInfo *kxinfo)
{
  return GNUNET_MQ_get_length (kxinfo->mq);
}


int
GSC_NEIGHBOURS_check_excess_bandwidth (const struct GSC_KeyExchangeInfo *kxinfo)
{
  return kxinfo->has_excess_bandwidth;
}


/**
 * Handle #GNUNET_MESSAGE_TYPE_CORE_MONITOR_PEERS request.  For this
 * request type, the client does not have to have transmitted an INIT
 * request.  All current peers are returned, regardless of which
 * message types they accept.
 *
 * @param mq message queue to add for monitoring
 */
void
GSC_KX_handle_client_monitor_peers (struct GNUNET_MQ_Handle *mq)
{
  struct GNUNET_MQ_Envelope *env;
  struct MonitorNotifyMessage *done_msg;
  struct GSC_KeyExchangeInfo *kx;

  GNUNET_notification_context_add (nc, mq);
  for (kx = kx_head; NULL != kx; kx = kx->next)
  {
    struct GNUNET_MQ_Envelope *env_notify;
    struct MonitorNotifyMessage *msg;

    env_notify = GNUNET_MQ_msg (msg, GNUNET_MESSAGE_TYPE_CORE_MONITOR_NOTIFY);
    msg->state = htonl ((uint32_t) kx->status);
    msg->peer = *kx->peer;
    msg->timeout = GNUNET_TIME_absolute_hton (kx->timeout);
    GNUNET_MQ_send (mq, env_notify);
  }
  env = GNUNET_MQ_msg (done_msg, GNUNET_MESSAGE_TYPE_CORE_MONITOR_NOTIFY);
  done_msg->state = htonl ((uint32_t) GNUNET_CORE_KX_ITERATION_FINISHED);
  done_msg->timeout = GNUNET_TIME_absolute_hton (GNUNET_TIME_UNIT_FOREVER_ABS);
  GNUNET_MQ_send (mq, env);
}


/* end of gnunet-service-core_kx.c */
