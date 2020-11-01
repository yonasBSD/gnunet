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
 * @file src/messenger/gnunet-service-messenger_room.c
 * @brief GNUnet MESSENGER service
 */

#include "gnunet-service-messenger_room.h"

#include "gnunet-service-messenger_member.h"
#include "gnunet-service-messenger_member_session.h"

#include "gnunet-service-messenger_message_kind.h"
#include "gnunet-service-messenger_message_handle.h"
#include "gnunet-service-messenger_message_send.h"

#include "gnunet-service-messenger_operation.h"

#include "gnunet-service-messenger.h"
#include "gnunet-service-messenger_service.h"
#include "gnunet-service-messenger_tunnel.h"
#include "messenger_api_util.h"

static void
idle_request_room_messages (void *cls);

struct GNUNET_MESSENGER_SrvRoom*
create_room (struct GNUNET_MESSENGER_SrvHandle *handle, const struct GNUNET_HashCode *key)
{
  GNUNET_assert((handle) && (key));

  struct GNUNET_MESSENGER_SrvRoom *room = GNUNET_new(struct GNUNET_MESSENGER_SrvRoom);

  room->service = handle->service;
  room->host = handle;
  room->port = NULL;

  GNUNET_memcpy(&(room->key), key, sizeof(struct GNUNET_HashCode));

  room->tunnels = GNUNET_CONTAINER_multipeermap_create (8, GNUNET_NO);

  init_member_store(get_room_member_store(room), room);
  init_message_store (get_room_message_store(room));
  init_operation_store(get_room_operation_store(room), room);

  init_list_tunnels (&(room->basement));
  init_list_messages (&(room->last_messages));

  room->peer_message = NULL;

  init_list_messages (&(room->handling));
  room->idle = NULL;

  if (room->service->dir)
    load_room (room);

  room->idle = GNUNET_SCHEDULER_add_with_priority (GNUNET_SCHEDULER_PRIORITY_IDLE, idle_request_room_messages, room);

  return room;
}

static int
iterate_destroy_tunnels (void *cls, const struct GNUNET_PeerIdentity *key, void *value)
{
  struct GNUNET_MESSENGER_SrvTunnel *tunnel = value;
  destroy_tunnel (tunnel);
  return GNUNET_YES;
}

static void
handle_room_messages (struct GNUNET_MESSENGER_SrvRoom *room);

void
destroy_room (struct GNUNET_MESSENGER_SrvRoom *room)
{
  GNUNET_assert(room);

  if (room->idle)
  {
    GNUNET_SCHEDULER_cancel (room->idle);

    room->idle = NULL;
  }

  if (room->port)
    GNUNET_CADET_close_port (room->port);

  GNUNET_CONTAINER_multipeermap_iterate (room->tunnels, iterate_destroy_tunnels, NULL);

  handle_room_messages (room);

  if (room->service->dir)
    save_room (room);

  clear_member_store (get_room_member_store(room));
  clear_message_store (get_room_message_store(room));
  clear_operation_store(get_room_operation_store(room));

  GNUNET_CONTAINER_multipeermap_destroy (room->tunnels);

  clear_list_tunnels (&(room->basement));
  clear_list_messages (&(room->last_messages));

  if (room->peer_message)
    GNUNET_free(room->peer_message);

  GNUNET_free(room);
}

struct GNUNET_MESSENGER_MemberStore*
get_room_member_store (struct GNUNET_MESSENGER_SrvRoom *room)
{
  GNUNET_assert(room);

  return &(room->member_store);
}

struct GNUNET_MESSENGER_MessageStore*
get_room_message_store (struct GNUNET_MESSENGER_SrvRoom *room)
{
  GNUNET_assert(room);

  return &(room->message_store);
}

struct GNUNET_MESSENGER_OperationStore*
get_room_operation_store (struct GNUNET_MESSENGER_SrvRoom *room)
{
  GNUNET_assert(room);

  return &(room->operation_store);
}

const struct GNUNET_ShortHashCode*
get_room_host_id (const struct GNUNET_MESSENGER_SrvRoom *room)
{
  GNUNET_assert(room);

  return get_handle_member_id (room->host, get_room_key(room));
}

void
change_room_host_id (struct GNUNET_MESSENGER_SrvRoom *room, const struct GNUNET_ShortHashCode *unique_id)
{
  GNUNET_assert(room);

  change_handle_member_id (room->host, get_room_key(room), unique_id);
}

static int
send_room_info (struct GNUNET_MESSENGER_SrvRoom *room, struct GNUNET_MESSENGER_SrvHandle *handle,
                struct GNUNET_MESSENGER_SrvTunnel *tunnel)
{
  if ((!handle) || (!is_tunnel_connected (tunnel)))
    return GNUNET_NO;

