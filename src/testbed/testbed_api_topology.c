/*
      This file is part of GNUnet
      (C) 2008--2012 Christian Grothoff (and other contributing authors)

      GNUnet is free software; you can redistribute it and/or modify
      it under the terms of the GNU General Public License as published
      by the Free Software Foundation; either version 3, or (at your
      option) any later version.

      GNUnet is distributed in the hope that it will be useful, but
      WITHOUT ANY WARRANTY; without even the implied warranty of
      MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
      General Public License for more details.

      You should have received a copy of the GNU General Public License
      along with GNUnet; see the file COPYING.  If not, write to the
      Free Software Foundation, Inc., 59 Temple Place - Suite 330,
      Boston, MA 02111-1307, USA.
 */

/**
 * @file testbed/testbed_api_topology.c
 * @brief topology-generation functions
 * @author Christian Grothoff
 */
#include "platform.h"
#include "gnunet_testbed_service.h"
#include "testbed_api.h"
#include "testbed_api_peers.h"
#include "testbed_api_operations.h"
#include "testbed_api_topology.h"

/**
 * Generic loggins shorthand
 */
#define LOG(kind,...)                                           \
  GNUNET_log_from (kind, "testbed-api-topology", __VA_ARGS__)


/**
 * Context information for topology operations
 */
struct TopologyContext;


/**
 * Representation of an overlay link
 */
struct OverlayLink
{

  /**
   * An operation corresponding to this link
   */
  struct GNUNET_TESTBED_Operation *op;

  /**
   * The topology context this link is a part of
   */  
  struct TopologyContext *tc;

  /**
   * position of peer A's handle in peers array
   */
  uint32_t A;

  /**
   * position of peer B's handle in peers array
   */
  uint32_t B;

};


/**
 * Context information for topology operations
 */
struct TopologyContext
{
  /**
   * The array of peers
   */
  struct GNUNET_TESTBED_Peer **peers;

  /**
   * An array of links; this array is of size link_array_size
   */
  struct OverlayLink *link_array;

  /**
   * The operation closure
   */
  void *op_cls;

  /**
   * The number of peers
   */
  unsigned int num_peers;

  /**
   * The size of the link array
   */
  unsigned int link_array_size;

  /**
   * should the automatic retry be disabled
   */
  int disable_retry;
  
};


/**
 * Callback to be called when an overlay_link operation complete
 *
 * @param cls element of the link_op array which points to the corresponding operation
 * @param op the operation that has been finished
 * @param emsg error message in case the operation has failed; will be NULL if
 *          operation has executed successfully.
 */
static void 
overlay_link_completed (void *cls,
			struct GNUNET_TESTBED_Operation *op, 
			const char *emsg)
{
  struct OverlayLink *link = cls;
  struct TopologyContext *tc;

  GNUNET_assert (op == link->op);
  GNUNET_TESTBED_operation_done (op);
  link->op = NULL;
  tc = link->tc;
  if ((NULL != emsg) && (GNUNET_NO == tc->disable_retry))
  {
    LOG (GNUNET_ERROR_TYPE_WARNING,
	 "Error while establishing a link: %s -- Retrying\n", emsg);
    link->op =
        GNUNET_TESTBED_overlay_connect (tc->op_cls,
                                        &overlay_link_completed,
                                        link,
                                        tc->peers[link->A],
                                        tc->peers[link->B]);
    return;
  }
}



/**
 * Function called when a overlay connect operation is ready
 *
 * @param cls the Topology context
 */
static void
opstart_overlay_configure_topology (void *cls)
{
  struct TopologyContext *tc = cls;
  unsigned int p;
  
  for (p = 0; p < tc->link_array_size; p++)
  {
    tc->link_array[p].op =
	GNUNET_TESTBED_overlay_connect (tc->op_cls, &overlay_link_completed,
					&tc->link_array[p],
					tc->peers[tc->link_array[p].A],
					tc->peers[tc->link_array[p].B]);   						  
  }
}


/**
 * Callback which will be called when overlay connect operation is released
 *
 * @param cls the Topology context
 */
static void
oprelease_overlay_configure_topology (void *cls)
{
  struct TopologyContext *tc = cls;
  unsigned int p;
  
  if (NULL != tc->link_array)
  {
    for (p = 0; p < tc->link_array_size; p++)
      if (NULL != tc->link_array[p].op)
        GNUNET_TESTBED_operation_done (tc->link_array[p].op);
    GNUNET_free (tc->link_array);
  }
  GNUNET_free (tc);
}


