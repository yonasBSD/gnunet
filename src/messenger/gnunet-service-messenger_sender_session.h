/*
   This file is part of GNUnet.
   Copyright (C) 2023 GNUnet e.V.

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
 * @file src/messenger/gnunet-service-messenger_sender_session.h
 * @brief GNUnet MESSENGER service
 */

#ifndef GNUNET_SERVICE_MESSENGER_SENDER_SESSION_H
#define GNUNET_SERVICE_MESSENGER_SENDER_SESSION_H

#include "platform.h"
#include "gnunet_util_lib.h"

#include "gnunet-service-messenger_member_session.h"

struct GNUNET_MESSENGER_SenderSession
{
  union
  {
    struct GNUNET_MESSENGER_MemberSession *member;
    struct GNUNET_PeerIdentity *peer;
  };
};

#endif /* GNUNET_SERVICE_MESSENGER_SENDER_SESSION_H */
