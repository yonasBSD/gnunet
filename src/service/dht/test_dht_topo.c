/*
     This file is part of GNUnet.
     Copyright (C) 2012, 2016 GNUnet e.V.

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
 * @file dht/test_dht_topo.c
 * @author Christian Grothoff
 * @brief Test for the dht service: store and retrieve in various topologies.
 * Each peer stores a value from the DHT and then each peer tries to get each
 * value from each other peer.
 */
#include "platform.h"
#include "gnunet_util_lib.h"
#include "gnunet_dht_service.h"
#include "dht_test_lib.h"

/**
 * How long until we give up on fetching the data?
 */
#define GET_TIMEOUT GNUNET_TIME_relative_multiply (GNUNET_TIME_UNIT_SECONDS, \
                                                   120)

/**
 * How frequently do we execute the PUTs?
 */
#define PUT_FREQUENCY GNUNET_TIME_relative_multiply (GNUNET_TIME_UNIT_SECONDS, \
                                                     5)


/**
 * Information we keep for each GET operation.
 */
struct GetOperation
{
  /**
   * DLL.
   */
  struct GetOperation *next;

  /**
   * DLL.
   */
  struct GetOperation *prev;

  /**
   * Operation to fetch @a me.
   */
  struct GNUNET_TESTBED_Operation *to;

  /**
   * Handle for the operation.
   */
  struct GNUNET_DHT_GetHandle *get;

  /**
   * DHT used by this operation.
   */
  struct GNUNET_DHT_Handle *dht;

  /**
   * Key we are looking up.
   */
  struct GNUNET_HashCode key;

  /**
   * At which peer is this operation being performed?
   */
  struct GNUNET_PeerIdentity me;
};


/**
 * Result of the test.
 */
static int ok;

/**
 * Task to do DHT_puts
 */
static struct GNUNET_SCHEDULER_Task *put_task;

/**
 * Task to do DHT_gets
 */
static struct GNUNET_SCHEDULER_Task *get_task;

/**
 * Task to time out / regular shutdown.
 */
static struct GNUNET_SCHEDULER_Task *timeout_task;

/**
 * Head of list of active GET operations.
 */
static struct GetOperation *get_head;

/**
 * Tail of list of active GET operations.
 */
static struct GetOperation *get_tail;

/**
 * Array of the testbed's peers.
 */
static struct GNUNET_TESTBED_Peer **my_peers;

/**
 * Number of peers to run.
 */
static unsigned int NUM_PEERS;


/**
 * Statistics we print out.
 */
static struct
{
  const char *subsystem;
  const char *name;
  unsigned long long total;
} stats[] = {
  { "core", "# bytes decrypted", 0 },
  { "core", "# bytes encrypted", 0 },
  { "core", "# type maps received", 0 },
  { "core", "# session keys confirmed via PONG", 0 },
  { "core", "# peers connected", 0 },
  { "core", "# key exchanges initiated", 0 },
  { "core", "# send requests dropped (disconnected)", 0 },
  { "core", "# transmissions delayed due to corking", 0 },
  { "core", "# messages discarded (expired prior to transmission)", 0 },
  { "core", "# messages discarded (disconnected)", 0 },
  { "core", "# discarded CORE_SEND requests", 0 },
  { "core", "# discarded lower priority CORE_SEND requests", 0 },
  { "transport", "# bytes received via TCP", 0 },
  { "transport", "# bytes transmitted via TCP", 0 },
  { "dht", "# PUT messages queued for transmission", 0 },
  { "dht", "# P2P PUT requests received", 0 },
  { "dht", "# GET messages queued for transmission", 0 },
  { "dht", "# P2P GET requests received", 0 },
  { "dht", "# RESULT messages queued for transmission", 0 },
  { "dht", "# P2P RESULTS received", 0 },
  { "dht", "# Queued messages discarded (peer disconnected)", 0 },
  { "dht", "# Peers excluded from routing due to Bloomfilter", 0 },
  { "dht", "# Peer selection failed", 0 },
  { "dht", "# FIND PEER requests ignored due to Bloomfilter", 0 },
  { "dht", "# FIND PEER requests ignored due to lack of HELLO", 0 },
  { "dht", "# P2P FIND PEER requests processed", 0 },
  { "dht", "# P2P GET requests ONLY routed", 0 },
  { "dht", "# Preference updates given to core", 0 },
  { "dht", "# REPLIES ignored for CLIENTS (no match)", 0 },
  { "dht", "# GET requests from clients injected", 0 },
  { "dht", "# GET requests received from clients", 0 },
  { "dht", "# GET STOP requests received from clients", 0 },
  { "dht", "# ITEMS stored in datacache", 0 },
  { "dht", "# Good RESULTS found in datacache", 0 },
  { "dht", "# GET requests given to datacache", 0 },
  { NULL, NULL, 0 }
};


