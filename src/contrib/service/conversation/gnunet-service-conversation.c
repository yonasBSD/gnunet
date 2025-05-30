/*
   This file is part of GNUnet.
   Copyright (C) 2013, 2016, 2017 GNUnet e.V.

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
 * @file conversation/gnunet-service-conversation.c
 * @brief conversation service implementation
 * @author Simon Dieterle
 * @author Andreas Fuchs
 * @author Christian Grothoff
 */
#include "platform.h"
#include "gnunet_util_lib.h"
#include "gnunet_protocols.h"
#include "gnunet_signatures.h"
#include "gnunet_cadet_service.h"
#include "conversation.h"


/**
 * How long is our signature on a call valid?  Needs to be long enough for time zone
 * differences and network latency to not matter.  No strong need for it to be short,
 * but we simply like all signatures to eventually expire.
 */
#define RING_TIMEOUT GNUNET_TIME_UNIT_DAYS


/**
 * A line connects a local client with a cadet channel (or, if it is an
 * open line, is waiting for a cadet channel).
 */
struct Line;

/**
 * The possible connection status
 */
enum ChannelStatus
{
  /**
   * We just got the connection, but no introduction yet.
   */
  CS_CALLEE_INIT,

  /**
   * Our phone is ringing, waiting for the client to pick up.
   */
  CS_CALLEE_RINGING,

  /**
   * We are talking!
   */
  CS_CALLEE_CONNECTED,

  /**
   * We're in shutdown, sending hangup messages before cleaning up.
   */
  CS_CALLEE_SHUTDOWN,

  /**
   * We are waiting for the phone to be picked up.
   */
  CS_CALLER_CALLING,

  /**
   * We are talking!
   */
  CS_CALLER_CONNECTED,

  /**
   * We're in shutdown, sending hangup messages before cleaning up.
   */
  CS_CALLER_SHUTDOWN
};


/**
 * A `struct Channel` represents a cadet channel, which is a P2P
 * connection to another conversation service.  Multiple channels can
 * be attached the the same `struct Line`, which represents a local
 * client.  We keep them in a linked list.
 */
struct Channel
{
  /**
   * This is a DLL.
   */
  struct Channel *next;

  /**
   * This is a DLL.
   */
  struct Channel *prev;

  /**
   * Line associated with the channel.
   */
  struct Line *line;

  /**
   * Handle for the channel.
   */
  struct GNUNET_CADET_Channel *channel;

  /**
   * Message queue for control messages
   */
  struct GNUNET_MQ_Handle *mq;

  /**
   * Temporary buffer for audio data in the @e mq.
   */
  struct GNUNET_MQ_Envelope *env;

  /**
   * Channel identifier we use for this call with the client.
   */
  uint32_t cid;

  /**
   * Current status of this line.
   */
  enum ChannelStatus status;

  /**
   * #GNUNET_YES if the channel was suspended by the other peer.
   */
  int8_t suspended_remote;

  /**
   * #GNUNET_YES if the channel was suspended by the local client.
   */
  int8_t suspended_local;
};


/**
 * A `struct Line` connects a local client with cadet channels.
 */
struct Line
{
  /**
   * This is a DLL.
   */
  struct Channel *channel_head;

  /**
   * This is a DLL.
   */
  struct Channel *channel_tail;

  /**
   * Handle to the line client.
   */
  struct GNUNET_SERVICE_Client *client;

  /**
   * Message queue for @e client.
   */
  struct GNUNET_MQ_Handle *mq;

  /**
   * Our open port.
   */
  struct GNUNET_CADET_Port *port;

  /**
   * Port number we are listening on (to verify signatures).
   * Only valid if @e port is non-NULL.
   */
  struct GNUNET_HashCode line_port;

  /**
   * Generator for channel IDs.
   */
  uint32_t cid_gen;
};


/**
 * Our configuration.
 */
static const struct GNUNET_CONFIGURATION_Handle *cfg;

/**
 * Handle for cadet
 */
static struct GNUNET_CADET_Handle *cadet;

/**
 * Identity of this peer.
 */
static struct GNUNET_PeerIdentity my_identity;


/**
 * Given a @a cid, find the corresponding channel given
 * a @a line.
 *
 * @param line a line to search
 * @param cid what to search for
 * @return NULL for not found
 */
static struct Channel *
find_channel_by_line (struct Line *line, uint32_t cid)
{
  for (struct Channel *ch = line->channel_head; NULL != ch; ch = ch->next)
    if (cid == ch->cid)
      return ch;
  return NULL;
}


/**
 * Function to handle a pickup request message from the client
 *
 * @param cls the `struct Line` of the client from which the message is
 * @param msg the message from the client
 */
