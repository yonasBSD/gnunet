/*
     This file is part of GNUnet.
     Copyright (C) 2012-2013 GNUnet e.V.

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
 * @file gnunet-dns2gns.c
 * @brief DNS server that translates DNS requests to GNS
 * @author Christian Grothoff
 */
#include "platform.h"
#include <gnunet_util_lib.h>
#include <gnunet_gns_service.h>
#include "gnunet_vpn_service.h"

/**
 * Timeout for DNS requests.
 */
#define TIMEOUT GNUNET_TIME_UNIT_MINUTES

/**
 * Default timeout for VPN redirections.
 */
#define VPN_TIMEOUT GNUNET_TIME_relative_multiply (GNUNET_TIME_UNIT_MINUTES, 30)


struct Request;

/**
 * Closure for #vpn_allocation_cb.
 */
struct VpnContext
{
  /**
   * Which resolution process are we processing.
   */
  struct Request *request;

  /**
   * Handle to the VPN request that we were performing.
   */
  struct GNUNET_VPN_RedirectionRequest *vpn_request;

  /**
   * Number of records serialized in @e rd_data.
   */
  unsigned int rd_count;

  /**
   * Serialized records.
   */
  char *rd_data;

  /**
   * Number of bytes in @e rd_data.
   */
  ssize_t rd_data_size;
};


/**
 * Data kept per request.
 */
struct Request
{
  /**
   * Socket to use for sending the reply.
   */
  struct GNUNET_NETWORK_Handle *lsock;

  /**
   * Destination address to use.
   */
  const void *addr;

  /**
   * Initially, this is the DNS request, it will then be
   * converted to the DNS response.
   */
  struct GNUNET_DNSPARSER_Packet *packet;

  /**
   * Our GNS request handle.
   */
  struct GNUNET_GNS_LookupWithTldRequest *lookup;

  /**
   * Our DNS request handle
   */
  struct GNUNET_DNSSTUB_RequestSocket *dns_lookup;

  /**
   * Task run on timeout or shutdown to clean up without
   * response.
   */
  struct GNUNET_SCHEDULER_Task *timeout_task;

  /**
   * Vpn resolution context
   */
  struct VpnContext *vpn_ctx;

  /**
   * Original UDP request message.
   */
  char *udp_msg;

  /**
   * Number of bytes in @e addr.
   */
  size_t addr_len;

  /**
   * Number of bytes in @e udp_msg.
   */
  size_t udp_msg_size;

  /**
   * ID of the original request.
   */
  uint16_t original_request_id;

};

/**
 * The address to bind to
 */
static in_addr_t address;

/**
 * The IPv6 address to bind to
 */
static struct in6_addr address6;


/**
 * Handle to GNS resolver.
 */
struct GNUNET_GNS_Handle *gns;

/**
 * Our handle to the vpn service
 */
static struct GNUNET_VPN_Handle *vpn_handle;

/**
 * Stub resolver
 */
struct GNUNET_DNSSTUB_Context *dns_stub;

/**
 * Listen socket for IPv4.
 */
static struct GNUNET_NETWORK_Handle *listen_socket4;

/**
 * Listen socket for IPv6.
 */
static struct GNUNET_NETWORK_Handle *listen_socket6;

/**
 * Task for IPv4 socket.
 */
static struct GNUNET_SCHEDULER_Task *t4;

/**
 * Task for IPv6 socket.
 */
static struct GNUNET_SCHEDULER_Task *t6;

/**
 * IP of DNS server
 */
static char *dns_ip;

/**
 * UDP Port we listen on for inbound DNS requests.
 */
static unsigned long long listen_port = 53;

/**
 * Configuration to use.
 */
static const struct GNUNET_CONFIGURATION_Handle *cfg;


/**
 * Task run on shutdown.  Cleans up everything.
 *
 * @param cls unused
 */