static struct GNUNET_DHT_TEST_Context *
stop_ops (void)
{
  struct GetOperation *get_op;
  struct GNUNET_DHT_TEST_Context *ctx = NULL;

  if (NULL != timeout_task)
  {
    ctx = GNUNET_SCHEDULER_cancel (timeout_task);
    timeout_task = NULL;
  }
  if (NULL != put_task)
  {
    GNUNET_SCHEDULER_cancel (put_task);
    put_task = NULL;
  }
  if (NULL != get_task)
  {
    GNUNET_SCHEDULER_cancel (get_task);
    get_task = NULL;
  }
  while (NULL != (get_op = get_tail))
  {
    if (NULL != get_op->to)
    {
      GNUNET_TESTBED_operation_done (get_op->to);
      get_op->to = NULL;
    }
    if (NULL != get_op->get)
    {
      GNUNET_DHT_get_stop (get_op->get);
      get_op->get = NULL;
    }
    GNUNET_CONTAINER_DLL_remove (get_head,
                                 get_tail,
                                 get_op);
    GNUNET_free (get_op);
  }
  return ctx;
}


/**
 * Function called once we're done processing stats.
 *
 * @param cls the test context
 * @param op the stats operation
 * @param emsg error message on failure
 */
static void
stats_finished (void *cls,
                struct GNUNET_TESTBED_Operation *op,
                const char *emsg)
{
  struct GNUNET_DHT_TEST_Context *ctx = cls;

  if (NULL != op)
    GNUNET_TESTBED_operation_done (op);
  if (NULL != emsg)
  {
    fprintf (stderr,
             _ ("Gathering statistics failed: %s\n"),
             emsg);
    GNUNET_SCHEDULER_cancel (put_task);
    GNUNET_DHT_TEST_cleanup (ctx);
    return;
  }
  for (unsigned int i = 0; NULL != stats[i].name; i++)
    fprintf (stderr,
             "%6s/%60s = %12llu\n",
             stats[i].subsystem,
             stats[i].name,
             stats[i].total);
  GNUNET_DHT_TEST_cleanup (ctx);
  GNUNET_SCHEDULER_shutdown ();
}


/**
 * Function called to process statistic values from all peers.
 *
 * @param cls closure
 * @param peer the peer the statistic belong to
 * @param subsystem name of subsystem that created the statistic
 * @param name the name of the datum
 * @param value the current value
 * @param is_persistent #GNUNET_YES if the value is persistent, #GNUNET_NO if not
 * @return #GNUNET_OK to continue, #GNUNET_SYSERR to abort iteration
 */
static enum GNUNET_GenericReturnValue
handle_stats (void *cls,
              const struct GNUNET_TESTBED_Peer *peer,
              const char *subsystem,
              const char *name,
              uint64_t value,
              int is_persistent)
{
  for (unsigned int i = 0; NULL != stats[i].name; i++)
    if ((0 == strcasecmp (subsystem,
                          stats[i].subsystem)) &&
        (0 == strcasecmp (name,
                          stats[i].name)))
      stats[i].total += value;
  return GNUNET_OK;
}


/**
 * Task run on shutdown to clean up.  Terminates active get operations
 * and shuts down the testbed.
 *
 * @param cls the 'struct GNUNET_DHT_TestContext'
 */
static void
shutdown_task (void *cls)
{
  struct GNUNET_DHT_TEST_Context *ctx;

  (void) cls;
  GNUNET_log (GNUNET_ERROR_TYPE_DEBUG,
              "Performing shutdown\n");
  ctx = stop_ops ();
  if (NULL != ctx)
    GNUNET_DHT_TEST_cleanup (ctx);
}


/**
 * Task run on timeout to clean up.  Terminates active get operations
 * and shuts down the testbed.
 *
 * @param cls the `struct GNUNET_DHT_TestContext`
 */
static void
timeout_cb (void *cls)
{
  struct GNUNET_DHT_TEST_Context *ctx = cls;

  timeout_task = GNUNET_SCHEDULER_add_delayed (GET_TIMEOUT,
                                               &timeout_cb,
                                               ctx);
  GNUNET_log (GNUNET_ERROR_TYPE_ERROR,
              "Timeout\n");
  GNUNET_SCHEDULER_shutdown ();
}


/**
 * Iterator called on each result obtained for a DHT
 * operation that expects a reply
 *
 * @param cls closure with our 'struct GetOperation'
 * @param exp when will this value expire
 * @param query query hash
 * @param trunc_peer peer the path was truncated at, or NULL
 * @param get_path peers on reply path (or NULL if not recorded)
 * @param get_path_length number of entries in @a get_path
 * @param put_path peers on the PUT path (or NULL if not recorded)
 * @param put_path_length number of entries in @a put_path
 * @param type type of the result
 * @param size number of bytes in @a data
 * @param data pointer to the result data
 */
