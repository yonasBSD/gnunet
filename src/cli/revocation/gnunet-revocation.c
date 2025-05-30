/*
     This file is part of GNUnet.
     Copyright (C) 2013 GNUnet e.V.

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
 * @file revocation/gnunet-revocation.c
 * @brief tool for revoking public keys
 * @author Christian Grothoff
 */
#include "platform.h"
#include "gnunet_gnsrecord_lib.h"
#include "gnunet_util_lib.h"
#include "gnunet_revocation_service.h"

/**
 * Pow passes
 */
static unsigned int pow_passes = 1;

/**
 * Final status code.
 */
static int ret;

/**
 * Was "-p" specified?
 */
static int perform;

/**
 * -f option.
 */
static char *filename;

/**
 * -R option
 */
static char *revoke_ego;

/**
 * -t option.
 */
static char *test_ego;

/**
 * -e option.
 */
static unsigned int epochs = 1;

/**
 * Handle for revocation query.
 */
static struct GNUNET_REVOCATION_Query *q;

/**
 * Handle for revocation.
 */
static struct GNUNET_REVOCATION_Handle *h;

/**
 * Handle for our ego lookup.
 */
static struct GNUNET_IDENTITY_EgoLookup *el;

/**
 * Our configuration.
 */
static const struct GNUNET_CONFIGURATION_Handle *cfg;

/**
 * Number of matching bits required for revocation.
 */
static unsigned long long matching_bits;

/**
 * Epoch length
 */
static struct GNUNET_TIME_Relative epoch_duration;

/**
 * Task used for proof-of-work calculation.
 */
static struct GNUNET_SCHEDULER_Task *pow_task;

/**
 * Proof-of-work object
 */
static struct GNUNET_GNSRECORD_PowP *proof_of_work;

/**
 * Function run if the user aborts with CTRL-C.
 *
 * @param cls closure
 */
static void
do_shutdown (void *cls)
{
  fprintf (stderr, "%s", _ ("Shutting down...\n"));
  if (NULL != el)
  {
    GNUNET_IDENTITY_ego_lookup_cancel (el);
    el = NULL;
  }
  if (NULL != q)
  {
    GNUNET_REVOCATION_query_cancel (q);
    q = NULL;
  }
  if (NULL != h)
  {
    GNUNET_REVOCATION_revoke_cancel (h);
    h = NULL;
  }
}


/**
 * Print the result from a revocation query.
 *
 * @param cls NULL
 * @param is_valid #GNUNET_YES if the key is still valid, #GNUNET_NO if not, #GNUNET_SYSERR on error
 */
static void
print_query_result (void *cls, int is_valid)
{
  q = NULL;
  switch (is_valid)
  {
  case GNUNET_YES:
    fprintf (stdout, _ ("Key `%s' is valid\n"), test_ego);
    break;

  case GNUNET_NO:
    fprintf (stdout, _ ("Key `%s' has been revoked\n"), test_ego);
    break;

  case GNUNET_SYSERR:
    fprintf (stdout, "%s", _ ("Internal error\n"));
    break;

  default:
    GNUNET_break (0);
    break;
  }
  GNUNET_SCHEDULER_shutdown ();
}


/**
 * Print the result from a revocation request.
 *
 * @param cls NULL
 * @param is_valid #GNUNET_YES if the key is still valid, #GNUNET_NO if not, #GNUNET_SYSERR on error
 */
static void
print_revocation_result (void *cls, int is_valid)
{
  h = NULL;
  switch (is_valid)
  {
  case GNUNET_YES:
    if (NULL != revoke_ego)
      fprintf (stdout,
               _ ("Key for ego `%s' is still valid, revocation failed (!)\n"),
               revoke_ego);
    else
      fprintf (stdout, "%s", _ ("Revocation failed (!)\n"));
    break;

  case GNUNET_NO:
    if (NULL != revoke_ego)
      fprintf (stdout,
               _ ("Key for ego `%s' has been successfully revoked\n"),
               revoke_ego);
    else
      fprintf (stdout, "%s", _ ("Revocation successful.\n"));
    break;

  case GNUNET_SYSERR:
    fprintf (stdout,
             "%s",
             _ ("Internal error, key revocation might have failed\n"));
    break;

  default:
    GNUNET_break (0);
    break;
  }
  GNUNET_SCHEDULER_shutdown ();
}


