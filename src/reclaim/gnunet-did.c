/*
   This file is part of GNUnet.
   Copyright (C) 2012-2021 GNUnet e.V.

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
 * FIXME: Do we only want to handle EdDSA identities?
 * TODO: Own GNS record type
 * TODO: Fix overwrite of records in @ if present look for other with same sub
 * TODO. Tests
 * TODO: Move constants to did.h
 * FIXME: Remove and lookup require differnt representations (did vs egoname)
 */

/**
 * @author Tristan Schwieren
 * @file src/did/gnunet-did.c
 * @brief DID Method Wrapper
 *
 */
#include "platform.h"
#include "gnunet_util_lib.h"
#include "gnunet_namestore_service.h"
#include "gnunet_gns_service.h"
#include "gnunet_gnsrecord_lib.h"
#include "jansson.h"

#define GNUNET_DID_METHOD_RECLAIM_PREFIX "did:reclaim:"
#define GNUNET_DID_DEFAULT_DID_DOCUMENT_EXPIRATION_TIME "1d"

/**
 * return value
 */
static int ret;

/**
 * Replace DID Document Flag
 */
static int replace;

/**
 * Remove DID Document Flag
 */
static int remove_did;

/**
 *  Get DID Documement for DID Flag
 */
static int get;

/**
 * Create DID Document Flag
 */
static int create;

/**
 * Show DID for Ego Flag
 */
static int show;

/**
 * Show DID for Ego Flag
 */
static int show_all;

/**
 * DID Attribut String
 */
static char *did;

/**
 * DID Document Attribut String
 */
static char *didd;

/**
 * Ego Attribut String
 */
static char *egoname;

/**
 * DID Document expiration Date Attribut String
 */
static char *expire;

/*
 * Handle to the GNS service
 */
static struct GNUNET_GNS_Handle *gns_handle;

/*
 * Handle to the NAMESTORE service
 */
static struct GNUNET_NAMESTORE_Handle *namestore_handle;

/*
 * Handle to the IDENTITY service
 */
static struct GNUNET_IDENTITY_Handle *identity_handle;


/*
 * The configuration
 */
const static struct GNUNET_CONFIGURATION_Handle *my_cfg;

/**
 * Give ego exists
 */
static int ego_exists = 0;

/**
 * @brief Disconnect and shutdown
 * @param cls closure
 */
static void
cleanup (void *cls)
{
  if (NULL != gns_handle)
    GNUNET_GNS_disconnect (gns_handle);
  if (NULL != namestore_handle)
    GNUNET_NAMESTORE_disconnect (namestore_handle);
  if (NULL != identity_handle)
    GNUNET_IDENTITY_disconnect (identity_handle);

  GNUNET_free (did);
  GNUNET_free (didd);
  GNUNET_free (egoname);
  GNUNET_free (expire);

  GNUNET_SCHEDULER_shutdown ();
}

char*
ego_to_did (struct GNUNET_IDENTITY_Ego *ego)
{
  struct GNUNET_IDENTITY_PublicKey pkey; // Get Public key
  char *pkey_str;
  char *did_str;
  size_t pkey_len;

  GNUNET_IDENTITY_ego_get_public_key (ego, &pkey);

  pkey_str = GNUNET_IDENTITY_public_key_to_string (&pkey);
  GNUNET_asprintf (&did_str, "%s%s",
                   GNUNET_DID_METHOD_RECLAIM_PREFIX,
                   pkey_str);

  free (pkey_str);
  return did_str;
}

/**
 * @brief Callback for ego loockup of get_did_for_ego()
 *
 * @param cls closure
 * @param ego the returned ego
 */
static void
get_did_for_ego_lookup_cb (void *cls, struct GNUNET_IDENTITY_Ego *ego)
{
  char *did_str;

  if (ego == NULL)
  {
    printf ("EGO not found\n");
    GNUNET_SCHEDULER_add_now (&cleanup, NULL);
    ret = 1;
    return;
  }
  did_str = ego_to_did (ego);

  printf ("%s\n", did_str);

  GNUNET_SCHEDULER_add_now (&cleanup, NULL);
  ret = 0;
  return;
}

