/*
     This file is part of GNUnet.
     Copyright (C) 2008 GNUnet e.V.

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
 * @file util/container_multihashmap32.c
 * @brief a version of hash map implemented in container_multihashmap.c but with
 *          uint32_t as keys
 * @author Christian Grothoff
 * @author Sree Harsha Totakura
 */


#include "platform.h"
#include "gnunet_util_lib.h"

#define LOG(kind, ...) \
  GNUNET_log_from (kind, "util-container-multihashmap32", __VA_ARGS__)


/**
 * Maximum recursion depth for callbacks of
 * #GNUNET_CONTAINER_multihashmap_get_multiple() themselves
 * again calling #GNUNET_CONTAINER_multihashmap_get_multiple().
 * Should be totally excessive, but if violated we die.
 */
#define NEXT_CACHE_SIZE 16

/**
 * An entry in the hash map.
 */
struct MapEntry
{
  /**
   * Key for the entry.
   */
  uint32_t key;

  /**
   * Value of the entry.
   */
  void *value;

  /**
   * If there is a hash collision, we create a linked list.
   */
  struct MapEntry *next;
};

/**
 * Internal representation of the hash map.
 */
struct GNUNET_CONTAINER_MultiHashMap32
{
  /**
   * All of our buckets.
   */
  struct MapEntry **map;

  /**
   * Number of entries in the map.
   */
  unsigned int size;

  /**
   * Length of the @e map array.
   */
  unsigned int map_length;

  /**
   * Counts the destructive modifications (grow, remove)
   * to the map, so that iterators can check if they are still valid.
   */
  unsigned int modification_counter;

  /**
   * Map entries indicating iteration positions currently
   * in use by #GNUNET_CONTAINER_multihashmap_get_multiple().
   * Only used up to @e next_cache_off.
   */
  struct MapEntry *next_cache[NEXT_CACHE_SIZE];

  /**
   * Offset of @e next_cache entries in use, must be smaller
   * than #NEXT_CACHE_SIZE.
   */
  unsigned int next_cache_off;
};


/**
 * Cursor into a multihashmap.
 * Allows to enumerate elements asynchronously.
 */
struct GNUNET_CONTAINER_MultiHashMap32Iterator
{
  /**
   * Position in the bucket @e idx
   */
  struct MapEntry *me;

  /**
   * Current bucket index.
   */
  unsigned int idx;

  /**
   * Modification counter as observed on the map when the iterator
   * was created.
   */
  unsigned int modification_counter;

  /**
   * Map that we are iterating over.
   */
  const struct GNUNET_CONTAINER_MultiHashMap32 *map;
};


/**
 * Create a multi hash map.
 *
 * @param len initial size (map will grow as needed)
 * @return NULL on error
 */
struct GNUNET_CONTAINER_MultiHashMap32 *
GNUNET_CONTAINER_multihashmap32_create (unsigned int len)
{
  struct GNUNET_CONTAINER_MultiHashMap32 *ret;

  GNUNET_assert (len > 0);
  ret = GNUNET_new (struct GNUNET_CONTAINER_MultiHashMap32);
  ret->map = GNUNET_malloc_large (len * sizeof(struct MapEntry *));
  if (NULL == ret->map)
  {
    GNUNET_free (ret);
    return NULL;
  }
  ret->map_length = len;
  return ret;
}


/**
 * Destroy a hash map.  Will not free any values
 * stored in the hash map!
 *
 * @param map the map
 */
void
GNUNET_CONTAINER_multihashmap32_destroy (
  struct GNUNET_CONTAINER_MultiHashMap32 *map)
{
  struct MapEntry *e;

  for (unsigned int i = 0; i < map->map_length; i++)
  {
    while (NULL != (e = map->map[i]))
    {
      map->map[i] = e->next;
      GNUNET_free (e);
    }
  }
  GNUNET_free (map->map);
  GNUNET_free (map);
}


/**
 * Compute the index of the bucket for the given key.
 *
 * @param m hash map for which to compute the index
 * @param key what key should the index be computed for
 * @return offset into the "map" array of "m"
 */
static unsigned int
idx_of (const struct GNUNET_CONTAINER_MultiHashMap32 *m, const uint32_t key)
{
  GNUNET_assert (NULL != m);
  return ((unsigned int) key) % m->map_length;
}


unsigned int
GNUNET_CONTAINER_multihashmap32_size (
  const struct GNUNET_CONTAINER_MultiHashMap32 *map)
{
  return map->size;
}


void *
GNUNET_CONTAINER_multihashmap32_get (
  const struct GNUNET_CONTAINER_MultiHashMap32 *map,
  uint32_t key)
{
  struct MapEntry *e;

  e = map->map[idx_of (map, key)];
  while (NULL != e)
  {
    if (key == e->key)
      return e->value;
    e = e->next;
  }
  return NULL;
}


