/*
     This file is part of GNUnet.
     Copyright (C) 2002, 2003, 2004, 2006, 2009, 2022 GNUnet e.V.

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
 * @author Christian Grothoff
 * @file util/test_crypto_hash.c
 * @brief Test for crypto_hash.c
 */

#include "platform.h"
#include "gnunet_util_lib.h"

static char block[65536];

#define FILENAME "testblock.dat"

static int
test (int number)
{
  struct GNUNET_HashCode h1;
  struct GNUNET_HashCode h2;
  struct GNUNET_CRYPTO_HashAsciiEncoded enc;

  memset (&h1,
          number,
          sizeof(struct GNUNET_HashCode));
  GNUNET_CRYPTO_hash_to_enc (&h1,
                             &enc);
  if (GNUNET_OK !=
      GNUNET_CRYPTO_hash_from_string ((char *) &enc,
                                      &h2))
  {
    printf ("enc2hash failed!\n");
    return 1;
  }
  if (0 != GNUNET_memcmp (&h1,
                          &h2))
    return 1;
  return 0;
}


static int
test_encoding (void)
{
  for (int i = 0; i < 255; i++)
    if (0 != test (i))
      return 1;
  return 0;
}


static int
test_arithmetic (void)
{
  struct GNUNET_HashCode h1;
  struct GNUNET_HashCode h2;
  struct GNUNET_HashCode d;
  struct GNUNET_HashCode s;
  struct GNUNET_CRYPTO_SymmetricSessionKey skey;
  struct GNUNET_CRYPTO_SymmetricInitializationVector iv;

  GNUNET_CRYPTO_hash_create_random (GNUNET_CRYPTO_QUALITY_WEAK,
                                    &h1);
  GNUNET_CRYPTO_hash_create_random (GNUNET_CRYPTO_QUALITY_WEAK,
                                    &h2);
  if (GNUNET_CRYPTO_hash_distance_u32 (&h1,
                                       &h2) !=
      GNUNET_CRYPTO_hash_distance_u32 (&h2,
                                       &h1))
    return 1;
  GNUNET_CRYPTO_hash_difference (&h1,
                                 &h2,
                                 &d);
  GNUNET_CRYPTO_hash_sum (&h1,
                          &d,
                          &s);
  if (0 !=
      GNUNET_CRYPTO_hash_cmp (&s,
                              &h2))
    return 1;
  GNUNET_CRYPTO_hash_xor (&h1,
                          &h2,
                          &d);
  GNUNET_CRYPTO_hash_xor (&h1,
                          &d,
                          &s);
  if (0 !=
      GNUNET_CRYPTO_hash_cmp (&s,
                              &h2))
    return 1;
  if (0 !=
      GNUNET_CRYPTO_hash_xorcmp (&s,
                                 &h2,
                                 &h1))
    return 1;
  if (-1 !=
      GNUNET_CRYPTO_hash_xorcmp (&h1,
                                 &h2,
                                 &h1))
    return 1;
  if (1 !=
      GNUNET_CRYPTO_hash_xorcmp (&h1,
                                 &h2,
                                 &h2))
    return 1;
  memset (&d,
          0,
          sizeof(d));
  GNUNET_CRYPTO_hash_to_aes_key (&d,
                                 &skey,
                                 &iv);
  memset (&h1,
          0,
          sizeof (h1));
  h1.bits[1] = htonl (0x00200000); /* 32 + 8 + 2 = 42 MSB bits cleared */
  GNUNET_assert (42 ==
                 GNUNET_CRYPTO_hash_count_leading_zeros (&h1));
  GNUNET_assert (512 - 42 - 1 ==
                 GNUNET_CRYPTO_hash_count_tailing_zeros (&h1));
  return 0;
}


static void
finished_task (void *cls,
               const struct GNUNET_HashCode *res)
{
  int *ret = cls;
  struct GNUNET_HashCode want;

  GNUNET_CRYPTO_hash (block,
                      sizeof(block),
                      &want);
  if (0 != GNUNET_memcmp (res,
                          &want))
    *ret = 2;
  else
    *ret = 0;
}


static void
file_hasher (void *cls)
{
  GNUNET_assert (NULL !=
                 GNUNET_CRYPTO_hash_file (GNUNET_SCHEDULER_PRIORITY_DEFAULT,
                                          FILENAME,
                                          1024,
                                          &finished_task,
                                          cls));
}


static int
test_file_hash (void)
{
  int ret;
  FILE *f;

  memset (block,
          42,
          sizeof(block) / 2);
  memset (&block[sizeof(block) / 2],
          43,
          sizeof(block) / 2);
  GNUNET_assert (NULL != (f = fopen (FILENAME, "w+")));
  GNUNET_break (sizeof(block) ==
                fwrite (block,
                        1,
                        sizeof(block),
                        f));
  GNUNET_break (0 == fclose (f));
  ret = 1;
  GNUNET_SCHEDULER_run (&file_hasher,
                        &ret);
  GNUNET_break (0 == unlink (FILENAME));
  return ret;
}


int
main (int argc,
      char *argv[])
{
  int failureCount = 0;

  GNUNET_log_setup ("test-crypto-hash",
                    "WARNING",
                    NULL);
  for (int i = 0; i < 10; i++)
    failureCount += test_encoding ();
  failureCount += test_arithmetic ();
  failureCount += test_file_hash ();
  if (0 != failureCount)
    return 1;
  return 0;
}


/* end of test_crypto_hash.c */