/**
 * @brief Get the DID for a given EGO
 *
 */
static void
get_did_for_ego ()
{
  if (egoname != NULL)
  {
    GNUNET_IDENTITY_ego_lookup (my_cfg,
                                egoname,
                                &get_did_for_ego_lookup_cb,
                                NULL);
  }
  else {
    printf ("Set the EGO argument to get the DID for a given EGO\n");
    GNUNET_SCHEDULER_add_now (&cleanup, NULL);
    ret = 1;
    return;
  }
}


/**
 * @brief Get the public key from did attribute given by the user
 *
 * @param pkey place to write the public key to
 */
static void
get_pkey_from_attr_did (struct GNUNET_IDENTITY_PublicKey *pkey)
{
  /* FIXME-MSC: I suggest introducing a
   * #define MAX_DID_LENGTH <something>
   * here and use it for parsing
   */
  char pkey_str[59];

  if ((1 != (sscanf (did, GNUNET_DID_METHOD_RECLAIM_PREFIX"%58s", pkey_str))) ||
      (GNUNET_OK != GNUNET_IDENTITY_public_key_from_string (pkey_str, pkey)))
  {
    fprintf (stderr, _("Invalid DID `%s'\n"), pkey_str);
    GNUNET_SCHEDULER_add_now (cleanup, NULL);
    ret = 1;
    return;
  }
}

/**
 * @brief GNS lookup callback. Prints the DID Document to standard out.
 * Fails if there is more than one DID record.
 *
 * @param cls closure
 * @param rd_count number of records in @a rd
 * @param rd the records in the reply
 */
static void
print_did_document (
  void *cls,
  uint32_t rd_count,
  const struct GNUNET_GNSRECORD_Data *rd)
{
  /*
   * FIXME-MSC: The user may decide to put other records here.
   * In general I am fine with the constraint here, but not when
   * we move it to "@"
   */
  if (rd_count != 1)
  {
    printf ("An ego should only have one DID Document\n");
    GNUNET_SCHEDULER_add_now (cleanup, NULL);
    ret = 1;
    return;
  }

  if (rd[0].record_type == GNUNET_DNSPARSER_TYPE_TXT)
  {
    printf ("%s\n", (char *) rd[0].data);
  }
  else {
    printf ("DID Document is not a TXT record\n");
  }

  GNUNET_SCHEDULER_add_now (cleanup, NULL);
  ret = 0;
  return;
}

/**
 * @brief Resolve a DID given by the user.
 */
static void
resolve_did_document ()
{
  struct GNUNET_IDENTITY_PublicKey pkey;

  if (did == NULL)
  {
    printf ("Set DID option to resolve DID\n");
    GNUNET_SCHEDULER_add_now (cleanup, NULL);
    ret = 1;
    return;
  }

  get_pkey_from_attr_did (&pkey);

  GNUNET_GNS_lookup (gns_handle, GNUNET_GNS_EMPTY_LABEL_AT, &pkey, GNUNET_DNSPARSER_TYPE_TXT,
                     GNUNET_GNS_LO_DEFAULT, &print_did_document, NULL);
}


/**
 * @brief Signature of a callback function that is called after a did has been removed
 */
typedef void
(*remove_did_document_callback) (void *cls);

/**
 * @brief A Structure containing a cont and cls. Can be passed as a cls to a callback function
 *
 */
struct Event
{
  remove_did_document_callback cont;
  void *cls;
};

/**
 * @brief Implements the GNUNET_NAMESTORE_ContinuationWithStatus
 * Calls the callback function and cls in the event struct
 *
 * @param cls closure containing the event struct
 * @param success
 * @param emgs
 */
static void
remove_did_document_namestore_cb (void *cls, int32_t success, const char *emgs)
{
  struct Event *event;

  if (success != GNUNET_SYSERR)
  {
    event = (struct Event *) cls;

    if (event->cont != NULL)
    {
      event->cont (event->cls);
      free (event);
    }
    else {
      free (event);
      GNUNET_SCHEDULER_add_now (cleanup, NULL);
      ret = 0;
      return;
    }
  } else {
    printf ("Something went wrong when deleting the DID Document\n");

    if (emgs != NULL)
    {
      printf ("%s\n", emgs);
    }

    GNUNET_SCHEDULER_add_now (cleanup, NULL);
    ret = 0;
    return;
  }
}

