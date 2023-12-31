/*
     This file is part of GNUnet.
     Copyright (C) 2009-2013, 2016 GNUnet e.V.

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
 * @addtogroup libgnunetutil
 * Multi-function utilities library for GNUnet programs
 * @{
 *
 * @addtogroup networking
 * @{
 *
 * @author Christian Grothoff
 *
 * @file
 * Library for tokenizing a message stream

 * @defgroup server  Server library
 * Library for tokenizing a message stream
 *
 * @see [Documentation](https://gnunet.org/mst)
 *
 * @{
 */

#if !defined (__GNUNET_UTIL_LIB_H_INSIDE__)
#error "Only <gnunet_util_lib.h> can be included directly."
#endif

#ifndef GNUNET_MST_LIB_H
#define GNUNET_MST_LIB_H

#ifdef __cplusplus
extern "C"
{
#if 0                           /* keep Emacsens' auto-indent happy */
}
#endif
#endif


/**
 * Handle to a message stream tokenizer.
 */
struct GNUNET_MessageStreamTokenizer;


/**
 * Functions with this signature are called whenever a
 * complete message is received by the tokenizer.
 *
 * Do not call #GNUNET_mst_destroy from within
 * the scope of this callback.
 *
 * @param cls closure
 * @param message the actual message
 * @return #GNUNET_OK on success,
 *     #GNUNET_NO to stop further processing due to disconnect (no error)
 *     #GNUNET_SYSERR to stop further processing due to error
 */
typedef int
(*GNUNET_MessageTokenizerCallback) (void *cls,
                                    const struct GNUNET_MessageHeader *message);


/**
 * Create a message stream tokenizer.
 *
 * @param cb function to call on completed messages
 * @param cb_cls closure for @a cb
 * @return handle to tokenizer
 */
struct GNUNET_MessageStreamTokenizer *
GNUNET_MST_create (GNUNET_MessageTokenizerCallback cb,
                   void *cb_cls);


/**
 * Add incoming data to the receive buffer and call the
 * callback for all complete messages.
 *
 * @param mst tokenizer to use
 * @param buf input data to add
 * @param size number of bytes in @a buf
 * @param purge should any excess bytes in the buffer be discarded
 *       (i.e. for packet-based services like UDP)
 * @param one_shot only call callback once, keep rest of message in buffer
 * @return #GNUNET_OK if we are done processing (need more data)
 *         #GNUNET_NO if one_shot was set and we have another message ready
 *         #GNUNET_SYSERR if the data stream is corrupt
 */
enum GNUNET_GenericReturnValue
GNUNET_MST_from_buffer (struct GNUNET_MessageStreamTokenizer *mst,
                        const char *buf,
                        size_t size,
                        int purge,
                        int one_shot);


/**
 * Add incoming data to the receive buffer and call the
 * callback for all complete messages.
 *
 * @param mst tokenizer to use
 * @param buf input data to add
 * @param size number of bytes in @a buf
 * @param purge should any excess bytes in the buffer be discarded
 *       (i.e. for packet-based services like UDP)
 * @param one_shot only call callback once, keep rest of message in buffer
 * @return #GNUNET_OK if we are done processing (need more data)
 *         #GNUNET_NO if one_shot was set and we have another message ready
 *         #GNUNET_SYSERR if the data stream is corrupt
 */
enum GNUNET_GenericReturnValue
GNUNET_MST_read (struct GNUNET_MessageStreamTokenizer *mst,
                 struct GNUNET_NETWORK_Handle *sock,
                 int purge,
                 int one_shot);


/**
 * Obtain the next message from the @a mst, assuming that
 * there are more unprocessed messages in the internal buffer
 * of the @a mst.
 *
 * @param mst tokenizer to use
 * @param one_shot only call callback once, keep rest of message in buffer
 * @return #GNUNET_OK if we are done processing (need more data)
 *         #GNUNET_NO if one_shot was set and we have another message ready
 *         #GNUNET_SYSERR if the data stream is corrupt
 */
enum GNUNET_GenericReturnValue
GNUNET_MST_next (struct GNUNET_MessageStreamTokenizer *mst,
                 int one_shot);


/**
 * Destroys a tokenizer.
 *
 * @param mst tokenizer to destroy
 */
void
GNUNET_MST_destroy (struct GNUNET_MessageStreamTokenizer *mst);


#if 0                           /* keep Emacsens' auto-indent happy */
{
#endif
#ifdef __cplusplus
}
#endif

#endif

/** @} */  /* end of group server */

/** @} */ /* end of group addition to networking */

/** @} */ /* end of group addition to libgnunetutil*/

/* end of gnunet_mst_lib.h */