static void
handle_client_pickup_message (void *cls,
                              const struct ClientPhonePickupMessage *msg)
{
  struct Line *line = cls;
  struct CadetPhonePickupMessage *mppm;
  struct GNUNET_MQ_Envelope *env;
  struct Channel *ch;

  if (NULL == line->port)
  {
    /* we never opened the port, bad client! */
    GNUNET_break_op (0);
    GNUNET_SERVICE_client_drop (line->client);
    return;
  }
  for (ch = line->channel_head; NULL != ch; ch = ch->next)
    if (msg->cid == ch->cid)
      break;
  if (NULL == ch)
  {
    /* could have been destroyed asynchronously, ignore message */
    GNUNET_log (GNUNET_ERROR_TYPE_DEBUG, "Channel %u not found\n", msg->cid);
    GNUNET_SERVICE_client_continue (line->client);
    return;
  }
  switch (ch->status)
  {
  case CS_CALLEE_INIT:
    GNUNET_break (0);
    GNUNET_SERVICE_client_drop (line->client);
    return;

  case CS_CALLEE_RINGING:
    ch->status = CS_CALLEE_CONNECTED;
    break;

  case CS_CALLEE_CONNECTED:
    GNUNET_break (0);
    GNUNET_SERVICE_client_drop (line->client);
    return;

  case CS_CALLEE_SHUTDOWN:
    GNUNET_log (GNUNET_ERROR_TYPE_DEBUG,
                "Ignoring client's PICKUP message, line is in SHUTDOWN\n");
    break;

  case CS_CALLER_CALLING:
  case CS_CALLER_CONNECTED:
  case CS_CALLER_SHUTDOWN:
    GNUNET_break (0);
    GNUNET_SERVICE_client_drop (line->client);
    return;
  }
  GNUNET_break (CS_CALLEE_CONNECTED == ch->status);
  GNUNET_log (GNUNET_ERROR_TYPE_DEBUG, "Sending PICK_UP message to cadet\n");
  env =
    GNUNET_MQ_msg (mppm, GNUNET_MESSAGE_TYPE_CONVERSATION_CADET_PHONE_PICK_UP);
  GNUNET_MQ_send (ch->mq, env);
  GNUNET_SERVICE_client_continue (line->client);
}


/**
 * Channel went down, notify client and free data
 * structure.
 *
 * @param ch channel that went down
 */
static void
clean_up_channel (struct Channel *ch)
{
  struct Line *line = ch->line;
  struct GNUNET_MQ_Envelope *env;
  struct ClientPhoneHangupMessage *hup;

  switch (ch->status)
  {
  case CS_CALLEE_INIT:
  case CS_CALLEE_SHUTDOWN:
  case CS_CALLER_SHUTDOWN:
    break;

  case CS_CALLEE_RINGING:
  case CS_CALLEE_CONNECTED:
  case CS_CALLER_CALLING:
  case CS_CALLER_CONNECTED:
    if (NULL != line)
    {
      env =
        GNUNET_MQ_msg (hup, GNUNET_MESSAGE_TYPE_CONVERSATION_CS_PHONE_HANG_UP);
      hup->cid = ch->cid;
      GNUNET_MQ_send (line->mq, env);
    }
    break;
  }
  if (NULL != line)
    GNUNET_CONTAINER_DLL_remove (line->channel_head, line->channel_tail, ch);
  GNUNET_free (ch);
}


/**
 * Destroy a channel.
 *
 * @param ch channel to destroy.
 */
static void
destroy_line_cadet_channels (struct Channel *ch)
{
  GNUNET_log (GNUNET_ERROR_TYPE_DEBUG, "Destroying cadet channels\n");
  if (NULL != ch->channel)
  {
    GNUNET_CADET_channel_destroy (ch->channel);
    ch->channel = NULL;
  }
  clean_up_channel (ch);
}


/**
 * We are done signalling shutdown to the other peer.  Close down
 * the channel.
 *
 * @param cls the `struct Channel` to reset/terminate
 */
static void
mq_done_finish_caller_shutdown (void *cls)
{
  struct Channel *ch = cls;

  switch (ch->status)
  {
  case CS_CALLEE_INIT:
    GNUNET_break (0);
    break;

  case CS_CALLEE_RINGING:
    GNUNET_break (0);
    break;

  case CS_CALLEE_CONNECTED:
    GNUNET_break (0);
    break;

  case CS_CALLEE_SHUTDOWN:
    destroy_line_cadet_channels (ch);
    break;

  case CS_CALLER_CALLING:
    GNUNET_break (0);
    break;

  case CS_CALLER_CONNECTED:
    GNUNET_break (0);
    break;

  case CS_CALLER_SHUTDOWN:
    destroy_line_cadet_channels (ch);
    break;
  }
}


/**
 * Function to handle a hangup request message from the client
 *
 * @param cls the `struct Line` the hangup is for
 * @param msg the message from the client
 */
static void
handle_client_hangup_message (void *cls,
                              const struct ClientPhoneHangupMessage *msg)
{
  struct Line *line = cls;
  struct GNUNET_MQ_Envelope *e;
  struct CadetPhoneHangupMessage *mhum;
  struct Channel *ch;

  for (ch = line->channel_head; NULL != ch; ch = ch->next)
    if (msg->cid == ch->cid)
      break;
  if (NULL == ch)
  {
    /* could have been destroyed asynchronously, ignore message */
    GNUNET_log (GNUNET_ERROR_TYPE_DEBUG, "Channel %u not found\n", msg->cid);
    GNUNET_SERVICE_client_continue (line->client);
    return;
  }
  GNUNET_log (GNUNET_ERROR_TYPE_DEBUG,
              "Received HANGUP for channel %u which is in state %d\n",
              msg->cid,
              ch->status);
  switch (ch->status)
  {
  case CS_CALLEE_INIT:
    GNUNET_break (0);
    GNUNET_SERVICE_client_drop (line->client);
    return;

  case CS_CALLEE_RINGING:
    ch->status = CS_CALLEE_SHUTDOWN;
    break;

  case CS_CALLEE_CONNECTED:
    ch->status = CS_CALLEE_SHUTDOWN;
    break;

  case CS_CALLEE_SHUTDOWN:
    /* maybe the other peer closed asynchronously... */
    GNUNET_SERVICE_client_continue (line->client);
    return;

  case CS_CALLER_CALLING:
    ch->status = CS_CALLER_SHUTDOWN;
    break;

  case CS_CALLER_CONNECTED:
    ch->status = CS_CALLER_SHUTDOWN;
    break;

  case CS_CALLER_SHUTDOWN:
    /* maybe the other peer closed asynchronously... */
    GNUNET_SERVICE_client_continue (line->client);
    return;
  }
  GNUNET_log (GNUNET_ERROR_TYPE_DEBUG, "Sending HANG_UP message via cadet\n");
  e =
    GNUNET_MQ_msg (mhum, GNUNET_MESSAGE_TYPE_CONVERSATION_CADET_PHONE_HANG_UP);
  GNUNET_MQ_notify_sent (e, &mq_done_finish_caller_shutdown, ch);
  GNUNET_MQ_send (ch->mq, e);
  GNUNET_SERVICE_client_continue (line->client);
}


