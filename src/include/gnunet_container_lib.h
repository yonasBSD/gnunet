/*
     This file is part of GNUnet.
     Copyright (C) 2001-2015 GNUnet e.V.

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
 * @author Christian Grothoff
 * @author Nils Durner
 *
 * @file
 * Container classes for GNUnet
 *
 * @addtogroup container
 * Common data structures in GNUnet programs
 * @{
 *
 * @defgroup hashmap  MultiHashMap
 * Hash map with multiple values per key.
 *
 * @see [Documentation](https://gnunet.org/util_multihashmap)
 *
 * @defgroup heap  Heap
 * Min- or max-heap with arbitrary element removal
 *
 * @defgroup bloomfilter  Bloom filter
 * Probabilistic set tests
 *
 * @defgroup dll  Doubly-linked list
 *
 * @see [Documentation](https://gnunet.org/mdll-api)
 *
 *
 * @}
 */

#include "gnunet_common.h"
#if !defined (__GNUNET_UTIL_LIB_H_INSIDE__)
#error "Only <gnunet_util_lib.h> can be included directly."
#endif

#ifndef GNUNET_CONTAINER_LIB_H
#define GNUNET_CONTAINER_LIB_H

#ifdef __cplusplus
extern "C" {
#if 0 /* keep Emacsens' auto-indent happy */
}
#endif
#endif


/* add error and config prototypes */

#include "gnunet_crypto_lib.h"


/**
 * Try to compress the given block of data using libz.  Only returns
 * the compressed block if compression worked and the new block is
 * actually smaller.  Decompress using #GNUNET_decompress().
 *
 * @param data block to compress; if compression
 *        resulted in a smaller block, the first
 *        bytes of data are updated to the compressed
 *        data
 * @param old_size number of bytes in data
 * @param[out] result set to the compressed data, if compression worked
 * @param[out] new_size set to size of result, if compression worked
 * @return #GNUNET_YES if compression reduce the size,
 *         #GNUNET_NO if compression did not help
 */
int
GNUNET_try_compression (const char *data,
                        size_t old_size,
                        char **result,
                        size_t *new_size);


/**
 * Decompress input, return the decompressed data as output.  Dual to
 * #GNUNET_try_compression(). Caller must set @a output_size to the
 * number of bytes that were originally compressed.
 *
 * @param input compressed data
 * @param input_size number of bytes in input
 * @param output_size expected size of the output
 * @return NULL on error, buffer of @a output_size decompressed bytes otherwise
 */
char *
GNUNET_decompress (const char *input, size_t input_size, size_t output_size);


/* ******************* bloomfilter ***************** */

/**
 * @brief bloomfilter representation (opaque)
 * @ingroup bloomfilter
 */
struct GNUNET_CONTAINER_BloomFilter;


/**
 * @ingroup bloomfilter
 * Iterator over `struct GNUNET_HashCode`.
 *
 * @param cls closure
 * @param next set to the next hash code
 * @return #GNUNET_YES if next was updated
 *         #GNUNET_NO if there are no more entries
 */
typedef int (*GNUNET_CONTAINER_HashCodeIterator) (void *cls,
                                                  struct GNUNET_HashCode *next);


/**
 * @ingroup bloomfilter
 * Load a Bloom filter from a file.
 *
 * @param filename the name of the file (or the prefix)
 * @param size the size of the bloom-filter (number of
 *        bytes of storage space to use); will be rounded up
 *        to next power of 2
 * @param k the number of #GNUNET_CRYPTO_hash-functions to apply per
 *        element (number of bits set per element in the set)
 * @return the bloomfilter
 */
struct GNUNET_CONTAINER_BloomFilter *
GNUNET_CONTAINER_bloomfilter_load (const char *filename,
                                   size_t size,
                                   unsigned int k);


/**
 * @ingroup bloomfilter
 * Create a Bloom filter from raw bits.
 *
 * @param data the raw bits in memory (maybe NULL,
 *        in which case all bits should be considered
 *        to be zero).
 * @param size the size of the bloom-filter (number of
 *        bytes of storage space to use); also size of @a data
 *        -- unless data is NULL.  Must be a power of 2.
 * @param k the number of #GNUNET_CRYPTO_hash-functions to apply per
 *        element (number of bits set per element in the set)
 * @return the bloomfilter
 */
struct GNUNET_CONTAINER_BloomFilter *
GNUNET_CONTAINER_bloomfilter_init (const char *data,
                                   size_t size,
                                   unsigned int k);


/**
 * @ingroup bloomfilter
 * Copy the raw data of this Bloom filter into
 * the given data array.
 *
 * @param data where to write the data
 * @param size the size of the given @a data array
 * @return #GNUNET_SYSERR if the data array of the wrong size
 */
enum GNUNET_GenericReturnValue
GNUNET_CONTAINER_bloomfilter_get_raw_data (
  const struct GNUNET_CONTAINER_BloomFilter *bf,
  char *data,
  size_t size);


/**
 * @ingroup bloomfilter
 * Test if an element is in the filter.
 *
 * @param e the element
 * @param bf the filter
 * @return true if the element is in the filter, false if not
 */
bool
GNUNET_CONTAINER_bloomfilter_test (
  const struct GNUNET_CONTAINER_BloomFilter *bf,
  const struct GNUNET_HashCode *e);


/**
 * @ingroup bloomfilter
 * Add an element to the filter.
 *
 * @param bf the filter
 * @param e the element
 */
void
GNUNET_CONTAINER_bloomfilter_add (struct GNUNET_CONTAINER_BloomFilter *bf,
                                  const struct GNUNET_HashCode *e);


/**
 * @ingroup bloomfilter
 * Remove an element from the filter.
 *
 * @param bf the filter
 * @param e the element to remove
 */
void
GNUNET_CONTAINER_bloomfilter_remove (struct GNUNET_CONTAINER_BloomFilter *bf,
                                     const struct GNUNET_HashCode *e);


/**
 * @ingroup bloomfilter
 * Create a copy of a bloomfilter.
 *
 * @param bf the filter
 * @return copy of bf
 */
struct GNUNET_CONTAINER_BloomFilter *
GNUNET_CONTAINER_bloomfilter_copy (
  const struct GNUNET_CONTAINER_BloomFilter *bf);


/**
 * @ingroup bloomfilter
 * Free the space associated with a filter
 * in memory, flush to drive if needed (do not
 * free the space on the drive).
 *
 * @param bf the filter
 */
void
GNUNET_CONTAINER_bloomfilter_free (struct GNUNET_CONTAINER_BloomFilter *bf);


/**
 * Get the number of the addresses set per element in the bloom filter.
 *
 * @param bf the filter
 * @return addresses set per element in the bf
 */
size_t
GNUNET_CONTAINER_bloomfilter_get_element_addresses (
  const struct GNUNET_CONTAINER_BloomFilter *bf);


/**
 * @ingroup bloomfilter
 * Get size of the bloom filter.
 *
 * @param bf the filter
 * @return number of bytes used for the data of the bloom filter
 */
size_t
GNUNET_CONTAINER_bloomfilter_get_size (
  const struct GNUNET_CONTAINER_BloomFilter *bf);


/**
 * @ingroup bloomfilter
 * Reset a Bloom filter to empty.
 *
 * @param bf the filter
 */
void
GNUNET_CONTAINER_bloomfilter_clear (struct GNUNET_CONTAINER_BloomFilter *bf);


/**
 * @ingroup bloomfilter
 * "or" the entries of the given raw data array with the
 * data of the given Bloom filter.  Assumes that
 * the @a size of the @a data array and the current filter
 * match.
 *
 * @param bf the filter
 * @param data data to OR-in
 * @param size size of @a data
 * @return #GNUNET_OK on success
 */
enum GNUNET_GenericReturnValue
GNUNET_CONTAINER_bloomfilter_or (struct GNUNET_CONTAINER_BloomFilter *bf,
                                 const char *data,
                                 size_t size);


/**
 * @ingroup bloomfilter
 * "or" the entries of the given raw data array with the
 * data of the given Bloom filter.  Assumes that
 * the size of the two filters matches.
 *
 * @param bf the filter
 * @param to_or the bloomfilter to or-in
 * @return #GNUNET_OK on success
 */
enum GNUNET_GenericReturnValue
GNUNET_CONTAINER_bloomfilter_or2 (
  struct GNUNET_CONTAINER_BloomFilter *bf,
  const struct GNUNET_CONTAINER_BloomFilter *to_or);


/**
 * @ingroup bloomfilter
 * Resize a bloom filter.  Note that this operation
 * is pretty costly.  Essentially, the Bloom filter
 * needs to be completely re-build.
 *
 * @param bf the filter
 * @param iterator an iterator over all elements stored in the BF
 * @param iterator_cls closure for @a iterator
 * @param size the new size for the filter
 * @param k the new number of #GNUNET_CRYPTO_hash-function to apply per element
 */
void
GNUNET_CONTAINER_bloomfilter_resize (struct GNUNET_CONTAINER_BloomFilter *bf,
                                     GNUNET_CONTAINER_HashCodeIterator iterator,
                                     void *iterator_cls,
                                     size_t size,
                                     unsigned int k);



/* ******************************* HashMap **************************** */

/**
 * @ingroup hashmap
 * Opaque handle for a HashMap.
 */
struct GNUNET_CONTAINER_MultiHashMap;