static void
do_shutdown (void *cls)
{
  (void) cls;
  if (NULL != t4)
  {
    GNUNET_SCHEDULER_cancel (t4);
    t4 = NULL;
  }
  if (NULL != t6)
  {
    GNUNET_SCHEDULER_cancel (t6);
    t6 = NULL;
  }
  if (NULL != listen_socket4)
  {
    GNUNET_NETWORK_socket_close (listen_socket4);
    listen_socket4 = NULL;
  }
  if (NULL != listen_socket6)
  {
    GNUNET_NETWORK_socket_close (listen_socket6);
    listen_socket6 = NULL;
  }
  if (NULL != gns)
  {
    GNUNET_GNS_disconnect (gns);
    gns = NULL;
  }
  if (NULL != vpn_handle)
  {
    GNUNET_VPN_disconnect (vpn_handle);
    vpn_handle = NULL;
  }
  if (NULL != dns_stub)
  {
    GNUNET_DNSSTUB_stop (dns_stub);
    dns_stub = NULL;
  }
}


/**
 * Shuffle answers
 * Fisher-Yates (aka Knuth) Shuffle
 *
 * @param request context for the request (with answers)
 */
static void
shuffle_answers (struct Request *request)
{
  unsigned int idx = request->packet->num_answers;
  unsigned int r_idx;
  struct GNUNET_DNSPARSER_Record tmp_answer;

  while (0 != idx)
  {
    r_idx = GNUNET_CRYPTO_random_u32 (GNUNET_CRYPTO_QUALITY_WEAK,
                                      request->packet->num_answers);
    idx--;
    tmp_answer = request->packet->answers[idx];
    memcpy (&request->packet->answers[idx], &request->packet->answers[r_idx],
            sizeof (struct GNUNET_DNSPARSER_Record));
    memcpy (&request->packet->answers[r_idx], &tmp_answer,
            sizeof (struct GNUNET_DNSPARSER_Record));
  }
}


/**
 * Send the response for the given request and clean up.
 *
 * @param request context for the request.
 */
static void
send_response (struct Request *request)
{
  char *buf;
  size_t size;
  ssize_t sret;

  shuffle_answers (request);
  if (GNUNET_SYSERR ==
      GNUNET_DNSPARSER_pack (request->packet,
                             UINT16_MAX /* is this not too much? */,
                             &buf,
                             &size))
  {
    GNUNET_log (GNUNET_ERROR_TYPE_WARNING,
                _ ("Failed to pack DNS response into UDP packet!\n"));
  }
  else
  {
    sret = GNUNET_NETWORK_socket_sendto (request->lsock,
                                         buf,
                                         size,
                                         request->addr,
                                         request->addr_len);
    if ((sret < 0) ||
        (size != (size_t) sret))
      GNUNET_log_strerror (GNUNET_ERROR_TYPE_WARNING,
                           "sendto");
    GNUNET_free (buf);
  }
  GNUNET_SCHEDULER_cancel (request->timeout_task);
  GNUNET_DNSPARSER_free_packet (request->packet);
  GNUNET_free (request->udp_msg);
  GNUNET_free (request);
}


/**
 * Task run on timeout.  Cleans up request.
 *
 * @param cls `struct Request *` of the request to clean up
 */
static void
do_timeout (void *cls)
{
  struct Request *request = cls;
  struct VpnContext *vpn_ctx;

  if (NULL != request->packet)
    GNUNET_DNSPARSER_free_packet (request->packet);
  if (NULL != request->lookup)
    GNUNET_GNS_lookup_with_tld_cancel (request->lookup);
  if (NULL != request->dns_lookup)
    GNUNET_DNSSTUB_resolve_cancel (request->dns_lookup);
  GNUNET_free (request->udp_msg);
  if (NULL != (vpn_ctx = request->vpn_ctx))
  {
    GNUNET_VPN_cancel_request (vpn_ctx->vpn_request);
    GNUNET_free (vpn_ctx->rd_data);
    GNUNET_free (vpn_ctx);
  }
  GNUNET_free (request);
}


