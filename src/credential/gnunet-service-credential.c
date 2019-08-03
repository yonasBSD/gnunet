/*
     This file is part of GNUnet.
     Copyright (C) 2011-2013 GNUnet e.V.

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
 * @file credential/gnunet-service-credential.c
 * @brief GNUnet Credential Service (main service)
 * @author Martin Schanzenbach
 */
#include "platform.h"

#include "gnunet_util_lib.h"

#include "credential.h"
#include "credential_serialization.h"
#include "gnunet_credential_service.h"
#include "gnunet_protocols.h"
#include "gnunet_signatures.h"
#include "gnunet_statistics_service.h"
#include <gnunet_dnsparser_lib.h>
#include <gnunet_gns_service.h>
#include <gnunet_gnsrecord_lib.h>
#include <gnunet_identity_service.h>
#include <gnunet_namestore_service.h>


#define GNUNET_CREDENTIAL_MAX_LENGTH 255

struct VerifyRequestHandle;

struct DelegationSetQueueEntry;


struct DelegationChainEntry
{
  /**
   * DLL
   */
  struct DelegationChainEntry *next;

  /**
   * DLL
   */
  struct DelegationChainEntry *prev;

  /**
   * The issuer
   */
  struct GNUNET_CRYPTO_EcdsaPublicKey issuer_key;

  /**
   * The subject
   */
  struct GNUNET_CRYPTO_EcdsaPublicKey subject_key;

  /**
   * The issued attribute
   */
  char *issuer_attribute;

  /**
   * The delegated attribute
   */
  char *subject_attribute;
};

/**
 * DLL for record
 */
struct CredentialRecordEntry
{
  /**
   * DLL
   */
  struct CredentialRecordEntry *next;

  /**
   * DLL
   */
  struct CredentialRecordEntry *prev;

  /**
   * Number of references in delegation chains
   */
  uint32_t refcount;

  /**
   * Payload
   */
  struct GNUNET_CREDENTIAL_Credential *credential;
};

/**
 * DLL for record
 */
struct DelegateRecordEntry
{
  /**
   * DLL
   */
  struct DelegateRecordEntry *next;

  /**
   * DLL
   */
  struct DelegateRecordEntry *prev;

  /**
   * Number of references in delegation chains
   */
  uint32_t refcount;

  /**
   * Payload
   */
  struct GNUNET_CREDENTIAL_Delegate *delegate;
};

/**
 * DLL used for delegations
 * Used for OR delegations
 */
struct DelegationQueueEntry
{
  /**
   * DLL
   */
  struct DelegationQueueEntry *next;

  /**
   * DLL
   */
  struct DelegationQueueEntry *prev;

  /**
   * Sets under this Queue
   */
  struct DelegationSetQueueEntry *set_entries_head;

  /**
   * Sets under this Queue
   */
  struct DelegationSetQueueEntry *set_entries_tail;

  /**
   * Parent set
   */
  struct DelegationSetQueueEntry *parent_set;

  /**
   * Required solutions
   */
  uint32_t required_solutions;
};

/**
 * DLL for delegation sets
 * Used for AND delegation set
 */
struct DelegationSetQueueEntry
{
  /**
   * DLL
   */
  struct DelegationSetQueueEntry *next;

  /**
   * DLL
   */
  struct DelegationSetQueueEntry *prev;

  /**
   * GNS handle
   */
  struct GNUNET_GNS_LookupRequest *lookup_request;

  /**
   * Verify handle
   */
  struct VerifyRequestHandle *handle;

  /**
   * Parent attribute delegation
   */
  struct DelegationQueueEntry *parent;

  /**
   * Issuer key
   */
  struct GNUNET_CRYPTO_EcdsaPublicKey *issuer_key;

  /**
   * Queue entries of this set
   */
  struct DelegationQueueEntry *queue_entries_head;

  /**
   * Queue entries of this set
   */
  struct DelegationQueueEntry *queue_entries_tail;

  /**
   * Parent QueueEntry
   */
  struct DelegationQueueEntry *parent_queue_entry;

  /**
   * Issuer attribute delegated to
   */
  char *issuer_attribute;

  /**
   * The current attribute to look up
   */
  char *lookup_attribute;

  /**
   * Trailing attribute context
   */
  char *attr_trailer;

  /**
   * Still to resolve delegation as string
   */
  char *unresolved_attribute_delegation;

  /**
   * The delegation chain entry
   */
  struct DelegationChainEntry *delegation_chain_entry;
};


/**
 * Handle to a lookup operation from api
 */
struct VerifyRequestHandle
{

  /**
   * We keep these in a DLL.
   */
  struct VerifyRequestHandle *next;

  /**
   * We keep these in a DLL.
   */
  struct VerifyRequestHandle *prev;

  /**
   * Handle to the requesting client
   */
  struct GNUNET_SERVICE_Client *client;

  /**
   * GNS handle
   */
  struct GNUNET_GNS_LookupRequest *lookup_request;

  /**
   * Size of delegation tree
   */
  uint32_t delegation_chain_size;

  /**
   * Children of this attribute
   */
  struct DelegationChainEntry *delegation_chain_head;

  /**
   * Children of this attribute
   */
  struct DelegationChainEntry *delegation_chain_tail;

  /**
   * Issuer public key
   */
  struct GNUNET_CRYPTO_EcdsaPublicKey issuer_key;

  /**
   * Issuer attribute
   */
  char *issuer_attribute;

  /**
   * Subject public key
   */
  struct GNUNET_CRYPTO_EcdsaPublicKey subject_key;

  /**
   * Credential DLL
   */
  struct CredentialRecordEntry *cred_chain_head;

  /**
   * Credential DLL
   */
  struct CredentialRecordEntry *cred_chain_tail;

  /**
   * Credential DLL size
   */
  uint32_t cred_chain_size;

  /**
   * Credential DLL
   */
  struct DelegateRecordEntry *del_chain_head;

  /**
   * Credential DLL
   */
  struct DelegateRecordEntry *del_chain_tail;

  /**
   * Credential DLL size
   */
  uint32_t del_chain_size;

  /**
   * Root Delegation Set
   */
  struct DelegationSetQueueEntry *root_set;

  /**
   * Current Delegation Pointer
   */
  struct DelegationQueueEntry *current_delegation;

  /**
   * request id
   */
  uint32_t request_id;

  /**
   * Pending lookups
   */
  uint64_t pending_lookups;

  /**
   * Direction of the resolution algo
   */
  enum direction resolution_algo;

  /**
   * Credential iterator
   */
  struct GNUNET_NAMESTORE_ZoneIterator *cred_collection_iter;

  /**
   * Credential iterator
   */
  struct GNUNET_NAMESTORE_QueueEntry *dele_qe;