  return send_tunnel_message (tunnel, handle, create_message_info (get_handle_ego (handle)));
}

static void*
callback_room_connect (void *cls, struct GNUNET_CADET_Channel *channel, const struct GNUNET_PeerIdentity *source)
{
  struct GNUNET_MESSENGER_SrvRoom *room = cls;

  struct GNUNET_MESSENGER_SrvTunnel *tunnel = create_tunnel (room, source);

  if ((tunnel) &&
      (GNUNET_OK != GNUNET_CONTAINER_multipeermap_put (room->tunnels, source, tunnel,
                                                       GNUNET_CONTAINER_MULTIHASHMAPOPTION_MULTIPLE)))
  {
    destroy_tunnel (tunnel);
    tunnel = NULL;
  }

  if (!tunnel)
  {
    delayed_disconnect_channel (channel);
    return NULL;
  }

  bind_tunnel(tunnel, channel);

  GNUNET_log(GNUNET_ERROR_TYPE_INFO, "New tunnel in room (%s) established to peer: %s\n",
             GNUNET_h2s(get_room_key(room)), GNUNET_i2s (source));

  if (GNUNET_YES == send_room_info (room, room->host, tunnel))
    return tunnel;

  disconnect_tunnel (tunnel);

  if (GNUNET_YES == GNUNET_CONTAINER_multipeermap_remove (room->tunnels, source, tunnel))
    destroy_tunnel (tunnel);

  return NULL;
}

static int
join_room (struct GNUNET_MESSENGER_SrvRoom *room, struct GNUNET_MESSENGER_SrvHandle *handle,
           struct GNUNET_MESSENGER_Member *member)
{
  GNUNET_assert((room) && (handle) && (member));

  GNUNET_log(GNUNET_ERROR_TYPE_DEBUG, "Joining room: %s (%s)\n", GNUNET_h2s (get_room_key (room)),
             GNUNET_sh2s (get_member_id(member)));

  if (GNUNET_OK != change_handle_member_id (handle, get_room_key(room), get_member_id(member)))
    return GNUNET_NO;

  struct GNUNET_MESSENGER_Message *message = create_message_join (get_handle_ego (handle));

  if (!message)
  {
    GNUNET_log(GNUNET_ERROR_TYPE_ERROR, "Your join message could not be created!\n");

    return GNUNET_NO;
  }

  return send_room_message (room, handle, message);
}

struct GNUNET_MESSENGER_MemberNotify
{
  struct GNUNET_MESSENGER_SrvRoom *room;
  struct GNUNET_MESSENGER_SrvHandle *handle;
  struct GNUNET_MESSENGER_MemberSession *session;
};

static int
iterate_notify_about_members (void *cls, const struct GNUNET_IDENTITY_PublicKey *public_key,
                              struct GNUNET_MESSENGER_MemberSession *session)
{
  struct GNUNET_MESSENGER_MemberNotify *notify = cls;

  if (notify->session == session)
    return GNUNET_YES;

  struct GNUNET_MESSENGER_ListMessage *element;

  for (element = session->messages.head; element; element = element->next)
  {
    const struct GNUNET_MESSENGER_Message *message = get_room_message (
        notify->room, notify->handle, &(element->hash), GNUNET_NO
    );

    if (message)
      notify_handle_message (notify->handle, get_room_key(notify->room), session, message, &(element->hash));
  }

  return GNUNET_YES;
}

static int
join_room_locally (struct GNUNET_MESSENGER_SrvRoom *room, struct GNUNET_MESSENGER_SrvHandle *handle)
{
  const struct GNUNET_ShortHashCode *member_id = get_handle_member_id (handle, get_room_key(room));

  struct GNUNET_MESSENGER_MemberStore *member_store = get_room_member_store(room);
  struct GNUNET_MESSENGER_Member *member = add_store_member(member_store, member_id);

  if (GNUNET_NO == join_room (room, handle, member))
    return GNUNET_NO;

  const struct GNUNET_MESSENGER_Ego *ego = get_handle_ego(handle);
  struct GNUNET_MESSENGER_MemberSession *session = get_member_session (member, &(ego->pub));

  if (!session)
  {
    GNUNET_log(GNUNET_ERROR_TYPE_WARNING, "A valid session is required to join a room!\n");
    return GNUNET_NO;
  }

  struct GNUNET_MESSENGER_MemberNotify notify;

  notify.room = room;
  notify.handle = handle;
  notify.session = session;

  iterate_store_members(get_room_member_store(room), iterate_notify_about_members, &notify);

  return GNUNET_YES;
}

extern int
check_tunnel_message (void *cls, const struct GNUNET_MessageHeader *header);

extern void
handle_tunnel_message (void *cls, const struct GNUNET_MessageHeader *header);

extern void
callback_tunnel_disconnect (void *cls, const struct GNUNET_CADET_Channel *channel);

