/*
     This file is part of GNUnet.
     Copyright (C) 2013, 2014, 2020 GNUnet e.V.

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
 * @author Florian Dold
 * @author Christian Grothoff
 * @file seti/gnunet-service-seti_protocol.h
 * @brief Peer-to-Peer messages for gnunet set
 */
#ifndef SETI_PROTOCOL_H
#define SETI_PROTOCOL_H

#include "platform.h"
#include "gnunet_common.h"


GNUNET_NETWORK_STRUCT_BEGIN

struct OperationRequestMessage
{
  /**
   * Type: #GNUNET_MESSAGE_TYPE_SET_P2P_OPERATION_REQUEST
   */
  struct GNUNET_MessageHeader header;

  /**
   * For Intersection: my element count
   */
  uint32_t element_count GNUNET_PACKED;

  /**
   * Application-specific identifier of the request.
   */
  struct GNUNET_HashCode app_idX;

  /* rest: optional message */
};


/**
 * During intersection, the first (and possibly second) message
 * send it the number of elements in the set, to allow the peers
 * to decide who should start with the Bloom filter.
 */
struct IntersectionElementInfoMessage
{
  /**
   * Type: #GNUNET_MESSAGE_TYPE_SET_INTERSECTION_P2P_ELEMENT_INFO
   */
  struct GNUNET_MessageHeader header;

  /**
   * mutator used with this bloomfilter.
   */
  uint32_t sender_element_count GNUNET_PACKED;
};


/**
 * Bloom filter messages exchanged for set intersection calculation.
 */
struct BFMessage
{
  /**
   * Type: #GNUNET_MESSAGE_TYPE_SET_INTERSECTION_P2P_BF
   */
  struct GNUNET_MessageHeader header;

  /**
   * Number of elements the sender still has in the set.
   */
  uint32_t sender_element_count GNUNET_PACKED;

  /**
   * XOR of all hashes over all elements remaining in the set.
   * Used to determine termination.
   */
  struct GNUNET_HashCode element_xor_hash;

  /**
   * Mutator used with this bloomfilter.
   */
  uint32_t sender_mutator GNUNET_PACKED;

  /**
   * Total length of the bloomfilter data.
   */
  uint32_t bloomfilter_total_length GNUNET_PACKED;

  /**
   * Number of bits (k-value) used in encoding the bloomfilter.
   */
  uint32_t bits_per_element GNUNET_PACKED;

  /**
   * rest: the sender's bloomfilter
   */
};


/**
 * Last message, send to confirm the final set.  Contains the element
 * count as it is possible that the peer determined that we were done
 * by getting the empty set, which in that case also needs to be
 * communicated.
 */
struct IntersectionDoneMessage
{
  /**
   * Type: #GNUNET_MESSAGE_TYPE_SET_INTERSECTION_P2P_DONE
   */
  struct GNUNET_MessageHeader header;

  /**
   * Final number of elements in intersection.
   */
  uint32_t final_element_count GNUNET_PACKED;

  /**
   * XOR of all hashes over all elements remaining in the set.
   */
  struct GNUNET_HashCode element_xor_hash;
};


GNUNET_NETWORK_STRUCT_END

#endif