  /**
   * Collect task
   */
  struct GNUNET_SCHEDULER_Task *collect_next_task;
};


/**
 * Head of the DLL.
 */
static struct VerifyRequestHandle *vrh_head;

/**
 * Tail of the DLL.
 */
static struct VerifyRequestHandle *vrh_tail;

/**
 * Handle to the statistics service
 */
static struct GNUNET_STATISTICS_Handle *statistics;

/**
 * Handle to GNS service.
 */
static struct GNUNET_GNS_Handle *gns;


/**
 * Handle to namestore service
 */
static struct GNUNET_NAMESTORE_Handle *namestore;

static void
cleanup_delegation_set (struct DelegationSetQueueEntry *ds_entry)
{
  struct DelegationQueueEntry *dq_entry;
  struct DelegationSetQueueEntry *child;

  if (NULL == ds_entry)
    return;

  for (dq_entry = ds_entry->queue_entries_head; NULL != dq_entry;
       dq_entry = ds_entry->queue_entries_head) {
    GNUNET_CONTAINER_DLL_remove (ds_entry->queue_entries_head,
                                 ds_entry->queue_entries_tail,
                                 dq_entry);
    for (child = dq_entry->set_entries_head; NULL != child;
         child = dq_entry->set_entries_head) {
      GNUNET_CONTAINER_DLL_remove (dq_entry->set_entries_head,
                                   dq_entry->set_entries_tail,
                                   child);
      cleanup_delegation_set (child);
    }
    GNUNET_free (dq_entry);
  }
  GNUNET_free_non_null (ds_entry->issuer_key);
  GNUNET_free_non_null (ds_entry->lookup_attribute);
  GNUNET_free_non_null (ds_entry->issuer_attribute);
  GNUNET_free_non_null (ds_entry->unresolved_attribute_delegation);
  GNUNET_free_non_null (ds_entry->attr_trailer);
  if (NULL != ds_entry->lookup_request) {
    GNUNET_GNS_lookup_cancel (ds_entry->lookup_request);
    ds_entry->lookup_request = NULL;
  }
  if (NULL != ds_entry->delegation_chain_entry) {
    GNUNET_free_non_null (ds_entry->delegation_chain_entry->subject_attribute);
    GNUNET_free_non_null (ds_entry->delegation_chain_entry->issuer_attribute);
    GNUNET_free (ds_entry->delegation_chain_entry);
  }
  GNUNET_free (ds_entry);
}

static void
cleanup_handle (struct VerifyRequestHandle *vrh)
{
  struct CredentialRecordEntry *cr_entry;
  GNUNET_log (GNUNET_ERROR_TYPE_DEBUG, "Cleaning up...\n");
  if (NULL != vrh->lookup_request) {
    GNUNET_GNS_lookup_cancel (vrh->lookup_request);
    vrh->lookup_request = NULL;
  }
  cleanup_delegation_set (vrh->root_set);
  GNUNET_free_non_null (vrh->issuer_attribute);
  for (cr_entry = vrh->cred_chain_head; NULL != vrh->cred_chain_head;
       cr_entry = vrh->cred_chain_head) {
    GNUNET_CONTAINER_DLL_remove (vrh->cred_chain_head,
                                 vrh->cred_chain_tail,
                                 cr_entry);
    GNUNET_free_non_null (cr_entry->credential);
    GNUNET_free (cr_entry);
  }
  GNUNET_free (vrh);
}

static void
shutdown_task (void *cls)
{
  struct VerifyRequestHandle *vrh;

  GNUNET_log (GNUNET_ERROR_TYPE_DEBUG, "Shutting down!\n");

  while (NULL != (vrh = vrh_head)) {
    // CREDENTIAL_resolver_lookup_cancel (clh->lookup);
    GNUNET_CONTAINER_DLL_remove (vrh_head, vrh_tail, vrh);
    cleanup_handle (vrh);
  }

  if (NULL != gns) {
    GNUNET_GNS_disconnect (gns);
    gns = NULL;
  }
  if (NULL != namestore) {
    GNUNET_NAMESTORE_disconnect (namestore);
    namestore = NULL;
  }
  if (NULL != statistics) {
    GNUNET_STATISTICS_destroy (statistics, GNUNET_NO);
    statistics = NULL;
  }
}


static void
send_lookup_response (struct VerifyRequestHandle *vrh)
{
  struct GNUNET_MQ_Envelope *env;
  struct DelegationChainResultMessage *rmsg;
  struct DelegationChainEntry *dce;
  struct GNUNET_CREDENTIAL_Delegation dd[vrh->delegation_chain_size];
  //TODO rename cred/cd
  //TODO rename all methods using credential
  struct GNUNET_CREDENTIAL_Delegate cred[vrh->del_chain_size];
  struct DelegateRecordEntry *cd;
  struct DelegateRecordEntry *tmp;
  size_t size;

  GNUNET_log (GNUNET_ERROR_TYPE_DEBUG, "Sending response\n");
  dce = vrh->delegation_chain_head;
  for (uint32_t i = 0; i < vrh->delegation_chain_size; i++) {
    dd[i].issuer_key = dce->issuer_key;
    dd[i].subject_key = dce->subject_key;
    dd[i].issuer_attribute = dce->issuer_attribute;
    dd[i].issuer_attribute_len = strlen (dce->issuer_attribute) + 1;
    dd[i].subject_attribute_len = 0;
    dd[i].subject_attribute = NULL;
    if (NULL != dce->subject_attribute) {
      dd[i].subject_attribute = dce->subject_attribute;
      dd[i].subject_attribute_len = strlen (dce->subject_attribute) + 1;
    }
    dce = dce->next;
  }

  // Remove all not needed credentials
  for (cd = vrh->del_chain_head; NULL != cd;) {
    if (cd->refcount > 0) {
      cd = cd->next;
      continue;
    }
    tmp = cd;
    cd = cd->next;
    GNUNET_CONTAINER_DLL_remove (vrh->del_chain_head,
                                 vrh->del_chain_tail,
                                 tmp);
    GNUNET_free (tmp->delegate);
    GNUNET_free (tmp);
    vrh->del_chain_size--;
  }

  /**
   * Get serialized record data
   * Append at the end of rmsg
   */
  cd = vrh->del_chain_head;
  for (uint32_t i = 0; i < vrh->del_chain_size; i++) {
    cred[i].issuer_key = cd->delegate->issuer_key;
    cred[i].subject_key = cd->delegate->subject_key;
    cred[i].issuer_attribute_len
      = strlen (cd->delegate->issuer_attribute) + 1;
    cred[i].issuer_attribute = cd->delegate->issuer_attribute;
    cred[i].expiration = cd->delegate->expiration;
    cred[i].signature = cd->delegate->signature;
    cd = cd->next;
  }
  size = GNUNET_CREDENTIAL_delegation_chain_get_size (vrh->delegation_chain_size,
                                                   dd,
                                                   vrh->del_chain_size,
                                                   cred);
  env = GNUNET_MQ_msg_extra (rmsg,
                             size,
                             GNUNET_MESSAGE_TYPE_CREDENTIAL_VERIFY_RESULT);
  // Assign id so that client can find associated request
  rmsg->id = vrh->request_id;
  rmsg->d_count = htonl (vrh->delegation_chain_size);
  rmsg->c_count = htonl (vrh->del_chain_size);

  if (0 < vrh->del_chain_size)
    rmsg->cred_found = htonl (GNUNET_YES);
  else
    rmsg->cred_found = htonl (GNUNET_NO);

  GNUNET_assert (
    -1
    != GNUNET_CREDENTIAL_delegation_chain_serialize (vrh->delegation_chain_size,
                                                     dd,
                                                     vrh->del_chain_size,
                                                     cred,
                                                     size,
                                                     (char *)&rmsg[1]));

  GNUNET_MQ_send (GNUNET_SERVICE_client_get_mq (vrh->client), env);
  GNUNET_CONTAINER_DLL_remove (vrh_head, vrh_tail, vrh);
  cleanup_handle (vrh);

  GNUNET_STATISTICS_update (statistics,
                            "Completed verifications",
                            1,
                            GNUNET_NO);
}