/**
 * @ingroup hashmap
 * Opaque handle to an iterator over
 * a multihashmap.
 */
struct GNUNET_CONTAINER_MultiHashMapIterator;

/**
 * @ingroup hashmap
 * Options for storing values in the HashMap.
 */
enum GNUNET_CONTAINER_MultiHashMapOption
{
  /**
   * @ingroup hashmap
   * If a value with the given key exists, replace it.  Note that the
   * old value would NOT be freed by replace (the application has to
   * make sure that this happens if required).
   */
  GNUNET_CONTAINER_MULTIHASHMAPOPTION_REPLACE,

  /**
   * @ingroup hashmap
   * Allow multiple values with the same key.
   */
  GNUNET_CONTAINER_MULTIHASHMAPOPTION_MULTIPLE,

  /**
   * @ingroup hashmap
   * There must only be one value per key; storing a value should fail
   * if a value under the same key already exists.
   */
  GNUNET_CONTAINER_MULTIHASHMAPOPTION_UNIQUE_ONLY,

  /**
   * @ingroup hashmap There must only be one value per key, but don't
   * bother checking if a value already exists (faster than
   * #GNUNET_CONTAINER_MULTIHASHMAPOPTION_UNIQUE_ONLY; implemented
   * just like #GNUNET_CONTAINER_MULTIHASHMAPOPTION_MULTIPLE but this
   * option documents better what is intended if
   * #GNUNET_CONTAINER_MULTIHASHMAPOPTION_UNIQUE_ONLY is what is
   * desired).
   */
  GNUNET_CONTAINER_MULTIHASHMAPOPTION_UNIQUE_FAST
};


/**
 * @ingroup hashmap
 * Iterator over hash map entries.
 *
 * @param cls closure
 * @param key current key code
 * @param value value in the hash map
 * @return #GNUNET_YES if we should continue to
 *         iterate,
 *         #GNUNET_NO if not.
 */
typedef enum GNUNET_GenericReturnValue
(*GNUNET_CONTAINER_MultiHashMapIteratorCallback)(
  void *cls,
  const struct GNUNET_HashCode *key,
  void *value);


/**
 * @ingroup hashmap
 * Create a multi hash map.
 *
 * @param len initial size (map will grow as needed)
 * @param do_not_copy_keys #GNUNET_NO is always safe and should be used by default;
 *                         #GNUNET_YES means that on 'put', the 'key' does not have
 *                         to be copied as the destination of the pointer is
 *                         guaranteed to be life as long as the value is stored in
 *                         the hashmap.  This can significantly reduce memory
 *                         consumption, but of course is also a recipe for
 *                         heap corruption if the assumption is not true.  Only
 *                         use this if (1) memory use is important in this case and
 *                         (2) you have triple-checked that the invariant holds
 * @return NULL on error
 */
struct GNUNET_CONTAINER_MultiHashMap *
GNUNET_CONTAINER_multihashmap_create (unsigned int len,
                                      int do_not_copy_keys);


/**
 * @ingroup hashmap
 * Destroy a hash map.  Will not free any values
 * stored in the hash map!
 *
 * @param map the map
 */
void
GNUNET_CONTAINER_multihashmap_destroy (struct
                                       GNUNET_CONTAINER_MultiHashMap *map);


/**
 * @ingroup hashmap
 * Given a key find a value in the map matching the key.
 *
 * @param map the map
 * @param key what to look for
 * @return NULL if no value was found; note that
 *   this is indistinguishable from values that just
 *   happen to be NULL; use "contains" to test for
 *   key-value pairs with value NULL
 */
void *
GNUNET_CONTAINER_multihashmap_get (
  const struct GNUNET_CONTAINER_MultiHashMap *map,
  const struct GNUNET_HashCode *key);


/**
 * @ingroup hashmap
 * Remove the given key-value pair from the map.  Note that if the
 * key-value pair is in the map multiple times, only one of the pairs
 * will be removed.
 *
 * @param map the map
 * @param key key of the key-value pair
 * @param value value of the key-value pair
 * @return #GNUNET_YES on success, #GNUNET_NO if the key-value pair
 *  is not in the map
 */
enum GNUNET_GenericReturnValue
GNUNET_CONTAINER_multihashmap_remove (struct GNUNET_CONTAINER_MultiHashMap *map,
                                      const struct GNUNET_HashCode *key,
                                      const void *value);

/**
 * @ingroup hashmap
 * Remove all entries for the given key from the map.
 * Note that the values would not be "freed".
 *
 * @param map the map
 * @param key identifies values to be removed
 * @return number of values removed
 */
int
GNUNET_CONTAINER_multihashmap_remove_all (
  struct GNUNET_CONTAINER_MultiHashMap *map,
  const struct GNUNET_HashCode *key);


/**
 * @ingroup hashmap
 * Remove all entries from the map.
 * Note that the values would not be "freed".
 *
 * @param map the map
 * @return number of values removed
 */
unsigned int
GNUNET_CONTAINER_multihashmap_clear (struct GNUNET_CONTAINER_MultiHashMap *map);


/**
 * @ingroup hashmap
 * Check if the map contains any value under the given
 * key (including values that are NULL).
 *
 * @param map the map
 * @param key the key to test if a value exists for it
 * @return #GNUNET_YES if such a value exists,
 *         #GNUNET_NO if not
 */
enum GNUNET_GenericReturnValue
GNUNET_CONTAINER_multihashmap_contains (
  const struct GNUNET_CONTAINER_MultiHashMap *map,
  const struct GNUNET_HashCode *key);


/**
 * @ingroup hashmap
 * Check if the map contains the given value under the given
 * key.
 *
 * @param map the map
 * @param key the key to test if a value exists for it
 * @param value value to test for
 * @return #GNUNET_YES if such a value exists,
 *         #GNUNET_NO if not
 */
enum GNUNET_GenericReturnValue
GNUNET_CONTAINER_multihashmap_contains_value (
  const struct GNUNET_CONTAINER_MultiHashMap *map,
  const struct GNUNET_HashCode *key,
  const void *value);


/**
 * @ingroup hashmap
 * Store a key-value pair in the map.
 *
 * @param map the map
 * @param key key to use
 * @param value value to use
 * @param opt options for put
 * @return #GNUNET_OK on success,
 *         #GNUNET_NO if a value was replaced (with REPLACE)
 *         #GNUNET_SYSERR if #GNUNET_CONTAINER_MULTIHASHMAPOPTION_UNIQUE_ONLY was the option and the
 *                       value already exists
 */
enum GNUNET_GenericReturnValue
GNUNET_CONTAINER_multihashmap_put (
  struct GNUNET_CONTAINER_MultiHashMap *map,
  const struct GNUNET_HashCode *key,
  void *value,
  enum GNUNET_CONTAINER_MultiHashMapOption opt);

/**
 * @ingroup hashmap
 * Get the number of key-value pairs in the map.
 *
 * @param map the map
 * @return the number of key value pairs
 */
unsigned int
GNUNET_CONTAINER_multihashmap_size (
  const struct GNUNET_CONTAINER_MultiHashMap *map);


/**
 * @ingroup hashmap
 * Iterate over all entries in the map.
 *
 * @param map the map
 * @param it function to call on each entry
 * @param it_cls extra argument to @a it
 * @return the number of key value pairs processed,
 *         #GNUNET_SYSERR if it aborted iteration
 */
int
GNUNET_CONTAINER_multihashmap_iterate (
  struct GNUNET_CONTAINER_MultiHashMap *map,
  GNUNET_CONTAINER_MultiHashMapIteratorCallback it,
  void *it_cls);


/**
 * @ingroup hashmap
 * Create an iterator for a multihashmap.
 * The iterator can be used to retrieve all the elements in the multihashmap
 * one by one, without having to handle all elements at once (in contrast to
 * #GNUNET_CONTAINER_multihashmap_iterate).  Note that the iterator can not be
 * used anymore if elements have been removed from 'map' after the creation of
 * the iterator, or 'map' has been destroyed.  Adding elements to 'map' may
 * result in skipped or repeated elements.
 *
 * @param map the map to create an iterator for
 * @return an iterator over the given multihashmap @a map
 */
struct GNUNET_CONTAINER_MultiHashMapIterator *
GNUNET_CONTAINER_multihashmap_iterator_create (
  const struct GNUNET_CONTAINER_MultiHashMap *map);


/**
 * @ingroup hashmap
 * Retrieve the next element from the hash map at the iterator's
 * position.  If there are no elements left, #GNUNET_NO is returned,
 * and @a key and @a value are not modified.  This operation is only
 * allowed if no elements have been removed from the multihashmap
 * since the creation of @a iter, and the map has not been destroyed.
 * Adding elements may result in repeating or skipping elements.
 *
 * @param iter the iterator to get the next element from
 * @param key pointer to store the key in, can be NULL
 * @param value pointer to store the value in, can be NULL
 * @return #GNUNET_YES we returned an element,
 *         #GNUNET_NO if we are out of elements
 */
enum GNUNET_GenericReturnValue
GNUNET_CONTAINER_multihashmap_iterator_next (
  struct GNUNET_CONTAINER_MultiHashMapIterator *iter,
  struct GNUNET_HashCode *key,
  const void **value);


/**
 * @ingroup hashmap
 * Destroy a multihashmap iterator.
 *
 * @param iter the iterator to destroy
 */