int
open_room (struct GNUNET_MESSENGER_SrvRoom *room, struct GNUNET_MESSENGER_SrvHandle *handle)
{
  GNUNET_assert((room) && (handle));

  if (room->port)
    return join_room_locally (room, handle);

  struct GNUNET_CADET_Handle *cadet = get_room_cadet (room);
  const struct GNUNET_HashCode *key = get_room_key (room);

  struct GNUNET_MQ_MessageHandler handlers[] = { GNUNET_MQ_hd_var_size(tunnel_message, GNUNET_MESSAGE_TYPE_CADET_CLI,
                                                                       struct GNUNET_MessageHeader, NULL),
                                                 GNUNET_MQ_handler_end() };

  room->port = GNUNET_CADET_open_port (cadet, key, callback_room_connect, room, NULL, callback_tunnel_disconnect,
                                       handlers);

  if (room->port)
    GNUNET_log(GNUNET_ERROR_TYPE_INFO, "Port of room (%s) was opened!\n",
               GNUNET_h2s(get_room_key(room)));
  else
    GNUNET_log(GNUNET_ERROR_TYPE_WARNING, "Port of room (%s) could not be opened!\n",
               GNUNET_h2s(get_room_key(room)));

  const struct GNUNET_ShortHashCode *member_id = get_handle_member_id (handle, get_room_key(room));

  struct GNUNET_MESSENGER_MemberStore *member_store = get_room_member_store(room);
  struct GNUNET_MESSENGER_Member *member = get_store_member(member_store, member_id);

  if (member)
    goto exit_open_room;

  member = add_store_member(member_store, member_id);

  if ((GNUNET_NO == join_room (room, handle, member)) && (room->port))
  {
    GNUNET_log(GNUNET_ERROR_TYPE_WARNING, "You could not join the room, therefore it keeps closed!\n");

    GNUNET_CADET_close_port (room->port);
    room->port = NULL;

    return GNUNET_NO;
  }

exit_open_room:
  return (room->port ? send_room_message (room, handle, create_message_peer (room->service)) : GNUNET_NO);
}

int
enter_room_at (struct GNUNET_MESSENGER_SrvRoom *room, struct GNUNET_MESSENGER_SrvHandle *handle,
               const struct GNUNET_PeerIdentity *door)
{
  GNUNET_assert((room) && (handle) && (door));

  struct GNUNET_PeerIdentity peer;

  if ((GNUNET_OK == get_service_peer_identity (room->service, &peer)) &&
      (0 == GNUNET_memcmp(&peer, door)))
    return join_room_locally (room, handle);

  struct GNUNET_MESSENGER_SrvTunnel *tunnel = GNUNET_CONTAINER_multipeermap_get (room->tunnels, door);

  if (!tunnel)
  {
    tunnel = create_tunnel (room, door);

    if (GNUNET_OK != GNUNET_CONTAINER_multipeermap_put (room->tunnels, door, tunnel,
                                                        GNUNET_CONTAINER_MULTIHASHMAPOPTION_MULTIPLE))
    {
      GNUNET_log(GNUNET_ERROR_TYPE_WARNING, "You could not connect to that door!\n");
      destroy_tunnel (tunnel);
      return GNUNET_NO;
    }
  }

  if (GNUNET_SYSERR == connect_tunnel (tunnel))
  {
    GNUNET_log(GNUNET_ERROR_TYPE_ERROR, "Connection failure during entrance!\n");
    GNUNET_CONTAINER_multipeermap_remove (room->tunnels, door, tunnel);
    destroy_tunnel (tunnel);
    return GNUNET_NO;
  }

  return join_room_locally (room, handle);
}

struct GNUNET_MQ_Envelope*
pack_room_message (const struct GNUNET_MESSENGER_SrvRoom *room, const struct GNUNET_MESSENGER_SrvHandle *handle,
                   struct GNUNET_MESSENGER_Message *message, struct GNUNET_HashCode *hash, int mode)
{
  GNUNET_assert((room) && (handle) && (message) && (hash));

  message->header.timestamp = GNUNET_TIME_absolute_hton (GNUNET_TIME_absolute_get ());

  const struct GNUNET_ShortHashCode *id = get_handle_member_id (handle, get_room_key(room));

  GNUNET_assert(id);

  GNUNET_memcpy(&(message->header.sender_id), id, sizeof(struct GNUNET_ShortHashCode));

  if (room->last_messages.head)
    GNUNET_memcpy(&(message->header.previous), &(room->last_messages.head->hash), sizeof(struct GNUNET_HashCode));
  else
    memset (&(message->header.previous), 0, sizeof(struct GNUNET_HashCode));

  return pack_message (message, hash, get_handle_ego (handle), mode);
}