static char* 
partial_match(char *tmp_trail, char *tmp_subattr, char *parent_trail, char *issuer_attribute)
{
  char *saveptr1, *saveptr2;
  char *trail_token;
  char *sub_token;
  char *attr_trailer;

  // tok both, parent->attr_trailer and del->sub_attr to see how far they match,
  // take rest of parent trailer (only when del->sub_attr token is null), and
  // create new/actual trailer with del->iss_attr
  trail_token = strtok_r (tmp_trail, ".", &saveptr1);
  sub_token = strtok_r (tmp_subattr, ".", &saveptr2);
  while (NULL != trail_token && NULL != sub_token)
  {
    if(0 == strcmp(trail_token,sub_token))
    {
      // good, matches, remove
    } else {
      // not relevant for solving the chain, end for iteration here
      return NULL;
    }
    
    trail_token = strtok_r (NULL, ".", &saveptr1);
    sub_token = strtok_r (NULL, ".", &saveptr2);
  }
  // skip this entry and go to next for if:
  // 1. at some point the attr of the trailer and the subject dont match
  // 2. the trailer is NULL, but the subject has more attributes
  // Reason: This will lead to "startzone.attribute" but we're looking for a solution
  // for "<- startzone"
  if(NULL == trail_token)
  {
    return NULL;
  }

  // do not have to check sub_token == NULL, if both would be NULL
  // at the same time, the complete match part above should have triggered already
  
  // otherwise, above while only ends when sub_token == NULL
  GNUNET_asprintf (&attr_trailer,
                    "%s",
                    trail_token);
  trail_token = strtok_r (NULL, ".", &saveptr1);                
  while(NULL != trail_token)
  {
    GNUNET_asprintf (&attr_trailer,
                    "%s.%s",
                    parent_trail,
                    trail_token);
    trail_token = strtok_r (NULL, ".", &saveptr1);                

  }
  GNUNET_asprintf (&attr_trailer,
                    "%s.%s",
                    issuer_attribute,
                    attr_trailer);
  return attr_trailer;
}