void
GNUNET_CONTAINER_multihashmap_iterator_destroy (
  struct GNUNET_CONTAINER_MultiHashMapIterator *iter);


/**
 * @ingroup hashmap
 * Iterate over all entries in the map that match a particular key.
 *
 * @param map the map
 * @param key key that the entries must correspond to
 * @param it function to call on each entry
 * @param it_cls extra argument to @a it
 * @return the number of key value pairs processed,
 *         #GNUNET_SYSERR if it aborted iteration
 */
enum GNUNET_GenericReturnValue
GNUNET_CONTAINER_multihashmap_get_multiple (
  struct GNUNET_CONTAINER_MultiHashMap *map,
  const struct GNUNET_HashCode *key,
  GNUNET_CONTAINER_MultiHashMapIteratorCallback it,
  void *it_cls);


/**
 * @ingroup hashmap
 * Call @a it on a random value from the map, or not at all
 * if the map is empty.  Note that this function has linear
 * complexity (in the size of the map).
 *
 * @param map the map
 * @param it function to call on a random entry
 * @param it_cls extra argument to @a it
 * @return the number of key value pairs processed, zero or one.
 */
unsigned int
GNUNET_CONTAINER_multihashmap_get_random (
  const struct GNUNET_CONTAINER_MultiHashMap *map,
  GNUNET_CONTAINER_MultiHashMapIteratorCallback it,
  void *it_cls);


/* ***************** Version of Multihashmap for peer identities ****************** */

/**
 * @ingroup hashmap
 * Iterator over hash map entries.
 *
 * @param cls closure
 * @param key current public key
 * @param value value in the hash map
 * @return #GNUNET_YES if we should continue to
 *         iterate,
 *         #GNUNET_NO if not.
 */
typedef enum GNUNET_GenericReturnValue
(*GNUNET_CONTAINER_PeerMapIterator)(
  void *cls,
  const struct GNUNET_PeerIdentity *key,
  void *value);


/**
 * Hash map from peer identities to values.
 */
struct GNUNET_CONTAINER_MultiPeerMap;


/**
 * @ingroup hashmap
 * Create a multi peer map (hash map for public keys of peers).
 *
 * @param len initial size (map will grow as needed)
 * @param do_not_copy_keys #GNUNET_NO is always safe and should be used by default;
 *                         #GNUNET_YES means that on 'put', the 'key' does not have
 *                         to be copied as the destination of the pointer is
 *                         guaranteed to be life as long as the value is stored in
 *                         the hashmap.  This can significantly reduce memory
 *                         consumption, but of course is also a recipe for
 *                         heap corruption if the assumption is not true.  Only
 *                         use this if (1) memory use is important in this case and
 *                         (2) you have triple-checked that the invariant holds
 * @return NULL on error
 */
struct GNUNET_CONTAINER_MultiPeerMap *
GNUNET_CONTAINER_multipeermap_create (unsigned int len, int do_not_copy_keys);


/**
 * @ingroup hashmap
 * Destroy a hash map.  Will not free any values
 * stored in the hash map!
 *
 * @param map the map
 */
void
GNUNET_CONTAINER_multipeermap_destroy (
  struct GNUNET_CONTAINER_MultiPeerMap *map);


/**
 * @ingroup hashmap
 * Given a key find a value in the map matching the key.
 *
 * @param map the map
 * @param key what to look for
 * @return NULL if no value was found; note that
 *   this is indistinguishable from values that just
 *   happen to be NULL; use "contains" to test for
 *   key-value pairs with value NULL
 */
void *
GNUNET_CONTAINER_multipeermap_get (
  const struct GNUNET_CONTAINER_MultiPeerMap *map,
  const struct GNUNET_PeerIdentity *key);


/**
 * @ingroup hashmap
 * Remove the given key-value pair from the map.  Note that if the
 * key-value pair is in the map multiple times, only one of the pairs
 * will be removed.
 *
 * @param map the map
 * @param key key of the key-value pair
 * @param value value of the key-value pair
 * @return #GNUNET_YES on success, #GNUNET_NO if the key-value pair
 *  is not in the map
 */
enum GNUNET_GenericReturnValue
GNUNET_CONTAINER_multipeermap_remove (struct GNUNET_CONTAINER_MultiPeerMap *map,
                                      const struct GNUNET_PeerIdentity *key,
                                      const void *value);

/**
 * @ingroup hashmap
 * Remove all entries for the given key from the map.
 * Note that the values would not be "freed".
 *
 * @param map the map
 * @param key identifies values to be removed
 * @return number of values removed
 */
int
GNUNET_CONTAINER_multipeermap_remove_all (
  struct GNUNET_CONTAINER_MultiPeerMap *map,
  const struct GNUNET_PeerIdentity *key);


/**
 * @ingroup hashmap
 * Check if the map contains any value under the given
 * key (including values that are NULL).
 *
 * @param map the map
 * @param key the key to test if a value exists for it
 * @return #GNUNET_YES if such a value exists,
 *         #GNUNET_NO if not
 */
enum GNUNET_GenericReturnValue
GNUNET_CONTAINER_multipeermap_contains (
  const struct GNUNET_CONTAINER_MultiPeerMap *map,
  const struct GNUNET_PeerIdentity *key);


/**
 * @ingroup hashmap
 * Check if the map contains the given value under the given
 * key.
 *
 * @param map the map
 * @param key the key to test if a value exists for it
 * @param value value to test for
 * @return #GNUNET_YES if such a value exists,
 *         #GNUNET_NO if not
 */
enum GNUNET_GenericReturnValue
GNUNET_CONTAINER_multipeermap_contains_value (
  const struct GNUNET_CONTAINER_MultiPeerMap *map,
  const struct GNUNET_PeerIdentity *key,
  const void *value);


/**
 * @ingroup hashmap
 * Store a key-value pair in the map.
 *
 * @param map the map
 * @param key key to use
 * @param value value to use
 * @param opt options for put
 * @return #GNUNET_OK on success,
 *         #GNUNET_NO if a value was replaced (with REPLACE)
 *         #GNUNET_SYSERR if #GNUNET_CONTAINER_MULTIHASHMAPOPTION_UNIQUE_ONLY was the option and the
 *                       value already exists
 */
int
GNUNET_CONTAINER_multipeermap_put (
  struct GNUNET_CONTAINER_MultiPeerMap *map,
  const struct GNUNET_PeerIdentity *key,
  void *value,
  enum GNUNET_CONTAINER_MultiHashMapOption opt);


/**
 * @ingroup hashmap
 * Get the number of key-value pairs in the map.
 *
 * @param map the map
 * @return the number of key value pairs
 */
unsigned int
GNUNET_CONTAINER_multipeermap_size (
  const struct GNUNET_CONTAINER_MultiPeerMap *map);


/**
 * @ingroup hashmap
 * Iterate over all entries in the map.
 *
 * @param map the map
 * @param it function to call on each entry
 * @param it_cls extra argument to @a it
 * @return the number of key value pairs processed,
 *         #GNUNET_SYSERR if it aborted iteration
 */
int
GNUNET_CONTAINER_multipeermap_iterate (
  struct GNUNET_CONTAINER_MultiPeerMap *map,
  GNUNET_CONTAINER_PeerMapIterator it,
  void *it_cls);


struct GNUNET_CONTAINER_MultiPeerMapIterator;
/**
 * @ingroup hashmap
 * Create an iterator for a multihashmap.
 * The iterator can be used to retrieve all the elements in the multihashmap
 * one by one, without having to handle all elements at once (in contrast to
 * #GNUNET_CONTAINER_multipeermap_iterate).  Note that the iterator can not be
 * used anymore if elements have been removed from @a map after the creation of
 * the iterator, or 'map' has been destroyed.  Adding elements to @a map may
 * result in skipped or repeated elements.
 *
 * @param map the map to create an iterator for
 * @return an iterator over the given multihashmap @a map
 */
struct GNUNET_CONTAINER_MultiPeerMapIterator *
GNUNET_CONTAINER_multipeermap_iterator_create (
  const struct GNUNET_CONTAINER_MultiPeerMap *map);


/**
 * @ingroup hashmap
 * Retrieve the next element from the hash map at the iterator's
 * position.  If there are no elements left, #GNUNET_NO is returned,
 * and @a key and @a value are not modified.
 *
 * This operation is only allowed if no elements have been removed
 * from the multihashmap since the creation of @a iter, and the map
 * has not been destroyed.
 *
 * Adding elements may result in repeating or skipping elements.
 *
 * @param iter the iterator to get the next element from
 * @param key pointer to store the key in, can be NULL
 * @param value pointer to store the value in, can be NULL
 * @return #GNUNET_YES we returned an element,
 *         #GNUNET_NO if we are out of elements
 */
enum GNUNET_GenericReturnValue
GNUNET_CONTAINER_multipeermap_iterator_next (
  struct GNUNET_CONTAINER_MultiPeerMapIterator *iter,
  struct GNUNET_PeerIdentity *key,
  const void **value);


/**
 * @ingroup hashmap
 * Destroy a multipeermap iterator.
 *
 * @param iter the iterator to destroy
 */
void
GNUNET_CONTAINER_multipeermap_iterator_destroy (
  struct GNUNET_CONTAINER_MultiPeerMapIterator *iter);


