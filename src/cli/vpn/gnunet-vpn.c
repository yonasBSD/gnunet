/*
     This file is part of GNUnet.
     Copyright (C) 2012 GNUnet e.V.

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
 * @file src/vpn/gnunet-vpn.c
 * @brief Tool to manually request VPN tunnels to be created
 * @author Christian Grothoff
 */

#include "platform.h"
#include "gnunet_util_lib.h"
#include "gnunet_vpn_service.h"


/**
 * Handle to vpn service.
 */
static struct GNUNET_VPN_Handle *handle;

/**
 * Opaque redirection request handle.
 */
static struct GNUNET_VPN_RedirectionRequest *request;

/**
 * Option -p: destination peer identity for service
 */
static char *peer_id;

/**
 * Option -s: service name (hash to get service descriptor)
 */
static char *service_name;

/**
 * Option -i: target IP
 */
static char *target_ip;

/**
 * Option -4: IPv4 requested.
 */
static int ipv4;

/**
 * Option -6: IPv6 requested.
 */
static int ipv6;

/**
 * Option -t: TCP requested.
 */
static int tcp;

/**
 * Option -u: UDP requested.
 */
static int udp;

/**
 * Selected level of verbosity.
 */
static unsigned int verbosity;

/**
 * Global return value.
 */
static int ret;

/**
 * Option '-d': duration of the mapping
 */
static struct GNUNET_TIME_Relative duration = { 5 * 60 * 1000 };


/**
 * Shutdown.
 */
static void
do_disconnect (void *cls)
{
  if (NULL != request)
  {
    GNUNET_VPN_cancel_request (request);
    request = NULL;
  }
  if (NULL != handle)
  {
    GNUNET_VPN_disconnect (handle);
    handle = NULL;
  }
  GNUNET_free (peer_id);
  GNUNET_free (service_name);
  GNUNET_free (target_ip);
}


/**
 * Callback invoked from the VPN service once a redirection is
 * available.  Provides the IP address that can now be used to
 * reach the requested destination.
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
allocation_cb (void *cls, int af, const void *address)
{
  char buf[INET6_ADDRSTRLEN];

  request = NULL;
  switch (af)
  {
  case AF_INET6:
  case AF_INET:
    fprintf (stdout, "%s\n", inet_ntop (af, address, buf, sizeof(buf)));
    break;

  case AF_UNSPEC:
    GNUNET_log (GNUNET_ERROR_TYPE_ERROR, _ ("Error creating tunnel\n"));
    ret = 1;
    break;

  default:
    break;
  }
  GNUNET_SCHEDULER_shutdown ();
}


/**
 * Main function that will be run by the scheduler.
 *
 * @param cls closure
 * @param args remaining command-line arguments
 * @param cfgfile name of the configuration file used (for saving, can be NULL!)
 * @param cfg configuration
 */