/**
 * Iterator called on obtained result for a DNS lookup
 *
 * @param cls closure
 * @param dns the DNS udp payload
 * @param r size of the DNS payload
 */
static void
dns_result_processor (void *cls,
                      const struct GNUNET_TUN_DnsHeader *dns,
                      size_t r)
{
  struct Request *request = cls;

  if (NULL == dns)
  {
    /* DNSSTUB gave up, so we trigger timeout early */
    GNUNET_SCHEDULER_cancel (request->timeout_task);
    do_timeout (request);
    return;
  }
  if (request->original_request_id != dns->id)
  {
    /* for a another query, ignore */
    return;
  }
  request->packet = GNUNET_DNSPARSER_parse ((char *) dns,
                                            r);
  if (NULL == request->packet)
  {
    GNUNET_log (GNUNET_ERROR_TYPE_WARNING,
                _ ("Failed to parse DNS response!\n"));
    GNUNET_SCHEDULER_cancel (request->timeout_task);
    do_timeout (request);
    return;
  }
  GNUNET_DNSSTUB_resolve_cancel (request->dns_lookup);
  send_response (request);
}


/**
 * Callback invoked from the VPN service once a redirection is
 * available.  Provides the IP address that can now be used to
 * reach the requested destination.  Replaces the "VPN" record
 * with the respective A/AAAA record and continues processing.
 *
 * @param cls closure
 * @param af address family, AF_INET or AF_INET6; AF_UNSPEC on error;
 *                will match 'result_af' from the request
 * @param address IP address (struct in_addr or struct in_addr6, depending on 'af')
 *                that the VPN allocated for the redirection;
 *                traffic to this IP will now be redirected to the
 *                specified target peer; NULL on error
 */
static void
vpn_allocation_cb (void *cls,
                   int af,
                   const void *vaddress)
{
  struct VpnContext *vpn_ctx = cls;
  struct Request *request = vpn_ctx->request;
  struct GNUNET_GNSRECORD_Data rd[vpn_ctx->rd_count];
  unsigned int i;

  vpn_ctx->vpn_request = NULL;
  request->vpn_ctx = NULL;
  GNUNET_assert (GNUNET_OK ==
                 GNUNET_GNSRECORD_records_deserialize (
                   (size_t) vpn_ctx->rd_data_size,
                   vpn_ctx->rd_data,
                   vpn_ctx->rd_count,
                   rd));
  for (i = 0; i < vpn_ctx->rd_count; i++)
  {
    if (GNUNET_GNSRECORD_TYPE_VPN == rd[i].record_type)
    {
      switch (af)
      {
      case AF_INET:
        rd[i].record_type = GNUNET_DNSPARSER_TYPE_A;
        rd[i].data_size = sizeof(struct in_addr);
        rd[i].expiration_time = GNUNET_TIME_relative_to_absolute (
          VPN_TIMEOUT).abs_value_us;
        rd[i].flags = 0;
        rd[i].data = vaddress;
        break;

      case AF_INET6:
        rd[i].record_type = GNUNET_DNSPARSER_TYPE_AAAA;
        rd[i].expiration_time = GNUNET_TIME_relative_to_absolute (
          VPN_TIMEOUT).abs_value_us;
        rd[i].flags = 0;
        rd[i].data = vaddress;
        rd[i].data_size = sizeof(struct in6_addr);
        break;

      default:
        GNUNET_assert (0);
      }
      break;
    }
  }
  GNUNET_assert (i < vpn_ctx->rd_count);
  if (0 == vpn_ctx->rd_count)
    GNUNET_log (GNUNET_ERROR_TYPE_WARNING,
                _ ("VPN returned empty result for `%s'\n"),
                request->packet->queries[0].name);
  send_response (request);
  GNUNET_free (vpn_ctx->rd_data);
  GNUNET_free (vpn_ctx);
}


/**
 * Iterator called on obtained result for a GNS lookup.
 *
 * @param cls closure
 * @param was_gns #GNUNET_NO if the TLD is not configured for GNS
 * @param rd_count number of records in @a rd
 * @param rd the records in reply
 */
