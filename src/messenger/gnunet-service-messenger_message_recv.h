/*
   This file is part of GNUnet.
   Copyright (C) 2020--2021 GNUnet e.V.

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
 * @author Tobias Frisch
 * @file src/messenger/gnunet-service-messenger_message_recv.h
 * @brief GNUnet MESSENGER service
 */

#ifndef GNUNET_SERVICE_MESSENGER_MESSAGE_RECV_H
#define GNUNET_SERVICE_MESSENGER_MESSAGE_RECV_H

#include "platform.h"
#include "gnunet_crypto_lib.h"

#include "gnunet-service-messenger_message_kind.h"

#include "gnunet-service-messenger_member_session.h"
#include "gnunet-service-messenger_tunnel.h"
#include "messenger_api_message.h"

/**
 * Handles a received info message to change the current member id to the one generated by
 * the host connected to. (all current tunnels will be informed about the id change)
 *
 * @param[in/out] room Room of the message
 * @param[in/out] tunnel Receiving connection
 * @param[in] message INFO-Message
 * @param[in] hash Hash of the message
 * @return #GNUNET_NO to not forward the message
 */
int
recv_message_info (struct GNUNET_MESSENGER_SrvRoom *room, struct GNUNET_MESSENGER_SrvTunnel *tunnel,
                   const struct GNUNET_MESSENGER_Message *message, const struct GNUNET_HashCode *hash);

/**
 * Handles a received peer message to link it to its origin tunnel if the peer identity matches.
 * (the peer message and the member id can potentially be linked to the tunnel)
 *
 * @param[in/out] room Room of the message
 * @param[in/out] tunnel Receiving connection
 * @param[in] message PEER-Message
 * @param[in] hash Hash of the message
 * @return #GNUNET_YES to forward the message
 */
int
recv_message_peer (struct GNUNET_MESSENGER_SrvRoom *room, struct GNUNET_MESSENGER_SrvTunnel *tunnel,
                   const struct GNUNET_MESSENGER_Message *message, const struct GNUNET_HashCode *hash);

/**
 * Handles a received request message by checking for the requested message and forwarding it back
 * if the message was found.
 * (this can also cause this peer to send a new request instead of only forwarding the received one)
 *
 * @param[in/out] room Room of the message
 * @param[in/out] tunnel Receiving connection
 * @param[in] message REQUEST-Message
 * @param[in] hash Hash of the message
 * @return #GNUNET_YES or #GNUNET_NO depending on required forwarding
 */
int
recv_message_request (struct GNUNET_MESSENGER_SrvRoom *room, struct GNUNET_MESSENGER_SrvTunnel *tunnel,
                      const struct GNUNET_MESSENGER_Message *message, const struct GNUNET_HashCode *hash);

#endif //GNUNET_SERVICE_MESSENGER_MESSAGE_RECV_H