struct GNUNET_MESSENGER_ClosureSendRoom
{
  struct GNUNET_MESSENGER_SrvRoom *room;
  struct GNUNET_MESSENGER_SrvHandle *handle;
  struct GNUNET_MESSENGER_SrvTunnel *exclude;
  struct GNUNET_MESSENGER_Message *message;
  struct GNUNET_HashCode *hash;
  int packed;
};

static int
iterate_send_room_message (void *cls, const struct GNUNET_PeerIdentity *key, void *value)
{
  struct GNUNET_MESSENGER_SrvTunnel *tunnel = value;

  if ((!is_tunnel_connected (tunnel)) ||
      (get_tunnel_messenger_version(tunnel) < GNUNET_MESSENGER_VERSION))
    return GNUNET_YES;

  struct GNUNET_MESSENGER_ClosureSendRoom *closure = cls;

  if (tunnel == closure->exclude)
    return GNUNET_YES;

  struct GNUNET_MQ_Envelope *env = NULL;

  if (closure->packed == GNUNET_NO)
  {
    env = pack_room_message (closure->room, closure->handle, closure->message, closure->hash,
    GNUNET_MESSENGER_PACK_MODE_ENVELOPE);

    if (env)
      closure->packed = GNUNET_YES;
  }
  else
    env = pack_message (closure->message, NULL, NULL, GNUNET_MESSENGER_PACK_MODE_ENVELOPE);

  if (env)
    send_tunnel_envelope (tunnel, env, closure->hash);

  return GNUNET_YES;
}

int
update_room_message (struct GNUNET_MESSENGER_SrvRoom *room,
                     struct GNUNET_MESSENGER_Message *message, const struct GNUNET_HashCode *hash);

void
callback_room_handle_message (struct GNUNET_MESSENGER_SrvRoom *room, struct GNUNET_MESSENGER_SrvHandle *handle,
                              const struct GNUNET_MESSENGER_Message *message, const struct GNUNET_HashCode *hash);

int
send_room_message (struct GNUNET_MESSENGER_SrvRoom *room, struct GNUNET_MESSENGER_SrvHandle *handle,
                   struct GNUNET_MESSENGER_Message *message)
{
  GNUNET_assert((room) && (handle));

  if (!message)
    return GNUNET_NO;

  if (GNUNET_YES == is_message_session_bound(message))
    merge_room_last_messages(room, handle);

  GNUNET_log(GNUNET_ERROR_TYPE_INFO, "Sending message from handle with member id: %s\n",
             GNUNET_sh2s(get_handle_member_id(handle, get_room_key(room))));

  struct GNUNET_HashCode hash;
  struct GNUNET_MESSENGER_ClosureSendRoom closure;

  closure.room = room;
  closure.handle = handle;
  closure.exclude = NULL;
  closure.message = message;
  closure.hash = &hash;
  closure.packed = GNUNET_NO;

  GNUNET_CONTAINER_multipeermap_iterate (room->tunnels, iterate_send_room_message, &closure);

  if (GNUNET_NO == closure.packed)
    pack_room_message (room, handle, message, &hash, GNUNET_MESSENGER_PACK_MODE_UNKNOWN);

  const int new_message = update_room_message (room, message, &hash);

  if (GNUNET_YES != new_message)
    return GNUNET_SYSERR;

  switch (message->header.kind)
  {
  case GNUNET_MESSENGER_KIND_JOIN:
    send_message_join (room, handle, message, &hash);
    break;
  case GNUNET_MESSENGER_KIND_PEER:
    send_message_peer (room, handle, message, &hash);
    break;
  case GNUNET_MESSENGER_KIND_REQUEST:
    send_message_request (room, handle, message, &hash);
    break;
  default:
    break;
  }

  callback_room_handle_message (room, handle, message, &hash);
  return GNUNET_YES;
}

void
forward_room_message (struct GNUNET_MESSENGER_SrvRoom *room, struct GNUNET_MESSENGER_SrvTunnel *tunnel,
                      struct GNUNET_MESSENGER_Message *message, const struct GNUNET_HashCode *hash)
{
  GNUNET_assert((room) && (tunnel));

  if (!message)
    return;

  struct GNUNET_MESSENGER_ClosureSendRoom closure;
  struct GNUNET_HashCode message_hash;

  GNUNET_memcpy(&message_hash, hash, sizeof(struct GNUNET_HashCode));

  closure.room = room;
  closure.handle = NULL;
  closure.exclude = tunnel;
  closure.message = message;
  closure.hash = &message_hash;
  closure.packed = GNUNET_YES;

  GNUNET_CONTAINER_multipeermap_iterate (room->tunnels, iterate_send_room_message, &closure);
}