/**
 * @brief Callback called after the ego has been locked up
 *
 * @param cls closure
 * @param ego the ego returned by the identity service
 */
static void
remove_did_document_ego_lookup_cb (void *cls, struct GNUNET_IDENTITY_Ego *ego)
{
  const struct GNUNET_IDENTITY_PrivateKey *skey =
    GNUNET_IDENTITY_ego_get_private_key (ego);

  GNUNET_NAMESTORE_records_store (namestore_handle,
                                  skey,
                                  GNUNET_GNS_EMPTY_LABEL_AT,
                                  0,
                                  NULL,
                                  &remove_did_document_namestore_cb,
                                  cls);
}

/**
 * @brief Remove a DID Document
 */
static void
remove_did_document (remove_did_document_callback cont, void *cls)
{
  struct Event *event;

  if (egoname == NULL)
  {
    printf ("Remove requieres an ego option\n");
    GNUNET_SCHEDULER_add_now (cleanup, NULL);
    ret = 1;
    return;
  }
  else {
    event = malloc (sizeof(*event));
    event->cont = cont;
    event->cls = cls;

    GNUNET_IDENTITY_ego_lookup (my_cfg,
                                egoname,
                                &remove_did_document_ego_lookup_cb,
                                (void *) event);
  }
}


/**
 * @brief Create a did generate did object
 *
 * @param pkey
 * @return void* Return pointer to the DID Document
 */
char *
create_did_generate (struct GNUNET_IDENTITY_PublicKey pkey)
{
  /* FIXME-MSC: I would prefer constants instead of magic numbers */
  char *pkey_str;  // Convert public key to string
  char did_str[71]; // 58 + 12 + 1 = 71
  char *didd_str;
  char verify_id_str[77]; // did_str len + "#key-1" = 71 + 6 = 77
  char *pkey_multibase_str;

  /* FIXME-MSC: This screams for a GNUNET_DID_identity_key_to_string() */
  char *b64;
  char pkx[34];
  pkx[0] = 0xed;
  pkx[1] = 0x01;
  memcpy (pkx + 2, &(pkey.eddsa_key), sizeof(pkey.eddsa_key));
  GNUNET_STRINGS_base64_encode (pkx, sizeof(pkx), &b64);

  GNUNET_asprintf (&pkey_multibase_str, "u%s", b64);

  json_t *didd;
  json_t *did_json;
  json_t *pkey_multibase_json;
  json_t *context_json;
  json_t *context_1_json;
  json_t *context_2_json;
  json_t *verify_json;
  json_t *verify_1_json;
  json_t *verify_1_type_json;
  json_t *verify_1_id_json;
  json_t *verify_relative_ref_json;
  json_t *auth_json;
  json_t *assert_json;

  /* FIXME-MSC: This screams for GNUNET_DID_identity_to_did() */
  pkey_str = GNUNET_IDENTITY_public_key_to_string (&pkey); // Convert public key to string
  sprintf (did_str, "did:reclaim:%s", pkey_str); // Convert the public key to a DID str
  sprintf (verify_id_str, "did:reclaim:%s#key-1", pkey_str); // Convert the public key to a DID str

  // sprintf(pkey_multibase_str, "V%s", pkey_str); // Convert the public key to MultiBase data format

  /* FIXME-MSC: This is effectively creating a DID Document default template for
   * the initial document.
   * Maybe this can be refactored to generate such a template for an identity?
   * Even if higher layers add/modify it, there should probably still be a
   * GNUNET_DID_document_template_from_identity()
   */
  // Create Json Strings
  did_json = json_string (did_str);
  pkey_multibase_json = json_string (pkey_multibase_str);

  context_1_json = json_string ("https://www.w3.org/ns/did/v1");
  context_2_json = json_string (
    "https://w3id.org/security/suites/ed25519-2020/v1");
  verify_1_id_json = json_string (verify_id_str);
  verify_1_type_json = json_string ("Ed25519VerificationKey2020");

  // Add a relative DID URL to reference a verifiation method
  // https://www.w3.org/TR/did-core/#relative-did-urls`
  verify_relative_ref_json = json_string ("#key-1");

  // Create DID Document
  didd = json_object ();

  // Add context
  context_json = json_array ();
  json_array_append (context_json, context_1_json);
  json_array_append (context_json, context_2_json);
  json_object_set (didd, "@context", context_json);

  // Add id
  json_object_set (didd, "id", did_json);

  // Add verification method
  verify_json = json_array ();
  verify_1_json = json_object ();
  json_object_set (verify_1_json, "id", verify_1_id_json);
  json_object_set (verify_1_json, "type", verify_1_type_json);
  json_object_set (verify_1_json, "controller", did_json);
  json_object_set (verify_1_json, "publicKeyMultiBase", pkey_multibase_json);
  json_array_append (verify_json, verify_1_json);
  json_object_set (didd, "verificationMethod", verify_json);

  // Add authentication method
  auth_json = json_array ();
  json_array_append (auth_json, verify_relative_ref_json);
  json_object_set (didd, "authentication", auth_json);

  // Add assertion method to issue a Verifiable Credential
  assert_json = json_array ();
  json_array_append (assert_json, verify_relative_ref_json);
  json_object_set (didd, "assertionMethod", assert_json);

  // Encode DID Document as JSON string
  didd_str = json_dumps (didd, JSON_INDENT (2));
  if (didd_str == NULL)
  {
    printf ("DID Document could not be encoded");
    GNUNET_SCHEDULER_add_now (&cleanup, NULL);
    ret = 1;
    return NULL;
  }

  // TODO: MORE FREEEEEEEE
  /* FIXME-MSC: json_t's are free'd using "json_decref". Also json_t usually
   * keeps a reference counter. Check jansson docs for how to use it.
   * Also: Use valgrind to find leaks.
   */
  free (pkey_multibase_str);
  free (b64);

  free (didd);
  free (did_json);
  free (pkey_multibase_json);
  free (context_json);
  free (context_1_json);
  free (context_2_json);
  free (verify_json);
  free (verify_1_json);
  free (verify_1_type_json);
  free (verify_1_id_json);
  free (auth_json);
  free (assert_json);
  free (verify_relative_ref_json);

  return didd_str;
}