static void
run (void *cls,
     char *const *args,
     const char *cfgfile,
     const struct GNUNET_CONFIGURATION_Handle *cfg)
{
  int dst_af;
  int req_af;
  struct GNUNET_PeerIdentity peer;
  struct GNUNET_HashCode sd;
  const void *addr;
  struct in_addr v4;
  struct in6_addr v6;
  uint8_t protocol;
  struct GNUNET_TIME_Absolute etime;

  etime = GNUNET_TIME_relative_to_absolute (duration);
  GNUNET_SCHEDULER_add_shutdown (&do_disconnect, NULL);
  handle = GNUNET_VPN_connect (cfg);
  if (NULL == handle)
    goto error;
  req_af = AF_UNSPEC;
  if (ipv4)
  {
    if (ipv6)
    {
      fprintf (stderr,
               _ ("Option `%s' makes no sense with option `%s'.\n"),
               "-4",
               "-6");
      goto error;
    }
    req_af = AF_INET;
  }
  if (ipv6)
    req_af = AF_INET6;

  if (NULL == target_ip)
  {
    if (NULL == service_name)
    {
      fprintf (stderr, _ ("Option `%s' or `%s' is required.\n"), "-i", "-s");
      goto error;
    }
    if (NULL == peer_id)
    {
      fprintf (stderr,
               _ ("Option `%s' is required when using option `%s'.\n"),
               "-p",
               "-s");
      goto error;
    }
    if (! (tcp | udp))
    {
      fprintf (stderr,
               _ ("Option `%s' or `%s' is required when using option `%s'.\n"),
               "-t",
               "-u",
               "-s");
      goto error;
    }
    if (tcp & udp)
    {
      fprintf (stderr,
               _ ("Option `%s' makes no sense with option `%s'.\n"),
               "-t",
               "-u");
      goto error;
    }
    if (tcp)
      protocol = IPPROTO_TCP;
    if (udp)
      protocol = IPPROTO_UDP;
    if (GNUNET_OK !=
        GNUNET_CRYPTO_eddsa_public_key_from_string (peer_id,
                                                    strlen (peer_id),
                                                    &peer.public_key))
    {
      fprintf (stderr, _ ("`%s' is not a valid peer identifier.\n"), peer_id);
      goto error;
    }
    GNUNET_TUN_service_name_to_hash (service_name, &sd);
    request = GNUNET_VPN_redirect_to_peer (handle,
                                           req_af,
                                           protocol,
                                           &peer,
                                           &sd,
                                           etime,
                                           &allocation_cb,
                                           NULL);
  }
  else
  {
    if (1 != inet_pton (AF_INET6, target_ip, &v6))
    {
      if (1 != inet_pton (AF_INET, target_ip, &v4))
      {
        fprintf (stderr, _ ("`%s' is not a valid IP address.\n"), target_ip);
        goto error;
      }
      else
      {
        dst_af = AF_INET;
        addr = &v4;
      }
    }
    else
    {
      dst_af = AF_INET6;
      addr = &v6;
    }
    request = GNUNET_VPN_redirect_to_ip (handle,
                                         req_af,
                                         dst_af,
                                         addr,
                                         etime,
                                         &allocation_cb,
                                         NULL);
  }
  return;

error:
  GNUNET_SCHEDULER_shutdown ();
  ret = 1;
}


int
main (int argc, char *const *argv)
{
  struct GNUNET_GETOPT_CommandLineOption options[] =
  { GNUNET_GETOPT_option_flag ('4',
                               "ipv4",
                               gettext_noop (
                                 "request that result should be an IPv4 address"),
                               &ipv4),

    GNUNET_GETOPT_option_flag ('6',
                               "ipv6",
                               gettext_noop (
                                 "request that result should be an IPv6 address"),
                               &ipv6),

    GNUNET_GETOPT_option_relative_time (
      'd',
      "duration",
      "TIME",
      gettext_noop ("how long should the mapping be valid for new tunnels?"),
      &duration),

    GNUNET_GETOPT_option_string ('i',
                                 "ip",
                                 "IP",
                                 gettext_noop (
                                   "destination IP for the tunnel"),
                                 &target_ip),

    GNUNET_GETOPT_option_string (
      'p',
      "peer",
      "PEERID",
      gettext_noop ("peer offering the service we would like to access"),
      &peer_id),

    GNUNET_GETOPT_option_string ('s',
                                 "service",
                                 "NAME",
                                 gettext_noop (
                                   "name of the service we would like to access"),
                                 &service_name),

    GNUNET_GETOPT_option_flag ('t',
                               "tcp",
                               gettext_noop ("service is offered via TCP"),
                               &tcp),

    GNUNET_GETOPT_option_flag ('u',
                               "udp",
                               gettext_noop ("service is offered via UDP"),
                               &udp),

    GNUNET_GETOPT_option_verbose (&verbosity),

    GNUNET_GETOPT_OPTION_END };

  ret =
    (GNUNET_OK ==
     GNUNET_PROGRAM_run (GNUNET_OS_project_data_gnunet (),
                         argc,
                         argv,
                         "gnunet-vpn",
                         gettext_noop ("Setup tunnels via VPN."),
                         options,
                         &run,
                         NULL))
    ? ret
    : 1;
  return ret;
}


/* end of gnunet-vpn.c */