int
GNUNET_CONTAINER_multihashmap32_iterate (
  struct GNUNET_CONTAINER_MultiHashMap32 *map,
  GNUNET_CONTAINER_MultiHashMapIterator32Callback it,
  void *it_cls)
{
  int count;
  struct MapEntry **ce;

  count = 0;
  GNUNET_assert (NULL != map);
  ce = &map->next_cache[map->next_cache_off];
  GNUNET_assert (++map->next_cache_off < NEXT_CACHE_SIZE);
  for (unsigned int i = 0; i < map->map_length; i++)
  {
    struct MapEntry *e;

    *ce = map->map[i];
    while (NULL != (e = *ce))
    {
      *ce = e->next;
      if (NULL != it)
      {
        if (GNUNET_OK != it (it_cls, e->key, e->value))
        {
          GNUNET_assert (--map->next_cache_off < NEXT_CACHE_SIZE);
          return GNUNET_SYSERR;
        }
      }
      count++;
    }
  }
  GNUNET_assert (--map->next_cache_off < NEXT_CACHE_SIZE);
  return count;
}


/**
 * We are about to free() the @a me, make sure it is not in
 * the list of next values for any iterator in the @a map's next_cache.
 *
 * @param map the map to check
 * @param me the entry that is about to be free'd
 */
static void
update_next_cache (struct GNUNET_CONTAINER_MultiHashMap32 *map,
                   const struct MapEntry *me)
{
  for (unsigned int i = 0; i < map->next_cache_off; i++)
    if (map->next_cache[i] == me)
      map->next_cache[i] = me->next;
}


enum GNUNET_GenericReturnValue
GNUNET_CONTAINER_multihashmap32_remove (
  struct GNUNET_CONTAINER_MultiHashMap32 *map,
  uint32_t key,
  const void *value)
{
  struct MapEntry *e;
  struct MapEntry *p;
  unsigned int i;

  map->modification_counter++;

  i = idx_of (map, key);
  p = NULL;
  e = map->map[i];
  while (e != NULL)
  {
    if ((key == e->key) && (value == e->value))
    {
      if (p == NULL)
        map->map[i] = e->next;
      else
        p->next = e->next;
      update_next_cache (map, e);
      GNUNET_free (e);
      map->size--;
      return GNUNET_YES;
    }
    p = e;
    e = e->next;
  }
  return GNUNET_NO;
}


int
GNUNET_CONTAINER_multihashmap32_remove_all (
  struct GNUNET_CONTAINER_MultiHashMap32 *map,
  uint32_t key)
{
  struct MapEntry *e;
  struct MapEntry *p;
  unsigned int i;
  int ret;

  map->modification_counter++;

  ret = 0;
  i = idx_of (map, key);
  p = NULL;
  e = map->map[i];
  while (e != NULL)
  {
    if (key == e->key)
    {
      if (p == NULL)
        map->map[i] = e->next;
      else
        p->next = e->next;
      update_next_cache (map, e);
      GNUNET_free (e);
      map->size--;
      if (p == NULL)
        e = map->map[i];
      else
        e = p->next;
      ret++;
    }
    else
    {
      p = e;
      e = e->next;
    }
  }
  return ret;
}


enum GNUNET_GenericReturnValue
GNUNET_CONTAINER_multihashmap32_contains (
  const struct GNUNET_CONTAINER_MultiHashMap32 *map,
  uint32_t key)
{
  struct MapEntry *e;

  e = map->map[idx_of (map, key)];
  while (e != NULL)
  {
    if (key == e->key)
      return GNUNET_YES;
    e = e->next;
  }
  return GNUNET_NO;
}


enum GNUNET_GenericReturnValue
GNUNET_CONTAINER_multihashmap32_contains_value (
  const struct GNUNET_CONTAINER_MultiHashMap32 *map,
  uint32_t key,
  const void *value)
{
  struct MapEntry *e;

  e = map->map[idx_of (map, key)];
  while (e != NULL)
  {
    if ((key == e->key) && (e->value == value))
      return GNUNET_YES;
    e = e->next;
  }
  return GNUNET_NO;
}


/**
 * Grow the given map to a more appropriate size.
 *
 * @param map the hash map to grow
 */
static void
grow (struct GNUNET_CONTAINER_MultiHashMap32 *map)
{
  struct MapEntry **old_map;
  struct MapEntry **new_map;
  struct MapEntry *e;
  unsigned int old_len;
  unsigned int new_len;
  unsigned int idx;

  old_map = map->map;
  old_len = map->map_length;
  new_len = old_len * 2;
  if (0 == new_len) /* 2^31 * 2 == 0 */
    new_len = old_len; /* never use 0 */
  if (new_len == old_len)
    return; /* nothing changed */
  new_map = GNUNET_malloc_large (new_len * sizeof(struct MapEntry *));
  if (NULL == new_map)
    return; /* grow not possible */
  map->modification_counter++;
  map->map_length = new_len;
  map->map = new_map;
  for (unsigned int i = 0; i < old_len; i++)
  {
    while (NULL != (e = old_map[i]))
    {
      old_map[i] = e->next;
      idx = idx_of (map, e->key);
      e->next = new_map[idx];
      new_map[idx] = e;
    }
  }
  GNUNET_free (old_map);
}