void
check_room_peer_status (struct GNUNET_MESSENGER_SrvRoom *room, struct GNUNET_MESSENGER_SrvTunnel *tunnel)
{
  if (!room->peer_message)
    return;

  const struct GNUNET_MESSENGER_Message *message = get_room_message(room, NULL, room->peer_message, GNUNET_NO);

  if (!message)
  {
    GNUNET_free(room->peer_message);
    room->peer_message = NULL;
    return;
  }

  struct GNUNET_MESSENGER_MemberStore *member_store = get_room_member_store(room);
  struct GNUNET_MESSENGER_Member *member = get_store_member_of(member_store, message);

  if (!member)
    goto resend_peer_message;

  struct GNUNET_MESSENGER_MemberSession *session = get_member_session_of(member, message, room->peer_message);

  if (GNUNET_YES == is_member_session_closed(session))
    goto resend_peer_message;

  if (tunnel)
    forward_tunnel_message(tunnel, message, room->peer_message);

  return;

resend_peer_message:
  if (room->host)
    send_room_message (room, room->host, create_message_peer (room->service));
}

static void
merge_room_message (struct GNUNET_MESSENGER_SrvRoom *room, struct GNUNET_MESSENGER_SrvHandle *handle,
                    const struct GNUNET_HashCode *hash)
{
  GNUNET_assert((room) && (handle) && (hash));

  send_room_message (room, handle, create_message_merge (hash));
}

void
merge_room_last_messages (struct GNUNET_MESSENGER_SrvRoom *room, struct GNUNET_MESSENGER_SrvHandle *handle)
{
  GNUNET_assert(room);

  if (!handle)
    return;

  if (!room->last_messages.head)
    return;

  while (room->last_messages.head != room->last_messages.tail)
    merge_room_message (room, handle, &(room->last_messages.tail->hash));
}

void
callback_room_deletion (struct GNUNET_MESSENGER_SrvRoom *room, const struct GNUNET_HashCode *hash)
{
  const struct GNUNET_MESSENGER_Message *message = get_room_message(room, NULL, hash, GNUNET_NO);

  if (message)
    add_to_list_messages(&(room->last_messages), &(message->header.previous));

  if (GNUNET_OK != delete_store_message (get_room_message_store(room), hash))
  {
    GNUNET_log(GNUNET_ERROR_TYPE_WARNING, "Deletion of message failed! (%s)\n", GNUNET_h2s(hash));
    return;
  }
}

void
callback_room_merge (struct GNUNET_MESSENGER_SrvRoom *room, const struct GNUNET_HashCode *hash)
{
  if (!room->host)
    return;

  merge_room_message (room, room->host, hash);
}

int
delete_room_message (struct GNUNET_MESSENGER_SrvRoom *room, struct GNUNET_MESSENGER_MemberSession *session,
                     const struct GNUNET_HashCode *hash, const struct GNUNET_TIME_Relative delay)
{
  GNUNET_assert((room) && (session) && (hash));

  const struct GNUNET_TIME_Relative forever = GNUNET_TIME_relative_get_forever_ ();

  if (0 == GNUNET_memcmp(&forever, &delay))
  {
    GNUNET_log(GNUNET_ERROR_TYPE_INFO, "Deletion is delayed forever: operation is impossible!\n");
    return GNUNET_SYSERR;
  }

  const struct GNUNET_MESSENGER_Message *message = get_room_message(room, NULL, hash, GNUNET_NO);

  if (!message)
    return GNUNET_YES;

  if (GNUNET_YES != check_member_session_history(session, hash, GNUNET_YES))
  {
    GNUNET_log(GNUNET_ERROR_TYPE_WARNING, "Unpermitted request for deletion by member (%s) of message (%s)!\n",
               GNUNET_sh2s(get_member_session_id(session)), GNUNET_h2s(hash));

    return GNUNET_NO;
  }

  struct GNUNET_MESSENGER_OperationStore *operation_store = get_room_operation_store(room);

  if (GNUNET_OK != use_store_operation(operation_store, hash, GNUNET_MESSENGER_OP_DELETE, delay))
  {
    GNUNET_log(GNUNET_ERROR_TYPE_ERROR, "Deletion has failed: operation denied!\n");
    return GNUNET_SYSERR;
  }

  return GNUNET_YES;
}

struct GNUNET_CADET_Handle*
get_room_cadet (struct GNUNET_MESSENGER_SrvRoom *room)
{
  GNUNET_assert(room);

  return room->service->cadet;
}

const struct GNUNET_HashCode*
get_room_key (const struct GNUNET_MESSENGER_SrvRoom *room)
{
  GNUNET_assert(room);

  return &(room->key);
}

const struct GNUNET_MESSENGER_SrvTunnel*
get_room_tunnel (const struct GNUNET_MESSENGER_SrvRoom *room, const struct GNUNET_PeerIdentity *peer)
{
  GNUNET_assert((room) && (peer));

  return GNUNET_CONTAINER_multipeermap_get (room->tunnels, peer);
}

