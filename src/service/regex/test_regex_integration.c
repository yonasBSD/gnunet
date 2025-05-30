/*
     This file is part of GNUnet.
     Copyright (C) 2013, 2015 GNUnet e.V.

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
 * @file regex/test_regex_integration.c
 * @brief base test case for regex integration with VPN;
 *        tests that the regexes generated by the TUN API
 *        for IP addresses work (for some simple cases)
 * @author Christian Grothoff
 */
#include "platform.h"
#include "gnunet_applications.h"
#include "gnunet_util_lib.h"
#include "gnunet_testing_lib.h"
#include "gnunet_regex_service.h"


/**
 * How long until we really give up on a particular testcase portion?
 */
#define TOTAL_TIMEOUT GNUNET_TIME_relative_multiply (GNUNET_TIME_UNIT_SECONDS, \
                                                     600)

/**
 * How long until we give up on any particular operation (and retry)?
 */
#define BASE_TIMEOUT GNUNET_TIME_relative_multiply (GNUNET_TIME_UNIT_SECONDS, 3)


static struct GNUNET_REGEX_Announcement *a4;

static struct GNUNET_REGEX_Search *s4;

static struct GNUNET_REGEX_Announcement *a6;

static struct GNUNET_REGEX_Search *s6;

static int ok = 1;

static struct GNUNET_SCHEDULER_Task *die_task;


static void
end (void *cls)
{
  die_task = NULL;
  GNUNET_REGEX_announce_cancel (a4);
  a4 = NULL;
  GNUNET_REGEX_search_cancel (s4);
  s4 = NULL;
  GNUNET_REGEX_announce_cancel (a6);
  a6 = NULL;
  GNUNET_REGEX_search_cancel (s6);
  s6 = NULL;
  ok = 0;
}


static void
end_badly ()
{
  fprintf (stderr, "%s", "Testcase failed (timeout).\n");
  end (NULL);
  ok = 1;
}


/**
 * Search callback function, invoked for every result that was found.
 *
 * @param cls Closure provided in #GNUNET_REGEX_search().
 * @param id Peer providing a regex that matches the string.
 * @param get_path Path of the get request.
 * @param get_path_length Length of @a get_path.
 * @param put_path Path of the put request.
 * @param put_path_length Length of the @a put_path.
 */
static void
found_cb (void *cls,
          const struct GNUNET_PeerIdentity *id,
          const struct GNUNET_PeerIdentity *get_path,
          unsigned int get_path_length,
          const struct GNUNET_PeerIdentity *put_path,
          unsigned int put_path_length)
{
  const char *str = cls;
  static int found;

  GNUNET_log (GNUNET_ERROR_TYPE_DEBUG,
              "IPv%s-exit found\n",
              str);
  if (0 == strcmp (str, "4"))
    found |= 4;
  if (0 == strcmp (str, "6"))
    found |= 2;
  if ((4 | 2) == found)
  {
    GNUNET_SCHEDULER_cancel (die_task);
    die_task =
      GNUNET_SCHEDULER_add_now (&end, NULL);
  }
}


static void
run (void *cls,
     const struct GNUNET_CONFIGURATION_Handle *cfg,
     struct GNUNET_TESTING_Peer *peer)
{
  char rxstr4[GNUNET_TUN_IPV4_REGEXLEN];
  char rxstr6[GNUNET_TUN_IPV6_REGEXLEN];
  char *p4r;
  char *p6r;
  char *p4;
  char *p6;
  char *ss4;
  char *ss6;
  struct in_addr i4;
  struct in6_addr i6;

  die_task =
    GNUNET_SCHEDULER_add_delayed (TOTAL_TIMEOUT,
                                  &end_badly, NULL);
  GNUNET_assert (1 ==
                 inet_pton (AF_INET,
                            "127.0.0.1",
                            &i4));
  GNUNET_assert (1 ==
                 inet_pton (AF_INET6,
                            "::1:5",
                            &i6));
  GNUNET_REGEX_ipv4toregexsearch (&i4,
                                8080,
                                rxstr4);
  GNUNET_REGEX_ipv6toregexsearch (&i6,
                                8686,
                                rxstr6);
  GNUNET_asprintf (&ss4,
                   "%s%s",
                   GNUNET_APPLICATION_TYPE_EXIT_REGEX_PREFIX,
                   rxstr4);
  GNUNET_asprintf (&ss6,
                   "%s%s",
                   GNUNET_APPLICATION_TYPE_EXIT_REGEX_PREFIX,
                   rxstr6);
  p4r = GNUNET_REGEX_ipv4policy2regex ("0.0.0.0/0:!25;");
  p6r = GNUNET_REGEX_ipv6policy2regex ("::/0:!25;");
  GNUNET_asprintf (&p4,
                   "%s%s",
                   GNUNET_APPLICATION_TYPE_EXIT_REGEX_PREFIX,
                   p4r);
  GNUNET_asprintf (&p6,
                   "%s%s",
                   GNUNET_APPLICATION_TYPE_EXIT_REGEX_PREFIX,
                   p6r);
  GNUNET_free (p4r);
  GNUNET_free (p6r);
  a4 = GNUNET_REGEX_announce (cfg,
                              p4,
                              GNUNET_TIME_relative_multiply (
                                GNUNET_TIME_UNIT_SECONDS,
                                5),
                              1);
  a6 = GNUNET_REGEX_announce (cfg,
                              p6,
                              GNUNET_TIME_relative_multiply (
                                GNUNET_TIME_UNIT_SECONDS,
                                5),
                              1);
  GNUNET_free (p4);
  GNUNET_free (p6);

  s4 = GNUNET_REGEX_search (cfg,
                            ss4,
                            &found_cb, "4");
  s6 = GNUNET_REGEX_search (cfg,
                            ss6,
                            &found_cb, "6");
  GNUNET_free (ss4);
  GNUNET_free (ss6);
}


int
main (int argc, char *argv[])
{
  if (0 != GNUNET_TESTING_peer_run ("test-regex-integration",
                                    "test_regex_api_data.conf",
                                    &run, NULL))
    return 1;
  return ok;
}


/* end of test_regex_integration.c */
