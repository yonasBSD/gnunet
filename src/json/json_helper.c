/*
   This file is part of GNUnet
   Copyright (C) 2014, 2015, 2016 GNUnet e.V.

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
 * @file json/json_helper.c
 * @brief functions to generate specifciations for JSON parsing
 * @author Florian Dold
 * @author Benedikt Mueller
 * @author Christian Grothoff
 */
#include "platform.h"
#include "gnunet_json_lib.h"


struct GNUNET_JSON_Specification
GNUNET_JSON_spec_end ()
{
  struct GNUNET_JSON_Specification ret = {
    .parser = NULL,
    .cleaner = NULL,
    .cls = NULL
  };

  return ret;
}


/**
 * Parse given JSON object to fixed size data
 *
 * @param cls closure, NULL
 * @param root the json object representing data
 * @param[out] spec where to write the data
 * @return #GNUNET_OK upon successful parsing; #GNUNET_SYSERR upon error
 */
static enum GNUNET_GenericReturnValue
parse_fixed_data (void *cls,
                  json_t *root,
                  struct GNUNET_JSON_Specification *spec)
{
  const char *enc;
  unsigned int len;

  if (NULL == (enc = json_string_value (root)))
  {
    GNUNET_break_op (0);
    return GNUNET_SYSERR;
  }
  len = strlen (enc);
  if (((len * 5) / 8) != spec->ptr_size)
  {
    GNUNET_break_op (0);
    return GNUNET_SYSERR;
  }
  if (GNUNET_OK !=
      GNUNET_STRINGS_string_to_data (enc,
                                     len,
                                     spec->ptr,
                                     spec->ptr_size))
  {
    GNUNET_break_op (0);
    return GNUNET_SYSERR;
  }
  return GNUNET_OK;
}


struct GNUNET_JSON_Specification
GNUNET_JSON_spec_fixed (const char *name,
                        void *obj,
                        size_t size)
{
  struct GNUNET_JSON_Specification ret = {
    .parser = &parse_fixed_data,
    .cleaner = NULL,
    .cls = NULL,
    .field = name,
    .ptr = obj,
    .ptr_size = size,
    .size_ptr = NULL
  };

  return ret;
}


/**
 * Parse given JSON object to variable size data
 *
 * @param cls closure, NULL
 * @param root the json object representing data
 * @param[out] spec where to write the data
 * @return #GNUNET_OK upon successful parsing; #GNUNET_SYSERR upon error
 */
static int
parse_variable_data (void *cls,
                     json_t *root,
                     struct GNUNET_JSON_Specification *spec)
{
  const char *str;
  size_t size;
  void *data;

  str = json_string_value (root);
  if (NULL == str)
  {
    GNUNET_break_op (0);
    return GNUNET_SYSERR;
  }
  if (GNUNET_OK !=
      GNUNET_STRINGS_string_to_data_alloc (str,
                                           strlen (str),
                                           &data,
                                           &size))
  {
    GNUNET_break_op (0);
    return GNUNET_SYSERR;
  }
  *(void **) spec->ptr = data;
  *spec->size_ptr = size;
  return GNUNET_OK;
}


/**
 * Cleanup data left from parsing variable size data
 *
 * @param cls closure, NULL
 * @param[out] spec where to free the data
 */
static void
clean_variable_data (void *cls,
                     struct GNUNET_JSON_Specification *spec)
{
  (void) cls;
  if (0 != *spec->size_ptr)
  {
    GNUNET_free (*(void **) spec->ptr);
    *(void **) spec->ptr = NULL;
    *spec->size_ptr = 0;
  }
}


struct GNUNET_JSON_Specification
GNUNET_JSON_spec_varsize (const char *name,
                          void **obj,
                          size_t *size)
{
  struct GNUNET_JSON_Specification ret = {
    .parser = &parse_variable_data,
    .cleaner = &clean_variable_data,
    .cls = NULL,
    .field = name,
    .ptr = obj,
    .ptr_size = 0,
    .size_ptr = size
  };

  *obj = NULL;
  *size = 0;
  return ret;
}