const struct GNUNET_MESSENGER_Message*
get_room_message (struct GNUNET_MESSENGER_SrvRoom *room, struct GNUNET_MESSENGER_SrvHandle *handle,
                  const struct GNUNET_HashCode *hash, int request)
{
  GNUNET_assert((room) && (hash));

  const struct GNUNET_MESSENGER_Message *message = get_store_message (get_room_message_store(room), hash);

  if ((message) || (!handle) || (GNUNET_YES != request))
    return message;

  struct GNUNET_MESSENGER_OperationStore* operation_store = get_room_operation_store(room);

  if (GNUNET_OK == use_store_operation(operation_store, hash, GNUNET_MESSENGER_OP_REQUEST, GNUNET_MESSENGER_REQUEST_DELAY))
    send_room_message (room, handle, create_message_request (hash));

  return NULL;
}

void
callback_room_disconnect (struct GNUNET_MESSENGER_SrvRoom *room, void *cls)
{
  struct GNUNET_MESSENGER_SrvTunnel *tunnel = cls;

  if (!room->host)
    return;

  struct GNUNET_PeerIdentity identity;
  get_tunnel_peer_identity(tunnel, &identity);

  if ((GNUNET_YES != GNUNET_CONTAINER_multipeermap_remove (room->tunnels, &identity, tunnel)) ||
      (GNUNET_YES == GNUNET_CONTAINER_multipeermap_contains(room->tunnels, &identity)))
    return;

  if (GNUNET_YES == contains_list_tunnels (&(room->basement), &identity))
    send_room_message (room, room->host, create_message_miss (&identity));
}

int
callback_verify_room_message (struct GNUNET_MESSENGER_SrvRoom *room, void *cls,
                              struct GNUNET_MESSENGER_Message *message, struct GNUNET_HashCode *hash)
{
  if (GNUNET_MESSENGER_KIND_UNKNOWN == message->header.kind)
  {
    GNUNET_log(GNUNET_ERROR_TYPE_ERROR, "Message error: Kind is unknown! (%d)\n", message->header.kind);
    return GNUNET_SYSERR;
  }

  GNUNET_log(GNUNET_ERROR_TYPE_DEBUG, "Receiving message of kind: %s!\n",
             GNUNET_MESSENGER_name_of_kind(message->header.kind));

  return GNUNET_OK;
}

static void
idle_request_room_messages (void *cls)
{
  struct GNUNET_MESSENGER_SrvRoom *room = cls;

  room->idle = NULL;

  struct GNUNET_MESSENGER_OperationStore *operation_store = get_room_operation_store(room);

  if ((room->last_messages.head != room->last_messages.tail) &&
      (GNUNET_MESSENGER_OP_UNKNOWN == get_store_operation_type(operation_store, &(room->last_messages.tail->hash))))
    use_store_operation(
        operation_store,
        &(room->last_messages.tail->hash),
        GNUNET_MESSENGER_OP_MERGE,
        GNUNET_MESSENGER_MERGE_DELAY
    );

  room->idle = GNUNET_SCHEDULER_add_delayed_with_priority (
      GNUNET_MESSENGER_IDLE_DELAY,
      GNUNET_SCHEDULER_PRIORITY_IDLE,
      idle_request_room_messages,
      cls
  );
}

void
rebuild_room_basement_structure (struct GNUNET_MESSENGER_SrvRoom *room)
{
  GNUNET_assert(room);

  struct GNUNET_PeerIdentity peer;
  size_t src;

  if ((GNUNET_OK != get_service_peer_identity (room->service, &peer)) ||
      (!find_list_tunnels (&(room->basement), &peer, &src)))
    return;

  size_t count = count_of_tunnels (&(room->basement));

  struct GNUNET_MESSENGER_ListTunnel *element = room->basement.head;
  struct GNUNET_MESSENGER_SrvTunnel *tunnel;

  size_t dst = 0;

  while (element)
  {
    GNUNET_PEER_resolve (element->peer, &peer);

    tunnel = GNUNET_CONTAINER_multipeermap_get (room->tunnels, &peer);

    if (!tunnel)
    {
      element = remove_from_list_tunnels (&(room->basement), element);
      continue;
    }

    if (GNUNET_YES == required_connection_between (count, src, dst))
    {
      if (GNUNET_SYSERR == connect_tunnel (tunnel))
      {
        element = remove_from_list_tunnels (&(room->basement), element);
        continue;
      }
    }
    else
      disconnect_tunnel (tunnel);

    element = element->next;
    dst++;
  }
}