/**
 * Generates line topology
 *
 * @param tc the topology context
 */
static void
gen_topo_line (struct TopologyContext *tc)
{
  unsigned int cnt;

  tc->link_array_size = tc->num_peers - 1;
  tc->link_array = GNUNET_malloc (sizeof (struct OverlayLink) *
                                  tc->link_array_size);
  for (cnt=0; cnt < (tc->num_peers - 1); cnt++)
  {
    tc->link_array[cnt].A = cnt;
    tc->link_array[cnt].B = cnt + 1;
    tc->link_array[cnt].tc = tc;
  }
}


/**
 * Generates ring topology
 *
 * @param tc the topology context
 */
static void
gen_topo_ring (struct TopologyContext *tc)
{
  gen_topo_line (tc);
  tc->link_array_size++;
  tc->link_array = GNUNET_realloc (tc->link_array,
                                   sizeof (struct OverlayLink) *
                                   tc->link_array_size);
  tc->link_array[tc->link_array_size - 1].op = NULL;
  tc->link_array[tc->link_array_size - 1].tc = tc;
  tc->link_array[tc->link_array_size - 1].A = tc->num_peers - 1;
  tc->link_array[tc->link_array_size - 1].B = 0;
}


/**
 * Returns the number of links that are required to generate a 2d torus for the
 * given number of peers. Also returns the arrangment (number of rows and the
 * length of each row)
 *
 * @param num_peers number of peers
 * @param rows number of rows in the 2d torus. Can be NULL
 * @param rows_len the length of each row. This array will be allocated
 *          fresh. The caller should free it. Can be NULL
 */
unsigned int
GNUNET_TESTBED_2dtorus_calc_links (unsigned int num_peers,
                                   unsigned int *rows,
                                   unsigned int **rows_len)
{
  double sq;
  unsigned int sq_floor;
  unsigned int _rows;
  unsigned int *_rows_len;
  unsigned int x;
  unsigned int y;
  unsigned int _num_peers;
  unsigned int cnt;
  
  sq = sqrt (num_peers);
  sq = floor (sq);
  sq_floor = (unsigned int) sq;
  _rows = (sq_floor + 1);
  _rows_len = GNUNET_malloc (sizeof (unsigned int) * _rows);  
  for (y = 0; y < _rows - 1; y++)
    _rows_len[y] = sq_floor;
  _num_peers = sq_floor * sq_floor;
  cnt = 2 * _num_peers;
  x = 0;
  y = 0;
  while (_num_peers < num_peers)
  {
    if (x < y)
      _rows_len[_rows - 1] = ++x;
    else
      _rows_len[y++]++;
    _num_peers++;
  }
  cnt += (x < 2) ? x : 2 * x;
  cnt += (y < 2) ? y : 2 * y;
  if (0 == _rows_len[_rows - 1])
    _rows--;
  if (NULL != rows)
    *rows = _rows;
  if (NULL != rows_len)
    *rows_len = _rows_len;
  else
    GNUNET_free (_rows_len);
  return cnt;
}


/**
 * Generates ring topology
 *
 * @param tc the topology context
 */
static void
gen_topo_2dtorus (struct TopologyContext *tc)
{
  unsigned int rows;
  unsigned int *rows_len;
  unsigned int x;
  unsigned int y;
  unsigned int cnt;
  unsigned int offset;

  tc->link_array_size = GNUNET_TESTBED_2dtorus_calc_links (tc->num_peers, &rows,
                                                           &rows_len);
  tc->link_array = GNUNET_malloc (sizeof (struct OverlayLink) *
                                  tc->link_array_size);
  cnt = 0;
  offset = 0;
  for (y = 0; y < rows; y++)
  {
    for (x = 0; x < rows_len[y] - 1; x++)
    {
      tc->link_array[cnt].tc = tc;
      tc->link_array[cnt].A = offset + x;
      tc->link_array[cnt].B = offset + x + 1;
      cnt++;
    }
    if (0 == x)
      break;
    tc->link_array[cnt].tc = tc;
    tc->link_array[cnt].A = offset + x;
    tc->link_array[cnt].B = offset;
    cnt++;
    offset += rows_len[y];
  }
  for (x = 0; x < rows_len[0]; x++)
  {
    offset = 0;
    for (y = 0; y < rows - 1; y++)
    {
      if (x == rows_len[y+1])
        break;
      GNUNET_assert (x < rows_len[y+1]);
      tc->link_array[cnt].tc= tc;
      tc->link_array[cnt].A = offset + x;
      offset += rows_len[y];
      tc->link_array[cnt].B = offset + x;
      cnt++;
    }
    if (0 == offset)
      break;
    tc->link_array[cnt].tc = tc;
    tc->link_array[cnt].A = offset + x;
    tc->link_array[cnt].B = x;
    cnt++;
  }
  GNUNET_assert (cnt == tc->link_array_size);
  GNUNET_free (rows_len);
}