/**
 * @ingroup hashmap
 * Iterate over all entries in the map that match a particular key.
 *
 * @param map the map
 * @param key public key that the entries must correspond to
 * @param it function to call on each entry
 * @param it_cls extra argument to @a it
 * @return the number of key value pairs processed,
 *         #GNUNET_SYSERR if it aborted iteration
 */
int
GNUNET_CONTAINER_multipeermap_get_multiple (
  struct GNUNET_CONTAINER_MultiPeerMap *map,
  const struct GNUNET_PeerIdentity *key,
  GNUNET_CONTAINER_PeerMapIterator it,
  void *it_cls);


/**
 * @ingroup hashmap
 * Call @a it on a random value from the map, or not at all
 * if the map is empty.  Note that this function has linear
 * complexity (in the size of the map).
 *
 * @param map the map
 * @param it function to call on a random entry
 * @param it_cls extra argument to @a it
 * @return the number of key value pairs processed, zero or one.
 */
unsigned int
GNUNET_CONTAINER_multipeermap_get_random (
  const struct GNUNET_CONTAINER_MultiPeerMap *map,
  GNUNET_CONTAINER_PeerMapIterator it,
  void *it_cls);


/* ***************** Version of Multihashmap for short hashes ****************** */

/**
 * @ingroup hashmap
 * Iterator over hash map entries.
 *
 * @param cls closure
 * @param key current public key
 * @param value value in the hash map
 * @return #GNUNET_YES if we should continue to
 *         iterate,
 *         #GNUNET_NO if not.
 */
typedef enum GNUNET_GenericReturnValue
(*GNUNET_CONTAINER_ShortmapIterator)(
  void *cls,
  const struct GNUNET_ShortHashCode *key,
  void *value);


/**
 * Hash map from peer identities to values.
 */
struct GNUNET_CONTAINER_MultiShortmap;


/**
 * @ingroup hashmap
 * Create a multi peer map (hash map for public keys of peers).
 *
 * @param len initial size (map will grow as needed)
 * @param do_not_copy_keys #GNUNET_NO is always safe and should be used by default;
 *                         #GNUNET_YES means that on 'put', the 'key' does not have
 *                         to be copied as the destination of the pointer is
 *                         guaranteed to be life as long as the value is stored in
 *                         the hashmap.  This can significantly reduce memory
 *                         consumption, but of course is also a recipe for
 *                         heap corruption if the assumption is not true.  Only
 *                         use this if (1) memory use is important in this case and
 *                         (2) you have triple-checked that the invariant holds
 * @return NULL on error
 */
struct GNUNET_CONTAINER_MultiShortmap *
GNUNET_CONTAINER_multishortmap_create (unsigned int len, int do_not_copy_keys);


/**
 * @ingroup hashmap
 * Destroy a hash map.  Will not free any values
 * stored in the hash map!
 *
 * @param map the map
 */
void
GNUNET_CONTAINER_multishortmap_destroy (
  struct GNUNET_CONTAINER_MultiShortmap *map);


/**
 * @ingroup hashmap
 * Given a key find a value in the map matching the key.
 *
 * @param map the map
 * @param key what to look for
 * @return NULL if no value was found; note that
 *   this is indistinguishable from values that just
 *   happen to be NULL; use "contains" to test for
 *   key-value pairs with value NULL
 */
void *
GNUNET_CONTAINER_multishortmap_get (
  const struct GNUNET_CONTAINER_MultiShortmap *map,
  const struct GNUNET_ShortHashCode *key);


/**
 * @ingroup hashmap
 * Remove the given key-value pair from the map.  Note that if the
 * key-value pair is in the map multiple times, only one of the pairs
 * will be removed.
 *
 * @param map the map
 * @param key key of the key-value pair
 * @param value value of the key-value pair
 * @return #GNUNET_YES on success, #GNUNET_NO if the key-value pair
 *  is not in the map
 */
int
GNUNET_CONTAINER_multishortmap_remove (
  struct GNUNET_CONTAINER_MultiShortmap *map,
  const struct GNUNET_ShortHashCode *key,
  const void *value);

/**
 * @ingroup hashmap
 * Remove all entries for the given key from the map.
 * Note that the values would not be "freed".
 *
 * @param map the map
 * @param key identifies values to be removed
 * @return number of values removed
 */
int
GNUNET_CONTAINER_multishortmap_remove_all (
  struct GNUNET_CONTAINER_MultiShortmap *map,
  const struct GNUNET_ShortHashCode *key);


/**
 * @ingroup hashmap
 * Check if the map contains any value under the given
 * key (including values that are NULL).
 *
 * @param map the map
 * @param key the key to test if a value exists for it
 * @return #GNUNET_YES if such a value exists,
 *         #GNUNET_NO if not
 */
int
GNUNET_CONTAINER_multishortmap_contains (
  const struct GNUNET_CONTAINER_MultiShortmap *map,
  const struct GNUNET_ShortHashCode *key);


/**
 * @ingroup hashmap
 * Check if the map contains the given value under the given
 * key.
 *
 * @param map the map
 * @param key the key to test if a value exists for it
 * @param value value to test for
 * @return #GNUNET_YES if such a value exists,
 *         #GNUNET_NO if not
 */
int
GNUNET_CONTAINER_multishortmap_contains_value (
  const struct GNUNET_CONTAINER_MultiShortmap *map,
  const struct GNUNET_ShortHashCode *key,
  const void *value);


/**
 * @ingroup hashmap
 * Store a key-value pair in the map.
 *
 * @param map the map
 * @param key key to use
 * @param value value to use
 * @param opt options for put
 * @return #GNUNET_OK on success,
 *         #GNUNET_NO if a value was replaced (with REPLACE)
 *         #GNUNET_SYSERR if #GNUNET_CONTAINER_MULTIHASHMAPOPTION_UNIQUE_ONLY was the option and the
 *                       value already exists
 */
enum GNUNET_GenericReturnValue
GNUNET_CONTAINER_multishortmap_put (
  struct GNUNET_CONTAINER_MultiShortmap *map,
  const struct GNUNET_ShortHashCode *key,
  void *value,
  enum GNUNET_CONTAINER_MultiHashMapOption opt);


/**
 * @ingroup hashmap
 * Get the number of key-value pairs in the map.
 *
 * @param map the map
 * @return the number of key value pairs
 */
unsigned int
GNUNET_CONTAINER_multishortmap_size (
  const struct GNUNET_CONTAINER_MultiShortmap *map);


/**
 * @ingroup hashmap
 * Iterate over all entries in the map.
 *
 * @param map the map
 * @param it function to call on each entry
 * @param it_cls extra argument to @a it
 * @return the number of key value pairs processed,
 *         #GNUNET_SYSERR if it aborted iteration
 */
int
GNUNET_CONTAINER_multishortmap_iterate (
  struct GNUNET_CONTAINER_MultiShortmap *map,
  GNUNET_CONTAINER_ShortmapIterator it,
  void *it_cls);


struct GNUNET_CONTAINER_MultiShortmapIterator;


/**
 * @ingroup hashmap
 * Create an iterator for a multihashmap.
 * The iterator can be used to retrieve all the elements in the multihashmap
 * one by one, without having to handle all elements at once (in contrast to
 * #GNUNET_CONTAINER_multishortmap_iterate).  Note that the iterator can not be
 * used anymore if elements have been removed from @a map after the creation of
 * the iterator, or 'map' has been destroyed.  Adding elements to @a map may
 * result in skipped or repeated elements.
 *
 * @param map the map to create an iterator for
 * @return an iterator over the given multihashmap @a map
 */
struct GNUNET_CONTAINER_MultiShortmapIterator *
GNUNET_CONTAINER_multishortmap_iterator_create (
  const struct GNUNET_CONTAINER_MultiShortmap *map);


/**
 * @ingroup hashmap
 * Retrieve the next element from the hash map at the iterator's
 * position.  If there are no elements left, #GNUNET_NO is returned,
 * and @a key and @a value are not modified.  This operation is only
 * allowed if no elements have been removed from the multihashmap
 * since the creation of @a iter, and the map has not been destroyed.
 * Adding elements may result in repeating or skipping elements.
 *
 * @param iter the iterator to get the next element from
 * @param key pointer to store the key in, can be NULL
 * @param value pointer to store the value in, can be NULL
 * @return #GNUNET_YES we returned an element,
 *         #GNUNET_NO if we are out of elements
 */
enum GNUNET_GenericReturnValue
GNUNET_CONTAINER_multishortmap_iterator_next (
  struct GNUNET_CONTAINER_MultiShortmapIterator *iter,
  struct GNUNET_ShortHashCode *key,
  const void **value);


/**
 * @ingroup hashmap
 * Destroy a multishortmap iterator.
 *
 * @param iter the iterator to destroy
 */
void
GNUNET_CONTAINER_multishortmap_iterator_destroy (
  struct GNUNET_CONTAINER_MultiShortmapIterator *iter);


/**
 * @ingroup hashmap
 * Iterate over all entries in the map that match a particular key.
 *
 * @param map the map
 * @param key public key that the entries must correspond to
 * @param it function to call on each entry
 * @param it_cls extra argument to @a it
 * @return the number of key value pairs processed,
 *         #GNUNET_SYSERR if it aborted iteration
 */
int
GNUNET_CONTAINER_multishortmap_get_multiple (
  struct GNUNET_CONTAINER_MultiShortmap *map,
  const struct GNUNET_ShortHashCode *key,
  GNUNET_CONTAINER_ShortmapIterator it,
  void *it_cls);


