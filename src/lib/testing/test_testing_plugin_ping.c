/*
      This file is part of GNUnet
      Copyright (C) 2021 GNUnet e.V.

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
 * @file testing/test_testing_plugin_ping.c
 * @brief a plugin to provide the API for running test cases.
 * @author t3sserakt
 */
#include "platform.h"
#include "gnunet_testing_lib.h"

GNUNET_TESTING_MAKE_PLUGIN (
  libgnunet_test_testing,
  ping,
  GNUNET_TESTING_cmd_exec_va ("ping",
                              GNUNET_OS_PROCESS_EXITED,
                              0,
                              "ping",
                              "-c1",
                              (0 == strcmp (strchr (my_node_id,
                                                    '-'),
                                            "-0001"))
                              ? "127.0.0.1"
                              : "127.0.0.2",
                              NULL),
  GNUNET_TESTING_cmd_barrier_create ("barrier",
                                     1)
#if LATER
  GNUNET_TESTING_cmd_barrier_reached ("label",
                                      "peers-started-barrier"
                                      ),
  // peer runs here!
#endif
  )

/* end of test_testing_plugin_ping.c */
