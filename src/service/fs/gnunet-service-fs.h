/*
     This file is part of GNUnet.
     Copyright (C) 2009, 2010 GNUnet e.V.

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
 * @file fs/gnunet-service-fs.h
 * @brief shared data structures of gnunet-service-fs.c
 * @author Christian Grothoff
 */
#ifndef GNUNET_SERVICE_FS_H
#define GNUNET_SERVICE_FS_H

#include "gnunet_util_lib.h"
#include "gnunet_statistics_service.h"
#include "gnunet_core_service.h"
#include "gnunet_block_lib.h"
#include "fs.h"


/**
 * By which amount do we decrement the TTL for simple forwarding /
 * indirection of the query; in milli-seconds.  Set somewhat in
 * accordance to your network latency (above the time it'll take you
 * to send a packet and get a reply).
 */
#define TTL_DECREMENT 5000

/**
 * At what frequency should our datastore load decrease
 * automatically (since if we don't use it, clearly the
 * load must be going down).
 */
#define DATASTORE_LOAD_AUTODECLINE GNUNET_TIME_relative_multiply ( \
    GNUNET_TIME_UNIT_MILLISECONDS, 250)

/**
 * Only the (mandatory) query is included.
 */
#define GET_MESSAGE_BIT_QUERY_ONLY 0

/**
 * The peer identity of a peer waiting for the
 * reply is included (used if the response
 * should be transmitted to someone other than
 * the sender of the GET).
 */
#define GET_MESSAGE_BIT_RETURN_TO 1

/**
 * The peer identity of a peer that had claimed to have the content
 * previously is included (can be used if responder-anonymity is not
 * desired; note that the precursor presumably lacked a direct
 * connection to the specified peer; still, the receiver is in no way
 * required to limit forwarding only to the specified peer, it should
 * only prefer it somewhat if possible).
 */
#define GET_MESSAGE_BIT_TRANSMIT_TO 4


GNUNET_NETWORK_STRUCT_BEGIN

/**
 * Message sent between peers asking for FS-content.
 */
struct GetMessage
{
  /**
   * Message type will be #GNUNET_MESSAGE_TYPE_FS_GET.
   */
  struct GNUNET_MessageHeader header;

  /**
   * Type of the query (block type).
   */
  uint32_t type GNUNET_PACKED;

  /**
   * How important is this request (network byte order)
   */
  uint32_t priority GNUNET_PACKED;

  /**
   * Relative time to live in MILLISECONDS (network byte order)
   */
  int32_t ttl GNUNET_PACKED;

  /**
   * These days not used.
   */
  uint32_t reserved GNUNET_PACKED;

  /**
   * Which of the optional hash codes are present at the end of the
   * message?  See GET_MESSAGE_BIT_xx constants.  For each bit that is
   * set, an additional `struct GNUNET_HashCode` with the respective content
   * (in order of the bits) will be appended to the end of the GET
   * message.
   */
  uint32_t hash_bitmap GNUNET_PACKED;

  /**
   * Hashcodes of the file(s) we're looking for.
   * Details depend on the query type.
   */
  struct GNUNET_HashCode query;

  /* this is followed by PeerIdentities as specified in the "hash_bitmap";
   * after that, an optional bloomfilter (with bits set for replies
   * that should be suppressed) can be present */
};


/**
 * Message send by a peer that wants to be excluded
 * from migration for a while.
 */
struct MigrationStopMessage
{
  /**
   * Message type will be
   * GNUNET_MESSAGE_TYPE_FS_MIGRATION_STOP.
   */
  struct GNUNET_MessageHeader header;

  /**
   * Always zero.
   */
  uint32_t reserved GNUNET_PACKED;

  /**
   * How long should the block last?
   */
  struct GNUNET_TIME_RelativeNBO duration;
};
GNUNET_NETWORK_STRUCT_END

/**
 * A connected peer.
 */
struct GSF_ConnectedPeer;

/**
 * An active request.
 */
struct GSF_PendingRequest;

/**
 * A local client.
 */
struct GSF_LocalClient;

/**
 * Information kept per plan per request ('pe' module).
 */
struct GSF_RequestPlan;

/**
 * Bijection between request plans and pending requests.
 */
struct GSF_PendingRequestPlanBijection;

/**
 * Our connection to the datastore.
 */
extern struct GNUNET_DATASTORE_Handle *GSF_dsh;

/**
 * Our configuration.
 */
extern const struct GNUNET_CONFIGURATION_Handle *GSF_cfg;

/**
 * Handle for reporting statistics.
 */
extern struct GNUNET_STATISTICS_Handle *GSF_stats;

/**
 * Pointer to handle to the core service (points to NULL until we've
 * connected to it).
 */
extern struct GNUNET_CORE_Handle *GSF_core;

/**
 * Handle for DHT operations.
 */
extern struct GNUNET_DHT_Handle *GSF_dht;

/**
 * How long do requests typically stay in the routing table?
 */
extern struct GNUNET_LOAD_Value *GSF_rt_entry_lifetime;

/**
 * Running average of the observed latency to other peers (round trip).
 */
extern struct GNUNET_TIME_Relative GSF_avg_latency;

/**
 * Handle to ATS service.
 */
extern struct GNUNET_ATS_PerformanceHandle *GSF_ats;

/**
 * Identity of this peer.
 */
extern struct GNUNET_PeerIdentity GSF_my_id;

/**
 * Typical priorities we're seeing from other peers right now.  Since
 * most priorities will be zero, this value is the weighted average of
 * non-zero priorities seen "recently".  In order to ensure that new
 * values do not dramatically change the ratio, values are first
 * "capped" to a reasonable range (+N of the current value) and then
 * averaged into the existing value by a ratio of 1:N.  Hence
 * receiving the largest possible priority can still only raise our
 * "current_priorities" by at most 1.
 */
extern double GSF_current_priorities;

/**
 * How many query messages have we received 'recently' that
 * have not yet been claimed as cover traffic?
 */
extern unsigned int GSF_cover_query_count;

/**
 * How many content messages have we received 'recently' that
 * have not yet been claimed as cover traffic?
 */
extern unsigned int GSF_cover_content_count;

/**
 * Our block context.
 */
extern struct GNUNET_BLOCK_Context *GSF_block_ctx;

/**
 * Are we introducing randomized delays for better anonymity?
 */
extern int GSF_enable_randomized_delays;

/**
 * Size of the datastore queue we assume for common requests.
 */
extern unsigned int GSF_datastore_queue_size;


/**
 * Function to be called after we're done processing
 * replies from the local lookup.  If the result status
 * code indicates that there may be more replies, plan
 * forwarding the request.
 *
 * @param cls closure (NULL)
 * @param pr the pending request we were processing
 * @param result final datastore lookup result
 */
void
GSF_consider_forwarding (void *cls,
                         struct GSF_PendingRequest *pr,
                         enum GNUNET_BLOCK_ReplyEvaluationResult result);


/**
 * Test if the DATABASE (GET) load on this peer is too high
 * to even consider processing the query at
 * all.
 *
 * @return #GNUNET_YES if the load is too high to do anything (load high)
 *         #GNUNET_NO to process normally (load normal)
 *         #GNUNET_SYSERR to process for free (load low)
 */
int
GSF_test_get_load_too_high_ (uint32_t priority);


/**
 * We've just now completed a datastore request.  Update our
 * datastore load calculations.
 *
 * @param start time when the datastore request was issued
 */
void
GSF_update_datastore_delay_ (struct GNUNET_TIME_Absolute start);


#endif
/* end of gnunet-service-fs.h */