/**
 * @ingroup hashmap
 * Call @a it on a random value from the map, or not at all
 * if the map is empty.  Note that this function has linear
 * complexity (in the size of the map).
 *
 * @param map the map
 * @param it function to call on a random entry
 * @param it_cls extra argument to @a it
 * @return the number of key value pairs processed, zero or one.
 */
unsigned int
GNUNET_CONTAINER_multishortmap_get_random (
  const struct GNUNET_CONTAINER_MultiShortmap *map,
  GNUNET_CONTAINER_ShortmapIterator it,
  void *it_cls);


/* ***************** Version of Multihashmap for UUIDs ****************** */


/**
 * @ingroup hashmap
 * Iterator over uuid map entries.
 *
 * @param cls closure
 * @param key current public key
 * @param value value in the hash map
 * @return #GNUNET_YES if we should continue to
 *         iterate,
 *         #GNUNET_NO if not.
 */
typedef enum GNUNET_GenericReturnValue
(*GNUNET_CONTAINER_MultiUuidmapIteratorCallback)(
  void *cls,
  const struct GNUNET_Uuid *key,
  void *value);


/**
 * Hash map from peer identities to values.
 */
struct GNUNET_CONTAINER_MultiUuidmap;


/**
 * @ingroup hashmap
 * Create a multi peer map (hash map for public keys of peers).
 *
 * @param len initial size (map will grow as needed)
 * @param do_not_copy_keys #GNUNET_NO is always safe and should be used by default;
 *                         #GNUNET_YES means that on 'put', the 'key' does not have
 *                         to be copied as the destination of the pointer is
 *                         guaranteed to be life as long as the value is stored in
 *                         the hashmap.  This can significantly reduce memory
 *                         consumption, but of course is also a recipe for
 *                         heap corruption if the assumption is not true.  Only
 *                         use this if (1) memory use is important in this case and
 *                         (2) you have triple-checked that the invariant holds
 * @return NULL on error
 */
struct GNUNET_CONTAINER_MultiUuidmap *
GNUNET_CONTAINER_multiuuidmap_create (unsigned int len, int do_not_copy_keys);


/**
 * @ingroup hashmap
 * Destroy a hash map.  Will not free any values
 * stored in the hash map!
 *
 * @param map the map
 */
void
GNUNET_CONTAINER_multiuuidmap_destroy (
  struct GNUNET_CONTAINER_MultiUuidmap *map);


/**
 * @ingroup hashmap
 * Given a key find a value in the map matching the key.
 *
 * @param map the map
 * @param key what to look for
 * @return NULL if no value was found; note that
 *   this is indistinguishable from values that just
 *   happen to be NULL; use "contains" to test for
 *   key-value pairs with value NULL
 */
void *
GNUNET_CONTAINER_multiuuidmap_get (
  const struct GNUNET_CONTAINER_MultiUuidmap *map,
  const struct GNUNET_Uuid *key);


/**
 * @ingroup hashmap
 * Remove the given key-value pair from the map.  Note that if the
 * key-value pair is in the map multiple times, only one of the pairs
 * will be removed.
 *
 * @param map the map
 * @param key key of the key-value pair
 * @param value value of the key-value pair
 * @return #GNUNET_YES on success, #GNUNET_NO if the key-value pair
 *  is not in the map
 */
enum GNUNET_GenericReturnValue
GNUNET_CONTAINER_multiuuidmap_remove (struct GNUNET_CONTAINER_MultiUuidmap *map,
                                      const struct GNUNET_Uuid *key,
                                      const void *value);

/**
 * @ingroup hashmap
 * Remove all entries for the given key from the map.
 * Note that the values would not be "freed".
 *
 * @param map the map
 * @param key identifies values to be removed
 * @return number of values removed
 */
int
GNUNET_CONTAINER_multiuuidmap_remove_all (
  struct GNUNET_CONTAINER_MultiUuidmap *map,
  const struct GNUNET_Uuid *key);


/**
 * @ingroup hashmap
 * Check if the map contains any value under the given
 * key (including values that are NULL).
 *
 * @param map the map
 * @param key the key to test if a value exists for it
 * @return #GNUNET_YES if such a value exists,
 *         #GNUNET_NO if not
 */
enum GNUNET_GenericReturnValue
GNUNET_CONTAINER_multiuuidmap_contains (
  const struct GNUNET_CONTAINER_MultiUuidmap *map,
  const struct GNUNET_Uuid *key);


/**
 * @ingroup hashmap
 * Check if the map contains the given value under the given
 * key.
 *
 * @param map the map
 * @param key the key to test if a value exists for it
 * @param value value to test for
 * @return #GNUNET_YES if such a value exists,
 *         #GNUNET_NO if not
 */
enum GNUNET_GenericReturnValue
GNUNET_CONTAINER_multiuuidmap_contains_value (
  const struct GNUNET_CONTAINER_MultiUuidmap *map,
  const struct GNUNET_Uuid *key,
  const void *value);


/**
 * @ingroup hashmap
 * Store a key-value pair in the map.
 *
 * @param map the map
 * @param key key to use
 * @param value value to use
 * @param opt options for put
 * @return #GNUNET_OK on success,
 *         #GNUNET_NO if a value was replaced (with REPLACE)
 *         #GNUNET_SYSERR if #GNUNET_CONTAINER_MULTIHASHMAPOPTION_UNIQUE_ONLY was the option and the
 *                       value already exists
 */
enum GNUNET_GenericReturnValue
GNUNET_CONTAINER_multiuuidmap_put (
  struct GNUNET_CONTAINER_MultiUuidmap *map,
  const struct GNUNET_Uuid *key,
  void *value,
  enum GNUNET_CONTAINER_MultiHashMapOption opt);


/**
 * @ingroup hashmap
 * Get the number of key-value pairs in the map.
 *
 * @param map the map
 * @return the number of key value pairs
 */
unsigned int
GNUNET_CONTAINER_multiuuidmap_size (
  const struct GNUNET_CONTAINER_MultiUuidmap *map);


/**
 * @ingroup hashmap
 * Iterate over all entries in the map.
 *
 * @param map the map
 * @param it function to call on each entry
 * @param it_cls extra argument to @a it
 * @return the number of key value pairs processed,
 *         #GNUNET_SYSERR if it aborted iteration
 */
enum GNUNET_GenericReturnValue
GNUNET_CONTAINER_multiuuidmap_iterate (
  struct GNUNET_CONTAINER_MultiUuidmap *map,
  GNUNET_CONTAINER_MultiUuidmapIteratorCallback it,
  void *it_cls);


struct GNUNET_CONTAINER_MultiUuidmapIterator;


/**
 * @ingroup hashmap
 * Create an iterator for a multihashmap.
 * The iterator can be used to retrieve all the elements in the multihashmap
 * one by one, without having to handle all elements at once (in contrast to
 * #GNUNET_CONTAINER_multiuuidmap_iterate).  Note that the iterator can not be
 * used anymore if elements have been removed from @a map after the creation of
 * the iterator, or 'map' has been destroyed.  Adding elements to @a map may
 * result in skipped or repeated elements.
 *
 * @param map the map to create an iterator for
 * @return an iterator over the given multihashmap @a map
 */
struct GNUNET_CONTAINER_MultiUuidmapIterator *
GNUNET_CONTAINER_multiuuidmap_iterator_create (
  const struct GNUNET_CONTAINER_MultiUuidmap *map);


/**
 * @ingroup hashmap
 * Retrieve the next element from the hash map at the iterator's
 * position.  If there are no elements left, #GNUNET_NO is returned,
 * and @a key and @a value are not modified.  This operation is only
 * allowed if no elements have been removed from the multihashmap
 * since the creation of @a iter, and the map has not been destroyed.
 * Adding elements may result in repeating or skipping elements.
 *
 * @param iter the iterator to get the next element from
 * @param key pointer to store the key in, can be NULL
 * @param value pointer to store the value in, can be NULL
 * @return #GNUNET_YES we returned an element,
 *         #GNUNET_NO if we are out of elements
 */
enum GNUNET_GenericReturnValue
GNUNET_CONTAINER_multiuuidmap_iterator_next (
  struct GNUNET_CONTAINER_MultiUuidmapIterator *iter,
  struct GNUNET_Uuid *key,
  const void **value);


/**
 * @ingroup hashmap
 * Destroy a multiuuidmap iterator.
 *
 * @param iter the iterator to destroy
 */
void
GNUNET_CONTAINER_multiuuidmap_iterator_destroy (
  struct GNUNET_CONTAINER_MultiUuidmapIterator *iter);


/**
 * @ingroup hashmap
 * Iterate over all entries in the map that match a particular key.
 *
 * @param map the map
 * @param key public key that the entries must correspond to
 * @param it function to call on each entry
 * @param it_cls extra argument to @a it
 * @return the number of key value pairs processed,
 *         #GNUNET_SYSERR if it aborted iteration
 */
int
GNUNET_CONTAINER_multiuuidmap_get_multiple (
  struct GNUNET_CONTAINER_MultiUuidmap *map,
  const struct GNUNET_Uuid *key,
  GNUNET_CONTAINER_MultiUuidmapIteratorCallback it,
  void *it_cls);