static void
result_processor (void *cls,
                  int was_gns,
                  uint32_t rd_count,
                  const struct GNUNET_GNSRECORD_Data *rd)
{
  struct Request *request = cls;
  struct GNUNET_DNSPARSER_Packet *packet;
  struct GNUNET_DNSPARSER_Record rec;
  struct VpnContext *vpn_ctx;
  const struct GNUNET_TUN_GnsVpnRecord *vpn;
  const char *vname;
  struct GNUNET_HashCode vhash;
  int af;

  request->lookup = NULL;
  if (GNUNET_NO == was_gns)
  {
    /* TLD not configured for GNS, fall back to DNS */
    GNUNET_log (GNUNET_ERROR_TYPE_DEBUG,
                "Using DNS resolver IP `%s' to resolve `%s'\n",
                dns_ip,
                request->packet->queries[0].name);
    request->original_request_id = request->packet->id;
    GNUNET_DNSPARSER_free_packet (request->packet);
    request->packet = NULL;
    request->dns_lookup = GNUNET_DNSSTUB_resolve (dns_stub,
                                                  request->udp_msg,
                                                  request->udp_msg_size,
                                                  &dns_result_processor,
                                                  request);
    return;
  }
  packet = request->packet;
  packet->flags.query_or_response = 1;
  packet->flags.return_code = GNUNET_TUN_DNS_RETURN_CODE_NO_ERROR;
  packet->flags.checking_disabled = 0;
  packet->flags.authenticated_data = 1;
  packet->flags.zero = 0;
  packet->flags.recursion_available = 1;
  packet->flags.message_truncated = 0;
  packet->flags.authoritative_answer = 0;
  // packet->flags.opcode = GNUNET_TUN_DNS_OPCODE_STATUS; // ???
  for (uint32_t i = 0; i < rd_count; i++)
  {
    rec.expiration_time.abs_value_us = rd[i].expiration_time;
    switch (rd[i].record_type)
    {
    case GNUNET_DNSPARSER_TYPE_A:
      GNUNET_assert (sizeof(struct in_addr) == rd[i].data_size);
      rec.name = GNUNET_strdup (packet->queries[0].name);
      rec.dns_traffic_class = GNUNET_TUN_DNS_CLASS_INTERNET;
      rec.type = GNUNET_DNSPARSER_TYPE_A;
      rec.data.raw.data = GNUNET_new (struct in_addr);
      GNUNET_memcpy (rec.data.raw.data,
                     rd[i].data,
                     rd[i].data_size);
      rec.data.raw.data_len = sizeof(struct in_addr);
      GNUNET_array_append (packet->answers,
                           packet->num_answers,
                           rec);
      break;

    case GNUNET_DNSPARSER_TYPE_AAAA:
      GNUNET_assert (sizeof(struct in6_addr) == rd[i].data_size);
      rec.name = GNUNET_strdup (packet->queries[0].name);
      rec.data.raw.data = GNUNET_new (struct in6_addr);
      rec.dns_traffic_class = GNUNET_TUN_DNS_CLASS_INTERNET;
      rec.type = GNUNET_DNSPARSER_TYPE_AAAA;
      GNUNET_memcpy (rec.data.raw.data,
                     rd[i].data,
                     rd[i].data_size);
      rec.data.raw.data_len = sizeof(struct in6_addr);
      GNUNET_array_append (packet->answers,
                           packet->num_answers,
                           rec);
      break;

    case GNUNET_DNSPARSER_TYPE_CNAME:
      rec.name = GNUNET_strdup (packet->queries[0].name);
      rec.data.hostname = GNUNET_strdup (rd[i].data);
      rec.dns_traffic_class = GNUNET_TUN_DNS_CLASS_INTERNET;
      rec.type = GNUNET_DNSPARSER_TYPE_CNAME;
      GNUNET_memcpy (rec.data.hostname,
                     rd[i].data,
                     rd[i].data_size);
      GNUNET_array_append (packet->answers,
                           packet->num_answers,
                           rec);
      break;
    case GNUNET_GNSRECORD_TYPE_VPN:
      if ((GNUNET_DNSPARSER_TYPE_A != request->packet->queries[0].type) &&
          (GNUNET_DNSPARSER_TYPE_AAAA != request->packet->queries[0].type))
        break;
      af = (GNUNET_DNSPARSER_TYPE_A == request->packet->queries[0].type) ?
           AF_INET :
           AF_INET6;
      if (sizeof(struct GNUNET_TUN_GnsVpnRecord) >
          rd[i].data_size)
      {
        GNUNET_break_op (0);
        break;
      }
      vpn = (const struct GNUNET_TUN_GnsVpnRecord *) rd[i].data;
      vname = (const char *) &vpn[1];
      if ('\0' != vname[rd[i].data_size - 1 - sizeof(struct
                                                     GNUNET_TUN_GnsVpnRecord)
          ])
      {
        GNUNET_break_op (0);
        break;
      }
      GNUNET_TUN_service_name_to_hash (vname,
                                       &vhash);
      GNUNET_log (GNUNET_ERROR_TYPE_DEBUG,
                  "Attempting VPN allocation for %s-%s (AF: %d, proto %d)\n",
                  GNUNET_i2s (&vpn->peer),
                  vname,
                  (int) af,
                  (int) ntohs (vpn->proto));
      vpn_ctx = GNUNET_new (struct VpnContext);
      request->vpn_ctx = vpn_ctx;
      vpn_ctx->request = request;
      vpn_ctx->rd_data_size = GNUNET_GNSRECORD_records_get_size (rd_count,
                                                                 rd);
      if (vpn_ctx->rd_data_size < 0)
      {
        GNUNET_break_op (0);
        GNUNET_free (vpn_ctx);
        break;
      }
      vpn_ctx->rd_data = GNUNET_malloc ((size_t) vpn_ctx->rd_data_size);
      vpn_ctx->rd_count = rd_count;
      GNUNET_assert (vpn_ctx->rd_data_size ==
                     GNUNET_GNSRECORD_records_serialize (rd_count,
                                                         rd,
                                                         (size_t) vpn_ctx
                                                         ->rd_data_size,
                                                         vpn_ctx->rd_data));
      vpn_ctx->vpn_request = GNUNET_VPN_redirect_to_peer (vpn_handle,
                                                          af,
                                                          ntohs (
                                                            vpn->proto),
                                                          &vpn->peer,
                                                          &vhash,
                                                          GNUNET_TIME_relative_to_absolute
                                                          (
                                                            VPN_TIMEOUT),
                                                          &
                                                          vpn_allocation_cb,
                                                          vpn_ctx);
      return;


    default:
      /* skip */
      break;
    }
  }
  send_response (request);
}


