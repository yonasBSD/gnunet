/*
     This file is part of GNUnet.
     Copyright (C) 2010, 2012 GNUnet e.V.

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
 * @file fs/test_gnunet_service_fs_p2p.c
 * @brief test P2P routing using simple publish + download operation
 * @author Christian Grothoff
 */
#include "platform.h"
#include "fs_test_lib.h"

#define VERBOSE GNUNET_NO

/**
 * File-size we use for testing.
 */
#define FILESIZE (1024 * 1024 * 1)

/**
 * How long until we give up on the download?
 */
#define TIMEOUT GNUNET_TIME_relative_multiply (GNUNET_TIME_UNIT_SECONDS, 300)

#define NUM_DAEMONS 2

#define SEED 42

static const char *progname;

static unsigned int anonymity_level;

static struct GNUNET_TESTBED_Peer *daemons[NUM_DAEMONS];

static int ok;

static struct GNUNET_TIME_Absolute start_time;


static void
do_stop (void *cls)
{
  char *fn = cls;
  struct GNUNET_TIME_Relative del;
  char *fancy;

  GNUNET_SCHEDULER_shutdown ();
  if (0 ==
      GNUNET_TIME_absolute_get_remaining (GNUNET_TIME_absolute_add (start_time,
                                                                    TIMEOUT)).
      rel_value_us)
  {
    GNUNET_log (GNUNET_ERROR_TYPE_DEBUG,
                "Timeout during download, shutting down with error\n");
    ok = 1;
  }
  else
  {
    del = GNUNET_TIME_absolute_get_duration (start_time);
    if (0 == del.rel_value_us)
      del.rel_value_us = 1;
    fancy =
      GNUNET_STRINGS_byte_size_fancy (((unsigned long long) FILESIZE)
                                      * 1000000LL / del.rel_value_us);
    fprintf (stdout,
             "Download speed was %s/s\n",
             fancy);
    GNUNET_free (fancy);
    GNUNET_log (GNUNET_ERROR_TYPE_DEBUG,
                "Finished download, shutting down\n");
  }
  if (NULL != fn)
  {
    GNUNET_DISK_directory_remove (fn);
    GNUNET_free (fn);
  }
}


static void
do_download (void *cls, const struct GNUNET_FS_Uri *uri,
             const char *fn)
{
  if (NULL == uri)
  {
    GNUNET_SCHEDULER_shutdown ();
    GNUNET_log (GNUNET_ERROR_TYPE_DEBUG,
                "Timeout during upload attempt, shutting down with error\n");
    ok = 1;
    return;
  }
  GNUNET_log (GNUNET_ERROR_TYPE_DEBUG, "Downloading %llu bytes\n",
              (unsigned long long) FILESIZE);
  start_time = GNUNET_TIME_absolute_get ();
  GNUNET_FS_TEST_download (daemons[0], TIMEOUT,
                           anonymity_level, SEED, uri,
                           VERBOSE, &do_stop,
                           (NULL == fn)
                           ? NULL
                           : GNUNET_strdup (fn));
}


static void
do_publish (void *cls,
            struct GNUNET_TESTBED_RunHandle *h,
            unsigned int num_peers,
            struct GNUNET_TESTBED_Peer **peers,
            unsigned int links_succeeded,
            unsigned int links_failed)
{
  unsigned int i;

  if (NULL != strstr (progname, "cadet"))
    anonymity_level = 0;
  else
    anonymity_level = 1;
  GNUNET_assert (NUM_DAEMONS == num_peers);
  for (i = 0; i < num_peers; i++)
    daemons[i] = peers[i];
  GNUNET_log (GNUNET_ERROR_TYPE_DEBUG, "Publishing %llu bytes\n",
              (unsigned long long) FILESIZE);
  GNUNET_FS_TEST_publish (daemons[1], TIMEOUT,
                          anonymity_level, GNUNET_NO,
                          FILESIZE, SEED,
                          VERBOSE, &do_download, NULL);
}


int
main (int argc, char *argv[])
{
  const char *config;

  progname = argv[0];
  if (NULL != strstr (progname, "cadet"))
    config = "test_gnunet_service_fs_p2p_cadet.conf";
  else
    config = "fs_test_lib_data.conf";
  (void) GNUNET_TESTBED_test_run ("test-gnunet-service-fs-p2p",
                                  config,
                                  NUM_DAEMONS,
                                  0, NULL, NULL,
                                  &do_publish, NULL);
  GNUNET_DISK_purge_cfg_dir (config,
                             "GNUNET_TEST_HOME");
  return ok;
}


/* end of test_gnunet_service_fs_p2p.c */