/**
 * @ingroup hashmap
 * Call @a it on a random value from the map, or not at all
 * if the map is empty.  Note that this function has linear
 * complexity (in the size of the map).
 *
 * @param map the map
 * @param it function to call on a random entry
 * @param it_cls extra argument to @a it
 * @return the number of key value pairs processed, zero or one.
 */
unsigned int
GNUNET_CONTAINER_multiuuidmap_get_random (
  const struct GNUNET_CONTAINER_MultiUuidmap *map,
  GNUNET_CONTAINER_MultiUuidmapIteratorCallback it,
  void *it_cls);


/* Version of multihashmap with 32 bit keys */

/**
 * @ingroup hashmap
 * Opaque handle for the 32-bit key HashMap.
 */
struct GNUNET_CONTAINER_MultiHashMap32;


/**
 * @ingroup hashmap
 * Opaque handle to an iterator over
 * a 32-bit key multihashmap.
 */
struct GNUNET_CONTAINER_MultiHashMap32Iterator;


/**
 * @ingroup hashmap
 * Iterator over hash map entries.
 *
 * @param cls closure
 * @param key current key code
 * @param value value in the hash map
 * @return #GNUNET_YES if we should continue to
 *         iterate,
 *         #GNUNET_NO if not.
 */
typedef enum GNUNET_GenericReturnValue
(*GNUNET_CONTAINER_MultiHashMapIterator32Callback)(
  void *cls,
  uint32_t key,
  void *value);


/**
 * @ingroup hashmap
 * Create a 32-bit key multi hash map.
 *
 * @param len initial size (map will grow as needed)
 * @return NULL on error
 */
struct GNUNET_CONTAINER_MultiHashMap32 *
GNUNET_CONTAINER_multihashmap32_create (unsigned int len);


/**
 * @ingroup hashmap
 * Destroy a 32-bit key hash map.  Will not free any values
 * stored in the hash map!
 *
 * @param map the map
 */
void
GNUNET_CONTAINER_multihashmap32_destroy (
  struct GNUNET_CONTAINER_MultiHashMap32 *map);


/**
 * @ingroup hashmap
 * Get the number of key-value pairs in the map.
 *
 * @param map the map
 * @return the number of key value pairs
 */
unsigned int
GNUNET_CONTAINER_multihashmap32_size (
  const struct GNUNET_CONTAINER_MultiHashMap32 *map);


/**
 * @ingroup hashmap
 * Given a key find a value in the map matching the key.
 *
 * @param map the map
 * @param key what to look for
 * @return NULL if no value was found; note that
 *   this is indistinguishable from values that just
 *   happen to be NULL; use "contains" to test for
 *   key-value pairs with value NULL
 */
void *
GNUNET_CONTAINER_multihashmap32_get (
  const struct GNUNET_CONTAINER_MultiHashMap32 *map,
  uint32_t key);


/**
 * @ingroup hashmap
 * Iterate over all entries in the map.
 *
 * @param map the map
 * @param it function to call on each entry
 * @param it_cls extra argument to @a it
 * @return the number of key value pairs processed,
 *         #GNUNET_SYSERR if it aborted iteration
 */
int
GNUNET_CONTAINER_multihashmap32_iterate (
  struct GNUNET_CONTAINER_MultiHashMap32 *map,
  GNUNET_CONTAINER_MultiHashMapIterator32Callback it,
  void *it_cls);


/**
 * @ingroup hashmap
 * Remove the given key-value pair from the map.  Note that if the
 * key-value pair is in the map multiple times, only one of the pairs
 * will be removed.
 *
 * @param map the map
 * @param key key of the key-value pair
 * @param value value of the key-value pair
 * @return #GNUNET_YES on success, #GNUNET_NO if the key-value pair
 *  is not in the map
 */
enum GNUNET_GenericReturnValue
GNUNET_CONTAINER_multihashmap32_remove (
  struct GNUNET_CONTAINER_MultiHashMap32 *map,
  uint32_t key,
  const void *value);


/**
 * @ingroup hashmap
 * Remove all entries for the given key from the map.
 * Note that the values would not be "freed".
 *
 * @param map the map
 * @param key identifies values to be removed
 * @return number of values removed
 */
int
GNUNET_CONTAINER_multihashmap32_remove_all (
  struct GNUNET_CONTAINER_MultiHashMap32 *map,
  uint32_t key);


/**
 * @ingroup hashmap
 * Check if the map contains any value under the given
 * key (including values that are NULL).
 *
 * @param map the map
 * @param key the key to test if a value exists for it
 * @return #GNUNET_YES if such a value exists,
 *         #GNUNET_NO if not
 */
enum GNUNET_GenericReturnValue
GNUNET_CONTAINER_multihashmap32_contains (
  const struct GNUNET_CONTAINER_MultiHashMap32 *map,
  uint32_t key);


/**
 * @ingroup hashmap
 * Check if the map contains the given value under the given
 * key.
 *
 * @param map the map
 * @param key the key to test if a value exists for it
 * @param value value to test for
 * @return #GNUNET_YES if such a value exists,
 *         #GNUNET_NO if not
 */
enum GNUNET_GenericReturnValue
GNUNET_CONTAINER_multihashmap32_contains_value (
  const struct GNUNET_CONTAINER_MultiHashMap32 *map,
  uint32_t key,
  const void *value);


/**
 * @ingroup hashmap
 * Store a key-value pair in the map.
 *
 * @param map the map
 * @param key key to use
 * @param value value to use
 * @param opt options for put
 * @return #GNUNET_OK on success,
 *         #GNUNET_NO if a value was replaced (with REPLACE)
 *         #GNUNET_SYSERR if #GNUNET_CONTAINER_MULTIHASHMAPOPTION_UNIQUE_ONLY was the option and the
 *                       value already exists
 */
enum GNUNET_GenericReturnValue
GNUNET_CONTAINER_multihashmap32_put (
  struct GNUNET_CONTAINER_MultiHashMap32 *map,
  uint32_t key,
  void *value,
  enum GNUNET_CONTAINER_MultiHashMapOption opt);


/**
 * @ingroup hashmap
 * Iterate over all entries in the map that match a particular key.
 *
 * @param map the map
 * @param key key that the entries must correspond to
 * @param it function to call on each entry
 * @param it_cls extra argument to @a it
 * @return the number of key value pairs processed,
 *         #GNUNET_SYSERR if it aborted iteration
 */
int
GNUNET_CONTAINER_multihashmap32_get_multiple (
  struct GNUNET_CONTAINER_MultiHashMap32 *map,
  uint32_t key,
  GNUNET_CONTAINER_MultiHashMapIterator32Callback it,
  void *it_cls);


/**
 * Create an iterator for a 32-bit multihashmap.
 * The iterator can be used to retrieve all the elements in the multihashmap
 * one by one, without having to handle all elements at once (in contrast to
 * #GNUNET_CONTAINER_multihashmap32_iterate).  Note that the iterator can not be
 * used anymore if elements have been removed from 'map' after the creation of
 * the iterator, or 'map' has been destroyed.  Adding elements to 'map' may
 * result in skipped or repeated elements.
 *
 * @param map the map to create an iterator for
 * @return an iterator over the given multihashmap map
 */
struct GNUNET_CONTAINER_MultiHashMap32Iterator *
GNUNET_CONTAINER_multihashmap32_iterator_create (
  const struct GNUNET_CONTAINER_MultiHashMap32 *map);


/**
 * Retrieve the next element from the hash map at the iterator's position.
 * If there are no elements left, GNUNET_NO is returned, and 'key' and 'value'
 * are not modified.
 * This operation is only allowed if no elements have been removed from the
 * multihashmap since the creation of 'iter', and the map has not been destroyed.
 * Adding elements may result in repeating or skipping elements.
 *
 * @param iter the iterator to get the next element from
 * @param key pointer to store the key in, can be NULL
 * @param value pointer to store the value in, can be NULL
 * @return #GNUNET_YES we returned an element,
 *         #GNUNET_NO if we are out of elements
 */
enum GNUNET_GenericReturnValue
GNUNET_CONTAINER_multihashmap32_iterator_next (
  struct GNUNET_CONTAINER_MultiHashMap32Iterator *iter,
  uint32_t *key,
  const void **value);


/**
 * Destroy a 32-bit multihashmap iterator.
 *
 * @param iter the iterator to destroy
 */
void
GNUNET_CONTAINER_multihashmap32_iterator_destroy (
  struct GNUNET_CONTAINER_MultiHashMapIterator *iter);


/* ******************** doubly-linked list *************** */
/* To avoid mistakes: head->prev == tail->next == NULL     */

/**
 * @ingroup dll
 * Insert an element at the head of a DLL. Assumes that head, tail and
 * element are structs with prev and next fields.
 *
 * @param head pointer to the head of the DLL
 * @param tail pointer to the tail of the DLL
 * @param element element to insert
 */
#define GNUNET_CONTAINER_DLL_insert(head, tail, element)                \
  do                                                                    \
  {                                                                     \
    GNUNET_assert ((NULL == (head)) == (NULL == (tail)));               \
    GNUNET_assert ((NULL == (element)->prev) && ((head) != (element))); \
    GNUNET_assert ((NULL == (element)->next) && ((tail) != (element))); \
    (element)->next = (head);                                           \
    (element)->prev = NULL;                                             \
    if (NULL == (tail))                                                 \
      (tail) = element;                                                 \
    else                                                                \
      (head)->prev = element;                                           \
    (head) = (element);                                                 \
  } while (0)