/**
 * Parse given JSON object to string.
 *
 * @param cls closure, NULL
 * @param root the json object representing data
 * @param[out] spec where to write the data
 * @return #GNUNET_OK upon successful parsing; #GNUNET_SYSERR upon error
 */
static int
parse_string (void *cls,
              json_t *root,
              struct GNUNET_JSON_Specification *spec)
{
  const char *str;

  (void) cls;
  str = json_string_value (root);
  if (NULL == str)
  {
    GNUNET_break_op (0);
    return GNUNET_SYSERR;
  }
  *(const char **) spec->ptr = str;
  return GNUNET_OK;
}


struct GNUNET_JSON_Specification
GNUNET_JSON_spec_string (const char *name,
                         const char **strptr)
{
  struct GNUNET_JSON_Specification ret = {
    .parser = &parse_string,
    .cleaner = NULL,
    .cls = NULL,
    .field = name,
    .ptr = strptr,
    .ptr_size = 0,
    .size_ptr = NULL
  };

  *strptr = NULL;
  return ret;
}


/**
 * Parse given JSON object to a JSON object. (Yes, trivial.)
 *
 * @param cls closure, NULL
 * @param root the json object representing data
 * @param[out] spec where to write the data
 * @return #GNUNET_OK upon successful parsing; #GNUNET_SYSERR upon error
 */
static int
parse_object (void *cls,
              json_t *root,
              struct GNUNET_JSON_Specification *spec)
{
  if (! (json_is_object (root) || json_is_array (root)))
  {
    GNUNET_break_op (0);
    return GNUNET_SYSERR;
  }
  json_incref (root);
  *(json_t **) spec->ptr = root;
  return GNUNET_OK;
}


/**
 * Cleanup data left from parsing JSON object.
 *
 * @param cls closure, NULL
 * @param[out] spec where to free the data
 */
static void
clean_object (void *cls,
              struct GNUNET_JSON_Specification *spec)
{
  json_t **ptr = (json_t **) spec->ptr;

  if (NULL != *ptr)
  {
    json_decref (*ptr);
    *ptr = NULL;
  }
}


struct GNUNET_JSON_Specification
GNUNET_JSON_spec_json (const char *name,
                       json_t **jsonp)
{
  struct GNUNET_JSON_Specification ret = {
    .parser = &parse_object,
    .cleaner = &clean_object,
    .cls = NULL,
    .field = name,
    .ptr = jsonp,
    .ptr_size = 0,
    .size_ptr = NULL
  };

  *jsonp = NULL;
  return ret;
}


/**
 * Parse given JSON object to a bool.
 *
 * @param cls closure, NULL
 * @param root the json object representing data
 * @param[out] spec where to write the data
 * @return #GNUNET_OK upon successful parsing; #GNUNET_SYSERR upon error
 */
static int
parse_bool (void *cls,
            json_t *root,
            struct GNUNET_JSON_Specification *spec)
{
  bool *b = spec->ptr;

  if (json_true () == root)
  {
    *b = true;
    return GNUNET_OK;
  }
  if (json_false () == root)
  {
    *b = false;
    return GNUNET_OK;
  }
  GNUNET_break_op (0);
  return GNUNET_SYSERR;
}


struct GNUNET_JSON_Specification
GNUNET_JSON_spec_bool (const char *name,
                       bool *b)
{
  struct GNUNET_JSON_Specification ret = {
    .parser = &parse_bool,
    .cleaner = NULL,
    .cls = NULL,
    .field = name,
    .ptr = b,
    .ptr_size = sizeof(bool),
    .size_ptr = NULL
  };

  return ret;
}


/**
 * Parse given JSON object to a uint8_t.
 *
 * @param cls closure, NULL
 * @param root the json object representing data
 * @param[out] spec where to write the data
 * @return #GNUNET_OK upon successful parsing; #GNUNET_SYSERR upon error
 */
static int
parse_u8 (void *cls,
          json_t *root,
          struct GNUNET_JSON_Specification *spec)
{
  json_int_t val;
  uint8_t *up = spec->ptr;

  if (! json_is_integer (root))
  {
    GNUNET_break_op (0);
    return GNUNET_SYSERR;
  }
  val = json_integer_value (root);
  if ((0 > val) || (val > UINT8_MAX))
  {
    GNUNET_break_op (0);
    return GNUNET_SYSERR;
  }
  *up = (uint8_t) val;
  return GNUNET_OK;
}


