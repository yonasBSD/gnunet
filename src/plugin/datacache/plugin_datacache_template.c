/*
     This file is part of GNUnet
     Copyright (C) 2006, 2009, 2015, 2022 GNUnet e.V.

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
 * @file datacache/plugin_datacache_template.c
 * @brief template for an implementation of a database backend for the datacache
 * @author Christian Grothoff
 */
#include "platform.h"
#include "gnunet_util_lib.h"
#include "gnunet_datacache_plugin.h"


/**
 * Context for all functions in this plugin.
 */
struct Plugin
{
  /**
   * Our execution environment.
   */
  struct GNUNET_DATACACHE_PluginEnvironment *env;
};


/**
 * Store an item in the datastore.
 *
 * @param cls closure (our `struct Plugin`)
 * @param xor_distance distance of @a key to our PID
 * @param block data to store
 * @return 0 if duplicate, -1 on error, number of bytes used otherwise
 */
static ssize_t
template_plugin_put (void *cls,
                     uint32_t xor_distance,
                     const struct GNUNET_DATACACHE_Block *block)
{
  GNUNET_break (0);
  return -1;
}


/**
 * Iterate over the results for a particular key
 * in the datastore.
 *
 * @param cls closure (our `struct Plugin`)
 * @param key
 * @param type entries of which type are relevant?
 * @param iter maybe NULL (to just count)
 * @param iter_cls closure for @a iter
 * @return the number of results found
 */
static unsigned int
template_plugin_get (void *cls,
                     const struct GNUNET_HashCode *key,
                     enum GNUNET_BLOCK_Type type,
                     GNUNET_DATACACHE_Iterator iter,
                     void *iter_cls)
{
  GNUNET_break (0);
  return 0;
}


/**
 * Delete the entry with the lowest expiration value
 * from the datacache right now.
 *
 * @param cls closure (our `struct Plugin`)
 * @return #GNUNET_OK on success, #GNUNET_SYSERR on error
 */
static enum GNUNET_GenericReturnValue
template_plugin_del (void *cls)
{
  GNUNET_break (0);
  return GNUNET_SYSERR;
}


/**
 * Iterate over the results that are "close" to a particular key in
 * the datacache.  "close" is defined as numerically larger than @a
 * key (when interpreted as a circular address space), with small
 * distance.
 *
 * @param cls closure (internal context for the plugin)
 * @param key area of the keyspace to look into
 * @param type desired block type for the replies
 * @param num_results number of results that should be returned to @a iter
 * @param iter maybe NULL (to just count)
 * @param iter_cls closure for @a iter
 * @return the number of results found
 */
static unsigned int
template_plugin_get_closest (void *cls,
                             const struct GNUNET_HashCode *key,
                             enum GNUNET_BLOCK_Type type,
                             unsigned int num_results,
                             GNUNET_DATACACHE_Iterator iter,
                             void *iter_cls)
{
  GNUNET_break (0);
  return 0;
}

void *
libgnunet_plugin_datacache_template_init (void *cls);

/**
 * Entry point for the plugin.
 *
 * @param cls closure (the `struct GNUNET_DATACACHE_PluginEnvironmnet`)
 * @return the plugin's closure (our `struct Plugin`)
 */
void *
libgnunet_plugin_datacache_template_init (void *cls)
{
  struct GNUNET_DATACACHE_PluginEnvironment *env = cls;
  struct GNUNET_DATACACHE_PluginFunctions *api;
  struct Plugin *plugin;

  plugin = GNUNET_new (struct Plugin);
  plugin->env = env;
  api = GNUNET_new (struct GNUNET_DATACACHE_PluginFunctions);
  api->cls = plugin;
  api->get = &template_plugin_get;
  api->put = &template_plugin_put;
  api->del = &template_plugin_del;
  api->get_closest = &template_plugin_get_closest;
  GNUNET_log_from (GNUNET_ERROR_TYPE_INFO,
                   "template",
                   "Template datacache running\n");
  return api;
}

void *
libgnunet_plugin_datacache_template_done (void *cls);

/**
 * Exit point from the plugin.
 *
 * @param cls closure (our `struct Plugin`)
 * @return NULL
 */
void *
libgnunet_plugin_datacache_template_done (void *cls)
{
  struct GNUNET_DATACACHE_PluginFunctions *api = cls;
  struct Plugin *plugin = api->cls;

  GNUNET_free (plugin);
  GNUNET_free (api);
  return NULL;
}


/* end of plugin_datacache_template.c */