static void
test_resolution (void *cls,
                     uint32_t rd_count,
                     const struct GNUNET_GNSRECORD_Data *rd)
{
  GNUNET_log (GNUNET_ERROR_TYPE_DEBUG, "Received %d entries.\n", rd_count);
  
  struct VerifyRequestHandle *vrh;
  struct DelegationSetQueueEntry *current_set;
  struct DelegationSetQueueEntry *ds_entry;
  struct DelegationQueueEntry *dq_entry;
  
  current_set = cls;
  // set handle to NULL (as el = NULL)
  current_set->lookup_request = NULL;
  vrh = current_set->handle;
  vrh->pending_lookups--;

  // Loop record entries
  for (uint32_t i = 0; i < rd_count; i++) {
    if (GNUNET_GNSRECORD_TYPE_DELEGATE != rd[i].record_type)
      continue;

    // Start deserialize into Delegate
    struct GNUNET_CREDENTIAL_Delegate *del;
    del = GNUNET_CREDENTIAL_delegate_deserialize(rd[i].data, rd[i].data_size);

    // Start: Create DQ Entry
    dq_entry = GNUNET_new (struct DelegationQueueEntry);
    // AND delegations are not possible, only 1 solution
    dq_entry->required_solutions = 1;
    dq_entry->parent_set = current_set;
    
    // Insert it into the current set
    GNUNET_CONTAINER_DLL_insert (current_set->queue_entries_head,
                                 current_set->queue_entries_tail,
                                 dq_entry);

    // Start: Create DS Entry
    ds_entry = GNUNET_new (struct DelegationSetQueueEntry);
 
    // (1) A.a <- A.b.c
    // (2) A.b <- D.d
    // (3) D.d <- E
    // (4) E.c <- F.c
    // (5) F.c <- G
    // Possibilities:
    // 1. complete match: trailer = 0, validate
    // 2. partial match: replace
    // 3. new solution: replace, add trailer

    // At resolution chain start trailer of parent is NULL
    if (NULL == current_set->attr_trailer) {
      // for (5) F.c <- G, remember .c when going upwards
      ds_entry->attr_trailer = GNUNET_strdup(del->issuer_attribute);
    } else {
      if (0 == del->subject_attribute_len){
        GNUNET_log (GNUNET_ERROR_TYPE_DEBUG, "Found: New solution\n");
        // new solution
        // create new trailer del->issuer_attribute, ds_entry->attr_trailer
        GNUNET_asprintf (&ds_entry->attr_trailer,
                          "%s.%s",
                          del->issuer_attribute,
                          current_set->attr_trailer);
      } else if(0 == strcmp(del->subject_attribute, current_set->attr_trailer)){
        GNUNET_log (GNUNET_ERROR_TYPE_DEBUG, "Found: Complete match\n");
        // complete match
        // new trailer == issuer attribute (e.g. (5) to (4))
        // TODO memleak, free trailer before
        ds_entry->attr_trailer = GNUNET_strdup(del->issuer_attribute);
      } else {
        GNUNET_log (GNUNET_ERROR_TYPE_DEBUG, "Found: Partial match\n");
        // partial match

        char *trail = partial_match(GNUNET_strdup (current_set->attr_trailer),
          GNUNET_strdup (del->subject_attribute),
          current_set->attr_trailer,
          GNUNET_strdup (del->issuer_attribute));

        // if null: skip this record entry (reasons: mismatch or overmatch, both not relevant)
        if(NULL == trail) {
          GNUNET_log (GNUNET_ERROR_TYPE_DEBUG, "Entry not relevant, discarding: %s.%s <- %s.%s\n",
            GNUNET_CRYPTO_ecdsa_public_key_to_string(&del->issuer_key),
            del->issuer_attribute,
            GNUNET_CRYPTO_ecdsa_public_key_to_string(&del->subject_key),
            del->subject_attribute);
          continue;
        } else
          ds_entry->attr_trailer = trail;
      }
    }


    // Start: Credential Chain Entry
    // issuer key is subject key, who needs to be contacted to resolve this (forward, therefore subject)
    // TODO: new ds_entry struct with subject_key (or one for both with contact_key or sth)
    ds_entry->issuer_key = GNUNET_new (struct GNUNET_CRYPTO_EcdsaPublicKey);
    GNUNET_memcpy (ds_entry->issuer_key,
                    &del->subject_key,
                    sizeof (struct GNUNET_CRYPTO_EcdsaPublicKey));

    ds_entry->delegation_chain_entry = GNUNET_new (struct DelegationChainEntry);
    ds_entry->delegation_chain_entry->subject_key = del->subject_key;
    if (0 < del->subject_attribute_len)
      ds_entry->delegation_chain_entry->subject_attribute = GNUNET_strdup (del->subject_attribute);
    ds_entry->delegation_chain_entry->issuer_key = del->issuer_key;
    ds_entry->delegation_chain_entry->issuer_attribute = GNUNET_strdup (del->issuer_attribute);

    // current delegation as parent
    ds_entry->parent_queue_entry = dq_entry;

    // Check for solution
    // if: issuer key we looking for
    if (0 == memcmp (&del->issuer_key,
                   &vrh->issuer_key,
                   sizeof (struct GNUNET_CRYPTO_EcdsaPublicKey)))
    {
      // if: issuer attr we looking for
      if (0 == strcmp (del->issuer_attribute,
                    vrh->issuer_attribute))
        {
          // if: complete match, meaning new trailer == issuer attr
          if(0 == strcmp (vrh->issuer_attribute, ds_entry->attr_trailer))
          {
            GNUNET_log (GNUNET_ERROR_TYPE_ERROR, "Found: Solution\n");
            
            // Add to delegation_chain
            struct DelegationSetQueueEntry *tmp_set;
            for (tmp_set = ds_entry; NULL != tmp_set->parent_queue_entry;
             tmp_set = tmp_set->parent_queue_entry->parent_set) {
              if (NULL != tmp_set->delegation_chain_entry) {
                vrh->delegation_chain_size++;
                GNUNET_CONTAINER_DLL_insert (vrh->delegation_chain_head,
                                            vrh->delegation_chain_tail,
                                            tmp_set->delegation_chain_entry);
              }
            }
            GNUNET_log (GNUNET_ERROR_TYPE_ERROR, "tmpentrylast %s %s\n",
            GNUNET_CRYPTO_ecdsa_public_key_to_string(&vrh->delegation_chain_head->subject_key),
            vrh->delegation_chain_head->subject_attribute);

            // Increase refcount for this delegate
            for (struct DelegateRecordEntry *del_entry = vrh->del_chain_head; del_entry != NULL; del_entry = del_entry->next) {
              if (0 == memcmp (&del_entry->delegate->issuer_key,
                   &vrh->delegation_chain_head->subject_key,
                   sizeof (struct GNUNET_CRYPTO_EcdsaPublicKey)))
              {
                if (0 == strcmp (del_entry->delegate->issuer_attribute,
                    vrh->delegation_chain_head->subject_attribute))
                {
                  del_entry->refcount++;
                }
              }
            } 

            send_lookup_response (vrh);
            return;
          }
        }
    }

    // Starting a new GNS lookup            
    vrh->pending_lookups++;
    ds_entry->handle = vrh;

    GNUNET_log (GNUNET_ERROR_TYPE_ERROR, "Starting to look up trailer %s in zone %s\n", ds_entry->attr_trailer, GNUNET_CRYPTO_ecdsa_public_key_to_string(&del->issuer_key));
    
    GNUNET_GNS_lookup (gns,
                             GNUNET_GNS_EMPTY_LABEL_AT,
                             &del->issuer_key,
                             GNUNET_GNSRECORD_TYPE_DELEGATE,
                             GNUNET_GNS_LO_DEFAULT,
                             &test_resolution,
                             ds_entry);
  }

  if (0 == vrh->pending_lookups) {
    GNUNET_log (GNUNET_ERROR_TYPE_DEBUG, "We are all out of attributes...\n");
    send_lookup_response (vrh);
    return;
  }
}

