/*
     This file is part of GNUnet.
     Copyright (C) 2009, 2012, 2016 GNUnet e.V.

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
 * @file statistics/test_statistics_api.c
 * @brief testcase for statistics_api.c
 * @author Christian Grothoff
 */
#include "platform.h"
#include "gnunet_util_lib.h"
#include "gnunet_statistics_service.h"


static struct GNUNET_STATISTICS_Handle *h;

static struct GNUNET_STATISTICS_GetHandle *g;


static void
do_shutdown ()
{
  if (NULL != g)
  {
    GNUNET_STATISTICS_get_cancel (g);
    g = NULL;
  }
  GNUNET_STATISTICS_destroy (h, GNUNET_NO);
  h = NULL;
}


static int
check_1 (void *cls,
         const char *subsystem,
         const char *name,
         uint64_t value,
         int is_persistent)
{
  GNUNET_log (GNUNET_ERROR_TYPE_DEBUG,
              "Received value %llu for `%s:%s\n",
              (unsigned long long) value,
              subsystem,
              name);
  GNUNET_assert (0 == strcmp (name, "test-1"));
  GNUNET_assert (0 == strcmp (subsystem, "test-statistics-api"));
  GNUNET_assert (value == 1);
  GNUNET_assert (is_persistent == GNUNET_NO);
  return GNUNET_OK;
}


static int
check_2 (void *cls,
         const char *subsystem,
         const char *name,
         uint64_t value,
         int is_persistent)
{
  GNUNET_log (GNUNET_ERROR_TYPE_DEBUG,
              "Received value %llu for `%s:%s\n",
              (unsigned long long) value,
              subsystem,
              name);
  GNUNET_assert (0 == strcmp (name, "test-2"));
  GNUNET_assert (0 == strcmp (subsystem, "test-statistics-api"));
  GNUNET_assert (value == 2);
  GNUNET_assert (is_persistent == GNUNET_NO);
  return GNUNET_OK;
}


static int
check_3 (void *cls,
         const char *subsystem,
         const char *name,
         uint64_t value,
         int is_persistent)
{
  GNUNET_log (GNUNET_ERROR_TYPE_DEBUG,
              "Received value %llu for `%s:%s\n",
              (unsigned long long) value,
              subsystem,
              name);
  GNUNET_assert (0 == strcmp (name, "test-3"));
  GNUNET_assert (0 == strcmp (subsystem, "test-statistics-api"));
  GNUNET_assert (value == 3);
  GNUNET_assert (is_persistent == GNUNET_YES);
  return GNUNET_OK;
}


static void
next_fin (void *cls,
          int success)
{
  int *ok = cls;

  g = NULL;
  GNUNET_SCHEDULER_shutdown ();
  GNUNET_assert (success == GNUNET_OK);
  *ok = 0;
}


static void
next (void *cls, int success)
{
  g = NULL;
  GNUNET_assert (success == GNUNET_OK);
  GNUNET_log (GNUNET_ERROR_TYPE_DEBUG,
              "Issuing GET request\n");
  GNUNET_break (NULL !=
                GNUNET_STATISTICS_get (h, NULL, "test-2",
                                       &next_fin,
                                       &check_2, cls));
}


static void
run (void *cls,
     char *const *args,
     const char *cfgfile,
     const struct GNUNET_CONFIGURATION_Handle *cfg)
{
  h = GNUNET_STATISTICS_create ("test-statistics-api", cfg);
  if (NULL == h)
  {
    GNUNET_break (0);
    return;
  }
  GNUNET_SCHEDULER_add_shutdown (&do_shutdown,
                                 NULL);
  GNUNET_STATISTICS_set (h, "test-1", 1, GNUNET_NO);
  GNUNET_STATISTICS_set (h, "test-2", 2, GNUNET_NO);
  GNUNET_STATISTICS_set (h, "test-3", 2, GNUNET_NO);
  GNUNET_STATISTICS_update (h, "test-3", 1, GNUNET_YES);
  GNUNET_log (GNUNET_ERROR_TYPE_DEBUG,
              "Issuing GET request\n");
  GNUNET_break (NULL !=
                (g = GNUNET_STATISTICS_get (h, NULL, "test-1",
                                            &next,
                                            &check_1, cls)));
}


static void
run_more (void *cls,
          char *const *args,
          const char *cfgfile,
          const struct GNUNET_CONFIGURATION_Handle *cfg)
{
  h = GNUNET_STATISTICS_create ("test-statistics-api",
                                cfg);
  GNUNET_SCHEDULER_add_shutdown (&do_shutdown,
                                 NULL);
  GNUNET_break (NULL !=
                (g = GNUNET_STATISTICS_get (h, NULL,
                                            "test-3",
                                            &next_fin,
                                            &check_3, cls)));
}


int
main (int argc, char *argv_ign[])
{
  int ok = 1;
  char *const argv[] = { "test-statistics-api",
                         "-c",
                         "test_statistics_api_data.conf",
                         "-L", "WARNING",
                         NULL };
  struct GNUNET_GETOPT_CommandLineOption options[] = {
    GNUNET_GETOPT_OPTION_END
  };
  struct GNUNET_OS_Process *proc;
  char *binary;

  GNUNET_log_setup ("test_statistics_api",
                    "WARNING",
                    NULL);
  binary = GNUNET_OS_get_libexec_binary_path (GNUNET_OS_project_data_gnunet (),
                                              "gnunet-service-statistics");
  proc =
    GNUNET_OS_start_process (GNUNET_OS_INHERIT_STD_OUT_AND_ERR
                             | GNUNET_OS_USE_PIPE_CONTROL,
                             NULL, NULL, NULL,
                             binary,
                             "gnunet-service-statistics",
                             "-c", "test_statistics_api_data.conf", NULL);
  GNUNET_assert (NULL != proc);
  GNUNET_PROGRAM_run (GNUNET_OS_project_data_gnunet (),
                      5, argv,
                      "test-statistics-api", "nohelp",
                      options, &run,
                      &ok);
  if (0 != GNUNET_OS_process_kill (proc,
                                   GNUNET_TERM_SIG))
  {
    GNUNET_log_strerror (GNUNET_ERROR_TYPE_WARNING, "kill");
    ok = 1;
  }
  GNUNET_OS_process_wait (proc);
  GNUNET_OS_process_destroy (proc);
  proc = NULL;
  if (ok != 0)
  {
    GNUNET_free (binary);
    return ok;
  }
  ok = 1;
  /* restart to check persistence! */
  proc =
    GNUNET_OS_start_process (GNUNET_OS_INHERIT_STD_OUT_AND_ERR
                             | GNUNET_OS_USE_PIPE_CONTROL,
                             NULL, NULL, NULL,
                             binary,
                             "gnunet-service-statistics",
                             "-c", "test_statistics_api_data.conf",
                             NULL);
  GNUNET_PROGRAM_run (GNUNET_OS_project_data_gnunet (),
                      5, argv,
                      "test-statistics-api", "nohelp",
                      options,
                      &run_more, &ok);
  if (0 != GNUNET_OS_process_kill (proc,
                                   GNUNET_TERM_SIG))
  {
    GNUNET_log_strerror (GNUNET_ERROR_TYPE_WARNING, "kill");
    ok = 1;
  }
  GNUNET_OS_process_wait (proc);
  GNUNET_OS_process_destroy (proc);
  proc = NULL;
  GNUNET_free (binary);
  return ok;
}


/* end of test_statistics_api.c */