/**
 * @brief Create a DID. Store DID in Namestore cb
 *
 */
static void
create_did_store_cb (void *cls, int32_t success, const char *emsg)
{
  GNUNET_SCHEDULER_add_now (&cleanup, NULL);
  ret = 0;
  return;
}

/**
 * @brief Create a did. Store DID in Namestore
 *
 * @param didd_str String endoced DID Docuement
 * @param ego Identity whos DID Document is stored
 */
static void
create_did_store (char *didd_str, struct GNUNET_IDENTITY_Ego *ego)
{

  struct GNUNET_TIME_Relative expire_time;
  struct GNUNET_GNSRECORD_Data record_data;
  const struct GNUNET_IDENTITY_PrivateKey *skey;

  if (GNUNET_STRINGS_fancy_time_to_relative ((NULL != expire) ?
                                             expire :
                                             GNUNET_DID_DEFAULT_DID_DOCUMENT_EXPIRATION_TIME,
                                             &expire_time) == GNUNET_OK)
  {
    record_data.data = didd_str;
    record_data.expiration_time = expire_time.rel_value_us;
    record_data.data_size = strlen (didd_str) + 1;
    record_data.record_type = GNUNET_GNSRECORD_typename_to_number ("TXT"),
    record_data.flags = GNUNET_GNSRECORD_RF_RELATIVE_EXPIRATION;

    skey = GNUNET_IDENTITY_ego_get_private_key (ego);

    GNUNET_NAMESTORE_records_store (namestore_handle,
                                    skey,
                                    GNUNET_GNS_EMPTY_LABEL_AT,
                                    1, //FIXME what if GNUNET_GNS_EMPTY_LABEL_AT has records
                                    &record_data,
                                    &create_did_store_cb,
                                    NULL);
  }
  else {
    printf ("Failed to read given expiration time\n");
    GNUNET_SCHEDULER_add_now (&cleanup, NULL);
    ret = 1;
    return;
  }
}