struct GNUNET_JSON_Specification
GNUNET_JSON_spec_uint8 (const char *name,
                        uint8_t *u8)
{
  struct GNUNET_JSON_Specification ret = {
    .parser = &parse_u8,
    .cleaner = NULL,
    .cls = NULL,
    .field = name,
    .ptr = u8,
    .ptr_size = sizeof(uint8_t),
    .size_ptr = NULL
  };

  return ret;
}


/**
 * Parse given JSON object to a uint16_t.
 *
 * @param cls closure, NULL
 * @param root the json object representing data
 * @param[out] spec where to write the data
 * @return #GNUNET_OK upon successful parsing; #GNUNET_SYSERR upon error
 */
static int
parse_u16 (void *cls,
           json_t *root,
           struct GNUNET_JSON_Specification *spec)
{
  json_int_t val;
  uint16_t *up = spec->ptr;

  if (! json_is_integer (root))
  {
    GNUNET_break_op (0);
    return GNUNET_SYSERR;
  }
  val = json_integer_value (root);
  if ((0 > val) || (val > UINT16_MAX))
  {
    GNUNET_break_op (0);
    return GNUNET_SYSERR;
  }
  *up = (uint16_t) val;
  return GNUNET_OK;
}


struct GNUNET_JSON_Specification
GNUNET_JSON_spec_uint16 (const char *name,
                         uint16_t *u16)
{
  struct GNUNET_JSON_Specification ret = {
    .parser = &parse_u16,
    .cleaner = NULL,
    .cls = NULL,
    .field = name,
    .ptr = u16,
    .ptr_size = sizeof(uint16_t),
    .size_ptr = NULL
  };

  return ret;
}


/**
 * Parse given JSON object to a uint32_t.
 *
 * @param cls closure, NULL
 * @param root the json object representing data
 * @param[out] spec where to write the data
 * @return #GNUNET_OK upon successful parsing; #GNUNET_SYSERR upon error
 */
static int
parse_u32 (void *cls,
           json_t *root,
           struct GNUNET_JSON_Specification *spec)
{
  json_int_t val;
  uint32_t *up = spec->ptr;

  if (! json_is_integer (root))
  {
    GNUNET_break_op (0);
    return GNUNET_SYSERR;
  }
  val = json_integer_value (root);
  if ((0 > val) || (val > UINT32_MAX))
  {
    GNUNET_break_op (0);
    return GNUNET_SYSERR;
  }
  *up = (uint32_t) val;
  return GNUNET_OK;
}


struct GNUNET_JSON_Specification
GNUNET_JSON_spec_uint32 (const char *name,
                         uint32_t *u32)
{
  struct GNUNET_JSON_Specification ret = {
    .parser = &parse_u32,
    .cleaner = NULL,
    .cls = NULL,
    .field = name,
    .ptr = u32,
    .ptr_size = sizeof(uint32_t),
    .size_ptr = NULL
  };

  return ret;
}


/**
 * Parse given JSON object to a uint64_t.
 *
 * @param cls closure, NULL
 * @param root the json object representing data
 * @param[out] spec where to write the data
 * @return #GNUNET_OK upon successful parsing; #GNUNET_SYSERR upon error
 */
static int
parse_u64 (void *cls,
           json_t *root,
           struct GNUNET_JSON_Specification *spec)
{
  json_int_t val;
  uint64_t *up = spec->ptr;

  if (! json_is_integer (root))
  {
    GNUNET_break_op (0);
    return GNUNET_SYSERR;
  }
  val = json_integer_value (root);
  *up = (uint64_t) val;
  return GNUNET_OK;
}


struct GNUNET_JSON_Specification
GNUNET_JSON_spec_uint64 (const char *name,
                         uint64_t *u64)
{
  struct GNUNET_JSON_Specification ret = {
    .parser = &parse_u64,
    .cleaner = NULL,
    .cls = NULL,
    .field = name,
    .ptr = u64,
    .ptr_size = sizeof(uint64_t),
    .size_ptr = NULL
  };

  return ret;
}