static void
dht_get_handler (void *cls,
                 struct GNUNET_TIME_Absolute exp,
                 const struct GNUNET_HashCode *query,
                 const struct GNUNET_PeerIdentity *trunc_peer,
                 const struct GNUNET_DHT_PathElement *get_path,
                 unsigned int get_path_length,
                 const struct GNUNET_DHT_PathElement *put_path,
                 unsigned int put_path_length,
                 enum GNUNET_BLOCK_Type type,
                 size_t size,
                 const void *data)
{
  struct GetOperation *get_op = cls;
  struct GNUNET_HashCode want;
  struct GNUNET_DHT_TEST_Context *ctx;

  GNUNET_log (GNUNET_ERROR_TYPE_DEBUG,
              "GET HANDLER called on PID %s\n",
              GNUNET_i2s (&get_op->me));
  if (sizeof(struct GNUNET_HashCode) != size)
  {
    GNUNET_break (0);
    return;
  }
  if (0 != GNUNET_memcmp (query,
                          &get_op->key))
  {
    /* exact search should only yield exact results */
    GNUNET_break (0);
    return;
  }
  GNUNET_CRYPTO_hash (query,
                      sizeof(*query),
                      &want);
  if (0 != memcmp (&want,
                   data,
                   sizeof(want)))
  {
    GNUNET_break (0);
    return;
  }
  if (0 !=
      GNUNET_DHT_verify_path (data,
                              size,
                              exp,
                              trunc_peer,
                              put_path,
                              put_path_length,
                              get_path,
                              get_path_length,
                              &get_op->me))
  {
    GNUNET_break (0);
    GNUNET_log (GNUNET_ERROR_TYPE_ERROR,
                "Path signature (%u/%u) verification failed for peer %s!\n",
                get_path_length,
                put_path_length,
                GNUNET_i2s (&get_op->me));
  }
  else
  {
    GNUNET_log (GNUNET_ERROR_TYPE_DEBUG,
                "Get successful\n");
    ok--;
  }
  GNUNET_DHT_get_stop (get_op->get);
  GNUNET_CONTAINER_DLL_remove (get_head,
                               get_tail,
                               get_op);
  GNUNET_free (get_op);
  if (NULL != get_head)
    return;
  /* all DHT GET operations successful; get stats! */
  GNUNET_log (GNUNET_ERROR_TYPE_INFO,
              "All DHT operations successful. Obtaining stats!\n");
  ctx = stop_ops ();
  GNUNET_assert (NULL != ctx);
  (void) GNUNET_TESTBED_get_statistics (NUM_PEERS,
                                        my_peers,
                                        NULL, NULL,
                                        &handle_stats,
                                        &stats_finished,
                                        ctx);
}


/**
 * Task to put the id of each peer into the DHT.
 *
 * @param cls array with NUM_PEERS DHT handles
 * @param tc Task context
 */
static void
do_puts (void *cls)
{
  struct GNUNET_DHT_Handle **hs = cls;
  struct GNUNET_HashCode key;
  struct GNUNET_HashCode value;

  put_task = NULL;
  GNUNET_log (GNUNET_ERROR_TYPE_DEBUG,
              "Putting %u values into DHT\n",
              NUM_PEERS);
  for (unsigned int i = 0; i < NUM_PEERS; i++)
  {
    GNUNET_CRYPTO_hash (&i,
                        sizeof(i),
                        &key);
    GNUNET_CRYPTO_hash (&key,
                        sizeof(key),
                        &value);
    GNUNET_DHT_put (hs[i],
                    &key,
                    10U,
                    GNUNET_DHT_RO_RECORD_ROUTE
                    | GNUNET_DHT_RO_DEMULTIPLEX_EVERYWHERE,
                    GNUNET_BLOCK_TYPE_TEST,
                    sizeof(value),
                    &value,
                    GNUNET_TIME_UNIT_FOREVER_ABS,
                    NULL,
                    NULL);
  }
  put_task = GNUNET_SCHEDULER_add_delayed (PUT_FREQUENCY,
                                           &do_puts,
                                           hs);
}


/**
 * Callback to be called when the requested peer information is available
 * The peer information in the callback is valid until the operation 'op' is canceled.
 *
 * @param cls a `struct GetOperation *`
 * @param op the operation this callback corresponds to
 * @param pinfo the result; will be NULL if the operation has failed
 * @param emsg error message if the operation has failed; will be NULL if the
 *          operation is successful
 */
