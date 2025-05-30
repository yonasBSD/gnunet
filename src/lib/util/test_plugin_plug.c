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
 * @file util/test_plugin_plug.c
 * @brief plugin for testing
 */

#include "platform.h"

void *
libgnunet_plugin_utiltest_init (void *arg);

void *
libgnunet_plugin_utiltest_init (void *arg)
{
  static char hello[] = "Hello";
  if (0 == strcmp (arg, "in"))
    return hello;
  return NULL;
}

void *
libgnunet_plugin_utiltest_done (void *arg);

void *
libgnunet_plugin_utiltest_done (void *arg)
{
  if (0 == strcmp (arg, "out"))
    return strdup ("World");
  return NULL;
}


/* end of test_plugin_plug.c */
