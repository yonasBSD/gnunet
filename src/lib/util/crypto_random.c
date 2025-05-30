/*
     This file is part of GNUnet.  Copyright (C) 2001-2014 Christian Grothoff
     (and other contributing authors)

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
 * @file util/crypto_random.c
 * @brief functions to gather random numbers
 * @author Christian Grothoff
 */

#include "platform.h"
#include "gnunet_util_lib.h"
#include <gcrypt.h>

#define LOG(kind, ...) GNUNET_log_from (kind, "util-crypto-random", __VA_ARGS__)

#define LOG_STRERROR(kind, syscall) \
  GNUNET_log_from_strerror (kind, "util-crypto-random", syscall)


/* TODO: ndurner, move this to plibc? */
/* The code is derived from glibc, obviously */
#if ! HAVE_RANDOM || ! HAVE_SRANDOM
#ifdef RANDOM
#undef RANDOM
#endif
#ifdef SRANDOM
#undef SRANDOM
#endif
#define RANDOM() glibc_weak_rand32 ()
#define SRANDOM(s) glibc_weak_srand32 (s)
#if defined(RAND_MAX)
#undef RAND_MAX
#endif
#define RAND_MAX 0x7fffffff /* Hopefully this is correct */


static int32_t glibc_weak_rand32_state = 1;


void
glibc_weak_srand32 (int32_t s)
{
  glibc_weak_rand32_state = s;
}


int32_t
glibc_weak_rand32 ()
{
  int32_t val = glibc_weak_rand32_state;

  val = ((glibc_weak_rand32_state * 1103515245) + 12345) & 0x7fffffff;
  glibc_weak_rand32_state = val;
  return val;
}


#endif

/**
 * Create a cryptographically weak pseudo-random number in the interval of 0 to 1.
 *
 * @return number between 0 and 1.
 */
static double
get_weak_random (void)
{
  return(random () / (double) RAND_MAX);
}


void
GNUNET_CRYPTO_seed_weak_random (int32_t seed)
{
#ifdef OPENBSD
  srandom_deterministic (seed);
#else
  srandom (seed);
#endif
}


/**
 * @ingroup crypto
 * Zero out @a buffer, securely against compiler optimizations.
 * Used to delete key material.
 *
 * @param buffer the buffer to zap
 * @param length buffer length
 */
void
GNUNET_CRYPTO_zero_keys (void *buffer, size_t length)
{
#if HAVE_MEMSET_S
  memset_s (buffer, length, 0, length);
#elif HAVE_EXPLICIT_BZERO
  explicit_bzero (buffer, length);
#else
  volatile unsigned char *p = buffer;
  while (length--)
    *p++ = 0;
#endif
}


/**
 * @ingroup crypto
 * Fill block with a random values.
 *
 * @param mode desired quality of the random number
 * @param buffer the buffer to fill
 * @param length buffer length
 */
void
GNUNET_CRYPTO_random_block (enum GNUNET_CRYPTO_Quality mode,
                            void *buffer,
                            size_t length)
{
#ifdef gcry_fast_random_poll
  static unsigned int invokeCount;
#endif
  switch (mode)
  {
  case GNUNET_CRYPTO_QUALITY_STRONG:
    /* see http://lists.gnupg.org/pipermail/gcrypt-devel/2004-May/000613.html */
#ifdef gcry_fast_random_poll
    if ((invokeCount++ % 256) == 0)
      gcry_fast_random_poll ();
#endif
    gcry_randomize (buffer, length, GCRY_STRONG_RANDOM);
    return;

  case GNUNET_CRYPTO_QUALITY_NONCE:
    gcry_create_nonce (buffer, length);
    return;

  case GNUNET_CRYPTO_QUALITY_WEAK:
    /* see http://lists.gnupg.org/pipermail/gcrypt-devel/2004-May/000613.html */
#ifdef gcry_fast_random_poll
    if ((invokeCount++ % 256) == 0)
      gcry_fast_random_poll ();
#endif
    gcry_randomize (buffer, length, GCRY_WEAK_RANDOM);
    return;

  default:
    GNUNET_assert (0);
  }
}