static void
pid_cb (void *cls,
        struct GNUNET_TESTBED_Operation *op,
        const struct GNUNET_TESTBED_PeerInformation *pinfo,
        const char *emsg)
{
  struct GetOperation *get_op = cls;

  if (NULL != emsg)
  {
    GNUNET_log (GNUNET_ERROR_TYPE_ERROR,
                "Testbed failure: %s\n",
                emsg);
    GNUNET_TESTBED_operation_done (get_op->to);
    get_op->to = NULL;
    GNUNET_SCHEDULER_shutdown ();
    return;
  }
  GNUNET_log (GNUNET_ERROR_TYPE_DEBUG,
              "Testbed provided PID %s\n",
              GNUNET_i2s (pinfo->result.id));
  get_op->me = *pinfo->result.id;
  GNUNET_TESTBED_operation_done (get_op->to);
  get_op->to = NULL;
  get_op->get = GNUNET_DHT_get_start (get_op->dht,
                                      GNUNET_BLOCK_TYPE_TEST,
                                      &get_op->key,
                                      4U,     /* replication level */
                                      GNUNET_DHT_RO_RECORD_ROUTE
                                      | GNUNET_DHT_RO_DEMULTIPLEX_EVERYWHERE,
                                      NULL,        /* xquery */
                                      0,      /* xquery bits */
                                      &dht_get_handler,
                                      get_op);
}


/**
 * Start GET operations.
 */
static void
start_get (void *cls)
{
  struct GNUNET_DHT_Handle **dhts = cls;

  get_task = NULL;
  for (unsigned int i = 0; i < NUM_PEERS; i++)
  {
    struct GNUNET_HashCode key;

    GNUNET_CRYPTO_hash (&i,
                        sizeof(i),
                        &key);
    for (unsigned int j = 0; j < NUM_PEERS; j++)
    {
      struct GetOperation *get_op;

      get_op = GNUNET_new (struct GetOperation);
      ok++;
      GNUNET_log (GNUNET_ERROR_TYPE_DEBUG,
                  "Starting GET %p\n",
                  get_op);
      get_op->key = key;
      get_op->dht = dhts[j];
      get_op->to = GNUNET_TESTBED_peer_get_information (my_peers[j],
                                                        GNUNET_TESTBED_PIT_IDENTITY,
                                                        &pid_cb,
                                                        get_op);
      GNUNET_CONTAINER_DLL_insert (get_head,
                                   get_tail,
                                   get_op);
    }
  }
}


/**
 * Main function of the test.
 *
 * @param cls closure (NULL)
 * @param ctx argument to give to #GNUNET_DHT_TEST_cleanup on test end
 * @param num_peers number of @a peers that are running
 * @param peers array of peers
 * @param dhts handle to each of the DHTs of the peers
 */
static void
run (void *cls,
     struct GNUNET_DHT_TEST_Context *ctx,
     unsigned int num_peers,
     struct GNUNET_TESTBED_Peer **peers,
     struct GNUNET_DHT_Handle **dhts)
{
  GNUNET_assert (NUM_PEERS == num_peers);
  my_peers = peers;
  GNUNET_log (GNUNET_ERROR_TYPE_DEBUG,
              "Peers setup, starting test\n");
  put_task = GNUNET_SCHEDULER_add_now (&do_puts,
                                       dhts);
  get_task = GNUNET_SCHEDULER_add_delayed (GNUNET_TIME_UNIT_SECONDS,
                                           &start_get,
                                           dhts);
  timeout_task = GNUNET_SCHEDULER_add_delayed (GET_TIMEOUT,
                                               &timeout_cb,
                                               ctx);
  GNUNET_SCHEDULER_add_shutdown (&shutdown_task,
                                 ctx);
}


/**
 * Main: start test
 */
int
main (int xargc, char *xargv[])
{
  const char *cfg_filename;
  const char *test_name;

  unsetenv ("XDG_DATA_HOME");
  unsetenv ("XDG_CONFIG_HOME");
  unsetenv ("XDG_CACHE_HOME");
  if (NULL != strstr (xargv[0], "test_dht_2dtorus"))
  {
    cfg_filename = "test_dht_2dtorus.conf";
    test_name = "test-dht-2dtorus";
    NUM_PEERS = 16;
  }
  else if (NULL != strstr (xargv[0], "test_dht_line"))
  {
    cfg_filename = "test_dht_line.conf";
    test_name = "test-dht-line";
    NUM_PEERS = 5;
  }
  else if (NULL != strstr (xargv[0], "test_dht_twopeer"))
  {
    cfg_filename = "test_dht_line.conf";
    test_name = "test-dht-twopeer";
    NUM_PEERS = 2;
  }
  else if (NULL != strstr (xargv[0], "test_dht_multipeer"))
  {
    cfg_filename = "test_dht_multipeer.conf";
    test_name = "test-dht-multipeer";
    NUM_PEERS = 10;
  }
  else
  {
    GNUNET_break (0);
    return 1;
  }
  GNUNET_DHT_TEST_run (test_name,
                       cfg_filename,
                       NUM_PEERS,
                       &run, NULL);
  return ok;
}


/* end of test_dht_topo.c */