/**
 * @brief Create a did ego lockup cb
 *
 * @param cls
 * @param ego
 */
static void
create_did_ego_lockup_cb (void *cls, struct GNUNET_IDENTITY_Ego *ego)
{
  struct GNUNET_IDENTITY_PublicKey pkey;
  char *didd_str;

  if (ego == NULL)
  {
    printf ("EGO not found\n");
    GNUNET_SCHEDULER_add_now (&cleanup, NULL);
    ret = 1;
    return;
  }

  GNUNET_IDENTITY_ego_get_public_key (ego, &pkey);

  if (ntohl (pkey.type) != GNUNET_GNSRECORD_TYPE_EDKEY)
  {
    printf ("The EGO has to have an EDDSA key pair\n");
    GNUNET_SCHEDULER_add_now (&cleanup, NULL);
    ret = 1;
    return;
  }

  if (didd != NULL)
  {
    printf (
      "DID Docuement is read from \"did-document\" argument (EXPERIMENTAL)\n");
    didd_str = strdup (didd);
  }
  else {
    // Generate DID Docuement from public key
    didd_str = create_did_generate (pkey);
  }

  // Print DID Document to stdout
  printf ("%s\n", didd_str);

  // Store the DID Document
  create_did_store (didd_str, ego);

  // Save DID Document String to GNS
  free (didd_str);
}

/**
 * @brief Create a did document - Create a new identity first
 */
static void
create_did_document_ego_create_cb (void *cls,
                                   const struct GNUNET_IDENTITY_PrivateKey *pk,
                                   const char *emsg)
{

  if (emsg != NULL)
  {
    printf ("%s\n", emsg);
    GNUNET_SCHEDULER_add_now (&cleanup, NULL);
    ret = 1;
    return;
  }

  GNUNET_IDENTITY_ego_lookup (my_cfg,
                              egoname,
                              &create_did_ego_lockup_cb,
                              NULL);
}

/**
 * @brief Create a did document
 *
 */
static void
create_did_document ()
{
  if ((egoname != NULL) && (expire != NULL))
  {
    GNUNET_IDENTITY_create (identity_handle,
                            egoname,
                            NULL,
                            GNUNET_IDENTITY_TYPE_EDDSA,
                            &create_did_document_ego_create_cb,
                            egoname);
  }
  else {
    printf (
      "Set the EGO and the Expiration-time argument to create a new DID(-Document)\n");
    GNUNET_SCHEDULER_add_now (&cleanup, NULL);
    ret = 1;
    return;
  }
}


/**
 * @brief Replace a DID Docuemnt. Callback function after ego lockup
 *
 * @param cls
 * @param ego
 */
static void
replace_did_document_ego_lookup_cb (void *cls, struct GNUNET_IDENTITY_Ego *ego)
{
  create_did_store (didd, ego);
}

/**
 * @brief Replace a DID Document. Callback functiona after remove
 *
 * @param cls
 */
static void
replace_did_document_remove_cb (void *cls)
{
  GNUNET_IDENTITY_ego_lookup (my_cfg,
                              egoname,
                              &replace_did_document_ego_lookup_cb,
                              NULL);
}

/**
 * @brief Replace a DID Docuemnt
 *
 */
static void
replace_did_document ()
{
  if ((didd != NULL) && (expire != NULL))
  {
    remove_did_document (&replace_did_document_remove_cb, NULL);
  }
  else {
    printf (
      "Set the DID Document and expiration time argument to replace the DID Document\n");
    GNUNET_SCHEDULER_add_now (&cleanup, NULL);
    ret = 1;
    return;
  }
}

static void
post_ego_iteration (void *cls)
{
  if (1 == replace)
  {
    replace_did_document ();
  }
  else if (1 == get)
  {
    resolve_did_document ();
  }
  else if (1 == remove_did)
  {
    remove_did_document (NULL, NULL);
  }
  else if (1 == create)
  {
    create_did_document ();
  }
  else {
    // No Argument found
    GNUNET_SCHEDULER_add_now (&cleanup, NULL);
    return;
  }
}