/**
 * Handle DNS request.
 *
 * @param lsock socket to use for sending the reply
 * @param addr address to use for sending the reply
 * @param addr_len number of bytes in @a addr
 * @param udp_msg DNS request payload
 * @param udp_msg_size number of bytes in @a udp_msg
 */
static void
handle_request (struct GNUNET_NETWORK_Handle *lsock,
                const void *addr,
                size_t addr_len,
                const char *udp_msg,
                size_t udp_msg_size)
{
  struct Request *request;
  struct GNUNET_DNSPARSER_Packet *packet;

  packet = GNUNET_DNSPARSER_parse (udp_msg,
                                   udp_msg_size);
  if (NULL == packet)
  {
    GNUNET_log (GNUNET_ERROR_TYPE_WARNING,
                _ ("Cannot parse DNS request from %s\n"),
                GNUNET_a2s (addr, addr_len));
    return;
  }
  GNUNET_log (GNUNET_ERROR_TYPE_DEBUG,
              "Received request for `%s' with flags %u, #answers %d, #auth %d, #additional %d\n",
              packet->queries[0].name,
              (unsigned int) packet->flags.query_or_response,
              (int) packet->num_answers,
              (int) packet->num_authority_records,
              (int) packet->num_additional_records);
  if ((0 != packet->flags.query_or_response) ||
      (0 != packet->num_answers) ||
      (0 != packet->num_authority_records))
  {
    GNUNET_log (GNUNET_ERROR_TYPE_WARNING,
                _ ("Received malformed DNS request from %s\n"),
                GNUNET_a2s (addr, addr_len));
    GNUNET_DNSPARSER_free_packet (packet);
    return;
  }
  if ((1 != packet->num_queries))
  {
    GNUNET_log (GNUNET_ERROR_TYPE_WARNING,
                _ ("Received unsupported DNS request from %s\n"),
                GNUNET_a2s (addr,
                            addr_len));
    GNUNET_DNSPARSER_free_packet (packet);
    return;
  }
  request = GNUNET_malloc (sizeof(struct Request) + addr_len);
  request->lsock = lsock;
  request->packet = packet;
  request->addr = &request[1];
  request->addr_len = addr_len;
  GNUNET_memcpy (&request[1],
                 addr,
                 addr_len);
  request->udp_msg_size = udp_msg_size;
  request->udp_msg = GNUNET_memdup (udp_msg,
                                    udp_msg_size);
  request->timeout_task = GNUNET_SCHEDULER_add_delayed (TIMEOUT,
                                                        &do_timeout,
                                                        request);
  GNUNET_log (GNUNET_ERROR_TYPE_DEBUG,
              "Calling GNS on `%s'\n",
              packet->queries[0].name);
  request->lookup = GNUNET_GNS_lookup_with_tld (gns,
                                                packet->queries[0].name,
                                                packet->queries[0].type,
                                                GNUNET_GNS_LO_DEFAULT,
                                                &result_processor,
                                                request);
}


