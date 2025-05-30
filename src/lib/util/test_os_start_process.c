/*
     This file is part of GNUnet.
     Copyright (C) 2009 GNUnet e.V.

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
 * @file util/test_os_start_process.c
 * @brief testcase for os start process code
 *
 * This testcase simply calls the os start process code
 * giving a file descriptor to write stdout to.  If the
 * correct data "HELLO" is read then all is well.
 */

#include "platform.h"
#include "gnunet_util_lib.h"
#include "disk.h"


static const char *test_phrase = "HELLO WORLD";

static int ok;

static struct GNUNET_OS_Process *proc;

/**
 * Pipe to write to started processes stdin (on write end)
 */
static struct GNUNET_DISK_PipeHandle *hello_pipe_stdin;

/**
 * Pipe to read from started processes stdout (on read end)
 */
static struct GNUNET_DISK_PipeHandle *hello_pipe_stdout;

static struct GNUNET_SCHEDULER_Task *die_task;

struct read_context
{
  char buf[16];
  int buf_offset;
  const struct GNUNET_DISK_FileHandle *stdout_read_handle;
};


static struct read_context rc;


static void
end_task (void *cls)
{
  if (0 != GNUNET_OS_process_kill (proc, GNUNET_TERM_SIG))
  {
    GNUNET_log_strerror (GNUNET_ERROR_TYPE_WARNING, "kill");
  }
  GNUNET_assert (GNUNET_OK == GNUNET_OS_process_wait (proc));
  GNUNET_OS_process_destroy (proc);
  proc = NULL;
  GNUNET_DISK_pipe_close (hello_pipe_stdout);
  GNUNET_DISK_pipe_close (hello_pipe_stdin);
}


static void
read_call (void *cls)
{
  int bytes;

  bytes = GNUNET_DISK_file_read (rc.stdout_read_handle,
                                 &rc.buf[rc.buf_offset],
                                 sizeof(rc.buf) - rc.buf_offset);
  GNUNET_log (GNUNET_ERROR_TYPE_DEBUG,
              "bytes is %d\n",
              bytes);

  if (bytes < 1)
  {
    GNUNET_break (0);
    ok = 1;
    GNUNET_SCHEDULER_cancel (die_task);
    (void) GNUNET_SCHEDULER_add_now (&end_task, NULL);
    return;
  }

  ok = strncmp (rc.buf, test_phrase, strlen (test_phrase));
  GNUNET_log (GNUNET_ERROR_TYPE_DEBUG,
              "read %s\n",
              &rc.buf[rc.buf_offset]);
  rc.buf_offset += bytes;

  if (0 == ok)
  {
    GNUNET_SCHEDULER_cancel (die_task);
    (void) GNUNET_SCHEDULER_add_now (&end_task, NULL);
    return;
  }

  GNUNET_SCHEDULER_add_read_file (GNUNET_TIME_UNIT_FOREVER_REL,
                                  rc.stdout_read_handle,
                                  &read_call,
                                  NULL);
}


static void
run_task (void *cls)
{
  const struct GNUNET_DISK_FileHandle *stdout_read_handle;
  const struct GNUNET_DISK_FileHandle *wh;

  hello_pipe_stdin = GNUNET_DISK_pipe (GNUNET_DISK_PF_BLOCKING_RW);
  hello_pipe_stdout = GNUNET_DISK_pipe (GNUNET_DISK_PF_BLOCKING_RW);
  if ((hello_pipe_stdout == NULL) || (hello_pipe_stdin == NULL))
  {
    GNUNET_break (0);
    ok = 1;
    return;
  }

  proc =
    GNUNET_OS_start_process (GNUNET_OS_INHERIT_STD_ERR,
                             hello_pipe_stdin, hello_pipe_stdout, NULL,
                             "cat",
                             "cat", "-", NULL);
  /* Close the write end of the read pipe */
  GNUNET_DISK_pipe_close_end (hello_pipe_stdout, GNUNET_DISK_PIPE_END_WRITE);
  /* Close the read end of the write pipe */
  GNUNET_DISK_pipe_close_end (hello_pipe_stdin, GNUNET_DISK_PIPE_END_READ);

  wh = GNUNET_DISK_pipe_handle (hello_pipe_stdin, GNUNET_DISK_PIPE_END_WRITE);

  /* Write the test_phrase to the cat process */
  if (GNUNET_DISK_file_write (wh, test_phrase, strlen (test_phrase) + 1) !=
      strlen (test_phrase) + 1)
  {
    GNUNET_break (0);
    ok = 1;
    return;
  }

  /* Close the write end to end the cycle! */
  GNUNET_DISK_pipe_close_end (hello_pipe_stdin, GNUNET_DISK_PIPE_END_WRITE);

  stdout_read_handle =
    GNUNET_DISK_pipe_handle (hello_pipe_stdout, GNUNET_DISK_PIPE_END_READ);

  die_task =
    GNUNET_SCHEDULER_add_delayed (GNUNET_TIME_relative_multiply
                                    (GNUNET_TIME_UNIT_MINUTES, 1),
                                  &end_task,
                                  NULL);

  memset (&rc, 0, sizeof(rc));
  rc.stdout_read_handle = stdout_read_handle;
  GNUNET_SCHEDULER_add_read_file (GNUNET_TIME_UNIT_FOREVER_REL,
                                  stdout_read_handle,
                                  &read_call,
                                  NULL);
}