/**
 * Perform the revocation.
 */
static void
perform_revocation ()
{
  h = GNUNET_REVOCATION_revoke (cfg,
                                proof_of_work,
                                &print_revocation_result,
                                NULL);
}


/**
 * Write the current state of the revocation data
 * to disk.
 *
 * @param rd data to sync
 */
static void
sync_pow ()
{
  size_t psize = GNUNET_GNSRECORD_proof_get_size (proof_of_work);
  if ((NULL != filename) &&
      (GNUNET_OK !=
       GNUNET_DISK_fn_write (filename,
                             proof_of_work,
                             psize,
                             GNUNET_DISK_PERM_USER_READ
                             | GNUNET_DISK_PERM_USER_WRITE)))
    GNUNET_log_strerror_file (GNUNET_ERROR_TYPE_ERROR, "write", filename);
}


/**
 * Perform the proof-of-work calculation.
 *
 * @param cls the `struct RevocationData`
 */
static void
calculate_pow_shutdown (void *cls)
{
  struct GNUNET_GNSRECORD_PowCalculationHandle *ph = cls;
  fprintf (stderr, "%s", _ ("Cancelling calculation.\n"));
  sync_pow ();
  if (NULL != pow_task)
  {
    GNUNET_SCHEDULER_cancel (pow_task);
    pow_task = NULL;
  }
  if (NULL != ph)
    GNUNET_GNSRECORD_pow_stop (ph);
}


/**
 * Perform the proof-of-work calculation.
 *
 * @param cls the `struct RevocationData`
 */
static void
calculate_pow (void *cls)
{
  struct GNUNET_GNSRECORD_PowCalculationHandle *ph = cls;
  size_t psize;

  /* store temporary results */
  pow_task = NULL;
  if (0 == (pow_passes % 128))
    sync_pow ();
  /* actually do POW calculation */
  if (GNUNET_OK == GNUNET_GNSRECORD_pow_round (ph))
  {
    psize = GNUNET_GNSRECORD_proof_get_size (proof_of_work);
    if (NULL != filename)
    {
      (void) GNUNET_DISK_directory_remove (filename);
      if (GNUNET_OK !=
          GNUNET_DISK_fn_write (filename,
                                proof_of_work,
                                psize,
                                GNUNET_DISK_PERM_USER_READ
                                | GNUNET_DISK_PERM_USER_WRITE))
        GNUNET_log_strerror_file (GNUNET_ERROR_TYPE_ERROR, "write", filename);
    }
    if (perform)
    {
      perform_revocation ();
    }
    else
    {
      fprintf (stderr, "%s", "\n");
      fprintf (stderr,
               _ ("Revocation certificate for `%s' stored in `%s'\n"),
               revoke_ego,
               filename);
      GNUNET_SCHEDULER_shutdown ();
    }
    return;
  }
  pow_passes++;
  pow_task = GNUNET_SCHEDULER_add_delayed (GNUNET_TIME_UNIT_MILLISECONDS,
                                           &calculate_pow,
                                           ph);

}


/**
 * Function called with the result from the ego lookup.
 *
 * @param cls closure
 * @param ego the ego, NULL if not found
 */