/**
 * Parse given JSON object to a int64_t.
 *
 * @param cls closure, NULL
 * @param root the json object representing data
 * @param[out] spec where to write the data
 * @return #GNUNET_OK upon successful parsing; #GNUNET_SYSERR upon error
 */
static int
parse_i64 (void *cls,
           json_t *root,
           struct GNUNET_JSON_Specification *spec)
{
  json_int_t val;
  int64_t *up = spec->ptr;

  if (! json_is_integer (root))
  {
    GNUNET_break_op (0);
    return GNUNET_SYSERR;
  }
  val = json_integer_value (root);
  *up = (int64_t) val;
  return GNUNET_OK;
}


struct GNUNET_JSON_Specification
GNUNET_JSON_spec_int64 (const char *name,
                        int64_t *i64)
{
  struct GNUNET_JSON_Specification ret = {
    .parser = &parse_i64,
    .cleaner = NULL,
    .cls = NULL,
    .field = name,
    .ptr = i64,
    .ptr_size = sizeof(int64_t),
    .size_ptr = NULL
  };

  return ret;
}


/* ************ GNUnet-specific parser specifications ******************* */

/**
 * Parse given JSON object to a timestamp.
 *
 * @param cls closure, NULL
 * @param root the json object representing data
 * @param[out] spec where to write the data
 * @return #GNUNET_OK upon successful parsing; #GNUNET_SYSERR upon error
 */
static enum GNUNET_GenericReturnValue
parse_timestamp (void *cls,
                 json_t *root,
                 struct GNUNET_JSON_Specification *spec)
{
  struct GNUNET_TIME_Timestamp *ts = spec->ptr;
  json_t *json_t_s;
  unsigned long long int tval;

  if (! json_is_object (root))
  {
    GNUNET_break_op (0);
    return GNUNET_SYSERR;
  }
  json_t_s = json_object_get (root,
                              "t_s");
  if (json_is_integer (json_t_s))
  {
    tval = json_integer_value (json_t_s);
    /* Time is in seconds in JSON, but in microseconds in GNUNET_TIME_Absolute */
    ts->abs_time.abs_value_us
      = tval * GNUNET_TIME_UNIT_SECONDS.rel_value_us;
    if (ts->abs_time.abs_value_us
        / GNUNET_TIME_UNIT_SECONDS.rel_value_us
        != tval)
    {
      /* Integer overflow */
      GNUNET_break_op (0);
      return GNUNET_SYSERR;
    }
    return GNUNET_OK;
  }
  if (json_is_string (json_t_s))
  {
    const char *val;

    val = json_string_value (json_t_s);
    if ((0 == strcasecmp (val,
                          "never")))
    {
      ts->abs_time = GNUNET_TIME_UNIT_FOREVER_ABS;
      return GNUNET_OK;
    }
    GNUNET_break_op (0);
    return GNUNET_SYSERR;
  }
  GNUNET_break_op (0);
  return GNUNET_SYSERR;
}


struct GNUNET_JSON_Specification
GNUNET_JSON_spec_timestamp (const char *name,
                            struct GNUNET_TIME_Timestamp *t)
{
  struct GNUNET_JSON_Specification ret = {
    .parser = &parse_timestamp,
    .field = name,
    .ptr = t,
    .ptr_size = sizeof(struct GNUNET_TIME_Timestamp)
  };

  return ret;
}


/**
 * Parse given JSON object to absolute time.
 *
 * @param cls closure, NULL
 * @param root the json object representing data
 * @param[out] spec where to write the data
 * @return #GNUNET_OK upon successful parsing; #GNUNET_SYSERR upon error
 */
static enum GNUNET_GenericReturnValue
parse_timestamp_nbo (void *cls,
                     json_t *root,
                     struct GNUNET_JSON_Specification *spec)
{
  struct GNUNET_TIME_TimestampNBO *ts = spec->ptr;
  struct GNUNET_TIME_Timestamp a;
  struct GNUNET_JSON_Specification ispec;

  ispec = *spec;
  ispec.parser = &parse_timestamp;
  ispec.ptr = &a;
  if (GNUNET_OK !=
      parse_timestamp (NULL,
                       root,
                       &ispec))
    return GNUNET_SYSERR;
  *ts = GNUNET_TIME_timestamp_hton (a);
  return GNUNET_OK;
}