/**
 * Main method, starts scheduler with task1,
 * checks that "ok" is correct at the end.
 */
static int
check_run ()
{
  ok = 1;
  GNUNET_SCHEDULER_run (&run_task, &ok);
  return ok;
}


/**
 * Test killing via pipe.
 */
static int
check_kill ()
{
  char *fn;

  hello_pipe_stdin = GNUNET_DISK_pipe (GNUNET_DISK_PF_BLOCKING_RW);
  hello_pipe_stdout = GNUNET_DISK_pipe (GNUNET_DISK_PF_BLOCKING_RW);
  if ((hello_pipe_stdout == NULL) || (hello_pipe_stdin == NULL))
  {
    return 1;
  }
  fn = GNUNET_OS_get_libexec_binary_path (GNUNET_OS_project_data_gnunet (),
                                          "gnunet-service-resolver");
  proc =
    GNUNET_OS_start_process (GNUNET_OS_INHERIT_STD_ERR
                             | GNUNET_OS_USE_PIPE_CONTROL,
                             hello_pipe_stdin,
                             hello_pipe_stdout,
                             NULL,
                             fn,
                             "gnunet-service-resolver", "-",
                             NULL);
  if (NULL == proc)
  {
    GNUNET_log (GNUNET_ERROR_TYPE_ERROR,
                "Failed to launch gnunet-service-resolver. Is your system setup correct?\n");
    return 77;
  }
  sleep (1);  /* give process time to start, so we actually use the pipe-kill mechanism! */
  GNUNET_free (fn);
  if (0 !=
      GNUNET_OS_process_kill (proc,
                              GNUNET_TERM_SIG))
    GNUNET_log_strerror (GNUNET_ERROR_TYPE_WARNING,
                         "kill");
  GNUNET_assert (GNUNET_OK ==
                 GNUNET_OS_process_wait (proc));
  GNUNET_OS_process_destroy (proc);
  proc = NULL;
  GNUNET_DISK_pipe_close (hello_pipe_stdout);
  GNUNET_DISK_pipe_close (hello_pipe_stdin);
  return 0;
}


/**
 * Test killing via pipe.
 */
static int
check_instant_kill ()
{
  char *fn;

  hello_pipe_stdin = GNUNET_DISK_pipe (GNUNET_DISK_PF_BLOCKING_RW);
  hello_pipe_stdout = GNUNET_DISK_pipe (GNUNET_DISK_PF_BLOCKING_RW);
  if ((hello_pipe_stdout == NULL) || (hello_pipe_stdin == NULL))
  {
    return 1;
  }
  fn = GNUNET_OS_get_libexec_binary_path (GNUNET_OS_project_data_gnunet (),
                                          "gnunet-service-resolver");
  proc =
    GNUNET_OS_start_process (GNUNET_OS_INHERIT_STD_ERR
                             | GNUNET_OS_USE_PIPE_CONTROL,
                             hello_pipe_stdin, hello_pipe_stdout, NULL,
                             fn,
                             "gnunet-service-resolver", "-", NULL);
  if (NULL == proc)
  {
    GNUNET_log (GNUNET_ERROR_TYPE_ERROR,
                "Failed to launch gnunet-service-resolver. Is your system setup correct?\n");
    return 77;
  }
  if (0 != GNUNET_OS_process_kill (proc,
                                   GNUNET_TERM_SIG))
  {
    GNUNET_log_strerror (GNUNET_ERROR_TYPE_WARNING,
                         "kill");
  }
  GNUNET_free (fn);
  GNUNET_assert (GNUNET_OK ==
                 GNUNET_OS_process_wait (proc));
  GNUNET_OS_process_destroy (proc);
  proc = NULL;
  GNUNET_DISK_pipe_close (hello_pipe_stdout);
  GNUNET_DISK_pipe_close (hello_pipe_stdin);
  return 0;
}


/**
 * Signal handler called for SIGPIPE.
 */
static void
sighandler_pipe (void)
{
  return;
}


int
main (int argc, char *argv[])
{
  int ret;
  struct GNUNET_SIGNAL_Context *shc_pipe;

  GNUNET_log_setup ("test-os-start-process",
                    "WARNING",
                    NULL);

  shc_pipe = GNUNET_SIGNAL_handler_install (SIGPIPE,
                                            &sighandler_pipe);
  ret = 0;
  ret |= check_run ();
  ret |= check_kill ();
  ret |= check_instant_kill ();
  GNUNET_SIGNAL_handler_uninstall (shc_pipe);
  return ret;
}


/* end of test_os_start_process.c */