/**
 * Produce a random unsigned 32-bit number modulo @a i.
 *
 * @param mode desired quality of the random number
 * @param i the upper limit (exclusive) for the random number
 * @return a random value in the interval [0,i[.
 */
uint32_t
GNUNET_CRYPTO_random_u32 (enum GNUNET_CRYPTO_Quality mode,
                          uint32_t i)
{
#ifdef gcry_fast_random_poll
  static unsigned int invokeCount;
#endif
  uint32_t ret;
  uint32_t ul;

  GNUNET_assert (i > 0);

  switch (mode)
  {
  case GNUNET_CRYPTO_QUALITY_STRONG:
    /* see http://lists.gnupg.org/pipermail/gcrypt-devel/2004-May/000613.html */
#ifdef gcry_fast_random_poll
    if ((invokeCount++ % 256) == 0)
      gcry_fast_random_poll ();
#endif
    ul = UINT32_MAX - (UINT32_MAX % i);
    do
    {
      gcry_randomize ((unsigned char *) &ret,
                      sizeof(uint32_t),
                      GCRY_STRONG_RANDOM);
    }
    while (ret >= ul);
    return ret % i;

  case GNUNET_CRYPTO_QUALITY_NONCE:
    ul = UINT32_MAX - (UINT32_MAX % i);
    do
    {
      gcry_create_nonce (&ret, sizeof(ret));
    }
    while (ret >= ul);
    return ret % i;

  case GNUNET_CRYPTO_QUALITY_WEAK:
    ret = i * get_weak_random ();
    if (ret >= i)
      ret = i - 1;
    return ret;

  default:
    GNUNET_assert (0);
  }
  return 0;
}


/**
 * Get an array with a random permutation of the
 * numbers 0...n-1.
 * @param mode #GNUNET_RANDOM_QUALITY_STRONG if the strong (but expensive)
 *        PRNG should be used, #GNUNET_RANDOM_QUALITY_WEAK otherwise
 * @param n the size of the array
 * @return the permutation array (allocated from heap)
 */
unsigned int *
GNUNET_CRYPTO_random_permute (enum GNUNET_CRYPTO_Quality mode,
                              unsigned int n)
{
  unsigned int *ret;
  unsigned int i;
  unsigned int tmp;
  uint32_t x;

  GNUNET_assert (n > 0);
  ret = GNUNET_malloc (n * sizeof(unsigned int));
  for (i = 0; i < n; i++)
    ret[i] = i;
  for (i = n - 1; i > 0; i--)
  {
    x = GNUNET_CRYPTO_random_u32 (mode, i + 1);
    tmp = ret[x];
    ret[x] = ret[i];
    ret[i] = tmp;
  }
  return ret;
}


uint64_t
GNUNET_CRYPTO_random_u64 (enum GNUNET_CRYPTO_Quality mode,
                          uint64_t max)
{
  uint64_t ret;
  uint64_t ul;

  GNUNET_assert (max > 0);
  switch (mode)
  {
  case GNUNET_CRYPTO_QUALITY_STRONG:
    ul = UINT64_MAX - (UINT64_MAX % max);
    do
    {
      gcry_randomize ((unsigned char *) &ret,
                      sizeof(uint64_t),
                      GCRY_STRONG_RANDOM);
    }
    while (ret >= ul);
    return ret % max;

  case GNUNET_CRYPTO_QUALITY_NONCE:
    ul = UINT64_MAX - (UINT64_MAX % max);
    do
    {
      gcry_create_nonce (&ret, sizeof(ret));
    }
    while (ret >= ul);

    return ret % max;

  case GNUNET_CRYPTO_QUALITY_WEAK:
    ret = max * get_weak_random ();
    if (ret >= max)
      ret = max - 1;
    return ret;

  default:
    GNUNET_assert (0);
  }
  return 0;
}