static void
ego_callback (void *cls, struct GNUNET_IDENTITY_Ego *ego)
{
  struct GNUNET_CRYPTO_PublicKey key;
  const struct GNUNET_CRYPTO_PrivateKey *privkey;
  struct GNUNET_GNSRECORD_PowCalculationHandle *ph = NULL;
  size_t psize;

  el = NULL;
  if (NULL == ego)
  {
    fprintf (stdout, _ ("Ego `%s' not found.\n"), revoke_ego);
    GNUNET_SCHEDULER_shutdown ();
    return;
  }
  GNUNET_IDENTITY_ego_get_public_key (ego, &key);
  privkey = GNUNET_IDENTITY_ego_get_private_key (ego);
  proof_of_work = GNUNET_malloc (GNUNET_MAX_POW_SIZE);
  if ((NULL != filename) && (GNUNET_YES == GNUNET_DISK_file_test (filename)) &&
      (0 < (psize =
              GNUNET_DISK_fn_read (filename, proof_of_work,
                                   GNUNET_MAX_POW_SIZE))))
  {
    ssize_t ksize = GNUNET_CRYPTO_public_key_get_length (&key);
    if (0 > ksize)
    {
      fprintf (stderr,
               _ ("Error: Key is invalid\n"));
      return;
    }
    if (((psize - sizeof (*proof_of_work)) < ksize) || // Key too small
        (0 != memcmp (&proof_of_work[1], &key, ksize))) // Keys do not match
    {
      fprintf (stderr,
               _ ("Error: revocation certificate in `%s' is not for `%s'\n"),
               filename,
               revoke_ego);
      return;
    }
    if (GNUNET_YES ==
        GNUNET_GNSRECORD_check_pow (proof_of_work,
                                    (unsigned int) matching_bits,
                                    epoch_duration))
    {
      fprintf (stderr, "%s", _ ("Revocation certificate ready\n"));
      if (perform)
        perform_revocation ();
      else
        GNUNET_SCHEDULER_shutdown ();
      return;
    }
    /**
     * Certificate not yet ready
     */
    fprintf (stderr,
             "%s",
             _ ("Continuing calculation where left off...\n"));
    ph = GNUNET_GNSRECORD_pow_start (proof_of_work,
                                     epochs,
                                     matching_bits);
  }
  fprintf (stderr,
           "%s",
           _ ("Revocation certificate not ready, calculating proof of work\n"));
  if (NULL == ph)
  {
    GNUNET_GNSRECORD_pow_init (privkey,
                               proof_of_work);
    ph = GNUNET_GNSRECORD_pow_start (proof_of_work,
                                     epochs,  /* Epochs */
                                     matching_bits);
  }
  pow_task = GNUNET_SCHEDULER_add_now (&calculate_pow, ph);
  GNUNET_SCHEDULER_add_shutdown (&calculate_pow_shutdown, ph);
}


/**
 * Main function that will be run by the scheduler.
 *
 * @param cls closure
 * @param args remaining command-line arguments
 * @param cfgfile name of the configuration file used (for saving, can be NULL!)
 * @param c configuration
 */