/**
 * Function to handle a suspend request message from the client
 *
 * @param cls the `struct Line` the message is about
 * @param msg the message from the client
 */
static void
handle_client_suspend_message (void *cls,
                               const struct ClientPhoneSuspendMessage *msg)
{
  struct Line *line = cls;
  struct GNUNET_MQ_Envelope *e;
  struct CadetPhoneSuspendMessage *mhum;
  struct Channel *ch;

  for (ch = line->channel_head; NULL != ch; ch = ch->next)
    if (msg->cid == ch->cid)
      break;
  if (NULL == ch)
  {
    /* could have been destroyed asynchronously, ignore message */
    GNUNET_log (GNUNET_ERROR_TYPE_DEBUG, "Channel %u not found\n", msg->cid);
    GNUNET_SERVICE_client_continue (line->client);
    return;
  }
  if (GNUNET_YES == ch->suspended_local)
  {
    GNUNET_break (0);
    GNUNET_SERVICE_client_drop (line->client);
    return;
  }
  GNUNET_log (GNUNET_ERROR_TYPE_DEBUG,
              "Received SUSPEND for channel %u which is in state %d\n",
              msg->cid,
              ch->status);
  switch (ch->status)
  {
  case CS_CALLEE_INIT:
    GNUNET_break (0);
    GNUNET_SERVICE_client_drop (line->client);
    return;

  case CS_CALLEE_RINGING:
    GNUNET_break (0);
    GNUNET_SERVICE_client_drop (line->client);
    return;

  case CS_CALLEE_CONNECTED:
    ch->suspended_local = GNUNET_YES;
    break;

  case CS_CALLEE_SHUTDOWN:
    /* maybe the other peer closed asynchronously... */
    GNUNET_SERVICE_client_continue (line->client);
    return;

  case CS_CALLER_CALLING:
    GNUNET_break (0);
    GNUNET_SERVICE_client_drop (line->client);
    return;

  case CS_CALLER_CONNECTED:
    ch->suspended_local = GNUNET_YES;
    break;

  case CS_CALLER_SHUTDOWN:
    /* maybe the other peer closed asynchronously... */
    GNUNET_SERVICE_client_continue (line->client);
    return;
  }
  GNUNET_log (GNUNET_ERROR_TYPE_DEBUG, "Sending SUSPEND message via cadet\n");
  e =
    GNUNET_MQ_msg (mhum, GNUNET_MESSAGE_TYPE_CONVERSATION_CADET_PHONE_SUSPEND);
  GNUNET_MQ_send (ch->mq, e);
  GNUNET_SERVICE_client_continue (line->client);
}


/**
 * Function to handle a resume request message from the client
 *
 * @param cls the `struct Line` the message is about
 * @param msg the message from the client
 */
static void
handle_client_resume_message (void *cls,
                              const struct ClientPhoneResumeMessage *msg)
{
  struct Line *line = cls;
  struct GNUNET_MQ_Envelope *e;
  struct CadetPhoneResumeMessage *mhum;
  struct Channel *ch;

  for (ch = line->channel_head; NULL != ch; ch = ch->next)
    if (msg->cid == ch->cid)
      break;
  if (NULL == ch)
  {
    /* could have been destroyed asynchronously, ignore message */
    GNUNET_log (GNUNET_ERROR_TYPE_DEBUG, "Channel %u not found\n", msg->cid);
    GNUNET_SERVICE_client_continue (line->client);
    return;
  }
  if (GNUNET_YES != ch->suspended_local)
  {
    GNUNET_break (0);
    GNUNET_SERVICE_client_drop (line->client);
    return;
  }
  GNUNET_log (GNUNET_ERROR_TYPE_DEBUG,
              "Received RESUME for channel %u which is in state %d\n",
              msg->cid,
              ch->status);
  switch (ch->status)
  {
  case CS_CALLEE_INIT:
    GNUNET_break (0);
    GNUNET_SERVICE_client_drop (line->client);
    return;

  case CS_CALLEE_RINGING:
    GNUNET_break (0);
    GNUNET_SERVICE_client_drop (line->client);
    return;

  case CS_CALLEE_CONNECTED:
    ch->suspended_local = GNUNET_NO;
    break;

  case CS_CALLEE_SHUTDOWN:
    /* maybe the other peer closed asynchronously... */
    GNUNET_SERVICE_client_continue (line->client);
    return;

  case CS_CALLER_CALLING:
    GNUNET_break (0);
    GNUNET_SERVICE_client_drop (line->client);
    return;

  case CS_CALLER_CONNECTED:
    ch->suspended_local = GNUNET_NO;
    break;

  case CS_CALLER_SHUTDOWN:
    /* maybe the other peer closed asynchronously... */
    GNUNET_SERVICE_client_drop (line->client);
    return;
  }
  GNUNET_log (GNUNET_ERROR_TYPE_DEBUG, "Sending RESUME message via cadet\n");
  e = GNUNET_MQ_msg (mhum, GNUNET_MESSAGE_TYPE_CONVERSATION_CADET_PHONE_RESUME);
  GNUNET_MQ_send (ch->mq, e);
  GNUNET_SERVICE_client_continue (line->client);
}


/**
 * Transmission of audio data via cadet channel finished.
 *
 * @param cls the `struct Channel` we are transmitting for
 */