static void
backward_resolution (void *cls,
                     uint32_t rd_count,
                     const struct GNUNET_GNSRECORD_Data *rd)
{
  struct VerifyRequestHandle *vrh;
  const struct GNUNET_CREDENTIAL_DelegationRecord *sets;
  struct DelegateRecordEntry *del_pointer;
  struct DelegationSetQueueEntry *current_set;
  struct DelegationSetQueueEntry *ds_entry;
  struct DelegationSetQueueEntry *tmp_set;
  struct DelegationQueueEntry *dq_entry;
  char *expanded_attr;
  char *lookup_attribute;

  current_set = cls;
  current_set->lookup_request = NULL;
  vrh = current_set->handle;
  vrh->pending_lookups--;

  // Each OR
  for (uint32_t i = 0; i < rd_count; i++) {
    if (GNUNET_GNSRECORD_TYPE_ATTRIBUTE != rd[i].record_type)
      continue;

    sets = rd[i].data;
    struct GNUNET_CREDENTIAL_DelegationSet set[ntohl (sets->set_count)];
    GNUNET_log (
      GNUNET_ERROR_TYPE_DEBUG,
      "Found new attribute delegation with %d sets. Creating new Job...\n",
      ntohl (sets->set_count));

    if (GNUNET_OK
        != GNUNET_CREDENTIAL_delegation_set_deserialize (
          GNUNET_ntohll (sets->data_size),
          (const char *)&sets[1],
          ntohl (sets->set_count),
          set)) {
      GNUNET_log (GNUNET_ERROR_TYPE_ERROR, "Failed to deserialize!\n");
      continue;
    }
    dq_entry = GNUNET_new (struct DelegationQueueEntry);
    dq_entry->required_solutions = ntohl (sets->set_count);
    dq_entry->parent_set = current_set;

    GNUNET_log (GNUNET_ERROR_TYPE_DEBUG, "# New OR entry into queue\n");

    GNUNET_CONTAINER_DLL_insert (current_set->queue_entries_head,
                                 current_set->queue_entries_tail,
                                 dq_entry);
    // Each AND
    for (uint32_t j = 0; j < ntohl (sets->set_count); j++) {
      ds_entry = GNUNET_new (struct DelegationSetQueueEntry);
      if (NULL != current_set->attr_trailer) {
        if (0 == set[j].subject_attribute_len) {
          GNUNET_asprintf (&expanded_attr, "%s", current_set->attr_trailer);

        } else {
          GNUNET_asprintf (&expanded_attr,
                           "%s.%s",
                           set[j].subject_attribute,
                           current_set->attr_trailer);
        }
        GNUNET_log (GNUNET_ERROR_TYPE_DEBUG, "Expanded to %s\n", expanded_attr);
        ds_entry->unresolved_attribute_delegation = expanded_attr;
      } else {
        if (0 != set[j].subject_attribute_len) {
          GNUNET_log (GNUNET_ERROR_TYPE_DEBUG,
                      "Not Expanding %s\n",
                      set[j].subject_attribute);
          ds_entry->unresolved_attribute_delegation
            = GNUNET_strdup (set[j].subject_attribute);
        }
      }

      // Add a credential chain entry
      ds_entry->delegation_chain_entry
        = GNUNET_new (struct DelegationChainEntry);
      ds_entry->delegation_chain_entry->subject_key = set[j].subject_key;
      ds_entry->issuer_key = GNUNET_new (struct GNUNET_CRYPTO_EcdsaPublicKey);
      GNUNET_memcpy (ds_entry->issuer_key,
                     &set[j].subject_key,
                     sizeof (struct GNUNET_CRYPTO_EcdsaPublicKey));
      if (0 < set[j].subject_attribute_len)
        ds_entry->delegation_chain_entry->subject_attribute
          = GNUNET_strdup (set[j].subject_attribute);
      ds_entry->delegation_chain_entry->issuer_key = *current_set->issuer_key;
      ds_entry->delegation_chain_entry->issuer_attribute
        = GNUNET_strdup (current_set->lookup_attribute);

      ds_entry->parent_queue_entry = dq_entry; // current_delegation;

      GNUNET_CONTAINER_DLL_insert (dq_entry->set_entries_head,
                                   dq_entry->set_entries_tail,
                                   ds_entry);

      GNUNET_log (GNUNET_ERROR_TYPE_DEBUG, "Checking for cred match\n");
      /**
       * Check if this delegation already matches one of our credentials
       */
      for (del_pointer = vrh->del_chain_head; del_pointer != NULL;
           del_pointer = del_pointer->next) {
        // If key and attribute match credential continue and backtrack
        if (0
            != memcmp (&set->subject_key,
                       &del_pointer->delegate->issuer_key,
                       sizeof (struct GNUNET_CRYPTO_EcdsaPublicKey)))
          continue;
        GNUNET_log (GNUNET_ERROR_TYPE_DEBUG,
                    "Checking if %s matches %s\n",
                    ds_entry->unresolved_attribute_delegation,
                    del_pointer->delegate->issuer_attribute);

        if (0
            != strcmp (ds_entry->unresolved_attribute_delegation,
                       del_pointer->delegate->issuer_attribute))
          continue;

        GNUNET_log (GNUNET_ERROR_TYPE_DEBUG, "Found issuer\n");
        del_pointer->refcount++;
        // Backtrack
        for (tmp_set = ds_entry; NULL != tmp_set->parent_queue_entry;
             tmp_set = tmp_set->parent_queue_entry->parent_set) {
          GNUNET_log (GNUNET_ERROR_TYPE_DEBUG, "# %s\n", tmp_set->unresolved_attribute_delegation);
          tmp_set->parent_queue_entry->required_solutions--;
          if (NULL != tmp_set->delegation_chain_entry) {
            vrh->delegation_chain_size++;
            GNUNET_CONTAINER_DLL_insert (vrh->delegation_chain_head,
                                         vrh->delegation_chain_tail,
                                         tmp_set->delegation_chain_entry);
          }
          if (0 < tmp_set->parent_queue_entry->required_solutions)
            break;
        }

        if (NULL == tmp_set->parent_queue_entry) {
          GNUNET_log (GNUNET_ERROR_TYPE_ERROR, "All solutions found\n");
          // Found match
          send_lookup_response (vrh);
          return;
        }
        GNUNET_log (GNUNET_ERROR_TYPE_ERROR, "Not all solutions found yet.\n");
        continue;
      }
      GNUNET_log (GNUNET_ERROR_TYPE_DEBUG,
                  "Building new lookup request from %s\n",
                  ds_entry->unresolved_attribute_delegation);
      // Continue with backward resolution
      char
        issuer_attribute_name[strlen (ds_entry->unresolved_attribute_delegation)
                              + 1];
      strcpy (issuer_attribute_name, ds_entry->unresolved_attribute_delegation);
      GNUNET_log (GNUNET_ERROR_TYPE_DEBUG, "# Issuer Att Name: %s\n", issuer_attribute_name);
      char *next_attr = strtok (issuer_attribute_name, ".");
      if (NULL == next_attr) {
        GNUNET_log (GNUNET_ERROR_TYPE_ERROR,
                    "Failed to parse next attribute\n");
        continue;
      }
      GNUNET_asprintf (&lookup_attribute, "%s", next_attr);
      GNUNET_asprintf (&ds_entry->lookup_attribute, "%s", next_attr);
      if (strlen (next_attr)
          == strlen (ds_entry->unresolved_attribute_delegation)) {
        ds_entry->attr_trailer = NULL;
      } else {
        next_attr += strlen (next_attr) + 1;
        ds_entry->attr_trailer = GNUNET_strdup (next_attr);
      }

      GNUNET_log (GNUNET_ERROR_TYPE_ERROR,
                  "Looking up %s\n",
                  ds_entry->lookup_attribute);
      if (NULL != ds_entry->attr_trailer)
        GNUNET_log (GNUNET_ERROR_TYPE_ERROR,
                    "%s still to go...\n",
                    ds_entry->attr_trailer);

      vrh->pending_lookups++;
      ds_entry->handle = vrh;
      ds_entry->lookup_request
        = GNUNET_GNS_lookup (gns,
                             lookup_attribute,
                             ds_entry->issuer_key, // issuer_key,
                             GNUNET_GNSRECORD_TYPE_ATTRIBUTE,
                             GNUNET_GNS_LO_DEFAULT,
                             &backward_resolution,
                             ds_entry);

      GNUNET_free (lookup_attribute);
    }
  }

  if (0 == vrh->pending_lookups) {
    GNUNET_log (GNUNET_ERROR_TYPE_DEBUG, "We are all out of attributes...\n");
    send_lookup_response (vrh);
    return;
  }
}