/**
 * Task to read IPv4 DNS packets.
 *
 * @param cls the 'listen_socket4'
 */
static void
read_dns4 (void *cls)
{
  struct sockaddr_in v4;
  socklen_t addrlen;
  ssize_t size;
  const struct GNUNET_SCHEDULER_TaskContext *tc;

  GNUNET_assert (listen_socket4 == cls);
  t4 = GNUNET_SCHEDULER_add_read_net (GNUNET_TIME_UNIT_FOREVER_REL,
                                      listen_socket4,
                                      &read_dns4,
                                      listen_socket4);
  tc = GNUNET_SCHEDULER_get_task_context ();
  if (0 == (GNUNET_SCHEDULER_REASON_READ_READY & tc->reason))
    return; /* shutdown? */
  size = GNUNET_NETWORK_socket_recvfrom_amount (listen_socket4);
  if (0 > size)
  {
    GNUNET_break (0);
    return;   /* read error!? */
  }
  {
    char buf[size + 1];
    ssize_t sret;

    addrlen = sizeof(v4);
    sret = GNUNET_NETWORK_socket_recvfrom (listen_socket4,
                                           buf,
                                           size + 1,
                                           (struct sockaddr *) &v4,
                                           &addrlen);
    if (0 > sret)
    {
      GNUNET_log_strerror (GNUNET_ERROR_TYPE_WARNING,
                           "recvfrom");
      return;
    }
    GNUNET_break (size != sret);
    handle_request (listen_socket4,
                    &v4,
                    addrlen,
                    buf,
                    size);
  }
}


/**
 * Task to read IPv6 DNS packets.
 *
 * @param cls the 'listen_socket6'
 */