static void
process_dids (void *cls, struct GNUNET_IDENTITY_Ego *ego,
              void **ctx, const char*name)
{
  char *did_str;

  if (ego == NULL)
  {
    if (1 == ego_exists)
    {
      GNUNET_SCHEDULER_add_now (&cleanup, NULL);
      return;
    }
    GNUNET_SCHEDULER_add_now (&post_ego_iteration, NULL);
    return;
  }
  if (NULL == name)
    return;
  if ((1 == create) &&
      (0 == strncmp (name, egoname, strlen (egoname))) &&
      (1 != ego_exists))
  {
    fprintf(stderr, "%s already exists!\n", egoname);
    ego_exists = 1;
    return;
  }
  if (1 == show_all)
  {
    did_str = ego_to_did (ego);
    printf ("%s\n", did_str);
    GNUNET_free (did_str);
    return;
  }
  if (1 == show)
  {
    if (0 == strncmp (name, egoname, strlen (egoname)))
    {
      did_str = ego_to_did (ego);
      printf ("%s\n", did_str);
      GNUNET_free (did_str);
      return;
    }
  }
}



static void
run (void *cls,
     char *const *args,
     const char *cfgfile,
     const struct GNUNET_CONFIGURATION_Handle *c)
{
  gns_handle = GNUNET_GNS_connect (c);
  namestore_handle = GNUNET_NAMESTORE_connect (c);
  my_cfg = c;

  // check if GNS_handle could connect
  if (gns_handle == NULL)
  {
    ret = 1;
    return;
  }

  // check if NAMESTORE_handle could connect
  if (namestore_handle == NULL)
  {
    GNUNET_SCHEDULER_add_now (&cleanup, NULL);
    ret = 1;
    return;
  }

  identity_handle = GNUNET_IDENTITY_connect (c, &process_dids, NULL);
  if (identity_handle == NULL)
  {
    GNUNET_SCHEDULER_add_now (&cleanup, NULL);
    ret = 1;
    return;
  }
}

int
main (int argc, char *const argv[])
{
  struct GNUNET_GETOPT_CommandLineOption options[] = {
    GNUNET_GETOPT_option_flag ('C',
                               "create",
                               gettext_noop (
                                 "Create a DID Document and display its DID"),
                               &create),
    GNUNET_GETOPT_option_flag ('g',
                               "get",
                               gettext_noop (
                                 "Get the DID Document associated with the given DID"),
                               &get),
    GNUNET_GETOPT_option_flag ('s',
                               "show",
                               gettext_noop ("Show the DID for a given ego"),
                               &show),
    GNUNET_GETOPT_option_flag ('r',
                               "remove",
                               gettext_noop (
                                 "Remove the DID"),
                               &remove_did),
    GNUNET_GETOPT_option_flag ('R',
                               "replace",
                               gettext_noop ("Replace the DID Document."),
                               &replace),
    GNUNET_GETOPT_option_flag ('A',
                               "--show-all",
                               gettext_noop ("Replace the DID Document."),
                               &show_all),
    GNUNET_GETOPT_option_string ('d',
                                 "did",
                                 "DID",
                                 gettext_noop (
                                   "The Decentralized Identity (DID)"),
                                 &did),
    GNUNET_GETOPT_option_string ('D',
                                 "--did-document",
                                 "JSON",
                                 gettext_noop (
                                   "The DID Document to store in GNUNET"),
                                 &didd),
    GNUNET_GETOPT_option_string ('e',
                                 "ego",
                                 "EGO",
                                 gettext_noop ("The name of the EGO"),
                                 &egoname),
    GNUNET_GETOPT_option_string ('t',
                                 "expiration-time",
                                 "TIME",
                                 gettext_noop (
                                   "The time until the DID Document is going to expire (e.g. 5d)"),
                                 &expire),
    GNUNET_GETOPT_OPTION_END
  };

  if (GNUNET_OK != GNUNET_PROGRAM_run (argc,
                                       argv,
                                       "gnunet-did",
                                       _ (
                                         "Manage Decentralized Identities (DIDs)"),
                                       options,
                                       &run,
                                       NULL))
    return 1;
  else
    return ret;
}