/**
 * @ingroup dll
 * Insert an element at the tail of a DLL. Assumes that head, tail and
 * element are structs with prev and next fields.
 *
 * @param head pointer to the head of the DLL
 * @param tail pointer to the tail of the DLL
 * @param element element to insert
 */
#define GNUNET_CONTAINER_DLL_insert_tail(head, tail, element)           \
  do                                                                    \
  {                                                                     \
    GNUNET_assert ((NULL == (head)) == (NULL == (tail)));               \
    GNUNET_assert ((NULL == (element)->prev) && ((head) != (element))); \
    GNUNET_assert ((NULL == (element)->next) && ((tail) != (element))); \
    (element)->prev = (tail);                                           \
    (element)->next = NULL;                                             \
    if (NULL == (head))                                                 \
      (head) = element;                                                 \
    else                                                                \
      (tail)->next = element;                                           \
    (tail) = (element);                                                 \
  } while (0)


/**
 * @ingroup dll
 * Insert an element into a DLL after the given other element.  Insert
 * at the head if the other element is NULL.
 *
 * @param head pointer to the head of the DLL
 * @param tail pointer to the tail of the DLL
 * @param other prior element, NULL for insertion at head of DLL
 * @param element element to insert
 */
#define GNUNET_CONTAINER_DLL_insert_after(head, tail, other, element)   \
  do                                                                    \
  {                                                                     \
    GNUNET_assert ((NULL == (head)) == (NULL == (tail)));               \
    GNUNET_assert ((NULL == (element)->prev) && ((head) != (element))); \
    GNUNET_assert ((NULL == (element)->next) && ((tail) != (element))); \
    (element)->prev = (other);                                          \
    if (NULL == other)                                                  \
    {                                                                   \
      (element)->next = (head);                                         \
      (head) = (element);                                               \
    }                                                                   \
    else                                                                \
    {                                                                   \
      (element)->next = (other)->next;                                  \
      (other)->next = (element);                                        \
    }                                                                   \
    if (NULL == (element)->next)                                        \
      (tail) = (element);                                               \
    else                                                                \
      (element)->next->prev = (element);                                \
  } while (0)


/**
 * @ingroup dll
 * Insert an element into a DLL before the given other element.  Insert
 * at the tail if the other element is NULL.
 *
 * @param head pointer to the head of the DLL
 * @param tail pointer to the tail of the DLL
 * @param other prior element, NULL for insertion at head of DLL
 * @param element element to insert
 */
#define GNUNET_CONTAINER_DLL_insert_before(head, tail, other, element)  \
  do                                                                    \
  {                                                                     \
    GNUNET_assert ((NULL == (head)) == (NULL == (tail)));               \
    GNUNET_assert ((NULL == (element)->prev) && ((head) != (element))); \
    GNUNET_assert ((NULL == (element)->next) && ((tail) != (element))); \
    (element)->next = (other);                                          \
    if (NULL == other)                                                  \
    {                                                                   \
      (element)->prev = (tail);                                         \
      (tail) = (element);                                               \
    }                                                                   \
    else                                                                \
    {                                                                   \
      (element)->prev = (other)->prev;                                  \
      (other)->prev = (element);                                        \
    }                                                                   \
    if (NULL == (element)->prev)                                        \
      (head) = (element);                                               \
    else                                                                \
      (element)->prev->next = (element);                                \
  } while (0)


/**
 * @ingroup dll
 * Remove an element from a DLL. Assumes that head, tail and
 * element point to structs with prev and next fields.
 *
 * Using the head or tail pointer as the element
 * argument does NOT work with this macro.
 * Make sure to store head/tail in another pointer
 * and use it to remove the head or tail of the list.
 *
 * @param head pointer to the head of the DLL
 * @param tail pointer to the tail of the DLL
 * @param element element to remove
 */
#define GNUNET_CONTAINER_DLL_remove(head, tail, element)                \
  do                                                                    \
  {                                                                     \
    GNUNET_assert ((NULL != (element)->prev) || ((head) == (element))); \
    GNUNET_assert ((NULL != (element)->next) || ((tail) == (element))); \
    if (NULL == (element)->prev)                                        \
      (head) = (element)->next;                                         \
    else                                                                \
      (element)->prev->next = (element)->next;                          \
    if (NULL == (element)->next)                                        \
      (tail) = (element)->prev;                                         \
    else                                                                \
      (element)->next->prev = (element)->prev;                          \
    (element)->next = NULL;                                             \
    (element)->prev = NULL;                                             \
  } while (0)


/* ************ Multi-DLL interface, allows DLL elements to be
   in multiple lists at the same time *********************** */

/**
 * @ingroup dll
 * Insert an element at the head of a MDLL. Assumes that head, tail and
 * element are structs with prev and next fields.
 *
 * @param mdll suffix name for the next and prev pointers in the element
 * @param head pointer to the head of the MDLL
 * @param tail pointer to the tail of the MDLL
 * @param element element to insert
 */