/**
 * Store a key-value pair in the map.
 *
 * @param map the map
 * @param key key to use
 * @param value value to use
 * @param opt options for put
 * @return #GNUNET_OK on success,
 *         #GNUNET_NO if a value was replaced (with REPLACE)
 *         #GNUNET_SYSERR if UNIQUE_ONLY was the option and the
 *                       value already exists
 */
enum GNUNET_GenericReturnValue
GNUNET_CONTAINER_multihashmap32_put (
  struct GNUNET_CONTAINER_MultiHashMap32 *map,
  uint32_t key,
  void *value,
  enum GNUNET_CONTAINER_MultiHashMapOption opt)
{
  struct MapEntry *e;
  unsigned int i;

  i = idx_of (map, key);
  if ((opt != GNUNET_CONTAINER_MULTIHASHMAPOPTION_MULTIPLE) &&
      (opt != GNUNET_CONTAINER_MULTIHASHMAPOPTION_UNIQUE_FAST))
  {
    e = map->map[i];
    while (e != NULL)
    {
      if (key == e->key)
      {
        if (opt == GNUNET_CONTAINER_MULTIHASHMAPOPTION_UNIQUE_ONLY)
          return GNUNET_SYSERR;
        e->value = value;
        return GNUNET_NO;
      }
      e = e->next;
    }
  }
  if (map->size / 3 >= map->map_length / 4)
  {
    grow (map);
    i = idx_of (map, key);
  }
  e = GNUNET_new (struct MapEntry);
  e->key = key;
  e->value = value;
  e->next = map->map[i];
  map->map[i] = e;
  map->size++;
  return GNUNET_OK;
}


int
GNUNET_CONTAINER_multihashmap32_get_multiple (
  struct GNUNET_CONTAINER_MultiHashMap32 *map,
  uint32_t key,
  GNUNET_CONTAINER_MultiHashMapIterator32Callback it,
  void *it_cls)
{
  int count;
  struct MapEntry *e;
  struct MapEntry **ce;

  count = 0;
  ce = &map->next_cache[map->next_cache_off];
  GNUNET_assert (++map->next_cache_off < NEXT_CACHE_SIZE);

  *ce = map->map[idx_of (map, key)];
  while (NULL != (e = *ce))
  {
    *ce = e->next;
    if (key != e->key)
      continue;
    if ((NULL != it) && (GNUNET_OK != it (it_cls, key, e->value)))
    {
      GNUNET_assert (--map->next_cache_off < NEXT_CACHE_SIZE);
      return GNUNET_SYSERR;
    }
    count++;
  }
  GNUNET_assert (--map->next_cache_off < NEXT_CACHE_SIZE);
  return count;
}


/**
 * Create an iterator for a multihashmap.
 * The iterator can be used to retrieve all the elements in the multihashmap
 * one by one, without having to handle all elements at once (in contrast to
 * GNUNET_CONTAINER_multihashmap_iterate()).  Note that the iterator can not be
 * used anymore if elements have been removed from 'map' after the creation of
 * the iterator, or 'map' has been destroyed.  Adding elements to 'map' may
 * result in skipped or repeated elements.
 *
 * @param map the map to create an iterator for
 * @return an iterator over the given multihashmap @a map
 */
struct GNUNET_CONTAINER_MultiHashMap32Iterator *
GNUNET_CONTAINER_multihashmap32_iterator_create (
  const struct GNUNET_CONTAINER_MultiHashMap32 *map)
{
  struct GNUNET_CONTAINER_MultiHashMap32Iterator *iter;

  iter = GNUNET_new (struct GNUNET_CONTAINER_MultiHashMap32Iterator);
  iter->map = map;
  iter->modification_counter = map->modification_counter;
  iter->me = map->map[0];
  return iter;
}


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
  const void **value)
{
  /* make sure the map has not been modified */
  GNUNET_assert (iter->modification_counter == iter->map->modification_counter);

  /* look for the next entry, skipping empty buckets */
  while (1)
  {
    if (iter->idx >= iter->map->map_length)
      return GNUNET_NO;
    if (NULL != iter->me)
    {
      if (NULL != key)
        *key = iter->me->key;
      if (NULL != value)
        *value = iter->me->value;
      iter->me = iter->me->next;
      return GNUNET_YES;
    }
    iter->idx += 1;
    if (iter->idx < iter->map->map_length)
      iter->me = iter->map->map[iter->idx];
  }
}


/**
 * Destroy a multihashmap iterator.
 *
 * @param iter the iterator to destroy
 */
void
GNUNET_CONTAINER_multihashmap32_iterator_destroy (
  struct GNUNET_CONTAINER_MultiHashMapIterator *iter)
{
  GNUNET_free (iter);
}


/* end of container_multihashmap.c */