struct GNUNET_JSON_Specification
GNUNET_JSON_spec_timestamp_nbo (const char *name,
                                struct GNUNET_TIME_TimestampNBO *at)
{
  struct GNUNET_JSON_Specification ret = {
    .parser = &parse_timestamp_nbo,
    .field = name,
    .ptr = at,
    .ptr_size = sizeof(struct GNUNET_TIME_TimestampNBO)
  };

  return ret;
}


/**
 * Parse given JSON object to relative time.
 *
 * @param cls closure, NULL
 * @param root the json object representing data
 * @param[out] spec where to write the data
 * @return #GNUNET_OK upon successful parsing; #GNUNET_SYSERR upon error
 */
static enum GNUNET_GenericReturnValue
parse_rel_time (void *cls,
                json_t *root,
                struct GNUNET_JSON_Specification *spec)
{
  struct GNUNET_TIME_Relative *rel = spec->ptr;
  json_t *json_d_us;
  unsigned long long int tval;

  if (! json_is_object (root))
  {
    GNUNET_break_op (0);
    return GNUNET_SYSERR;
  }
  json_d_us = json_object_get (root,
                               "d_us");
  if (json_is_integer (json_d_us))
  {
    tval = json_integer_value (json_d_us);
    if (tval >= (1LLU << 53))
    {
      /* value is larger than allowed */
      GNUNET_break_op (0);
      return GNUNET_SYSERR;
    }
    rel->rel_value_us = tval;
    return GNUNET_OK;
  }
  if (json_is_string (json_d_us))
  {
    const char *val;

    val = json_string_value (json_d_us);
    if ((0 == strcasecmp (val,
                          "forever")))
    {
      *rel = GNUNET_TIME_UNIT_FOREVER_REL;
      return GNUNET_OK;
    }
    GNUNET_break_op (0);
    return GNUNET_SYSERR;
  }
  GNUNET_break_op (0);
  return GNUNET_SYSERR;
}


struct GNUNET_JSON_Specification
GNUNET_JSON_spec_relative_time (const char *name,
                                struct GNUNET_TIME_Relative *rt)
{
  struct GNUNET_JSON_Specification ret = {
    .parser = &parse_rel_time,
    .field = name,
    .ptr = rt,
    .ptr_size = sizeof(struct GNUNET_TIME_Relative)
  };

  return ret;
}


/**
 * Parse given JSON object to RSA public key.
 *
 * @param cls closure, NULL
 * @param root the json object representing data
 * @param[out] spec where to write the data
 * @return #GNUNET_OK upon successful parsing; #GNUNET_SYSERR upon error
 */
static enum GNUNET_GenericReturnValue
parse_rsa_public_key (void *cls,
                      json_t *root,
                      struct GNUNET_JSON_Specification *spec)
{
  struct GNUNET_CRYPTO_RsaPublicKey **pk = spec->ptr;
  const char *enc;
  char *buf;
  size_t len;
  size_t buf_len;

  if (NULL == (enc = json_string_value (root)))
  {
    GNUNET_break_op (0);
    return GNUNET_SYSERR;
  }
  len = strlen (enc);
  buf_len = (len * 5) / 8;
  buf = GNUNET_malloc (buf_len);
  if (GNUNET_OK !=
      GNUNET_STRINGS_string_to_data (enc,
                                     len,
                                     buf,
                                     buf_len))
  {
    GNUNET_break_op (0);
    GNUNET_free (buf);
    return GNUNET_SYSERR;
  }
  if (NULL == (*pk = GNUNET_CRYPTO_rsa_public_key_decode (buf,
                                                          buf_len)))
  {
    GNUNET_break_op (0);
    GNUNET_free (buf);
    return GNUNET_SYSERR;
  }
  GNUNET_free (buf);
  return GNUNET_OK;
}


/**
 * Cleanup data left from parsing RSA public key.
 *
 * @param cls closure, NULL
 * @param[out] spec where to free the data
 */
static void
clean_rsa_public_key (void *cls,
                      struct GNUNET_JSON_Specification *spec)
{
  struct GNUNET_CRYPTO_RsaPublicKey **pk = spec->ptr;