static void
handle_room_messages (struct GNUNET_MESSENGER_SrvRoom *room)
{
  while (room->handling.head)
  {
    struct GNUNET_MESSENGER_ListMessage *element = room->handling.head;

    const struct GNUNET_MESSENGER_Message *msg = get_room_message (room, room->host, &(element->hash), GNUNET_NO);

    if (!msg)
      goto finish_handling;

    struct GNUNET_MESSENGER_MemberStore *member_store = get_room_member_store(room);
    struct GNUNET_MESSENGER_Member *member = get_store_member_of(member_store, msg);

    if (!member)
      goto finish_handling;

    struct GNUNET_MESSENGER_MemberSession *session = get_member_session_of(member, msg, &(element->hash));

    if (session)
      handle_service_message (room->service, room, session, msg, &(element->hash));

finish_handling:
    GNUNET_CONTAINER_DLL_remove(room->handling.head, room->handling.tail, element);
    GNUNET_free(element);
  }
}

static void
remove_room_last_message (struct GNUNET_MESSENGER_SrvRoom *room, const struct GNUNET_HashCode *hash)
{
  remove_from_list_messages(&(room->last_messages), hash);

  struct GNUNET_MESSENGER_OperationStore *operation_store = get_room_operation_store(room);

  if (GNUNET_MESSENGER_OP_MERGE == get_store_operation_type(operation_store, hash))
    cancel_store_operation(operation_store, hash);
}

int
update_room_message (struct GNUNET_MESSENGER_SrvRoom *room,
                     struct GNUNET_MESSENGER_Message *message, const struct GNUNET_HashCode *hash)
{
  GNUNET_assert((room) && (message) && (hash));

  struct GNUNET_MESSENGER_OperationStore *operation_store = get_room_operation_store(room);

  const int requested = (GNUNET_MESSENGER_OP_REQUEST == get_store_operation_type(operation_store, hash)?
      GNUNET_YES : GNUNET_NO
  );

  if (GNUNET_YES == requested)
    cancel_store_operation(operation_store, hash);

  const struct GNUNET_MESSENGER_Message *old_message = get_room_message (room, NULL, hash, GNUNET_NO);

  GNUNET_log(GNUNET_ERROR_TYPE_DEBUG, "Handle a message in room (%s).\n", GNUNET_h2s (get_room_key(room)));

  if ((old_message) || (GNUNET_OK != put_store_message (get_room_message_store(room), hash, message)))
  {
    if (old_message != message)
      destroy_message(message);

    GNUNET_log(GNUNET_ERROR_TYPE_DEBUG, "Duplicate message got dropped!\n");
    return GNUNET_NO;
  }

  if ((GNUNET_YES == requested) ||
      (GNUNET_MESSENGER_KIND_INFO == message->header.kind) ||
      (GNUNET_MESSENGER_KIND_REQUEST == message->header.kind))
    return GNUNET_YES;

  if (GNUNET_MESSENGER_KIND_MERGE == message->header.kind)
    remove_room_last_message(room, &(message->body.merge.previous));
  remove_room_last_message(room, &(message->header.previous));

  add_to_list_messages (&(room->last_messages), hash);

  return GNUNET_YES;
}

struct GNUNET_MESSENGER_MemberSessionCompletion
{
  struct GNUNET_MESSENGER_MemberSessionCompletion *prev;
  struct GNUNET_MESSENGER_MemberSessionCompletion *next;

  struct GNUNET_MESSENGER_MemberSession *session;
};

struct GNUNET_MESSENGER_MemberUpdate
{
  const struct GNUNET_MESSENGER_Message *message;
  const struct GNUNET_HashCode *hash;

  struct GNUNET_MESSENGER_MemberSessionCompletion *head;
  struct GNUNET_MESSENGER_MemberSessionCompletion *tail;
};

static int
iterate_update_member_sessions (void *cls, const struct GNUNET_IDENTITY_PublicKey *public_key,
                                struct GNUNET_MESSENGER_MemberSession *session)
{
  struct GNUNET_MESSENGER_MemberUpdate *update = cls;

  update_member_session_history(session, update->message, update->hash);

  if (GNUNET_YES == is_member_session_completed(session))
  {
    struct GNUNET_MESSENGER_MemberSessionCompletion *element = GNUNET_new(
        struct GNUNET_MESSENGER_MemberSessionCompletion
    );

    element->session = session;

    GNUNET_CONTAINER_DLL_insert_tail(update->head, update->tail, element);
  }

  return GNUNET_YES;
}