static void
read_dns6 (void *cls)
{
  struct sockaddr_in6 v6;
  socklen_t addrlen;
  ssize_t size;
  const struct GNUNET_SCHEDULER_TaskContext *tc;

  GNUNET_assert (listen_socket6 == cls);
  t6 = GNUNET_SCHEDULER_add_read_net (GNUNET_TIME_UNIT_FOREVER_REL,
                                      listen_socket6,
                                      &read_dns6,
                                      listen_socket6);
  tc = GNUNET_SCHEDULER_get_task_context ();
  if (0 == (GNUNET_SCHEDULER_REASON_READ_READY & tc->reason))
    return; /* shutdown? */
  size = GNUNET_NETWORK_socket_recvfrom_amount (listen_socket6);
  if (0 > size)
  {
    GNUNET_break (0);
    return;   /* read error!? */
  }
  {
    char buf[size];
    ssize_t sret;

    addrlen = sizeof(v6);
    sret = GNUNET_NETWORK_socket_recvfrom (listen_socket6,
                                           buf,
                                           size,
                                           (struct sockaddr *) &v6,
                                           &addrlen);
    if (0 > sret)
    {
      GNUNET_log_strerror (GNUNET_ERROR_TYPE_WARNING,
                           "recvfrom");
      return;
    }
    GNUNET_break (size != sret);
    handle_request (listen_socket6,
                    &v6,
                    addrlen,
                    buf,
                    size);
  }
}


