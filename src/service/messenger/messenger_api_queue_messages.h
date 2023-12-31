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
 * @file src/messenger/messenger_api_queue_messages.h
 * @brief messenger api: client implementation of GNUnet MESSENGER service
 */

#ifndef GNUNET_MESSENGER_API_QUEUE_MESSAGES_H
#define GNUNET_MESSENGER_API_QUEUE_MESSAGES_H

#include "platform.h"
#include "gnunet_identity_service.h"
#include "gnunet_util_lib.h"

#include "messenger_api_message.h"

struct GNUNET_MESSENGER_QueueMessage
{
  struct GNUNET_MESSENGER_QueueMessage *prev;
  struct GNUNET_MESSENGER_QueueMessage *next;

  struct GNUNET_CRYPTO_PrivateKey sender;
  struct GNUNET_MESSENGER_Message *message;
};

struct GNUNET_MESSENGER_QueueMessages
{
  struct GNUNET_MESSENGER_QueueMessage *head;
  struct GNUNET_MESSENGER_QueueMessage *tail;
};

/**
 * Initializes queue of messages as empty queue.
 *
 * @param[out] messages Queue of messages
 */
void
init_queue_messages (struct GNUNET_MESSENGER_QueueMessages *messages);

/**
 * Clears the queue of messages.
 *
 * @param[in,out] messages Queue of messages
 */
void
clear_queue_messages (struct GNUNET_MESSENGER_QueueMessages *messages);

/**
 * Adds a specific <i>message</i> to the end or the beginning of
 * the queue depending on its <i>priority</i>.
 *
 * @param[in,out] messages Queue of messages
 * @param[in] sender Private sender key
 * @param[in] message Message
 * @param[in] priority Whether the message has priority
 */
void
enqueue_to_messages (struct GNUNET_MESSENGER_QueueMessages *messages,
                     const struct GNUNET_CRYPTO_PrivateKey *sender,
                     const struct GNUNET_MESSENGER_Message *message,
                     enum GNUNET_GenericReturnValue priority);

/**
 * Remove the message from the front of the queue and returns it.
 *
 * @param[in,out] messages Queue of messages
 * @param[out] sender Private sender key
 * @return Message from front or NULL
 */
struct GNUNET_MESSENGER_Message*
dequeue_from_messages (struct GNUNET_MESSENGER_QueueMessages *messages,
                       struct GNUNET_CRYPTO_PrivateKey *sender);

#endif //GNUNET_MESSENGER_API_QUEUE_MESSAGES_H
