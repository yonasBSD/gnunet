/*
     This file is part of GNUnet.
     Copyright (C) 2009-2013 GNUnet e.V.

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
 * @file gnsrecord/gnsrecord.c
 * @brief API to access GNS record data
 * @author Martin Schanzenbach
 * @author Matthias Wachs
 * @author Christian Grothoff
 */
#include "gnunet_util_lib.h"
#include "gnunet_gnsrecord_lib.h"
#include "gnunet_gnsrecord_plugin.h"

#define LOG(kind, ...) GNUNET_log_from (kind, "gnsrecord", __VA_ARGS__)


/**
 * Handle for a plugin.
 */
struct Plugin
{
  /**
   * Name of the shared library.
   */
  char *library_name;

  /**
   * Plugin API.
   */
  struct GNUNET_GNSRECORD_PluginFunctions *api;
};


/**
 * Array of our plugins.
 */
static struct Plugin **gns_plugins;

/**
 * Size of the 'plugins' array.
 */
static unsigned int num_plugins;

/**
 * Global to mark if we've run the initialization.
 */
static int once;


/**
 * Add a plugin to the list managed by the block library.
 *
 * @param cls NULL
 * @param library_name name of the plugin
 * @param lib_ret the plugin API
 */
static void
add_plugin (void *cls,
            const char *library_name,
            void *lib_ret)
{
  struct GNUNET_GNSRECORD_PluginFunctions *api = lib_ret;
  struct Plugin *plugin;

  GNUNET_log (GNUNET_ERROR_TYPE_DEBUG,
              "Loading block plugin `%s'\n",
              library_name);
  plugin = GNUNET_new (struct Plugin);
  plugin->api = api;
  plugin->library_name = GNUNET_strdup (library_name);
  GNUNET_array_append (gns_plugins, num_plugins, plugin);
}


/**
 * Loads all plugins (lazy initialization).
 */
static void
init ()
{
  if (1 == once)
    return;
  once = 1;
  GNUNET_PLUGIN_load_all (GNUNET_OS_project_data_gnunet (),
                          "libgnunet_plugin_gnsrecord_",
                          NULL,
                          &add_plugin,
                          NULL);
}


void
GNSRECORD_fini (void);


/**
 * Dual function to #init().
 */
void __attribute__ ((destructor))
GNSRECORD_fini (void)
{
  struct Plugin *plugin;

  for (unsigned int i = 0; i < num_plugins; i++)
  {
    plugin = gns_plugins[i];
    GNUNET_break (NULL ==
                  GNUNET_PLUGIN_unload (plugin->library_name,
                                        plugin->api));
    GNUNET_free (plugin->library_name);
    GNUNET_free (plugin);
  }
  GNUNET_free (gns_plugins);
  gns_plugins = NULL;
  once = 0;
  num_plugins = 0;
}


/**
 * Convert the 'value' of a record to a string.
 *
 * @param type type of the record
 * @param data value in binary encoding
 * @param data_size number of bytes in @a data
 * @return NULL on error, otherwise human-readable representation of the value
 */
char *
GNUNET_GNSRECORD_value_to_string (uint32_t type,
                                  const void *data,
                                  size_t data_size)
{
  struct Plugin *plugin;
  char *ret;

  init ();
  for (unsigned int i = 0; i < num_plugins; i++)
  {
    plugin = gns_plugins[i];
    if (NULL != (ret = plugin->api->value_to_string (plugin->api->cls,
                                                     type,
                                                     data,
                                                     data_size)))
      return ret;
  }
  return NULL;
}


int
GNUNET_GNSRECORD_string_to_value (uint32_t type,
                                  const char *s,
                                  void **data,
                                  size_t *data_size)
{
  struct Plugin *plugin;

  init ();
  for (unsigned int i = 0; i < num_plugins; i++)
  {
    plugin = gns_plugins[i];
    if (GNUNET_OK == plugin->api->string_to_value (plugin->api->cls,
                                                   type,
                                                   s,
                                                   data,
                                                   data_size))
      return GNUNET_OK;
  }
  return GNUNET_SYSERR;
}


uint32_t
GNUNET_GNSRECORD_typename_to_number (const char *dns_typename)
{
  struct Plugin *plugin;
  uint32_t ret;

  if (0 == strcasecmp (dns_typename,
                       "ANY"))
    return GNUNET_GNSRECORD_TYPE_ANY;
  init ();
  for (unsigned int i = 0; i < num_plugins; i++)
  {
    plugin = gns_plugins[i];
    if (UINT32_MAX != (ret = plugin->api->typename_to_number (plugin->api->cls,
                                                              dns_typename)))
      return ret;
  }
  return UINT32_MAX;
}


/**
 * Convert a type number to the corresponding type string (e.g. 1 to "A")
 *
 * @param type number of a type to convert
 * @return corresponding typestring, NULL on error
 */
const char *
GNUNET_GNSRECORD_number_to_typename (uint32_t type)
{
  struct Plugin *plugin;
  const char *ret;

  if (GNUNET_GNSRECORD_TYPE_ANY == type)
    return "ANY";
  init ();
  for (unsigned int i = 0; i < num_plugins; i++)
  {
    plugin = gns_plugins[i];
    if (NULL != (ret = plugin->api->number_to_typename (plugin->api->cls,
                                                        type)))
      return ret;
  }
  return NULL;
}


enum GNUNET_GenericReturnValue
GNUNET_GNSRECORD_is_critical (uint32_t type)
{
  struct Plugin *plugin;

  if (GNUNET_GNSRECORD_TYPE_ANY == type)
    return GNUNET_NO;
  init ();
  for (unsigned int i = 0; i < num_plugins; i++)
  {
    plugin = gns_plugins[i];
    if (NULL == plugin->api->is_critical)
      continue;
    if (GNUNET_NO == plugin->api->is_critical (plugin->api->cls, type))
      continue;
    return GNUNET_YES;
  }
  return GNUNET_NO;
}


/* end of gnsrecord.c */
