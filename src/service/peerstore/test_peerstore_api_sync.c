/*
     This file is part of GNUnet.
     Copyright (C) 2015 GNUnet e.V.

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
 * @file peerstore/test_peerstore_api_sync.c
 * @brief testcase for peerstore sync-on-disconnect feature. Stores
 *        a value just before disconnecting, and then checks that
 *        this value is actually stored.
 * @author Omar Tarabai
 * @author Christian Grothoff (minor fix, comments)
 */
#include "platform.h"
#include "gnunet_util_lib.h"
#include "gnunet_testing_lib.h"
#include "gnunet_peerstore_service.h"

/**
 * Overall result, 0 for success.
 */
static int ok = 404;

/**
 * Configuration we use.
 */
static const struct GNUNET_CONFIGURATION_Handle *cfg;

/**
 * handle to talk to the peerstore.
 */
static struct GNUNET_PEERSTORE_Handle *h;

/**
 * Subsystem we store the value for.
 */
static const char *subsystem = "test_peerstore_api_sync";

/**
 * Fake PID under which we store the value.
 */
static struct GNUNET_PeerIdentity pid;

/**
 * Test key we're storing the test value under.
 */
static const char *key = "test_peerstore_api_store_key";

/**
 * Test value we are storing.
 */
static const char *val = "test_peerstore_api_store_val";


/**
 * Timeout
 */
#define TIMEOUT GNUNET_TIME_relative_multiply (GNUNET_TIME_UNIT_SECONDS, 10)

/**
 * Timeout task
 */
static struct GNUNET_SCHEDULER_Task *to;

/**
 * Iterate handle
 */
static struct GNUNET_PEERSTORE_IterateContext *it;

static void
test_cont (void *cls);

/**
 * Function that should be called with the result of the
 * lookup, and finally once with NULL to signal the end
 * of the iteration.
 *
 * Upon the first call, we set "ok" to success. On the
 * second call (end of iteration) we terminate the test.
 *
 * @param cls NULL
 * @param record the information stored in the peerstore
 * @param emsg any error message
 * @return #GNUNET_YES (all good, continue)
 */
static void
iterate_cb (void *cls,
            const struct GNUNET_PEERSTORE_Record *record,
            const char *emsg)
{
  const char *rec_val;

  GNUNET_break (NULL == emsg);
  if (NULL == record)
  {
    it = NULL;
    if (0 == ok)
    {
      GNUNET_PEERSTORE_disconnect (h,
                                   GNUNET_NO);
      if (NULL != to)
      {
        GNUNET_SCHEDULER_cancel (to);
        to = NULL;
      }
      GNUNET_SCHEDULER_shutdown ();
      return;
    }
    /**
     * Try again
     */
    GNUNET_SCHEDULER_add_now (&test_cont,
                              NULL);
    return;
  }
  rec_val = record->value;
  GNUNET_break (0 == strcmp (rec_val, val));
  ok = 0;
}


static void
timeout_task (void *cls)
{
  to = NULL;
  GNUNET_log (GNUNET_ERROR_TYPE_ERROR,
              "Timeout reached\n");
  if (NULL != it)
    GNUNET_PEERSTORE_iterate_cancel (it);
  it = NULL;
  GNUNET_PEERSTORE_disconnect (h,
                               GNUNET_NO);
  GNUNET_SCHEDULER_shutdown ();
  return;
}


/**
 * Run the 2nd stage of the test where we fetch the
 * data that should have been stored.
 *
 * @param cls NULL
 */
static void
test_cont (void *cls)
{
  it = GNUNET_PEERSTORE_iterate (h,
                                 subsystem,
                                 &pid, key,
                                 &iterate_cb,
                                 NULL);
}


static void
disc_cont (void *cls)
{
  GNUNET_PEERSTORE_disconnect (h, GNUNET_YES);
  h = GNUNET_PEERSTORE_connect (cfg);
  GNUNET_SCHEDULER_add_now (&test_cont,
                            NULL);
}


static void
store_cont (void *cls, int success)
{
  ok = success;
  GNUNET_log (GNUNET_ERROR_TYPE_DEBUG,
              "Success: %s\n",
              (GNUNET_SYSERR == ok) ? "no" : "yes");
  /* We need to wait a little bit to give the disconnect
     a chance to actually finish the operation; otherwise,
     the test may fail non-deterministically if the new
     connection is faster than the cleanup routine of the
     old one. */
  GNUNET_SCHEDULER_add_delayed (GNUNET_TIME_UNIT_SECONDS,
                                &disc_cont,
                                NULL);
}


/**
 * Actually run the test.
 */
static void
test1 ()
{
  h = GNUNET_PEERSTORE_connect (cfg);
  GNUNET_PEERSTORE_store (h,
                          subsystem,
                          &pid,
                          key,
                          val, strlen (val) + 1,
                          GNUNET_TIME_UNIT_FOREVER_ABS,
                          GNUNET_PEERSTORE_STOREOPTION_REPLACE,
                          &store_cont, NULL);
}


/**
 * Initialize globals and launch the test.
 *
 * @param cls NULL
 * @param c configuration to use
 * @param peer handle to our peer (unused)
 */
static void
run (void *cls,
     const struct GNUNET_CONFIGURATION_Handle *c,
     struct GNUNET_TESTING_Peer *peer)
{
  cfg = c;
  memset (&pid, 1, sizeof(pid));
  to = GNUNET_SCHEDULER_add_delayed (TIMEOUT,
                                     &timeout_task,
                                     NULL);
  GNUNET_SCHEDULER_add_now (&test1, NULL);
}


int
main (int argc, char *argv[])
{
  if (0 !=
      GNUNET_TESTING_service_run ("test-gnunet-peerstore-sync",
                                  "peerstore",
                                  "peerstore.conf",
                                  &run, NULL))
    return 1;
  if (0 != ok)
    fprintf (stderr,
             "Test failed: %d\n",
             ok);
  return ok;
}


/* end of test_peerstore_api_sync.c */