static void
run (void *cls,
     char *const *args,
     const char *cfgfile,
     const struct GNUNET_CONFIGURATION_Handle *c)
{
  struct GNUNET_CRYPTO_PublicKey pk;
  size_t psize;

  cfg = c;
  if (NULL != test_ego)
  {
    if (GNUNET_OK !=
        GNUNET_CRYPTO_public_key_from_string (test_ego,
                                              &pk))
    {
      fprintf (stderr, _ ("Public key `%s' malformed\n"), test_ego);
      return;
    }
    GNUNET_SCHEDULER_add_shutdown (&do_shutdown, NULL);
    q = GNUNET_REVOCATION_query (cfg, &pk, &print_query_result, NULL);
    if (NULL != revoke_ego)
      fprintf (
        stderr,
        "%s",
        _ (
          "Testing and revoking at the same time is not allowed, only executing test.\n"));
    return;
  }
  if (GNUNET_OK != GNUNET_CONFIGURATION_get_value_number (cfg,
                                                          "REVOCATION",
                                                          "WORKBITS",
                                                          &matching_bits))
  {
    GNUNET_log_config_missing (GNUNET_ERROR_TYPE_ERROR,
                               "REVOCATION",
                               "WORKBITS");
    return;
  }
  if (GNUNET_OK != GNUNET_CONFIGURATION_get_value_time (cfg,
                                                        "REVOCATION",
                                                        "EPOCH_DURATION",
                                                        &epoch_duration))
  {
    GNUNET_log_config_missing (GNUNET_ERROR_TYPE_ERROR,
                               "REVOCATION",
                               "EPOCH_DURATION");
    return;
  }

  if (NULL != revoke_ego)
  {
    if (! perform && (NULL == filename))
    {
      fprintf (stderr,
               "%s",
               _ ("No filename to store revocation certificate given.\n"));
      return;
    }
    /* main code here */
    el = GNUNET_IDENTITY_ego_lookup (cfg, revoke_ego, &ego_callback, NULL);
    GNUNET_SCHEDULER_add_shutdown (&do_shutdown, NULL);
    return;
  }
  if ((NULL != filename) && (perform))
  {
    size_t bread;
    proof_of_work = GNUNET_malloc (GNUNET_MAX_POW_SIZE);
    if (0 < (bread = GNUNET_DISK_fn_read (filename,
                                          proof_of_work,
                                          GNUNET_MAX_POW_SIZE)))
    {
      fprintf (stderr,
               _ ("Failed to read revocation certificate from `%s'\n"),
               filename);
      return;
    }
    psize = GNUNET_GNSRECORD_proof_get_size (proof_of_work);
    if (bread != psize)
    {
      fprintf (stderr,
               _ ("Revocation certificate corrupted in `%s'\n"),
               filename);
      return;
    }
    GNUNET_SCHEDULER_add_shutdown (&do_shutdown, NULL);
    if (GNUNET_YES !=
        GNUNET_GNSRECORD_check_pow (proof_of_work,
                                    (unsigned int) matching_bits,
                                    epoch_duration))
    {
      struct GNUNET_GNSRECORD_PowCalculationHandle *ph;
      ph = GNUNET_GNSRECORD_pow_start (proof_of_work,
                                       epochs,  /* Epochs */
                                       matching_bits);

      pow_task = GNUNET_SCHEDULER_add_now (&calculate_pow, ph);
      GNUNET_SCHEDULER_add_shutdown (&calculate_pow_shutdown, ph);
      return;
    }
    perform_revocation ();
    return;
  }
  fprintf (stderr, "%s", _ ("No action specified. Nothing to do.\n"));
}


/**
 * The main function of gnunet-revocation.
 *
 * @param argc number of arguments from the command line
 * @param argv command line arguments
 * @return 0 ok, 1 on error
 */
int
main (int argc, char *const *argv)
{
  struct GNUNET_GETOPT_CommandLineOption options[] = {
    GNUNET_GETOPT_option_string ('f',
                                 "filename",
                                 "NAME",
                                 gettext_noop (
                                   "use NAME for the name of the revocation file"),
                                 &filename),

    GNUNET_GETOPT_option_string (
      'R',
      "revoke",
      "NAME",
      gettext_noop (
        "revoke the private key associated for the the private key associated with the ego NAME "),
      &revoke_ego),

    GNUNET_GETOPT_option_flag (
      'p',
      "perform",
      gettext_noop (
        "actually perform revocation, otherwise we just do the precomputation"),
      &perform),

    GNUNET_GETOPT_option_string ('t',
                                 "test",
                                 "KEY",
                                 gettext_noop (
                                   "test if the public key KEY has been revoked"),
                                 &test_ego),
    GNUNET_GETOPT_option_uint ('e',
                               "epochs",
                               "EPOCHS",
                               gettext_noop (
                                 "number of epochs to calculate for"),
                               &epochs),

    GNUNET_GETOPT_OPTION_END
  };

  ret = (GNUNET_OK ==
         GNUNET_PROGRAM_run (GNUNET_OS_project_data_gnunet (),
                             argc,
                             argv,
                             "gnunet-revocation",
                             gettext_noop ("help text"),
                             options,
                             &run,
                             NULL))
        ? ret
        : 1;
  return ret;
}


/* end of gnunet-revocation.c */