/**
 * Result from GNS lookup.
 *
 * @param cls the closure (our client lookup handle)
 */
static void
delegation_chain_resolution_start (void *cls)
{
  struct VerifyRequestHandle *vrh = cls;
  struct DelegationSetQueueEntry *ds_entry;
  struct DelegateRecordEntry *del_entry;
  vrh->lookup_request = NULL;

  if (0 == vrh->del_chain_size) {
    GNUNET_log (GNUNET_ERROR_TYPE_ERROR, "No delegates found\n");
    send_lookup_response (vrh);
    return;
  }

  for (del_entry = vrh->del_chain_head; del_entry != NULL;
       del_entry = del_entry->next) {
    if (0 != memcmp (&del_entry->delegate->issuer_key,
                   &vrh->issuer_key,
                   sizeof (struct GNUNET_CRYPTO_EcdsaPublicKey)))
      continue;
    if (0 != strcmp (del_entry->delegate->issuer_attribute,
                   vrh->issuer_attribute))
      continue;
    del_entry->refcount++;
    // Found match prematurely
    send_lookup_response (vrh);
    return;
  }

  /**
   * Check for attributes from the issuer and follow the chain
   * till you get the required subject's attributes
   */
  char issuer_attribute_name[strlen (vrh->issuer_attribute) + 1];
  strcpy (issuer_attribute_name, vrh->issuer_attribute);
  GNUNET_log (GNUNET_ERROR_TYPE_DEBUG,
              "Looking up %s\n",
              issuer_attribute_name);
  ds_entry = GNUNET_new (struct DelegationSetQueueEntry);
  ds_entry->issuer_key = GNUNET_new (struct GNUNET_CRYPTO_EcdsaPublicKey);
  GNUNET_memcpy (ds_entry->issuer_key,
                 &vrh->issuer_key,
                 sizeof (struct GNUNET_CRYPTO_EcdsaPublicKey));
  ds_entry->issuer_attribute = GNUNET_strdup (vrh->issuer_attribute);
  ds_entry->handle = vrh;
  ds_entry->lookup_attribute = GNUNET_strdup (vrh->issuer_attribute);
  vrh->root_set = ds_entry;
  vrh->pending_lookups = 1;
  // Start with backward resolution
  GNUNET_log (GNUNET_ERROR_TYPE_DEBUG, "# Start Backward Resolution\n");

  ds_entry->lookup_request = GNUNET_GNS_lookup (gns,
                                                issuer_attribute_name,
                                                &vrh->issuer_key, // issuer_key,
                                                GNUNET_GNSRECORD_TYPE_ATTRIBUTE,
                                                GNUNET_GNS_LO_DEFAULT,
                                                &backward_resolution,
                                                ds_entry);
}

static void
delegation_chain_fw_resolution_start (void *cls)
{
  struct VerifyRequestHandle *vrh = cls;
  struct DelegationSetQueueEntry *ds_entry;
  struct DelegateRecordEntry *del_entry;

  vrh->lookup_request = NULL;
  // set to 0 and increase on each lookup: for fw multiple lookups (may be) started
  vrh->pending_lookups = 0;

  //TODO no pre-check with vrh->dele_chain_bla if match issuer_key
  //otherwise: start mutliple lookups for each vrh->dele_chain
  // A.a <- ...
  // X.x <- C
  // Y.y <- C
  // wenn X.x oder Y.y nicht == A.a dann starte bei X und bei Y

  // bei backward: check every cred entry if match issuer key
  // otherwise: start at issuer and go down till match
  // A.a <- ...
  // X.x <- C
  // Y.y <- C
  // wenn X.x oder Y.y nicht == A.a dann starte von A
  if (0 == vrh->del_chain_size) {
    GNUNET_log (GNUNET_ERROR_TYPE_ERROR, "No delegations found\n");
    send_lookup_response (vrh);
    return;
  }

  // Check if one of the delegations of the subject already match 
  for (del_entry = vrh->del_chain_head; del_entry != NULL; del_entry = del_entry->next) {
    if (0 != memcmp (&del_entry->delegate->issuer_key,
                   &vrh->issuer_key,
                   sizeof (struct GNUNET_CRYPTO_EcdsaPublicKey)))
      continue;
    if (0 != strcmp (del_entry->delegate->issuer_attribute,
                   vrh->issuer_attribute))
      continue;
    del_entry->refcount++;
    // Found match prematurely
    send_lookup_response (vrh);
    return;
  }

  // None match, therefore start for every delegation found a lookup chain
  // Return and end collect process on first chain iss <-> sub found

  // ds_entry created belongs to the first lookup, vrh still has the
  // issuer+attr we look for
  for (del_entry = vrh->del_chain_head; del_entry != NULL; del_entry = del_entry->next) {
    //char issuer_attribute_name[strlen (vrh->issuer_attribute) + 1];
    //strcpy (issuer_attribute_name, vrh->issuer_attribute);

    GNUNET_log (GNUNET_ERROR_TYPE_DEBUG,
                "Looking for %s.%s\n",
                GNUNET_CRYPTO_ecdsa_public_key_to_string(&del_entry->delegate->issuer_key), del_entry->delegate->issuer_attribute);
                
    ds_entry = GNUNET_new (struct DelegationSetQueueEntry);
    ds_entry->issuer_key = GNUNET_new (struct GNUNET_CRYPTO_EcdsaPublicKey);
    // TODO: new ds_entry struct with subject_key (or one for both with contact_key or sth)
    GNUNET_memcpy (ds_entry->issuer_key,
                    &del_entry->delegate->subject_key,
                    sizeof (struct GNUNET_CRYPTO_EcdsaPublicKey));
    //ds_entry->issuer_attribute = GNUNET_strdup (vrh->issuer_attribute);
    ds_entry->attr_trailer = GNUNET_strdup(del_entry->delegate->issuer_attribute);
    ds_entry->handle = vrh;
    // TODO: no lookup attribute for forward?
    //ds_entry->lookup_attribute = GNUNET_strdup (vrh->issuer_attribute);

    vrh->root_set = ds_entry;
    vrh->pending_lookups ++;
    // Start with forward resolution
    GNUNET_log (GNUNET_ERROR_TYPE_DEBUG, "# Start Forward Resolution\n");

    ds_entry->lookup_request = GNUNET_GNS_lookup (gns,
                                                  GNUNET_GNS_EMPTY_LABEL_AT,
                                                  &del_entry->delegate->issuer_key, // issuer_key,
                                                  GNUNET_GNSRECORD_TYPE_DELEGATE,
                                                  GNUNET_GNS_LO_DEFAULT,
                                                  &test_resolution,
                                                  ds_entry);
  }
}