static void
channel_audio_sent_notify (void *cls)
{
  struct Channel *ch = cls;

  ch->env = NULL;
}


/**
 * Function to check audio data from the client
 *
 * @param cls the `struct Line` the message is about
 * @param msg the message from the client
 * @return #GNUNET_OK (any data is ok)
 */
static int
check_client_audio_message (void *cls, const struct ClientAudioMessage *msg)
{
  (void) cls;
  (void) msg;
  return GNUNET_OK;
}


/**
 * Function to handle audio data from the client
 *
 * @param cls the `struct Line` the message is about
 * @param msg the message from the client
 */
static void
handle_client_audio_message (void *cls, const struct ClientAudioMessage *msg)
{
  struct Line *line = cls;
  struct CadetAudioMessage *mam;
  struct Channel *ch;
  size_t size;

  size = ntohs (msg->header.size) - sizeof(struct ClientAudioMessage);
  ch = find_channel_by_line (line, msg->cid);
  if (NULL == ch)
  {
    /* could have been destroyed asynchronously, ignore message */
    GNUNET_log (GNUNET_ERROR_TYPE_DEBUG, "Channel %u not found\n", msg->cid);
    GNUNET_SERVICE_client_continue (line->client);
    return;
  }

  switch (ch->status)
  {
  case CS_CALLEE_INIT:
  case CS_CALLEE_RINGING:
  case CS_CALLER_CALLING:
    GNUNET_break (0);
    GNUNET_SERVICE_client_drop (line->client);
    return;

  case CS_CALLEE_CONNECTED:
  case CS_CALLER_CONNECTED:
    /* common case, handled below */
    break;

  case CS_CALLEE_SHUTDOWN:
  case CS_CALLER_SHUTDOWN:
    GNUNET_log (GNUNET_ERROR_TYPE_DEBUG | GNUNET_ERROR_TYPE_BULK,
                "Cadet audio channel in shutdown; audio data dropped\n");
    GNUNET_SERVICE_client_continue (line->client);
    return;
  }
  if (GNUNET_YES == ch->suspended_local)
  {
    GNUNET_log (GNUNET_ERROR_TYPE_WARNING,
                "This channel is suspended locally\n");
    GNUNET_SERVICE_client_drop (line->client);
    return;
  }
  if (NULL != ch->env)
  {
    /* NOTE: we may want to not do this and instead combine the data */
    GNUNET_log (GNUNET_ERROR_TYPE_DEBUG,
                "Bandwidth insufficient; dropping previous audio data segment\n");
    GNUNET_MQ_send_cancel (ch->env);
    ch->env = NULL;
  }

  GNUNET_log (GNUNET_ERROR_TYPE_DEBUG,
              "Received %u bytes of AUDIO data from client CID %u\n",
              (unsigned int) size,
              msg->cid);
  ch->env = GNUNET_MQ_msg_extra (mam,
                                 size,
                                 GNUNET_MESSAGE_TYPE_CONVERSATION_CADET_AUDIO);
  GNUNET_memcpy (&mam[1], &msg[1], size);
  /* FIXME: set options for unreliable transmission */
  GNUNET_MQ_notify_sent (ch->env, &channel_audio_sent_notify, ch);
  GNUNET_MQ_send (ch->mq, ch->env);
  GNUNET_SERVICE_client_continue (line->client);
}

/**
 * Function to handle a ring message incoming over cadet
 *
 * @param cls closure, NULL
 * @param msg the incoming message
 */
static enum GNUNET_GenericReturnValue
check_cadet_ring_message (void *cls, const struct CadetPhoneRingMessage *msg)
{
  // FIXME
  return GNUNET_OK;
}

/**
 * Function to handle a ring message incoming over cadet
 *
 * @param cls closure, NULL
 * @param msg the incoming message
 */
static void
handle_cadet_ring_message (void *cls, const struct CadetPhoneRingMessage *msg)
{
  struct Channel *ch = cls;
  struct Line *line = ch->line;
  struct GNUNET_MQ_Envelope *env;
  struct ClientPhoneRingMessage *cring;
  struct CadetPhoneRingInfoPS rs;
  struct GNUNET_CRYPTO_PublicKey identity;
  struct GNUNET_CRYPTO_Signature sig;
  size_t key_len;
  size_t sig_len;
  size_t read;

  rs.purpose.purpose = htonl (GNUNET_SIGNATURE_PURPOSE_CONVERSATION_RING);
  rs.purpose.size = htonl (sizeof(struct CadetPhoneRingInfoPS));
  rs.line_port = line->line_port;
  rs.target_peer = my_identity;
  rs.expiration_time = msg->expiration_time;
  key_len = ntohl (msg->key_len);
  sig_len = ntohl (msg->sig_len);

  if ((GNUNET_SYSERR ==
       GNUNET_CRYPTO_read_public_key_from_buffer (&msg[1],
                                                    key_len,
                                                    &identity,
                                                    &read)) ||
      (read != key_len))
  {
    GNUNET_break_op (0);
    destroy_line_cadet_channels (ch);
    return;
  }
  GNUNET_CRYPTO_read_signature_from_buffer (&sig,
                                              (char*) &msg[1] + read,
                                              sig_len);
  if (GNUNET_OK !=
      GNUNET_CRYPTO_signature_verify (
        GNUNET_SIGNATURE_PURPOSE_CONVERSATION_RING,
        &rs,
        &sig,
        &identity))
  {
    GNUNET_break_op (0);
    destroy_line_cadet_channels (ch);
    return;
  }
  if (0 == GNUNET_TIME_absolute_get_remaining (
        GNUNET_TIME_absolute_ntoh (msg->expiration_time))
      .rel_value_us)
  {
    /* ancient call, replay? */
    GNUNET_break_op (0);
    /* Note that our reliance on time here is awkward; better would be
       to use a more complex challenge-response protocol against
       replay attacks.  Left for future work ;-). */
    destroy_line_cadet_channels (ch);
    return;
  }
  if (CS_CALLEE_INIT != ch->status)
  {
    GNUNET_break_op (0);
    destroy_line_cadet_channels (ch);
    return;
  }
  GNUNET_CADET_receive_done (ch->channel);
  ch->status = CS_CALLEE_RINGING;
  env = GNUNET_MQ_msg_extra (cring,
                             key_len,
                             GNUNET_MESSAGE_TYPE_CONVERSATION_CS_PHONE_RING);
  cring->cid = ch->cid;
  memcpy (&cring[1], &msg[1], key_len);
  cring->key_len = msg->key_len;
  GNUNET_log (GNUNET_ERROR_TYPE_DEBUG,
              "Sending RING message to client. CID is %u\n",
              (unsigned int) ch->cid);
  GNUNET_MQ_send (line->mq, env);
}