#define GNUNET_CONTAINER_MDLL_insert(mdll, head, tail, element)                  \
  do                                                                             \
  {                                                                              \
    GNUNET_assert ((NULL == (head)) == (NULL == (tail)));                        \
    GNUNET_assert ((NULL == (element)->prev_ ## mdll) && ((head) != (element))); \
    GNUNET_assert ((NULL == (element)->next_ ## mdll) && ((tail) != (element))); \
    (element)->next_ ## mdll = (head);                                           \
    (element)->prev_ ## mdll = NULL;                                             \
    if (NULL == (tail))                                                          \
      (tail) = element;                                                          \
    else                                                                         \
      (head)->prev_ ## mdll = element;                                           \
    (head) = (element);                                                          \
  } while (0)


/**
 * @ingroup dll
 * Insert an element at the tail of a MDLL. Assumes that head, tail and
 * element are structs with prev and next fields.
 *
 * @param mdll suffix name for the next and prev pointers in the element
 * @param head pointer to the head of the MDLL
 * @param tail pointer to the tail of the MDLL
 * @param element element to insert
 */
#define GNUNET_CONTAINER_MDLL_insert_tail(mdll, head, tail, element)             \
  do                                                                             \
  {                                                                              \
    GNUNET_assert ((NULL == (head)) == (NULL == (tail)));                        \
    GNUNET_assert ((NULL == (element)->prev_ ## mdll) && ((head) != (element))); \
    GNUNET_assert ((NULL == (element)->next_ ## mdll) && ((tail) != (element))); \
    (element)->prev_ ## mdll = (tail);                                           \
    (element)->next_ ## mdll = NULL;                                             \
    if (NULL == (head))                                                          \
      (head) = element;                                                          \
    else                                                                         \
      (tail)->next_ ## mdll = element;                                           \
    (tail) = (element);                                                          \
  } while (0)


/**
 * @ingroup dll
 * Insert an element into a MDLL after the given other element.  Insert
 * at the head if the other element is NULL.
 *
 * @param mdll suffix name for the next and prev pointers in the element
 * @param head pointer to the head of the MDLL
 * @param tail pointer to the tail of the MDLL
 * @param other prior element, NULL for insertion at head of MDLL
 * @param element element to insert
 */
#define GNUNET_CONTAINER_MDLL_insert_after(mdll, head, tail, other, element)     \
  do                                                                             \
  {                                                                              \
    GNUNET_assert ((NULL == (head)) == (NULL == (tail)));                        \
    GNUNET_assert ((NULL == (element)->prev_ ## mdll) && ((head) != (element))); \
    GNUNET_assert ((NULL == (element)->next_ ## mdll) && ((tail) != (element))); \
    (element)->prev_ ## mdll = (other);                                          \
    if (NULL == other)                                                           \
    {                                                                            \
      (element)->next_ ## mdll = (head);                                         \
      (head) = (element);                                                        \
    }                                                                            \
    else                                                                         \
    {                                                                            \
      (element)->next_ ## mdll = (other)->next_ ## mdll;                         \
      (other)->next_ ## mdll = (element);                                        \
    }                                                                            \
    if (NULL == (element)->next_ ## mdll)                                        \
      (tail) = (element);                                                        \
    else                                                                         \
      (element)->next_ ## mdll->prev_ ## mdll = (element);                       \
  } while (0)


/**
 * @ingroup dll
 * Insert an element into a MDLL before the given other element.  Insert
 * at the tail if the other element is NULL.
 *
 * @param mdll suffix name for the next and prev pointers in the element
 * @param head pointer to the head of the MDLL
 * @param tail pointer to the tail of the MDLL
 * @param other prior element, NULL for insertion at head of MDLL
 * @param element element to insert
 */
#define GNUNET_CONTAINER_MDLL_insert_before(mdll, head, tail, other, element)    \
  do                                                                             \
  {                                                                              \
    GNUNET_assert ((NULL == (head)) == (NULL == (tail)));                        \
    GNUNET_assert (((element)->prev_ ## mdll == NULL) && ((head) != (element))); \
    GNUNET_assert (((element)->next_ ## mdll == NULL) && ((tail) != (element))); \
    (element)->next_ ## mdll = (other);                                          \
    if (NULL == other)                                                           \
    {                                                                            \
      (element)->prev = (tail);                                                  \
      (tail) = (element);                                                        \
    }                                                                            \
    else                                                                         \
    {                                                                            \
      (element)->prev_ ## mdll = (other)->prev_ ## mdll;                         \
      (other)->prev_ ## mdll = (element);                                        \
    }                                                                            \
    if (NULL == (element)->prev_ ## mdll)                                        \
      (head) = (element);                                                        \
    else                                                                         \
      (element)->prev_ ## mdll->next_ ## mdll = (element);                       \
  } while (0)


/**
 * @ingroup dll
 * Remove an element from a MDLL. Assumes
 * that head, tail and element are structs
 * with prev and next fields.
 *
 * @param mdll suffix name for the next and prev pointers in the element
 * @param head pointer to the head of the MDLL
 * @param tail pointer to the tail of the MDLL
 * @param element element to remove
 */
#define GNUNET_CONTAINER_MDLL_remove(mdll, head, tail, element)                  \
  do                                                                             \
  {                                                                              \
    GNUNET_assert ((NULL != (element)->prev_ ## mdll) || ((head) == (element))); \
    GNUNET_assert ((NULL != (element)->next_ ## mdll) || ((tail) == (element))); \
    if (NULL == (element)->prev_ ## mdll)                                        \
      (head) = (element)->next_ ## mdll;                                         \
    else                                                                         \
      (element)->prev_ ## mdll->next_ ## mdll = (element)->next_ ## mdll;        \
    if (NULL == (element)->next_ ## mdll)                                        \
      (tail) = (element)->prev_ ## mdll;                                         \
    else                                                                         \
      (element)->next_ ## mdll->prev_ ## mdll = (element)->prev_ ## mdll;        \
    (element)->next_ ## mdll = NULL;                                             \
    (element)->prev_ ## mdll = NULL;                                             \
  } while (0)


/**
 * Insertion sort of @a element into DLL from @a head to @a tail
 * sorted by @a comparator.
 *
 * @param TYPE element type of the elements, e.g. `struct ListElement`
 * @param comparator function like memcmp() to compare elements; takes
 *                   three arguments, the @a comparator_cls and two elements,
 *                   returns an `int` (-1, 0 or 1)
 * @param comparator_cls closure for @a comparator
 * @param[in,out] head head of DLL
 * @param[in,out] tail tail of DLL
 * @param element element to insert
 */
#define GNUNET_CONTAINER_DLL_insert_sorted(TYPE,                            \
                                           comparator,                      \
                                           comparator_cls,                  \
                                           head,                            \
                                           tail,                            \
                                           element)                         \
  do                                                                        \
  {                                                                         \
    if ((NULL == head) || (0 < comparator (comparator_cls, element, head))) \
    {                                                                       \
      /* insert at head, element < head */                                  \
      GNUNET_CONTAINER_DLL_insert (head, tail, element);                    \
    }                                                                       \
    else                                                                    \
    {                                                                       \
      TYPE *pos;                                                            \
                                                                            \
      for (pos = head; NULL != pos; pos = pos->next)                        \
        if (0 < comparator (comparator_cls, element, pos))                  \
          break;     /* element < pos */                                    \
      if (NULL == pos)     /* => element > tail */                          \
      {                                                                     \
        GNUNET_CONTAINER_DLL_insert_tail (head, tail, element);             \
      }                                                                     \
      else     /* prev < element < pos */                                   \
      {                                                                     \
        GNUNET_CONTAINER_DLL_insert_after (head, tail, pos->prev, element); \
      }                                                                     \
    }                                                                       \
  } while (0)


/* ******************** Heap *************** */


/**
 * @ingroup heap
 * Cost by which elements in a heap can be ordered.
 */
typedef uint64_t GNUNET_CONTAINER_HeapCostType;


/**
 * @ingroup heap
 * Heap type, either max or min.
 */
enum GNUNET_CONTAINER_HeapOrder
{
  /**
   * @ingroup heap
   * Heap with the maximum cost at the root.
   */
  GNUNET_CONTAINER_HEAP_ORDER_MAX,

  /**
   * @ingroup heap
   * Heap with the minimum cost at the root.
   */
  GNUNET_CONTAINER_HEAP_ORDER_MIN
};


/**
 * @ingroup heap
 * Handle to a Heap.
 */
struct GNUNET_CONTAINER_Heap;


/**
 * @ingroup heap
 * Handle to a node in a heap.
 */
struct GNUNET_CONTAINER_HeapNode;


/**
 * @ingroup heap
 * Create a new heap.
 *
 * @param order how should the heap be sorted?
 * @return handle to the heap
 */
struct GNUNET_CONTAINER_Heap *
GNUNET_CONTAINER_heap_create (enum GNUNET_CONTAINER_HeapOrder order);


/**
 * @ingroup heap
 * Destroys the heap.  Only call on a heap that
 * is already empty.
 *
 * @param heap heap to destroy
 */
void
GNUNET_CONTAINER_heap_destroy (struct GNUNET_CONTAINER_Heap *heap);


/**
 * @ingroup heap
 * Get element stored at the root of @a heap.
 *
 * @param heap  Heap to inspect.
 * @return Element at the root, or NULL if heap is empty.
 */
void *
GNUNET_CONTAINER_heap_peek (const struct GNUNET_CONTAINER_Heap *heap);


/**
 * Get @a element and @a cost stored at the root of @a heap.
 *
 * @param[in]  heap     Heap to inspect.
 * @param[out] element  Root element is returned here.
 * @param[out] cost     Cost of @a element is returned here.
 * @return #GNUNET_YES if an element is returned,
 *         #GNUNET_NO  if the heap is empty.
 */
enum GNUNET_GenericReturnValue
GNUNET_CONTAINER_heap_peek2 (const struct GNUNET_CONTAINER_Heap *heap,
                             void **element,
                             GNUNET_CONTAINER_HeapCostType *cost);


/**
 * @ingroup heap
 * Get the current size of the heap
 *
 * @param heap the heap to get the size of
 * @return number of elements stored
 */
unsigned int
GNUNET_CONTAINER_heap_get_size (const struct GNUNET_CONTAINER_Heap *heap);


/**
 * @ingroup heap
 * Get the current cost of the node
 *
 * @param node the node to get the cost of
 * @return cost of the node
 */
GNUNET_CONTAINER_HeapCostType
GNUNET_CONTAINER_heap_node_get_cost (
  const struct GNUNET_CONTAINER_HeapNode *node);


/**
 * @ingroup heap
 * Iterator for heap
 *
 * @param cls closure
 * @param node internal node of the heap
 * @param element value stored at the node
 * @param cost cost associated with the node
 * @return #GNUNET_YES if we should continue to iterate,
 *         #GNUNET_NO if not.
 */
typedef enum GNUNET_GenericReturnValue
(*GNUNET_CONTAINER_HeapIterator)(
  void *cls,
  struct GNUNET_CONTAINER_HeapNode *node,
  void *element,
  GNUNET_CONTAINER_HeapCostType cost);


/**
 * @ingroup heap
 * Iterate over all entries in the heap.
 *
 * @param heap the heap
 * @param iterator function to call on each entry
 * @param iterator_cls closure for @a iterator
 */
void
GNUNET_CONTAINER_heap_iterate (const struct GNUNET_CONTAINER_Heap *heap,
                               GNUNET_CONTAINER_HeapIterator iterator,
                               void *iterator_cls);

/**
 * @ingroup heap
 * Perform a random walk of the tree.  The walk is biased
 * towards elements closer to the root of the tree (since
 * each walk starts at the root and ends at a random leaf).
 * The heap internally tracks the current position of the
 * walk.
 *
 * @param heap heap to walk
 * @return data stored at the next random node in the walk;
 *         NULL if the tree is empty.
 */
void *
GNUNET_CONTAINER_heap_walk_get_next (struct GNUNET_CONTAINER_Heap *heap);


/**
 * @ingroup heap
 * Inserts a new element into the heap.
 *
 * @param heap heap to modify
 * @param element element to insert
 * @param cost cost for the element
 * @return node for the new element
 */
struct GNUNET_CONTAINER_HeapNode *
GNUNET_CONTAINER_heap_insert (struct GNUNET_CONTAINER_Heap *heap,
                              void *element,
                              GNUNET_CONTAINER_HeapCostType cost);


/**
 * @ingroup heap
 * Remove root of the heap.
 *
 * @param heap heap to modify
 * @return element data stored at the root node
 */
void *
GNUNET_CONTAINER_heap_remove_root (struct GNUNET_CONTAINER_Heap *heap);


/**
 * @ingroup heap
 * Removes a node from the heap.
 *
 * @param node node to remove
 * @return element data stored at the node, NULL if heap is empty
 */
void *
GNUNET_CONTAINER_heap_remove_node (struct GNUNET_CONTAINER_HeapNode *node);


/**
 * @ingroup heap
 * Updates the cost of any node in the tree
 *
 * @param node node for which the cost is to be changed
 * @param new_cost new cost for the node
 */
void
GNUNET_CONTAINER_heap_update_cost (struct GNUNET_CONTAINER_HeapNode *node,
                                   GNUNET_CONTAINER_HeapCostType new_cost);


#if 0 /* keep Emacsens' auto-indent happy */
{
#endif
#ifdef __cplusplus
}
#endif


/* ifndef GNUNET_CONTAINER_LIB_H */
#endif

/** @} */ /* end of group addition */

/* end of gnunet_container_lib.h */