static int
check_verify (void *cls, const struct VerifyMessage *v_msg)
{
  size_t msg_size;
  const char *attr;

  msg_size = ntohs (v_msg->header.size);
  if (msg_size < sizeof (struct VerifyMessage)) {
    GNUNET_break (0);
    return GNUNET_SYSERR;
  }
  if (ntohs (v_msg->issuer_attribute_len) > GNUNET_CREDENTIAL_MAX_LENGTH) {
    GNUNET_break (0);
    return GNUNET_SYSERR;
  }
  attr = (const char *)&v_msg[1];

  if (strlen (attr) > GNUNET_CREDENTIAL_MAX_LENGTH) {
    GNUNET_break (0);
    return GNUNET_SYSERR;
  }
  return GNUNET_OK;
}

static void
handle_verify (void *cls, const struct VerifyMessage *v_msg)
{
  struct VerifyRequestHandle *vrh;
  struct GNUNET_SERVICE_Client *client = cls;
  struct DelegateRecordEntry *del_entry;
  uint32_t credentials_count;
  uint32_t credential_data_size;
  char attr[GNUNET_CREDENTIAL_MAX_LENGTH + 1];
  char issuer_attribute[GNUNET_CREDENTIAL_MAX_LENGTH + 1];
  char *attrptr = attr;
  char *credential_data;
  const char *utf_in;

  GNUNET_log (GNUNET_ERROR_TYPE_DEBUG, "Received VERIFY message\n");
  utf_in = (const char *)&v_msg[1];
  GNUNET_STRINGS_utf8_tolower (utf_in, attrptr);
  GNUNET_memcpy (issuer_attribute, attr, ntohs (v_msg->issuer_attribute_len));
  issuer_attribute[ntohs (v_msg->issuer_attribute_len)] = '\0';
  vrh = GNUNET_new (struct VerifyRequestHandle);
  GNUNET_CONTAINER_DLL_insert (vrh_head, vrh_tail, vrh);
  vrh->client = client;
  vrh->request_id = v_msg->id;
  vrh->issuer_key = v_msg->issuer_key;
  vrh->subject_key = v_msg->subject_key;
  vrh->issuer_attribute = GNUNET_strdup (issuer_attribute);
  vrh->resolution_algo = ntohs(v_msg->resolution_algo);

  GNUNET_SERVICE_client_continue (vrh->client);
  if (0 == strlen (issuer_attribute)) {
    GNUNET_log (GNUNET_ERROR_TYPE_ERROR, "No issuer attribute provided!\n");
    send_lookup_response (vrh);
    return;
  }
  
  // Parse delegates from verifaction message
  credentials_count = ntohl (v_msg->c_count);
  credential_data_size = ntohs (v_msg->header.size)
                         - sizeof (struct VerifyMessage)
                         - ntohs (v_msg->issuer_attribute_len) - 1;
  struct GNUNET_CREDENTIAL_Delegate credentials[credentials_count];
  memset (credentials,
          0,
          sizeof (struct GNUNET_CREDENTIAL_Delegate) * credentials_count);
  credential_data = (char *)&v_msg[1] + ntohs (v_msg->issuer_attribute_len) + 1;
  if (GNUNET_OK
      != GNUNET_CREDENTIAL_credentials_deserialize (credential_data_size,
                                                    credential_data,
                                                    credentials_count,
                                                    credentials)) {
    GNUNET_log (GNUNET_ERROR_TYPE_ERROR, "Cannot deserialize credentials!\n");
    send_lookup_response (vrh);
    return;
  }

  // Prepare vrh delegation chain for later validation
  for (uint32_t i = 0; i < credentials_count; i++) {
    del_entry = GNUNET_new (struct DelegateRecordEntry);
    del_entry->delegate
      = GNUNET_malloc (sizeof (struct GNUNET_CREDENTIAL_Delegate)
                       + credentials[i].issuer_attribute_len + 1);
    GNUNET_memcpy (del_entry->delegate,
                   &credentials[i],
                   sizeof (struct GNUNET_CREDENTIAL_Delegate));
    GNUNET_memcpy (&del_entry->delegate[1],
                   credentials[i].issuer_attribute,
                   credentials[i].issuer_attribute_len);
    del_entry->delegate->issuer_attribute_len
      = credentials[i].issuer_attribute_len;
    del_entry->delegate->issuer_attribute = (char *)&del_entry->delegate[1];
    GNUNET_CONTAINER_DLL_insert_tail (vrh->del_chain_head,
                                      vrh->del_chain_tail,
                                      del_entry);
    vrh->del_chain_size++;
  }

  // Switch resolution algo
  if(Backward == vrh->resolution_algo){
    GNUNET_log (GNUNET_ERROR_TYPE_DEBUG, "+++++++++++++++++backward\n");
    delegation_chain_resolution_start (vrh);
  } else if (Forward == vrh->resolution_algo){
    GNUNET_log (GNUNET_ERROR_TYPE_DEBUG, "+++++++++++++++++forward\n");
    delegation_chain_fw_resolution_start (vrh);
  } else{
    //TODO
  }
}

static void
handle_cred_collection_error_cb (void *cls)
{
  struct VerifyRequestHandle *vrh = cls;
  GNUNET_log (GNUNET_ERROR_TYPE_DEBUG,
              "Got disconnected from namestore database.\n");
  vrh->cred_collection_iter = NULL;
  send_lookup_response (vrh);
}

static void
handle_cred_collection_finished_cb (void *cls)
{
  struct VerifyRequestHandle *vrh = cls;
  GNUNET_log (GNUNET_ERROR_TYPE_DEBUG, "Done collecting credentials.\n");
  vrh->cred_collection_iter = NULL;
  if(Backward == vrh->resolution_algo){
    GNUNET_log (GNUNET_ERROR_TYPE_DEBUG, "+++++++++++++++++backward\n");
    delegation_chain_resolution_start (vrh);
  } else if (Forward == vrh->resolution_algo){
    GNUNET_log (GNUNET_ERROR_TYPE_DEBUG, "+++++++++++++++++forward\n");
    delegation_chain_fw_resolution_start (vrh);
  } else{
    //TODO
  }
}