/**
 * Function to handle a hangup message incoming over cadet
 *
 * @param cls closure, our `struct Channel *`
 * @param message the incoming message
 */
static void
handle_cadet_hangup_message (void *cls,
                             const struct CadetPhoneHangupMessage *message)
{
  struct Channel *ch = cls;
  struct Line *line = ch->line;
  struct GNUNET_MQ_Envelope *env;
  struct ClientPhoneHangupMessage *hup;
  enum ChannelStatus status;
  uint32_t cid;

  (void) message;
  GNUNET_CADET_receive_done (ch->channel);
  cid = ch->cid;
  status = ch->status;
  destroy_line_cadet_channels (ch);
  switch (status)
  {
  case CS_CALLEE_INIT:
    GNUNET_break_op (0);
    return;

  case CS_CALLEE_RINGING:
  case CS_CALLEE_CONNECTED:
    break;

  case CS_CALLEE_SHUTDOWN:
    return;

  case CS_CALLER_CALLING:
  case CS_CALLER_CONNECTED:
    break;

  case CS_CALLER_SHUTDOWN:
    return;
  }
  GNUNET_log (GNUNET_ERROR_TYPE_DEBUG, "Sending HANG UP message to client\n");
  env = GNUNET_MQ_msg (hup, GNUNET_MESSAGE_TYPE_CONVERSATION_CS_PHONE_HANG_UP);
  hup->cid = cid;
  GNUNET_MQ_send (line->mq, env);
}


/**
 * Function to handle a pickup message incoming over cadet
 *
 * @param cls closure, our `struct Channel *`
 * @param message the incoming message
 */
static void
handle_cadet_pickup_message (void *cls,
                             const struct CadetPhonePickupMessage *message)
{
  struct Channel *ch = cls;
  struct Line *line = ch->line;
  struct GNUNET_MQ_Envelope *env;
  struct ClientPhonePickedupMessage *pick;

  (void) message;
  GNUNET_CADET_receive_done (ch->channel);
  switch (ch->status)
  {
  case CS_CALLEE_INIT:
  case CS_CALLEE_RINGING:
  case CS_CALLEE_CONNECTED:
    GNUNET_break_op (0);
    destroy_line_cadet_channels (ch);
    return;

  case CS_CALLEE_SHUTDOWN:
    GNUNET_break_op (0);
    destroy_line_cadet_channels (ch);
    return;

  case CS_CALLER_CALLING:
    ch->status = CS_CALLER_CONNECTED;
    break;

  case CS_CALLER_CONNECTED:
    GNUNET_break_op (0);
    return;

  case CS_CALLER_SHUTDOWN:
    GNUNET_break_op (0);
    mq_done_finish_caller_shutdown (ch);
    return;
  }
  GNUNET_log (GNUNET_ERROR_TYPE_DEBUG, "Sending PICKED UP message to client\n");
  env =
    GNUNET_MQ_msg (pick, GNUNET_MESSAGE_TYPE_CONVERSATION_CS_PHONE_PICKED_UP);
  pick->cid = ch->cid;
  GNUNET_MQ_send (line->mq, env);
}


/**
 * Function to handle a suspend message incoming over cadet
 *
 * @param cls closure, our `struct Channel *`
 * @param message the incoming message
 */
static void
handle_cadet_suspend_message (void *cls,
                              const struct CadetPhoneSuspendMessage *message)
{
  struct Channel *ch = cls;
  struct Line *line = ch->line;
  struct GNUNET_MQ_Envelope *env;
  struct ClientPhoneSuspendMessage *suspend;

  (void) message;
  GNUNET_CADET_receive_done (ch->channel);
  GNUNET_log (GNUNET_ERROR_TYPE_DEBUG, "Suspending channel CID: %u\n", ch->cid);
  switch (ch->status)
  {
  case CS_CALLEE_INIT:
    GNUNET_break_op (0);
    break;

  case CS_CALLEE_RINGING:
    GNUNET_break_op (0);
    break;

  case CS_CALLEE_CONNECTED:
    ch->suspended_remote = GNUNET_YES;
    break;

  case CS_CALLEE_SHUTDOWN:
    return;

  case CS_CALLER_CALLING:
    GNUNET_break_op (0);
    break;

  case CS_CALLER_CONNECTED:
    ch->suspended_remote = GNUNET_YES;
    break;

  case CS_CALLER_SHUTDOWN:
    return;
  }
  env =
    GNUNET_MQ_msg (suspend, GNUNET_MESSAGE_TYPE_CONVERSATION_CS_PHONE_SUSPEND);
  suspend->cid = ch->cid;
  GNUNET_MQ_send (line->mq, env);
}