/**
 * @ingroup crypto
 * Fill UUID with a timeflake pseudo-random value.  Note that
 * timeflakes use only 80 bits of randomness and 48 bits
 * to encode a timestamp in milliseconds. So what we return
 * here is not a completely random number.
 *
 * @param mode desired quality of the random number
 * @param uuid the value to fill
 */
void
GNUNET_CRYPTO_random_timeflake (enum GNUNET_CRYPTO_Quality mode,
                                struct GNUNET_Uuid *uuid)
{
  struct GNUNET_TIME_Absolute now;
  uint64_t ms;
  uint64_t be;
  char *base;

  GNUNET_CRYPTO_random_block (mode,
                              uuid,
                              sizeof (struct GNUNET_Uuid));
  now = GNUNET_TIME_absolute_get ();
  ms = now.abs_value_us / GNUNET_TIME_UNIT_MILLISECONDS.rel_value_us;
  be = GNUNET_htonll (ms);
  base = (char *) &be;
  memcpy (uuid,
          base + 2,
          sizeof (be) - 2);
}


/**
 * Allocation wrapper for libgcrypt, used to avoid bad locking
 * strategy of libgcrypt implementation.
 */
static void *
w_malloc (size_t n)
{
  return calloc (n, 1);
}


/**
 * Allocation wrapper for libgcrypt, used to avoid bad locking
 * strategy of libgcrypt implementation.
 */
static int
w_check (const void *p)
{
  (void) p;
  return 0; /* not secure memory */
}

void
GNUNET_CRYPTO_random_init (void);

/**
 * Initialize libgcrypt.
 */
void __attribute__ ((constructor))
GNUNET_CRYPTO_random_init ()
{
  gcry_error_t rc;

  if (! gcry_check_version (NEED_LIBGCRYPT_VERSION))
  {
    fprintf (
      stderr,
      _ ("libgcrypt has not the expected version (version %s is required).\n"),
      NEED_LIBGCRYPT_VERSION);
    GNUNET_assert (0);
  }
  /* set custom allocators */
  gcry_set_allocation_handler (&w_malloc, &w_malloc, &w_check, &realloc, &free);
  /* Disable use of secure memory */
  if ((rc = gcry_control (GCRYCTL_DISABLE_SECMEM, 0)))
    fprintf (stderr,
             "Failed to set libgcrypt option %s: %s\n",
             "DISABLE_SECMEM",
             gcry_strerror (rc));
  /* Otherwise gnunet-ecc takes forever to complete, besides
     we are fine with "just" using GCRY_STRONG_RANDOM */
  if ((rc = gcry_control (GCRYCTL_ENABLE_QUICK_RANDOM, 0)))
    fprintf (stderr,
             "Failed to set libgcrypt option %s: %s\n",
             "ENABLE_QUICK_RANDOM",
             gcry_strerror (rc));
  gcry_control (GCRYCTL_INITIALIZATION_FINISHED, 0);
  gcry_fast_random_poll ();
  GNUNET_CRYPTO_seed_weak_random (
    time (NULL)
    ^ GNUNET_CRYPTO_random_u32 (GNUNET_CRYPTO_QUALITY_NONCE, UINT32_MAX));
}

void
GNUNET_CRYPTO_random_fini (void);

/**
 * Nicely shut down libgcrypt.
 */
void __attribute__ ((destructor))
GNUNET_CRYPTO_random_fini (void)
{
  gcry_set_progress_handler (NULL, NULL);
#ifdef GCRYCTL_CLOSE_RANDOM_DEVICE
  (void) gcry_control (GCRYCTL_CLOSE_RANDOM_DEVICE, 0);
#endif
}


/* end of crypto_random.c */