static void
tmp_handle_cred_collection_cb (void *cls,
                           const struct GNUNET_CRYPTO_EcdsaPrivateKey *key,
                           const char *label,
                           unsigned int rd_count,
                           const struct GNUNET_GNSRECORD_Data *rd)
{
  struct VerifyRequestHandle *vrh = cls;
  struct GNUNET_CREDENTIAL_Delegate *del;
  struct DelegateRecordEntry *del_entry;
  int cred_record_count;
  cred_record_count = 0;
  vrh->dele_qe = NULL;

  //TODO not all, only private and with sub_attr_len == 0
  for (uint32_t i = 0; i < rd_count; i++) {
    if (GNUNET_GNSRECORD_TYPE_DELEGATE != rd[i].record_type)
      continue;
    cred_record_count++;
    del = GNUNET_CREDENTIAL_delegate_deserialize (rd[i].data, rd[i].data_size);
    if (NULL == del) {
      GNUNET_log (GNUNET_ERROR_TYPE_WARNING, "Invalid credential found\n");
      continue;
    }
    del_entry = GNUNET_new (struct DelegateRecordEntry);
    del_entry->delegate = del;
    GNUNET_CONTAINER_DLL_insert_tail (vrh->del_chain_head,
                                      vrh->del_chain_tail,
                                      del_entry);
    vrh->del_chain_size++;
  }
  // No need to collect next, should have all already
  //vrh->collect_next_task = GNUNET_SCHEDULER_add_now (&collect_next, vrh);
  handle_cred_collection_finished_cb(vrh);
}

static void
handle_collect (void *cls, const struct CollectMessage *c_msg)
{
  char attr[GNUNET_CREDENTIAL_MAX_LENGTH + 1];
  char issuer_attribute[GNUNET_CREDENTIAL_MAX_LENGTH + 1];
  struct VerifyRequestHandle *vrh;
  struct GNUNET_SERVICE_Client *client = cls;
  char *attrptr = attr;
  const char *utf_in;

  GNUNET_log (GNUNET_ERROR_TYPE_DEBUG, "Received COLLECT message\n");

  utf_in = (const char *)&c_msg[1];
  GNUNET_STRINGS_utf8_tolower (utf_in, attrptr);

  GNUNET_memcpy (issuer_attribute, attr, ntohs (c_msg->issuer_attribute_len));
  issuer_attribute[ntohs (c_msg->issuer_attribute_len)] = '\0';
  vrh = GNUNET_new (struct VerifyRequestHandle);
  GNUNET_CONTAINER_DLL_insert (vrh_head, vrh_tail, vrh);
  vrh->client = client;
  vrh->request_id = c_msg->id;
  vrh->issuer_key = c_msg->issuer_key;
  GNUNET_CRYPTO_ecdsa_key_get_public (&c_msg->subject_key, &vrh->subject_key);
  vrh->issuer_attribute = GNUNET_strdup (issuer_attribute);
  vrh->resolution_algo = ntohs(c_msg->resolution_algo);

  if (0 == strlen (issuer_attribute)) {
    GNUNET_log (GNUNET_ERROR_TYPE_ERROR, "No issuer attribute provided!\n");
    send_lookup_response (vrh);
    return;
  }
  GNUNET_log (GNUNET_ERROR_TYPE_DEBUG, "Getting credentials for subject\n");
  /**
   * First, get attribute from subject
   */
  // TODO NAMESTORE_lookup auf empty label statt iteration, iteration genutzt da nicht wusste welches label
  /*vrh->cred_collection_iter = GNUNET_NAMESTORE_zone_iteration_start (
    namestore,
    &c_msg->subject_key,
    &handle_cred_collection_error_cb,
    vrh,
    &handle_cred_collection_cb,
    vrh,
    &handle_cred_collection_finished_cb,
    vrh);*/
  //TODO rename tmp_handle_... and test_resolution..
  vrh->dele_qe = GNUNET_NAMESTORE_records_lookup (namestore,
                                          &c_msg->subject_key,
                                          GNUNET_GNS_EMPTY_LABEL_AT,
                                          &handle_cred_collection_error_cb,
                                          vrh,
                                          &tmp_handle_cred_collection_cb,
                                          vrh);
  GNUNET_SERVICE_client_continue (vrh->client);
}


static int
check_collect (void *cls, const struct CollectMessage *c_msg)
{
  size_t msg_size;
  const char *attr;

  msg_size = ntohs (c_msg->header.size);
  if (msg_size < sizeof (struct CollectMessage)) {
    GNUNET_break (0);
    return GNUNET_SYSERR;
  }
  if (ntohs (c_msg->issuer_attribute_len) > GNUNET_CREDENTIAL_MAX_LENGTH) {
    GNUNET_break (0);
    return GNUNET_SYSERR;
  }
  attr = (const char *)&c_msg[1];

  if (('\0' != attr[msg_size - sizeof (struct CollectMessage) - 1])
      || (strlen (attr) > GNUNET_CREDENTIAL_MAX_LENGTH)) {
    GNUNET_break (0);
    return GNUNET_SYSERR;
  }
  return GNUNET_OK;
}

static void
client_disconnect_cb (void *cls,
                      struct GNUNET_SERVICE_Client *client,
                      void *app_ctx)
{
  GNUNET_log (GNUNET_ERROR_TYPE_DEBUG, "Client %p disconnected\n", client);
}

static void *
client_connect_cb (void *cls,
                   struct GNUNET_SERVICE_Client *client,
                   struct GNUNET_MQ_Handle *mq)
{
  GNUNET_log (GNUNET_ERROR_TYPE_DEBUG, "Client %p connected\n", client);
  return client;
}

/**
 * Process Credential requests.
 *
 * @param cls closure
 * @param c configuration to use
 * @param handle service handle
 */
static void
run (void *cls,
     const struct GNUNET_CONFIGURATION_Handle *c,
     struct GNUNET_SERVICE_Handle *handle)
{

  gns = GNUNET_GNS_connect (c);
  if (NULL == gns) {
    fprintf (stderr, _ ("Failed to connect to GNS\n"));
  }
  namestore = GNUNET_NAMESTORE_connect (c);
  if (NULL == namestore) {
    fprintf (stderr, _ ("Failed to connect to namestore\n"));
  }

  statistics = GNUNET_STATISTICS_create ("credential", c);
  GNUNET_SCHEDULER_add_shutdown (&shutdown_task, NULL);
}


/**
 * Define "main" method using service macro
 */
GNUNET_SERVICE_MAIN (
  "credential",
  GNUNET_SERVICE_OPTION_NONE,
  &run,
  &client_connect_cb,
  &client_disconnect_cb,
  NULL,
  GNUNET_MQ_hd_var_size (verify,
                         GNUNET_MESSAGE_TYPE_CREDENTIAL_VERIFY,
                         struct VerifyMessage,
                         NULL),
  GNUNET_MQ_hd_var_size (collect,
                         GNUNET_MESSAGE_TYPE_CREDENTIAL_COLLECT,
                         struct CollectMessage,
                         NULL),
  GNUNET_MQ_handler_end ());

/* end of gnunet-service-credential.c */