/**
 * Generates ring topology
 *
 * @param tc the topology context
 * @param links the number of random links to establish
 * @param append GNUNET_YES to add links to existing link array; GNUNET_NO to
 *          create a new link array
 */
static void
gen_topo_random (struct TopologyContext *tc, unsigned int links, int append)
{
  unsigned int cnt;
  unsigned int index;
  uint32_t A_rand;
  uint32_t B_rand;
  
  if (GNUNET_YES == append)
  {
    GNUNET_assert ((0 < tc->link_array_size) && (NULL != tc->link_array));
    index = tc->link_array_size;   
    tc->link_array_size += links;
    tc->link_array = GNUNET_realloc (tc->link_array,
                                   sizeof (struct OverlayLink) *
                                     tc->link_array_size);
  }
  else
  {
    GNUNET_assert ((0 == tc->link_array_size) && (NULL == tc->link_array));
    index = 0;   
    tc->link_array_size = links;
    tc->link_array = GNUNET_malloc (sizeof (struct OverlayLink) *
                                    tc->link_array_size);
  }
  for (cnt = 0; cnt < links; cnt++)
  {
    do {
      A_rand = GNUNET_CRYPTO_random_u32 (GNUNET_CRYPTO_QUALITY_WEAK,
                                         tc->num_peers);
      B_rand = GNUNET_CRYPTO_random_u32 (GNUNET_CRYPTO_QUALITY_WEAK,
                                         tc->num_peers);
    } while (A_rand == B_rand);
    tc->link_array[index + cnt].op = NULL;
    tc->link_array[index + cnt].A = A_rand;
    tc->link_array[index + cnt].B = B_rand;
    tc->link_array[index + cnt].tc = tc;
  }
}


/**
 * Configure overall network topology to have a particular shape.
 *
 * @param op_cls closure argument to give with the operation event
 * @param num_peers number of peers in 'peers'
 * @param peers array of 'num_peers' with the peers to configure
 * @param topo desired underlay topology to use
 * @param ap topology-specific options
 * @return handle to the operation, NULL if configuring the topology
 *         is not allowed at this time
 */
struct GNUNET_TESTBED_Operation *
GNUNET_TESTBED_underlay_configure_topology_va (void *op_cls,
                                               unsigned int num_peers,
                                               struct GNUNET_TESTBED_Peer
                                               **peers,
                                               enum
                                               GNUNET_TESTBED_TopologyOption
                                               topo, va_list ap)
{
  GNUNET_break (0);
  return NULL;
}


/**
 * Configure overall network topology to have a particular shape.
 *
 * @param op_cls closure argument to give with the operation event
 * @param num_peers number of peers in 'peers'
 * @param peers array of 'num_peers' with the peers to configure
 * @param topo desired underlay topology to use
 * @param ... topology-specific options
 * @return handle to the operation, NULL if configuring the topology
 *         is not allowed at this time
 */
struct GNUNET_TESTBED_Operation *
GNUNET_TESTBED_underlay_configure_topology (void *op_cls,
                                            unsigned int num_peers,
                                            struct GNUNET_TESTBED_Peer **peers,
                                            enum GNUNET_TESTBED_TopologyOption
                                            topo, ...)
{
  GNUNET_break (0);
  return NULL;
}


/**
 * All peers must have been started before calling this function.
 * This function then connects the given peers in the P2P overlay
 * using the given topology.
 *
 * @param op_cls closure argument to give with the operation event
 * @param num_peers number of peers in 'peers'
 * @param peers array of 'num_peers' with the peers to configure
 * @param topo desired underlay topology to use
 * @param va topology-specific options
 * @return handle to the operation, NULL if connecting these
 *         peers is fundamentally not possible at this time (peers
 *         not running or underlay disallows) or if num_peers is less than 2
 */
