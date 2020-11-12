/*
   This file is part of GNUnet.
   Copyright (C) 2020 GNUnet e.V.

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
 * @file src/messenger/gnunet-service-messenger_contact.c
 * @brief GNUnet MESSENGER service
 */

#include "gnunet-service-messenger_contact.h"

struct GNUNET_MESSENGER_SrvContact*
create_contact (const struct GNUNET_IDENTITY_PublicKey *key)
{
  struct GNUNET_MESSENGER_SrvContact *contact = GNUNET_new(struct GNUNET_MESSENGER_SrvContact);

  contact->name = NULL;
  contact->rc = 0;

  GNUNET_memcpy(&(contact->public_key), key, sizeof(contact->public_key));

  return contact;
}

void
destroy_contact (struct GNUNET_MESSENGER_SrvContact *contact)
{
  if (contact->name)
    GNUNET_free(contact->name);

  GNUNET_free(contact);
}

const char*
get_contact_name (const struct GNUNET_MESSENGER_SrvContact *contact)
{
  return contact->name;
}

void
set_contact_name (struct GNUNET_MESSENGER_SrvContact *contact, const char *name)
{
  GNUNET_assert(name);

  if (contact->name)
    GNUNET_free(contact->name);

  contact->name = GNUNET_strdup(name);
}

const struct GNUNET_IDENTITY_PublicKey*
get_contact_key (const struct GNUNET_MESSENGER_SrvContact *contact)
{
  return &(contact->public_key);
}

void
increase_contact_rc (struct GNUNET_MESSENGER_SrvContact *contact)
{
  contact->rc++;
}

int
decrease_contact_rc (struct GNUNET_MESSENGER_SrvContact *contact)
{
  if (contact->rc > 0)
    contact->rc--;

  return contact->rc ? GNUNET_NO : GNUNET_YES;
}

const struct GNUNET_HashCode*
get_contact_id_from_key (const struct GNUNET_MESSENGER_SrvContact *contact)
{
  static struct GNUNET_HashCode id;

  GNUNET_CRYPTO_hash (&(contact->public_key), sizeof(contact->public_key), &id);

  return &id;
}