  if (NULL != *pk)
  {
    GNUNET_CRYPTO_rsa_public_key_free (*pk);
    *pk = NULL;
  }
}


struct GNUNET_JSON_Specification
GNUNET_JSON_spec_rsa_public_key (const char *name,
                                 struct GNUNET_CRYPTO_RsaPublicKey **pk)
{
  struct GNUNET_JSON_Specification ret = {
    .parser = &parse_rsa_public_key,
    .cleaner = &clean_rsa_public_key,
    .field = name,
    .ptr = pk
  };

  *pk = NULL;
  return ret;
}


/**
 * Parse given JSON object to RSA signature.
 *
 * @param cls closure, NULL
 * @param root the json object representing data
 * @param[out] spec where to write the data
 * @return #GNUNET_OK upon successful parsing; #GNUNET_SYSERR upon error
 */
static enum GNUNET_GenericReturnValue
parse_rsa_signature (void *cls,
                     json_t *root,
                     struct GNUNET_JSON_Specification *spec)
{
  struct GNUNET_CRYPTO_RsaSignature **sig = spec->ptr;
  size_t size;
  const char *str;
  int res;
  void *buf;

  str = json_string_value (root);
  if (NULL == str)
  {
    GNUNET_break_op (0);
    return GNUNET_SYSERR;
  }
  size = (strlen (str) * 5) / 8;
  buf = GNUNET_malloc (size);
  res = GNUNET_STRINGS_string_to_data (str,
                                       strlen (str),
                                       buf,
                                       size);
  if (GNUNET_OK != res)
  {
    GNUNET_free (buf);
    GNUNET_break_op (0);
    return GNUNET_SYSERR;
  }
  if (NULL == (*sig = GNUNET_CRYPTO_rsa_signature_decode (buf,
                                                          size)))
  {
    GNUNET_break_op (0);
    GNUNET_free (buf);
    return GNUNET_SYSERR;
  }
  GNUNET_free (buf);
  return GNUNET_OK;
}


/**
 * Cleanup data left from parsing RSA signature.
 *
 * @param cls closure, NULL
 * @param[out] spec where to free the data
 */
static void
clean_rsa_signature (void *cls,
                     struct GNUNET_JSON_Specification *spec)
{
  struct GNUNET_CRYPTO_RsaSignature  **sig = spec->ptr;

  if (NULL != *sig)
  {
    GNUNET_CRYPTO_rsa_signature_free (*sig);
    *sig = NULL;
  }
}


struct GNUNET_JSON_Specification
GNUNET_JSON_spec_rsa_signature (const char *name,
                                struct GNUNET_CRYPTO_RsaSignature **sig)
{
  struct GNUNET_JSON_Specification ret = {
    .parser = &parse_rsa_signature,
    .cleaner = &clean_rsa_signature,
    .cls = NULL,
    .field = name,
    .ptr = sig,
    .ptr_size = 0,
    .size_ptr = NULL
  };

  *sig = NULL;
  return ret;
}


/**
 * Parse given JSON object to an int as a boolean.
 *
 * @param cls closure, NULL
 * @param root the json object representing data
 * @param[out] spec where to write the data
 * @return #GNUNET_OK upon successful parsing; #GNUNET_SYSERR upon error
 */
static enum GNUNET_GenericReturnValue
parse_boolean (void *cls,
               json_t *root,
               struct GNUNET_JSON_Specification *spec)
{
  int *bp = spec->ptr;

  if (! json_is_boolean (root))
  {
    GNUNET_break_op (0);
    return GNUNET_SYSERR;
  }
  *bp = json_boolean_value (root) ? GNUNET_YES : GNUNET_NO;
  return GNUNET_OK;
}


struct GNUNET_JSON_Specification
GNUNET_JSON_spec_boolean (const char *name,
                          int *boolean)
{
  struct GNUNET_JSON_Specification ret = {
    .parser = &parse_boolean,
    .cleaner = NULL,
    .cls = NULL,
    .field = name,
    .ptr = boolean,
    .ptr_size = sizeof(int),
    .size_ptr = NULL
  };

  return ret;
}


/* end of json_helper.c */
