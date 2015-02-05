/*
     This file is part of GNUnet.
     (C) 2011-2015 Christian Grothoff (and other contributing authors)

     GNUnet is free software; you can redistribute it and/or modify
     it under the terms of the GNU General Public License as published
     by the Free Software Foundation; either version 3, or (at your
     option) any later version.

     GNUnet is distributed in the hope that it will be useful, but
     WITHOUT ANY WARRANTY; without even the implied warranty of
     MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
     General Public License for more details.

     You should have received a copy of the GNU General Public License
     along with GNUnet; see the file COPYING.  If not, write to the
     Free Software Foundation, Inc., 59 Temple Place - Suite 330,
     Boston, MA 02111-1307, USA.
*/
/**
 * @file ats/gnunet-service-ats_performance.c
 * @brief ats service, interaction with 'performance' API
 * @author Matthias Wachs
 * @author Christian Grothoff
 */
#include "platform.h"
#include "gnunet-service-ats.h"
#include "gnunet-service-ats_addresses.h"
#include "gnunet-service-ats_performance.h"
#include "ats.h"


/**
 * Context for sending messages to performance clients without PIC.
 */
static struct GNUNET_SERVER_NotificationContext *nc;

/**
 * Context for sending messages to performance clients with PIC.
 */
static struct GNUNET_SERVER_NotificationContext *nc_pic;


/**
 * Transmit the given performance information to all performance
 * clients.
 *
 * @param pc client to send to, NULL for all
 * @param peer peer for which this is an address suggestion
 * @param plugin_name 0-termintated string specifying the transport plugin
 * @param plugin_addr binary address for the plugin to use
 * @param plugin_addr_len number of bytes in plugin_addr
 * @param active #GNUNET_YES if this address is actively used
 *        to maintain a connection to a peer;
 *        #GNUNET_NO if the address is not actively used;
 *        #GNUNET_SYSERR if this address is no longer available for ATS
 * @param atsi performance data for the address
 * @param atsi_count number of performance records in @a atsi
 * @param bandwidth_out assigned outbound bandwidth
 * @param bandwidth_in assigned inbound bandwidth
 */
static void
notify_client (struct GNUNET_SERVER_Client *client,
               const struct GNUNET_PeerIdentity *peer,
               const char *plugin_name,
               const void *plugin_addr,
               size_t plugin_addr_len,
               int active,
               const struct GNUNET_ATS_Information *atsi,
               uint32_t atsi_count,
               struct GNUNET_BANDWIDTH_Value32NBO bandwidth_out,
               struct GNUNET_BANDWIDTH_Value32NBO bandwidth_in)
{
  struct PeerInformationMessage *msg;
  size_t plugin_name_length = strlen (plugin_name) + 1;
  size_t msize =
      sizeof (struct PeerInformationMessage) +
      atsi_count * sizeof (struct GNUNET_ATS_Information) + plugin_addr_len +
      plugin_name_length;
  char buf[msize] GNUNET_ALIGN;
  struct GNUNET_ATS_Information *atsp;
  char *addrp;

  GNUNET_assert (msize < GNUNET_SERVER_MAX_MESSAGE_SIZE);
  GNUNET_assert (atsi_count <
                 GNUNET_SERVER_MAX_MESSAGE_SIZE /
                 sizeof (struct GNUNET_ATS_Information));
  msg = (struct PeerInformationMessage *) buf;
  msg->header.size = htons (msize);
  msg->header.type = htons (GNUNET_MESSAGE_TYPE_ATS_PEER_INFORMATION);
  msg->id = htonl (0);
  msg->ats_count = htonl (atsi_count);
  msg->peer = *peer;
  msg->address_length = htons (plugin_addr_len);
  msg->address_active = ntohl ((uint32_t) active);
  msg->plugin_name_length = htons (plugin_name_length);
  msg->bandwidth_out = bandwidth_out;
  msg->bandwidth_in = bandwidth_in;
  atsp = (struct GNUNET_ATS_Information *) &msg[1];
  memcpy (atsp, atsi, sizeof (struct GNUNET_ATS_Information) * atsi_count);
  addrp = (char *) &atsp[atsi_count];
  memcpy (addrp, plugin_addr, plugin_addr_len);
  strcpy (&addrp[plugin_addr_len], plugin_name);
  if (NULL == client)
  {
    GNUNET_SERVER_notification_context_broadcast (nc_pic,
                                                  &msg->header,
                                                  GNUNET_YES);
  }
  else
  {
    GNUNET_SERVER_notification_context_unicast (nc,
                                                client,
                                                &msg->header,
                                                GNUNET_YES);
  }
}