/**
 * Function to handle a resume message incoming over cadet
 *
 * @param cls closure, our `struct Channel *`
 * @param msg the incoming message
 */
static void
handle_cadet_resume_message (void *cls,
                             const struct CadetPhoneResumeMessage *msg)
{
  struct Channel *ch = cls;
  struct Line *line;
  struct GNUNET_MQ_Envelope *env;
  struct ClientPhoneResumeMessage *resume;

  (void) msg;
  line = ch->line;
  GNUNET_CADET_receive_done (ch->channel);
  if (GNUNET_YES != ch->suspended_remote)
  {
    GNUNET_log (
      GNUNET_ERROR_TYPE_DEBUG,
      "RESUME message received for non-suspended channel, dropping channel.\n");
    destroy_line_cadet_channels (ch);
    return;
  }
  switch (ch->status)
  {
  case CS_CALLEE_INIT:
    GNUNET_break (0);
    break;

  case CS_CALLEE_RINGING:
    GNUNET_break (0);
    break;

  case CS_CALLEE_CONNECTED:
    ch->suspended_remote = GNUNET_NO;
    break;

  case CS_CALLEE_SHUTDOWN:
    return;

  case CS_CALLER_CALLING:
    GNUNET_break (0);
    break;

  case CS_CALLER_CONNECTED:
    ch->suspended_remote = GNUNET_NO;
    break;

  case CS_CALLER_SHUTDOWN:
    return;
  }
  env =
    GNUNET_MQ_msg (resume, GNUNET_MESSAGE_TYPE_CONVERSATION_CS_PHONE_RESUME);
  resume->cid = ch->cid;
  GNUNET_MQ_send (line->mq, env);
}


/**
 * Function to check an audio message incoming over cadet
 *
 * @param cls closure, our `struct Channel *`
 * @param msg the incoming message
 * @return #GNUNET_OK (always)
 */
static int
check_cadet_audio_message (void *cls, const struct CadetAudioMessage *msg)
{
  (void) cls;
  (void) msg;
  return GNUNET_OK; /* any payload is fine */
}


/**
 * Function to handle an audio message incoming over cadet
 *
 * @param cls closure, our `struct Channel *`
 * @param msg the incoming message
 */
static void
handle_cadet_audio_message (void *cls, const struct CadetAudioMessage *msg)
{
  struct Channel *ch = cls;
  size_t msize = ntohs (msg->header.size) - sizeof(struct CadetAudioMessage);
  struct GNUNET_MQ_Envelope *env;
  struct ClientAudioMessage *cam;

  GNUNET_CADET_receive_done (ch->channel);
  if ((GNUNET_YES == ch->suspended_local) ||
      (GNUNET_YES == ch->suspended_remote))
  {
    GNUNET_log (
      GNUNET_ERROR_TYPE_DEBUG,
      "Received %u bytes of AUDIO data on suspended channel CID %u; dropping\n",
      (unsigned int) msize,
      ch->cid);
    return;
  }
  GNUNET_log (GNUNET_ERROR_TYPE_DEBUG,
              "Forwarding %u bytes of AUDIO data to client CID %u\n",
              (unsigned int) msize,
              ch->cid);
  env =
    GNUNET_MQ_msg_extra (cam, msize, GNUNET_MESSAGE_TYPE_CONVERSATION_CS_AUDIO);
  cam->cid = ch->cid;
  GNUNET_memcpy (&cam[1], &msg[1], msize);
  GNUNET_MQ_send (ch->line->mq, env);
}


/**
 * Function called whenever an inbound channel is destroyed.  Should clean up
 * any associated state.
 *
 * @param cls closure (set from #GNUNET_CADET_connect)
 * @param channel connection to the other end (henceforth invalid)
 */
static void
inbound_end (void *cls, const struct GNUNET_CADET_Channel *channel)
{
  struct Channel *ch = cls;

  GNUNET_assert (channel == ch->channel);
  ch->channel = NULL;
  GNUNET_log (GNUNET_ERROR_TYPE_DEBUG,
              "Channel destroyed by CADET in state %d\n",
              ch->status);
  clean_up_channel (ch);
}

/**
 * Function to handle call request from the client
 *
 * @param cls the `struct Line` the message is about
 * @param msg the message from the client
 */
static enum GNUNET_GenericReturnValue
check_client_call_message (void *cls, const struct ClientCallMessage *msg)
{
  // FIXME
  return GNUNET_OK;
}

/**
 * Function to handle call request from the client
 *
 * @param cls the `struct Line` the message is about
 * @param msg the message from the client
 */