/**
 * Main function that will be run.
 *
 * @param cls closure
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
  char *addr_str;

  (void) cls;
  (void) args;
  (void) cfgfile;
  cfg = c;
  if (NULL == dns_ip)
  {
    GNUNET_log (GNUNET_ERROR_TYPE_ERROR,
                _ ("No DNS server specified!\n"));
    return;
  }
  GNUNET_SCHEDULER_add_shutdown (&do_shutdown,
                                 NULL);
  if (NULL == (gns = GNUNET_GNS_connect (cfg)))
    return;
  if (NULL == (vpn_handle = GNUNET_VPN_connect (cfg)))
    return;
  GNUNET_assert (NULL != (dns_stub = GNUNET_DNSSTUB_start (128)));
  if (GNUNET_OK !=
      GNUNET_DNSSTUB_add_dns_ip (dns_stub,
                                 dns_ip))
  {
    GNUNET_DNSSTUB_stop (dns_stub);
    GNUNET_GNS_disconnect (gns);
    gns = NULL;
    GNUNET_VPN_disconnect (vpn_handle);
    vpn_handle = NULL;
    return;
  }

  /* Get address to bind to */
  if (GNUNET_OK != GNUNET_CONFIGURATION_get_value_string (c, "dns2gns",
                                                          "BIND_TO",
                                                          &addr_str))
  {
    // No address specified
    GNUNET_log (GNUNET_ERROR_TYPE_ERROR,
                "Don't know what to bind to...\n");
    GNUNET_free (addr_str);
    GNUNET_SCHEDULER_shutdown ();
    return;
  }
  if (1 != inet_pton (AF_INET, addr_str, &address))
  {
    GNUNET_log (GNUNET_ERROR_TYPE_ERROR,
                "Unable to parse address %s\n",
                addr_str);
    GNUNET_free (addr_str);
    GNUNET_SCHEDULER_shutdown ();
    return;
  }
  GNUNET_free (addr_str);
  /* Get address to bind to */
  if (GNUNET_OK != GNUNET_CONFIGURATION_get_value_string (c, "dns2gns",
                                                          "BIND_TO6",
                                                          &addr_str))
  {
    // No address specified
    GNUNET_log (GNUNET_ERROR_TYPE_ERROR,
                "Don't know what to bind6 to...\n");
    GNUNET_free (addr_str);
    GNUNET_SCHEDULER_shutdown ();
    return;
  }
  if (1 != inet_pton (AF_INET6, addr_str, &address6))
  {
    GNUNET_log (GNUNET_ERROR_TYPE_ERROR,
                "Unable to parse IPv6 address %s\n",
                addr_str);
    GNUNET_free (addr_str);
    GNUNET_SCHEDULER_shutdown ();
    return;
  }
  GNUNET_free (addr_str);
  if (GNUNET_OK == GNUNET_CONFIGURATION_get_value_number (c, "dns2gns",
                                                          "PORT",
                                                          &listen_port))
    GNUNET_log (GNUNET_ERROR_TYPE_DEBUG,
                "Listening on %llu\n", listen_port);

  listen_socket4 = GNUNET_NETWORK_socket_create (PF_INET,
                                                 SOCK_DGRAM,
                                                 IPPROTO_UDP);
  if (NULL != listen_socket4)
  {
    struct sockaddr_in v4;

    memset (&v4, 0, sizeof(v4));
    v4.sin_family = AF_INET;
    v4.sin_addr.s_addr = address;
#if HAVE_SOCKADDR_IN_SIN_LEN
    v4.sin_len = sizeof(v4);
#endif
    v4.sin_port = htons (listen_port);
    if (GNUNET_OK !=
        GNUNET_NETWORK_socket_bind (listen_socket4,
                                    (struct sockaddr *) &v4,
                                    sizeof(v4)))
    {
      GNUNET_log_strerror (GNUNET_ERROR_TYPE_ERROR, "bind");
      GNUNET_NETWORK_socket_close (listen_socket4);
      listen_socket4 = NULL;
    }
  }
  listen_socket6 = GNUNET_NETWORK_socket_create (PF_INET6,
                                                 SOCK_DGRAM,
                                                 IPPROTO_UDP);
  if (NULL != listen_socket6)
  {
    struct sockaddr_in6 v6;

    memset (&v6, 0, sizeof(v6));
    v6.sin6_family = AF_INET6;
    v6.sin6_addr = address6;
#if HAVE_SOCKADDR_IN_SIN_LEN
    v6.sin6_len = sizeof(v6);
#endif
    v6.sin6_port = htons (listen_port);
    if (GNUNET_OK !=
        GNUNET_NETWORK_socket_bind (listen_socket6,
                                    (struct sockaddr *) &v6,
                                    sizeof(v6)))
    {
      GNUNET_log_strerror (GNUNET_ERROR_TYPE_ERROR, "bind");
      GNUNET_NETWORK_socket_close (listen_socket6);
      listen_socket6 = NULL;
    }
  }
  if ((NULL == listen_socket4) &&
      (NULL == listen_socket6))
  {
    GNUNET_GNS_disconnect (gns);
    gns = NULL;
    GNUNET_VPN_disconnect (vpn_handle);
    vpn_handle = NULL;
    GNUNET_DNSSTUB_stop (dns_stub);
    dns_stub = NULL;
    return;
  }
  if (NULL != listen_socket4)
    t4 = GNUNET_SCHEDULER_add_read_net (GNUNET_TIME_UNIT_FOREVER_REL,
                                        listen_socket4,
                                        &read_dns4,
                                        listen_socket4);
  if (NULL != listen_socket6)
    t6 = GNUNET_SCHEDULER_add_read_net (GNUNET_TIME_UNIT_FOREVER_REL,
                                        listen_socket6,
                                        &read_dns6,
                                        listen_socket6);
}


/**
 * The main function for the dns2gns daemon.
 *
 * @param argc number of arguments from the command line
 * @param argv command line arguments
 * @return 0 ok, 1 on error
 */
int
main (int argc,
      char *const *argv)
{
  struct GNUNET_GETOPT_CommandLineOption options[] = {
    GNUNET_GETOPT_option_string ('d',
                                 "dns",
                                 "IP",
                                 gettext_noop (
                                   "IP of recursive DNS resolver to use (required)"),
                                 &dns_ip),
    GNUNET_GETOPT_OPTION_END
  };
  int ret;

  GNUNET_log_setup ("gnunet-dns2gns",
                    "WARNING",
                    NULL);
  ret =
    (GNUNET_OK ==
     GNUNET_PROGRAM_run (GNUNET_OS_project_data_gnunet (),
                         argc, argv,
                         "gnunet-dns2gns",
                         _ ("GNUnet DNS-to-GNS proxy (a DNS server)"),
                         options,
                         &run, NULL)) ? 0 : 1;
  return ret;
}


/* end of gnunet-dns2gns.c */