/**
 * Transmit the given performance information to all performance
 * clients.
 *
 * @param peer peer for which this is an address suggestion
 * @param plugin_name 0-termintated string specifying the transport plugin
 * @param plugin_addr binary address for the plugin to use
 * @param plugin_addr_len number of bytes in @a plugin_addr
 * @param active #GNUNET_YES if this address is actively used
 *        to maintain a connection to a peer;
 *        #GNUNET_NO if the address is not actively used;
 *        #GNUNET_SYSERR if this address is no longer available for ATS
 * @param atsi performance data for the address
 * @param atsi_count number of performance records in @a atsi
 * @param bandwidth_out assigned outbound bandwidth
 * @param bandwidth_in assigned inbound bandwidth
 */
void
GAS_performance_notify_all_clients (const struct GNUNET_PeerIdentity *peer,
                                    const char *plugin_name,
                                    const void *plugin_addr,
                                    size_t plugin_addr_len,
                                    int active,
                                    const struct GNUNET_ATS_Information *atsi,
                                    uint32_t atsi_count,
                                    struct GNUNET_BANDWIDTH_Value32NBO bandwidth_out,
                                    struct GNUNET_BANDWIDTH_Value32NBO bandwidth_in)
{
  notify_client (NULL,
                 peer,
                 plugin_name,
                 plugin_addr,
                 plugin_addr_len,
                 active,
                 atsi, atsi_count,
                 bandwidth_out,
                 bandwidth_in);
  GNUNET_STATISTICS_update (GSA_stats,
                            "# performance updates given to clients",
                            1,
                            GNUNET_NO);
}


/**
 * Iterator for called from #GAS_addresses_get_peer_info()
 *
 * @param cls closure with the `struct GNUNET_SERVER_Client *` to inform.
 * @param id the peer id
 * @param plugin_name plugin name
 * @param plugin_addr address
 * @param plugin_addr_len length of @a plugin_addr
 * @param active is address actively used
 * @param atsi ats performance information
 * @param atsi_count number of ats performance elements in @a atsi
 * @param bandwidth_out current outbound bandwidth assigned to address
 * @param bandwidth_in current inbound bandwidth assigned to address
 */
static void
peerinfo_it (void *cls,
             const struct GNUNET_PeerIdentity *id,
             const char *plugin_name,
             const void *plugin_addr,
             size_t plugin_addr_len,
             int active,
             const struct GNUNET_ATS_Information *atsi,
             uint32_t atsi_count,
             struct GNUNET_BANDWIDTH_Value32NBO bandwidth_out,
             struct GNUNET_BANDWIDTH_Value32NBO bandwidth_in)
{
  struct GNUNET_SERVER_Client *client = cls;

  if (NULL == id)
    return;
  GNUNET_log (GNUNET_ERROR_TYPE_DEBUG,
              "Callback for peer `%s' plugin `%s' BW out %u, BW in %u \n",
              GNUNET_i2s (id),
              plugin_name,
              (unsigned int) ntohl (bandwidth_out.value__),
              (unsigned int) ntohl (bandwidth_in.value__));
  notify_client (client,
                 id,
                 plugin_name,
                 plugin_addr,
                 plugin_addr_len,
                 active,
                 atsi, atsi_count,
                 bandwidth_out,
                 bandwidth_in);
}


/**
 * Register a new performance client.
 *
 * @param client handle of the new client
 * @param flag flag specifying the type of the client
 */
void
GAS_performance_add_client (struct GNUNET_SERVER_Client *client,
                            enum StartFlag flag)
{
  if (START_FLAG_PERFORMANCE_WITH_PIC == flag)
  {
    GNUNET_SERVER_notification_context_add (nc_pic,
                                            client);
    GNUNET_SERVER_notification_context_add (nc,
                                            client);
  }
  else
    GNUNET_SERVER_notification_context_add (nc,
                                            client);
  GAS_addresses_get_peer_info (NULL,
                               &peerinfo_it,
                               client);
}


/**
 * Initialize performance subsystem.
 *
 * @param server handle to our server
 */
void
GAS_performance_init (struct GNUNET_SERVER_Handle *server)
{
  nc = GNUNET_SERVER_notification_context_create (server, 32);
  nc_pic = GNUNET_SERVER_notification_context_create (server, 32);
}


/**
 * Shutdown performance subsystem.
 */
void
GAS_performance_done ()
{
  GNUNET_SERVER_notification_context_destroy (nc);
  nc = NULL;
  GNUNET_SERVER_notification_context_destroy (nc_pic);
  nc_pic = NULL;
}

/* end of gnunet-service-ats_performance.c */