static void
handle_client_call_message (void *cls, const struct ClientCallMessage *msg)
{
  struct Line *line = cls;
  struct Channel *ch = GNUNET_new (struct Channel);
  struct GNUNET_MQ_MessageHandler cadet_handlers[] =
  { GNUNET_MQ_hd_fixed_size (cadet_hangup_message,
                             GNUNET_MESSAGE_TYPE_CONVERSATION_CADET_PHONE_HANG_UP,
                             struct CadetPhoneHangupMessage,
                             ch),
    GNUNET_MQ_hd_fixed_size (cadet_pickup_message,
                             GNUNET_MESSAGE_TYPE_CONVERSATION_CADET_PHONE_PICK_UP,
                             struct CadetPhonePickupMessage,
                             ch),
    GNUNET_MQ_hd_fixed_size (cadet_suspend_message,
                             GNUNET_MESSAGE_TYPE_CONVERSATION_CADET_PHONE_SUSPEND,
                             struct CadetPhoneSuspendMessage,
                             ch),
    GNUNET_MQ_hd_fixed_size (cadet_resume_message,
                             GNUNET_MESSAGE_TYPE_CONVERSATION_CADET_PHONE_RESUME,
                             struct CadetPhoneResumeMessage,
                             ch),
    GNUNET_MQ_hd_var_size (cadet_audio_message,
                           GNUNET_MESSAGE_TYPE_CONVERSATION_CADET_AUDIO,
                           struct CadetAudioMessage,
                           ch),
    GNUNET_MQ_handler_end () };
  struct GNUNET_MQ_Envelope *e;
  struct CadetPhoneRingMessage *ring;
  struct CadetPhoneRingInfoPS rs;
  struct GNUNET_CRYPTO_PrivateKey caller_id;
  struct GNUNET_CRYPTO_PublicKey caller_id_pub;
  struct GNUNET_CRYPTO_Signature sig;
  ssize_t written;
  size_t key_len;
  size_t pkey_len;
  size_t sig_len;
  size_t read;

  line->line_port = msg->line_port;
  rs.purpose.purpose = htonl (GNUNET_SIGNATURE_PURPOSE_CONVERSATION_RING);
  rs.purpose.size = htonl (sizeof(struct CadetPhoneRingInfoPS));
  rs.line_port = line->line_port;
  rs.target_peer = msg->target;
  rs.expiration_time =
    GNUNET_TIME_absolute_hton (GNUNET_TIME_relative_to_absolute (RING_TIMEOUT));
  key_len = ntohl (msg->key_len);
  if (GNUNET_SYSERR ==
      GNUNET_CRYPTO_read_private_key_from_buffer (&msg[1],
                                                    key_len,
                                                    &caller_id,
                                                    &read))
  {
    GNUNET_break_op (0);
    GNUNET_free (ch);
    GNUNET_SERVICE_client_drop (line->client);
    return;
  }
  ch->line = line;
  GNUNET_CONTAINER_DLL_insert (line->channel_head, line->channel_tail, ch);
  ch->status = CS_CALLER_CALLING;
  ch->channel = GNUNET_CADET_channel_create (cadet,
                                             ch,
                                             &msg->target,
                                             &msg->line_port,
                                             NULL,
                                             &inbound_end,
                                             cadet_handlers);
  ch->mq = GNUNET_CADET_get_mq (ch->channel);
  GNUNET_assert (read == key_len);
  GNUNET_CRYPTO_sign (&caller_id, &rs, &sig);
  sig_len = GNUNET_CRYPTO_signature_get_length (&sig);
  GNUNET_CRYPTO_key_get_public (&caller_id, &caller_id_pub);
  pkey_len = GNUNET_CRYPTO_public_key_get_length (&caller_id_pub);
  e = GNUNET_MQ_msg_extra (ring, pkey_len + sig_len,
                           GNUNET_MESSAGE_TYPE_CONVERSATION_CADET_PHONE_RING);
  written = GNUNET_CRYPTO_write_public_key_to_buffer (&caller_id_pub,
                                                        &ring[1],
                                                        pkey_len);
  ring->expiration_time = rs.expiration_time;
  ring->key_len = htonl (pkey_len);
  ring->sig_len = htonl (sig_len);
  GNUNET_CRYPTO_write_signature_to_buffer (&sig,
                                             (char *) &ring[1] + written,
                                             sig_len);
  GNUNET_log (GNUNET_ERROR_TYPE_DEBUG, "Sending RING message via CADET\n");
  GNUNET_MQ_send (ch->mq, e);
  GNUNET_SERVICE_client_continue (line->client);
}


/**
 * Method called whenever another peer has added us to a channel
 * the other peer initiated.
 *
 * @param cls the `struct Line` receiving a connection
 * @param channel new handle to the channel
 * @param initiator peer that started the channel
 * @return initial channel context for the channel
 */
static void *
inbound_channel (void *cls,
                 struct GNUNET_CADET_Channel *channel,
                 const struct GNUNET_PeerIdentity *initiator)
{
  struct Line *line = cls;
  struct Channel *ch;

  (void) initiator;
  GNUNET_log (GNUNET_ERROR_TYPE_DEBUG,
              "Received incoming cadet channel on line %p\n",
              line);
  ch = GNUNET_new (struct Channel);
  ch->status = CS_CALLEE_INIT;
  ch->line = line;
  ch->channel = channel;
  ch->mq = GNUNET_CADET_get_mq (ch->channel);
  ch->cid = line->cid_gen++;
  GNUNET_CONTAINER_DLL_insert (line->channel_head, line->channel_tail, ch);
  return ch;
}


/**
 * A client connected.  Initialize the `struct Line` data structure.
 *
 * @param cls closure, NULL
 * @param client identification of the client
 * @param mq message queue for @a client
 * @return the `struct Line` for the client
 */
static void *
client_connect_cb (void *cls,
                   struct GNUNET_SERVICE_Client *client,
                   struct GNUNET_MQ_Handle *mq)
{
  struct Line *line;

  (void) cls;
  line = GNUNET_new (struct Line);
  line->client = client;
  line->mq = mq;
  return line;
}


/**
 * A client disconnected.  Remove all of its data structure entries.
 *
 * @param cls closure, NULL
 * @param client identification of the client
 * @param app_ctx our `struct Line *` for @a client
 */
static void
client_disconnect_cb (void *cls,
                      struct GNUNET_SERVICE_Client *client,
                      void *app_ctx)
{
  struct Line *line = app_ctx;
  struct Channel *chn;

  (void) cls;
  (void) client;
  GNUNET_log (GNUNET_ERROR_TYPE_DEBUG, "Client disconnected, closing line\n");
  if (NULL != line->port)
  {
    GNUNET_CADET_close_port (line->port);
    line->port = NULL;
  }
  for (struct Channel *ch = line->channel_head; NULL != ch; ch = chn)
  {
    chn = ch->next;
    ch->line = NULL;
    destroy_line_cadet_channels (ch);
  }
  GNUNET_free (line);
}