struct GNUNET_TESTBED_Operation *
GNUNET_TESTBED_overlay_configure_topology_va (void *op_cls,
                                              unsigned int num_peers,
                                              struct GNUNET_TESTBED_Peer **peers,
                                              enum GNUNET_TESTBED_TopologyOption
                                              topo, va_list va)
{
  struct TopologyContext *tc;
  struct GNUNET_TESTBED_Operation *op;
  struct GNUNET_TESTBED_Controller *c;
  enum GNUNET_TESTBED_TopologyOption secondary_option;
  unsigned int cnt;

  if (num_peers < 2)
    return NULL;
  c = peers[0]->controller;
  tc = GNUNET_malloc (sizeof (struct TopologyContext));
  tc->peers = peers;
  tc->num_peers = num_peers;
  tc->op_cls = op_cls;
  switch (topo)
  {
  case GNUNET_TESTBED_TOPOLOGY_LINE:
    gen_topo_line (tc);
    break;
  case GNUNET_TESTBED_TOPOLOGY_RING:
    gen_topo_ring (tc);
    break;
  case GNUNET_TESTBED_TOPOLOGY_ERDOS_RENYI:
    gen_topo_random (tc,
                     va_arg (va, unsigned int),
                     GNUNET_NO);
    break;
  case GNUNET_TESTBED_TOPOLOGY_SMALL_WORLD_RING:
    gen_topo_ring (tc);
    gen_topo_random (tc,
                     va_arg (va, unsigned int),
                     GNUNET_YES);
    break;
  case GNUNET_TESTBED_TOPOLOGY_CLIQUE:
    tc->link_array_size = num_peers * (num_peers - 1);
    tc->link_array = GNUNET_malloc (sizeof (struct OverlayLink) *
                                    tc->link_array_size);
    {
      unsigned int offset;
      
      offset = 0;
      for (cnt=0; cnt < num_peers; cnt++)
      {
        unsigned int neighbour;
        
        for (neighbour=0; neighbour < num_peers; neighbour++)
        {
          if (neighbour == cnt)
            continue;
          tc->link_array[offset].A = cnt;
          tc->link_array[offset].B = neighbour;
          tc->link_array[offset].tc = tc;
          offset++;
        }
      }
    }
    break;
  case GNUNET_TESTBED_TOPOLOGY_2D_TORUS:
    gen_topo_2dtorus (tc);
    break;
  default:
    GNUNET_break (0);
    GNUNET_free (tc);
    return NULL;
  }
  do
  {
    secondary_option = va_arg (va, enum GNUNET_TESTBED_TopologyOption);
    switch (secondary_option)
    {
    case GNUNET_TESTBED_TOPOLOGY_DISABLE_AUTO_RETRY:
      tc->disable_retry = GNUNET_YES;
      break;
    case GNUNET_TESTBED_TOPOLOGY_OPTION_END:
      break;
    default:
      GNUNET_break (0);         /* Should not use any other option apart from
                                   the ones handled here */
      GNUNET_free_non_null (tc->link_array);
      GNUNET_free (tc);
      return NULL;
    }
  } while (GNUNET_TESTBED_TOPOLOGY_OPTION_END != secondary_option);
  op = GNUNET_TESTBED_operation_create_ (tc,
					 &opstart_overlay_configure_topology,
					 &oprelease_overlay_configure_topology);
  GNUNET_TESTBED_operation_queue_insert_
      (c->opq_parallel_topology_config_operations, op);
  GNUNET_TESTBED_operation_begin_wait_ (op);
  return op;
}


/**
 * All peers must have been started before calling this function.
 * This function then connects the given peers in the P2P overlay
 * using the given topology.
 *
 * @param op_cls closure argument to give with the operation event
 * @param num_peers number of peers in 'peers'
 * @param peers array of 'num_peers' with the peers to configure
 * @param topo desired underlay topology to use
 * @param ... topology-specific options
 * @return handle to the operation, NULL if connecting these
 *         peers is fundamentally not possible at this time (peers
 *         not running or underlay disallows) or if num_peers is less than 2
 */
struct GNUNET_TESTBED_Operation *
GNUNET_TESTBED_overlay_configure_topology (void *op_cls, unsigned int num_peers,
                                           struct GNUNET_TESTBED_Peer **peers,
                                           enum GNUNET_TESTBED_TopologyOption
                                           topo, ...)
{
  struct GNUNET_TESTBED_Operation *op;
  va_list vargs;

  GNUNET_assert (topo < GNUNET_TESTBED_TOPOLOGY_OPTION_END);
  va_start (vargs, topo);
  op = GNUNET_TESTBED_overlay_configure_topology_va (op_cls, num_peers, peers,
						     topo, vargs);
  va_end (vargs);
  return op;
}

/* end of testbed_api_topology.c */
