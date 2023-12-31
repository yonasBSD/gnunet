/*
     This file is part of GNUnet.
     Copyright (C) 2006, 2009 GNUnet e.V.

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

#if !defined (__GNUNET_UTIL_LIB_H_INSIDE__)
#error "Only <gnunet_util_lib.h> can be included directly."
#endif

/**
 * @addtogroup libgnunetutil
 * Multi-function utilities library for GNUnet programs
 * @{
 *
 * @author Christian Grothoff
 *
 * @file
 * Helper library for interning of peer identifiers
 *
 * @defgroup peer  Peer library
 * Helper library for interning of peer identifiers
 * @{
 */

#ifndef GNUNET_PEER_LIB_H
#define GNUNET_PEER_LIB_H


#include "gnunet_util_lib.h"

#ifdef __cplusplus
extern "C"
{
#if 0                           /* keep Emacsens' auto-indent happy */
}
#endif
#endif

/**
 * A GNUNET_PEER_Id is simply a shorter version of a "struct
 * GNUNET_PeerIdentifier" that can be used inside of a GNUnet peer to
 * save memory when the same identifier needs to be used over and over
 * again.
 */
typedef unsigned int GNUNET_PEER_Id;


/**
 * Search for a peer identity. The reference counter is not changed.
 *
 * @param pid identity to find
 * @return the interned identity or 0.
 */
GNUNET_PEER_Id
GNUNET_PEER_search (const struct GNUNET_PeerIdentity *pid);


/**
 * Intern an peer identity.  If the identity is already known, its
 * reference counter will be increased by one.
 *
 * @param pid identity to intern
 * @return the interned identity.
 */
GNUNET_PEER_Id
GNUNET_PEER_intern (const struct GNUNET_PeerIdentity *pid);


/**
 * Change the reference counter of an interned PID.
 *
 * @param id identity to change the RC of
 * @param delta how much to change the RC
 */
void
GNUNET_PEER_change_rc (GNUNET_PEER_Id id, int delta);


/**
 * Decrement multiple RCs of peer identities by one.
 *
 * @param ids array of PIDs to decrement the RCs of
 * @param count size of the @a ids array
 */
void
GNUNET_PEER_decrement_rcs (const GNUNET_PEER_Id *ids,
                           unsigned int count);


/**
 * Convert an interned PID to a normal peer identity.
 *
 * @param id interned PID to convert
 * @param pid where to write the normal peer identity
 */
void
GNUNET_PEER_resolve (GNUNET_PEER_Id id,
                     struct GNUNET_PeerIdentity *pid);


/**
 * Convert an interned PID to a normal peer identity.
 *
 * @param id interned PID to convert
 * @return pointer to peer identity, valid as long @a id is valid
 */
const struct GNUNET_PeerIdentity *
GNUNET_PEER_resolve2 (GNUNET_PEER_Id id);


#if 0                           /* keep Emacsens' auto-indent happy */
{
#endif
#ifdef __cplusplus
}
#endif

/* ifndef GNUNET_PEER_LIB_H */
#endif

/** @} */  /* end of group */

/** @} */ /* end of group addition */

/* end of gnunet_peer_lib.h */