/**
 * Function to register a phone.
 *
 * @param cls the `struct Line` of the client from which the message is
 * @param msg the message from the client
 */
static void
handle_client_register_message (void *cls,
                                const struct ClientPhoneRegisterMessage *msg)
{
  struct Line *line = cls;
  struct GNUNET_MQ_MessageHandler cadet_handlers[] =
  { GNUNET_MQ_hd_var_size (cadet_ring_message,
                           GNUNET_MESSAGE_TYPE_CONVERSATION_CADET_PHONE_RING,
                           struct CadetPhoneRingMessage,
                           NULL),
    GNUNET_MQ_hd_fixed_size (cadet_hangup_message,
                             GNUNET_MESSAGE_TYPE_CONVERSATION_CADET_PHONE_HANG_UP,
                             struct CadetPhoneHangupMessage,
                             NULL),
    GNUNET_MQ_hd_fixed_size (cadet_pickup_message,
                             GNUNET_MESSAGE_TYPE_CONVERSATION_CADET_PHONE_PICK_UP,
                             struct CadetPhonePickupMessage,
                             NULL),
    GNUNET_MQ_hd_fixed_size (cadet_suspend_message,
                             GNUNET_MESSAGE_TYPE_CONVERSATION_CADET_PHONE_SUSPEND,
                             struct CadetPhoneSuspendMessage,
                             NULL),
    GNUNET_MQ_hd_fixed_size (cadet_resume_message,
                             GNUNET_MESSAGE_TYPE_CONVERSATION_CADET_PHONE_RESUME,
                             struct CadetPhoneResumeMessage,
                             NULL),
    GNUNET_MQ_hd_var_size (cadet_audio_message,
                           GNUNET_MESSAGE_TYPE_CONVERSATION_CADET_AUDIO,
                           struct CadetAudioMessage,
                           NULL),
    GNUNET_MQ_handler_end () };

  line->line_port = msg->line_port;
  line->port = GNUNET_CADET_open_port (cadet,
                                       &msg->line_port,
                                       &inbound_channel,
                                       line,
                                       NULL,
                                       &inbound_end,
                                       cadet_handlers);
  if (NULL == line->port)
  {
    GNUNET_log (GNUNET_ERROR_TYPE_WARNING,
                _ ("Could not open line, port %s already in use!\n"),
                GNUNET_h2s (&msg->line_port));
    GNUNET_SERVICE_client_drop (line->client);
    return;
  }
  GNUNET_SERVICE_client_continue (line->client);
}


/**
 * Shutdown nicely
 *
 * @param cls closure, NULL
 */
static void
do_shutdown (void *cls)
{
  (void) cls;
  if (NULL != cadet)
  {
    GNUNET_CADET_disconnect (cadet);
    cadet = NULL;
  }
}


/**
 * Main function that will be run by the scheduler.
 *
 * @param cls closure
 * @param c configuration
 * @param service service handle
 */
static void
run (void *cls,
     const struct GNUNET_CONFIGURATION_Handle *c,
     struct GNUNET_SERVICE_Handle *service)
{
  (void) cls;
  (void) service;
  cfg = c;
  GNUNET_assert (GNUNET_OK ==
                 GNUNET_CRYPTO_get_peer_identity (cfg, &my_identity));
  cadet = GNUNET_CADET_connect (cfg);
  if (NULL == cadet)
  {
    GNUNET_break (0);
    GNUNET_SCHEDULER_shutdown ();
    return;
  }
  GNUNET_SCHEDULER_add_shutdown (&do_shutdown, NULL);
}


/**
 * Define "main" method using service macro.
 */
GNUNET_SERVICE_MAIN (
  GNUNET_OS_project_data_gnunet(),
  "conversation",
  GNUNET_SERVICE_OPTION_NONE,
  &run,
  &client_connect_cb,
  &client_disconnect_cb,
  NULL,
  GNUNET_MQ_hd_fixed_size (client_register_message,
                           GNUNET_MESSAGE_TYPE_CONVERSATION_CS_PHONE_REGISTER,
                           struct ClientPhoneRegisterMessage,
                           NULL),
  GNUNET_MQ_hd_fixed_size (client_pickup_message,
                           GNUNET_MESSAGE_TYPE_CONVERSATION_CS_PHONE_PICK_UP,
                           struct ClientPhonePickupMessage,
                           NULL),
  GNUNET_MQ_hd_fixed_size (client_suspend_message,
                           GNUNET_MESSAGE_TYPE_CONVERSATION_CS_PHONE_SUSPEND,
                           struct ClientPhoneSuspendMessage,
                           NULL),
  GNUNET_MQ_hd_fixed_size (client_resume_message,
                           GNUNET_MESSAGE_TYPE_CONVERSATION_CS_PHONE_RESUME,
                           struct ClientPhoneResumeMessage,
                           NULL),
  GNUNET_MQ_hd_fixed_size (client_hangup_message,
                           GNUNET_MESSAGE_TYPE_CONVERSATION_CS_PHONE_HANG_UP,
                           struct ClientPhoneHangupMessage,
                           NULL),
  GNUNET_MQ_hd_var_size (client_call_message,
                         GNUNET_MESSAGE_TYPE_CONVERSATION_CS_PHONE_CALL,
                         struct ClientCallMessage,
                         NULL),
  GNUNET_MQ_hd_var_size (client_audio_message,
                         GNUNET_MESSAGE_TYPE_CONVERSATION_CS_AUDIO,
                         struct ClientAudioMessage,
                         NULL),
  GNUNET_MQ_handler_end ());


/* end of gnunet-service-conversation.c */
