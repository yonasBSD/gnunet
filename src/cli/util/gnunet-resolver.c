/*
     This file is part of GNUnet.
     Copyright (C) 2010 GNUnet e.V.

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
 * @file util/gnunet-resolver.c
 * @brief tool to test resolver
 * @author Christian Grothoff
 */

#include "platform.h"
#include "gnunet_util_lib.h"
#include "gnunet_resolver_service.h"

#define GET_TIMEOUT GNUNET_TIME_relative_multiply (GNUNET_TIME_UNIT_SECONDS, 5)

/**
 * Flag for reverse lookup.
 */
static int reverse;


/**
 * Prints each hostname obtained from DNS.
 *
 * @param cls closure (unused)
 * @param hostname one of the names for the host, NULL
 *        on the last call to the callback
 */
static void
print_hostname (void *cls,
                const char *hostname)
{
  (void) cls;
  if (NULL == hostname)
    return;
  fprintf (stdout,
           "%s\n",
           hostname);
}


/**
 * Callback function to display address.
 *
 * @param cls closure (unused)
 * @param addr one of the addresses of the host, NULL for the last address
 * @param addrlen length of the address
 */
static void
print_sockaddr (void *cls,
                const struct sockaddr *addr,
                socklen_t addrlen)
{
  (void) cls;
  if (NULL == addr)
    return;
  fprintf (stdout,
           "%s\n",
           GNUNET_a2s (addr,
                       addrlen));
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
  const struct sockaddr *sa;
  socklen_t salen;
  struct sockaddr_in v4;
  struct sockaddr_in6 v6;

  (void) cls;
  (void) cfgfile;
  (void) cfg;
  if (NULL == args[0])
    return;
  if (! reverse)
  {
    GNUNET_RESOLVER_ip_get (args[0],
                            AF_UNSPEC,
                            GET_TIMEOUT,
                            &print_sockaddr,
                            NULL);
    return;
  }

  sa = NULL;
  memset (&v4, 0, sizeof(v4));
  v4.sin_family = AF_INET;
#if HAVE_SOCKADDR_IN_SIN_LEN
  v4.sin_len = sizeof(v4);
#endif
  if (1 == inet_pton (AF_INET,
                      args[0],
                      &v4.sin_addr))
  {
    sa = (struct sockaddr *) &v4;
    salen = sizeof(v4);
  }
  memset (&v6, 0, sizeof(v6));
  v6.sin6_family = AF_INET6;
#if HAVE_SOCKADDR_IN_SIN_LEN
  v6.sin6_len = sizeof(v6);
#endif
  if (1 == inet_pton (AF_INET6,
                      args[0],
                      &v6.sin6_addr))
  {
    sa = (struct sockaddr *) &v6;
    salen = sizeof(v6);
  }
  if (NULL == sa)
  {
    fprintf (stderr,
             "`%s' is not a valid IP: %s\n",
             args[0],
             strerror (errno));
    return;
  }
  GNUNET_RESOLVER_hostname_get (sa, salen,
                                GNUNET_YES,
                                GET_TIMEOUT,
                                &print_hostname,
                                NULL);
}


/**
 * The main function to access GNUnet's DNS resolver.
 *
 * @param argc number of arguments from the command line
 * @param argv command line arguments
 * @return 0 ok, 1 on error
 */
int
main (int argc, char *const *argv)
{
  struct GNUNET_GETOPT_CommandLineOption options[] = {
    GNUNET_GETOPT_option_flag ('r',
                               "reverse",
                               gettext_noop ("perform a reverse lookup"),
                               &reverse),
    GNUNET_GETOPT_OPTION_END
  };
  int ret;

  ret = (GNUNET_OK ==
         GNUNET_PROGRAM_run (GNUNET_OS_project_data_gnunet (),
                             argc, argv,
                             "gnunet-resolver [hostname]",
                             gettext_noop ("Use built-in GNUnet stub resolver"),
                             options,
                             &run, NULL)) ? 0 : 1;
  return ret;
}


/* end of gnunet-resolver.c */