void
callback_room_handle_message (struct GNUNET_MESSENGER_SrvRoom *room, struct GNUNET_MESSENGER_SrvHandle *handle,
                              const struct GNUNET_MESSENGER_Message *message, const struct GNUNET_HashCode *hash)
{
  struct GNUNET_MESSENGER_MemberStore *member_store = get_room_member_store(room);
  struct GNUNET_MESSENGER_Member *member = get_store_member_of(member_store, message);

  GNUNET_log(GNUNET_ERROR_TYPE_DEBUG, "Callback for message (%s)\n", GNUNET_h2s (hash));

  if (!member)
  {
    GNUNET_log(GNUNET_ERROR_TYPE_WARNING, "Message handling dropped: Member is missing!\n");
    return;
  }

  struct GNUNET_MESSENGER_MemberSession *session = get_member_session_of(member, message, hash);

  if (!session)
  {
    GNUNET_log(GNUNET_ERROR_TYPE_WARNING, "Message handling dropped: Session is missing!\n");
    return;
  }

  struct GNUNET_MESSENGER_MemberUpdate update;
  update.message = message;
  update.hash = hash;

  update.head = NULL;
  update.tail = NULL;

  iterate_store_members(member_store, iterate_update_member_sessions, &update);

  while (update.head)
  {
    struct GNUNET_MESSENGER_MemberSessionCompletion *element = update.head;

    remove_member_session (element->session->member, element->session);

    GNUNET_CONTAINER_DLL_remove(update.head, update.tail, element);
    GNUNET_free (element);
  }

  const int start_handle = room->handling.head ? GNUNET_NO : GNUNET_YES;

  add_to_list_messages (&(room->handling), hash);

  switch (message->header.kind)
  {
  case GNUNET_MESSENGER_KIND_JOIN:
    handle_message_join (room, session, message, hash);
    break;
  case GNUNET_MESSENGER_KIND_LEAVE:
    handle_message_leave (room, session, message, hash);
    break;
  case GNUNET_MESSENGER_KIND_NAME:
    handle_message_name (room, session, message, hash);
    break;
  case GNUNET_MESSENGER_KIND_KEY:
    handle_message_key (room, session, message, hash);
    break;
  case GNUNET_MESSENGER_KIND_PEER:
    handle_message_peer (room, session, message, hash);
    break;
  case GNUNET_MESSENGER_KIND_ID:
    handle_message_id (room, session, message, hash);
    break;
  case GNUNET_MESSENGER_KIND_MISS:
    handle_message_miss (room, session, message, hash);
    break;
  case GNUNET_MESSENGER_KIND_DELETE:
    handle_message_delete (room, session, message, hash);
    break;
  default:
    break;
  }

  if (GNUNET_YES == start_handle)
    handle_room_messages (room);
}

static void
get_room_data_subdir (struct GNUNET_MESSENGER_SrvRoom *room, char **dir)
{
  GNUNET_assert((room) && (dir));

  GNUNET_asprintf (dir, "%s%s%c%s%c", room->service->dir, "rooms", DIR_SEPARATOR, GNUNET_h2s (get_room_key(room)), DIR_SEPARATOR);
}

void
load_room (struct GNUNET_MESSENGER_SrvRoom *room)
{
  GNUNET_assert(room);

  char *room_dir;
  get_room_data_subdir (room, &room_dir);

  if (GNUNET_YES == GNUNET_DISK_directory_test (room_dir, GNUNET_YES))
  {
    load_member_store (get_room_member_store(room), room_dir);
    load_message_store (get_room_message_store(room), room_dir);
    load_operation_store(get_room_operation_store(room), room_dir);

    char *basement_file;
    GNUNET_asprintf (&basement_file, "%s%s", room_dir, "basement.list");

    load_list_tunnels(&(room->basement), basement_file);
    GNUNET_free(basement_file);

    char *last_messages_file;
    GNUNET_asprintf (&last_messages_file, "%s%s", room_dir, "last_messages.list");

    load_list_messages(&(room->last_messages), last_messages_file);
    GNUNET_free(last_messages_file);
  }

  GNUNET_free(room_dir);
}

void
save_room (struct GNUNET_MESSENGER_SrvRoom *room)
{
  GNUNET_assert(room);

  char *room_dir;
  get_room_data_subdir (room, &room_dir);

  if ((GNUNET_YES == GNUNET_DISK_directory_test (room_dir, GNUNET_NO)) ||
      (GNUNET_OK == GNUNET_DISK_directory_create (room_dir)))
  {
    save_member_store(get_room_member_store(room), room_dir);
    save_message_store (get_room_message_store(room), room_dir);
    save_operation_store(get_room_operation_store(room), room_dir);

    char *basement_file;
    GNUNET_asprintf (&basement_file, "%s%s", room_dir, "basement.list");

    save_list_tunnels(&(room->basement), basement_file);
    GNUNET_free(basement_file);

    char *last_messages_file;
    GNUNET_asprintf (&last_messages_file, "%s%s", room_dir, "last_messages.list");

    save_list_messages(&(room->last_messages), last_messages_file);
    GNUNET_free(last_messages_file);
  }

  GNUNET_free(room_dir);
}
