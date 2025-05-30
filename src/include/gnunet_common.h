/*
     This file is part of GNUnet.
     Copyright (C) 2006-2022 GNUnet e.V.

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
 * @addtogroup libgnunetutil
 * Multi-function utilities library for GNUnet programs
 * @{
 *
 * @file include/gnunet_common.h
 * @brief commonly used definitions; globals in this file
 *        are exempt from the rule that the module name ("common")
 *        must be part of the symbol name.
 *
 * @author Christian Grothoff
 * @author Nils Durner
 * @author Martin Schanzenbach
 *
 * @defgroup logging Logging
 * @see [Documentation](https://gnunet.org/logging)
 *
 * @defgroup memory Memory management
 */
#ifndef GNUNET_COMMON_H
#define GNUNET_COMMON_H

#include "gnunet_config.h"

#include <stdbool.h>
#include <stdlib.h>
#include <sys/socket.h>
#include <sys/un.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <stdint.h>
#include <stdarg.h>
#include <sys/types.h>

#if defined(__FreeBSD__)

#include <sys/endian.h>
#define bswap_32(x) bswap32 (x)
#define bswap_64(x) bswap64 (x)

#elif defined(__OpenBSD__)

#define bswap_32(x) swap32 (x)
#define bswap_64(x) swap64 (x)

#elif defined(__NetBSD__)

#include <machine/bswap.h>
#if defined(__BSWAP_RENAME) && ! defined(__bswap_32)
#define bswap_32(x) bswap32 (x)
#define bswap_64(x) bswap64 (x)
#endif

#elif defined(__linux__) || defined(GNU)
#include <byteswap.h>
#endif


/* This is also included in platform.h, but over there a couple of
   GNUnet-specific gettext-related macros are defined in addition to including
   the header file.  Because this header file uses gettext, this include
   statement makes sure gettext macros are defined even when platform.h is
   unavailable. */
#ifndef _LIBGETTEXT_H
#include "gettext.h"
#endif

#ifdef __cplusplus
extern "C" {
#if 0 /* keep Emacsens' auto-indent happy */
}
#endif
#endif

/**
 * Version of the API (for entire gnunetutil.so library).
 */
#define GNUNET_UTIL_VERSION 0x000A0104


/**
 * Named constants for return values.  The following invariants hold:
 * `GNUNET_NO == 0` (to allow `if (GNUNET_NO)`) `GNUNET_OK !=
 * GNUNET_SYSERR`, `GNUNET_OK != GNUNET_NO`, `GNUNET_NO !=
 * GNUNET_SYSERR` and finally `GNUNET_YES != GNUNET_NO`.
 */
enum GNUNET_GenericReturnValue
{
  GNUNET_SYSERR = -1,
  GNUNET_NO = 0,
  GNUNET_OK = 1,
  /* intentionally identical to #GNUNET_OK! */
  GNUNET_YES = 1,
};


#define GNUNET_MIN(a, b) (((a) < (b)) ? (a) : (b))

#define GNUNET_MAX(a, b) (((a) > (b)) ? (a) : (b))

/* some systems use one underscore only, and mingw uses no underscore... */
#ifndef __BYTE_ORDER
#ifdef _BYTE_ORDER
#define __BYTE_ORDER _BYTE_ORDER
#else
#ifdef BYTE_ORDER
#define __BYTE_ORDER BYTE_ORDER
#endif
#endif
#endif
#ifndef __BIG_ENDIAN
#ifdef _BIG_ENDIAN
#define __BIG_ENDIAN _BIG_ENDIAN
#else
#ifdef BIG_ENDIAN
#define __BIG_ENDIAN BIG_ENDIAN
#endif
#endif
#endif
#ifndef __LITTLE_ENDIAN
#ifdef _LITTLE_ENDIAN
#define __LITTLE_ENDIAN _LITTLE_ENDIAN
#else
#ifdef LITTLE_ENDIAN
#define __LITTLE_ENDIAN LITTLE_ENDIAN
#endif
#endif
#endif

/**
 * @ingroup logging
 * define #GNUNET_EXTRA_LOGGING if using this header outside the GNUnet source
 * tree where gnunet_config.h is unavailable
 */
#ifndef GNUNET_EXTRA_LOGGING
#define GNUNET_EXTRA_LOGGING 1
#endif

/**
 * Endian operations
 */

#if defined(bswap_16) || defined(bswap_32) || defined(bswap_64)
#define BYTE_SWAP_16(x) bswap_16 (x)
#define BYTE_SWAP_32(x) bswap_32 (x)
#define BYTE_SWAP_64(x) bswap_64 (x)
#else
#define BYTE_SWAP_16(x) ((((x) & 0x00ff) << 8) | (((x) & 0xff00) >> 8))

#define BYTE_SWAP_32(x)                                   \
        ((((x) & 0x000000ffU) << 24) | (((x) & 0x0000ff00U) << 8)   \
         | (((x) & 0x00ff0000U) >> 8) | (((x) & 0xff000000U) >> 24))

#define BYTE_SWAP_64(x)                                                      \
        ((((x) & 0x00000000000000ffUL) << 56) | (((x) & 0x000000000000ff00UL) << \
                                                 40)   \
         | (((x) & 0x0000000000ff0000UL) << 24) | (((x) & 0x00000000ff000000UL) \
                                                   << 8)    \
         | (((x) & 0x000000ff00000000UL) >> 8) | (((x) & 0x0000ff0000000000UL) \
                                                  >> 24)    \
         | (((x) & 0x00ff000000000000UL) >> 40) | (((x) & 0xff00000000000000UL) \
                                                   >> \
                                                   56))
#endif

#if __BYTE_ORDER == __LITTLE_ENDIAN
#define GNUNET_htobe16(x) BYTE_SWAP_16 (x)
#define GNUNET_htole16(x) (x)
#define GNUNET_be16toh(x) BYTE_SWAP_16 (x)
#define GNUNET_le16toh(x) (x)

#define GNUNET_htobe32(x) BYTE_SWAP_32 (x)
#define GNUNET_htole32(x) (x)
#define GNUNET_be32toh(x) BYTE_SWAP_32 (x)
#define GNUNET_le32toh(x) (x)

#define GNUNET_htobe64(x) BYTE_SWAP_64 (x)
#define GNUNET_htole64(x) (x)
#define GNUNET_be64toh(x) BYTE_SWAP_64 (x)
#define GNUNET_le64toh(x) (x)
#endif
#if __BYTE_ORDER == __BIG_ENDIAN
#define GNUNET_htobe16(x) (x)
#define GNUNET_htole16(x) BYTE_SWAP_16 (x)
#define GNUNET_be16toh(x) (x)
#define GNUNET_le16toh(x) BYTE_SWAP_16 (x)

#define GNUNET_htobe32(x) (x)
#define GNUNET_htole32(x) BYTE_SWAP_32 (x)
#define GNUNET_be32toh(x) (x)
#define GNUNET_le32toh(x) BYTE_SWAP_32 (x)

#define GNUNET_htobe64(x) (x)
#define GNUNET_htole64(x) BYTE_SWAP_64 (x)
#define GNUNET_be64toh(x) (x)
#define GNUNET_le64toh(x) BYTE_SWAP_64 (x)
#endif


/**
 * Macro used to avoid using 0 for the length of a variable-size
 * array (Non-Zero-Length).
 *
 * Basically, C standard says that "int[n] x;" is undefined if n=0.
 * This was supposed to prevent issues with pointer aliasing.
 * However, C compilers may conclude that n!=0 as n=0 would be
 * undefined, and then optimize under the assumption n!=0, which
 * could cause actual issues.  Hence, when initializing an array
 * on the stack with a variable-length that might be zero, write
 * "int[GNUNET_NZL(n)] x;" instead of "int[n] x".
 */
#define GNUNET_NZL(l) GNUNET_MAX (1, l)


/**
 * gcc-ism to get packed structs.
 */
#define GNUNET_PACKED __attribute__ ((packed))

/**
 * gcc-ism to get gcc bitfield layout when compiling with -mms-bitfields
 */
#define GNUNET_GCC_STRUCT_LAYOUT

/**
 * gcc-ism to force alignment; we use this to align char-arrays
 * that may then be cast to 'struct's.  See also gcc
 * bug #33594.
 */
#ifdef __BIGGEST_ALIGNMENT__
#define GNUNET_ALIGN __attribute__ ((aligned (__BIGGEST_ALIGNMENT__)))
#else
#define GNUNET_ALIGN __attribute__ ((aligned (8)))
#endif

/**
 * gcc-ism to document unused arguments
 */
#define GNUNET_UNUSED __attribute__ ((unused))

/**
 * gcc-ism to document functions that don't return
 */
#define GNUNET_NORETURN __attribute__ ((noreturn))

/**
 * Define as empty, GNUNET_PACKED should suffice, but this won't work on W32
 */
#define GNUNET_NETWORK_STRUCT_BEGIN

/**
 * Define as empty, GNUNET_PACKED should suffice, but this won't work on W32;
 */
#define GNUNET_NETWORK_STRUCT_END

/* ************************ super-general types *********************** */

GNUNET_NETWORK_STRUCT_BEGIN

/**
 * @brief A 512-bit hashcode.  These are the default length for GNUnet, using SHA-512.
 */
struct GNUNET_HashCode
{
  uint32_t bits[512 / 8 / sizeof(uint32_t)];  /* = 16 */
};


/**
 * @brief A 256-bit hashcode.  Used under special conditions, like when space
 * is critical and security is not impacted by it.
 */
struct GNUNET_ShortHashCode
{
  uint32_t bits[256 / 8 / sizeof(uint32_t)];  /* = 8 */
};


/**
 * A UUID, a 128 bit "random" value.  We OFTEN use
 * timeflakes (see: https://github.com/anthonynsimon/timeflake),
 * where only 80 bits are random and the rest encodes
 * a timestamp to improve database access.
 *
 * See #GNUNET_CRYPTO_random_timeflake().
 */
struct GNUNET_Uuid
{
  /**
   * 128 random bits.
   */
  uint32_t value[4];
};


/**
 * Header for all communications.
 */
struct GNUNET_MessageHeader
{
  /**
   * The length of the struct (in bytes, including the length field itself),
   * in big-endian format.
   */
  uint16_t size GNUNET_PACKED;

  /**
   * The type of the message (GNUNET_MESSAGE_TYPE_XXXX), in big-endian format.
   */
  uint16_t type GNUNET_PACKED;
};


/**
 * Identifier for an asynchronous execution context.
 */
struct GNUNET_AsyncScopeId
{
  uint32_t bits[16 / sizeof(uint32_t)];  /* = 16 bytes */
};

GNUNET_NETWORK_STRUCT_END


/**
 * Saved async scope identifier or root scope.
 */
struct GNUNET_AsyncScopeSave
{
  /**
   * Saved scope.  Unused if 'have_scope==GNUNET_NO'.
   */
  struct GNUNET_AsyncScopeId scope_id;

  /**
   * GNUNET_YES unless this saved scope is the unnamed root scope.
   */
  int have_scope;
};


/**
 * Function called with a filename.
 *
 * @param cls closure
 * @param filename complete filename (absolute path)
 * @return #GNUNET_OK to continue to iterate,
 *  #GNUNET_NO to stop iteration with no error,
 *  #GNUNET_SYSERR to abort iteration with error!
 */
typedef enum GNUNET_GenericReturnValue
(*GNUNET_FileNameCallback) (void *cls,
                            const char *filename);


/**
 * Generic continuation callback.
 *
 * @param cls  Closure.
 */
typedef void
(*GNUNET_ContinuationCallback) (void *cls);


/**
 * Function called with the result of an asynchronous operation.
 *
 * @param cls
 *        Closure.
 * @param result_code
 *        Result code for the operation.
 * @param data
 *        Data result for the operation.
 * @param data_size
 *        Size of @a data.
 */
typedef void
(*GNUNET_ResultCallback) (void *cls,
                          int64_t result_code,
                          const void *data,
                          uint16_t data_size);


/* ****************************** logging ***************************** */

/**
 * @ingroup logging
 * Types of errors.
 */
enum GNUNET_ErrorType
{
  GNUNET_ERROR_TYPE_UNSPECIFIED = -1,
  GNUNET_ERROR_TYPE_NONE = 0,
  GNUNET_ERROR_TYPE_ERROR = 1,
  GNUNET_ERROR_TYPE_WARNING = 2,
  /* UX: We need a message type that is output by
   * default without looking like there is a problem.
   */
  GNUNET_ERROR_TYPE_MESSAGE = 4,
  GNUNET_ERROR_TYPE_INFO = 8,
  GNUNET_ERROR_TYPE_DEBUG = 16,
  GNUNET_ERROR_TYPE_INVALID = 32,
  GNUNET_ERROR_TYPE_BULK = 64
};


/**
 * @ingroup logging
 * User-defined handler for log messages.
 *
 * @param cls closure
 * @param kind severeity
 * @param component what component is issuing the message?
 * @param date when was the message logged?
 * @param message what is the message
 */
typedef void
(*GNUNET_Logger) (void *cls,
                  enum GNUNET_ErrorType kind,
                  const char *component,
                  const char *date,
                  const char *message);


/**
 * @ingroup logging
 * Get the number of log calls that are going to be skipped
 *
 * @return number of log calls to be ignored
 */
int
GNUNET_get_log_skip (void);


#if ! defined(GNUNET_CULL_LOGGING)
int
GNUNET_get_log_call_status (int caller_level,
                            const char *comp,
                            const char *file,
                            const char *function,
                            int line);

#endif

/* *INDENT-OFF* */
/**
 * @ingroup logging
 * Main log function.
 *
 * @param kind how serious is the error?
 * @param message what is the message (format string)
 * @param ... arguments for format string
 */
void
GNUNET_log_nocheck (enum GNUNET_ErrorType kind,
                    const char *message,
                    ...)
__attribute__ ((format (printf, 2, 3)));

/* from glib */
#if defined(__GNUC__) && (__GNUC__ > 2) && defined(__OPTIMIZE__)
#define _GNUNET_BOOLEAN_EXPR(expr) \
  __extension__ ({                 \
    int _gnunet_boolean_var_;      \
    if (expr)                      \
      _gnunet_boolean_var_ = 1;    \
    else                           \
      _gnunet_boolean_var_ = 0;    \
    _gnunet_boolean_var_;          \
  })
#define GN_LIKELY(expr) (__builtin_expect (_GNUNET_BOOLEAN_EXPR (expr), 1))
#define GN_UNLIKELY(expr) (__builtin_expect (_GNUNET_BOOLEAN_EXPR (expr), 0))
#else
#define GN_LIKELY(expr) (expr)
#define GN_UNLIKELY(expr) (expr)
#endif

#if ! defined(GNUNET_LOG_CALL_STATUS)
#define GNUNET_LOG_CALL_STATUS -1
#endif
/* *INDENT-ON* */


/**
 * @ingroup logging
 * Log function that specifies an alternative component.
 * This function should be used by plugins.
 *
 * @param kind how serious is the error?
 * @param comp component responsible for generating the message
 * @param message what is the message (format string)
 * @param ... arguments for format string
 */
void
GNUNET_log_from_nocheck (enum GNUNET_ErrorType kind,
                         const char *comp,
                         const char *message,
                         ...)
__attribute__ ((format (printf, 3, 4)));

#if ! defined(GNUNET_CULL_LOGGING)
#define GNUNET_log_from(kind, comp, ...)                                  \
        do                                                                      \
        {                                                                       \
          static int log_call_enabled = GNUNET_LOG_CALL_STATUS;                 \
          if ((GNUNET_EXTRA_LOGGING > 0) ||                                     \
              ((GNUNET_ERROR_TYPE_DEBUG & (kind)) == 0))                        \
          {                                                                     \
            if (GN_UNLIKELY (log_call_enabled == -1))                           \
            log_call_enabled =                                                \
              GNUNET_get_log_call_status ((kind) & (~GNUNET_ERROR_TYPE_BULK), \
                                          (comp),                             \
                                          __FILE__,                           \
                                          __func__,                       \
                                          __LINE__);                          \
            if (GN_UNLIKELY (GNUNET_get_log_skip () > 0))                       \
            {                                                                   \
              GNUNET_log_skip (-1, GNUNET_NO);                                  \
            }                                                                   \
            else                                                                \
            {                                                                   \
              if (GN_UNLIKELY (log_call_enabled))                               \
              GNUNET_log_from_nocheck ((kind), comp, __VA_ARGS__);            \
            }                                                                   \
          }                                                                     \
        } while (0)

#define GNUNET_log(kind, ...)                                             \
        do                                                                      \
        {                                                                       \
          static int log_call_enabled = GNUNET_LOG_CALL_STATUS;                 \
          if ((GNUNET_EXTRA_LOGGING > 0) ||                                     \
              ((GNUNET_ERROR_TYPE_DEBUG & (kind)) == 0))                        \
          {                                                                     \
            if (GN_UNLIKELY (log_call_enabled == -1))                           \
            log_call_enabled =                                                \
              GNUNET_get_log_call_status ((kind) & (~GNUNET_ERROR_TYPE_BULK), \
                                          NULL,                               \
                                          __FILE__,                           \
                                          __func__,                       \
                                          __LINE__);                          \
            if (GN_UNLIKELY (GNUNET_get_log_skip () > 0))                       \
            {                                                                   \
              GNUNET_log_skip (-1, GNUNET_NO);                                  \
            }                                                                   \
            else                                                                \
            {                                                                   \
              if (GN_UNLIKELY (log_call_enabled))                               \
              GNUNET_log_nocheck ((kind), __VA_ARGS__);                       \
            }                                                                   \
          }                                                                     \
        } while (0)
#else
#define GNUNET_log(...)
#define GNUNET_log_from(...)
#endif


/**
 * @ingroup logging
 * Log error message about missing configuration option.
 *
 * @param kind log level
 * @param section section with missing option
 * @param option name of missing option
 */
void
GNUNET_log_config_missing (enum GNUNET_ErrorType kind,
                           const char *section,
                           const char *option);


/**
 * @ingroup logging
 * Log error message about invalid configuration option value.
 *
 * @param kind log level
 * @param section section with invalid option
 * @param option name of invalid option
 * @param required what is required that is invalid about the option
 */
void
GNUNET_log_config_invalid (enum GNUNET_ErrorType kind,
                           const char *section,
                           const char *option,
                           const char *required);


/**
 * @ingroup logging
 * Abort the process, generate a core dump if possible.
 * Most code should use `GNUNET_assert (0)` instead to
 * first log the location of the failure.
 */
void
GNUNET_abort_ (void) GNUNET_NORETURN;


/**
 * Convert a buffer to an 8-character string
 * representative of the contents. This is used
 * for logging binary data when debugging.
 *
 * @param buf buffer to log
 * @param buf_size number of bytes in @a buf
 * @return text representation of buf, valid until next
 *         call to this function
 */
const char *
GNUNET_b2s (const void *buf,
            size_t buf_size);


/**
 * Convert a fixed-sized object to a string using
 * #GNUNET_b2s().
 *
 * @param obj address of object to convert
 * @return string representing the binary obj buffer
 */
#define GNUNET_B2S(obj) GNUNET_b2s ((obj), sizeof (*(obj)))


/**
 * @ingroup logging
 * Ignore the next @a n calls to the log function.
 *
 * @param n number of log calls to ignore (could be negative)
 * @param check_reset #GNUNET_YES to assert that the log skip counter is currently zero
 */
void
GNUNET_log_skip (int n, int check_reset);


/**
 * @ingroup logging
 * Setup logging.
 *
 * @param comp default component to use
 * @param loglevel what types of messages should be logged
 * @param logfile change logging to logfile (use NULL to keep stderr)
 * @return #GNUNET_OK on success, #GNUNET_SYSERR if logfile could not be opened
 */
enum GNUNET_GenericReturnValue
GNUNET_log_setup (const char *comp,
                  const char *loglevel,
                  const char *logfile);


/**
 * @ingroup logging
 * Add a custom logger.  Note that installing any custom logger
 * will disable the standard logger.  When multiple custom loggers
 * are installed, all will be called.  The standard logger will
 * only be used if no custom loggers are present.
 *
 * @param logger log function
 * @param logger_cls closure for @a logger
 */
void
GNUNET_logger_add (GNUNET_Logger logger,
                   void *logger_cls);


/**
 * @ingroup logging
 * Remove a custom logger.
 *
 * @param logger log function
 * @param logger_cls closure for @a logger
 */
void
GNUNET_logger_remove (GNUNET_Logger logger,
                      void *logger_cls);


/**
 * @ingroup logging
 * Convert a short hash value to a string (for printing debug messages).
 * This is one of the very few calls in the entire API that is
 * NOT reentrant!
 *
 * @param shc the hash code
 * @return string
 */
const char *
GNUNET_sh2s (const struct GNUNET_ShortHashCode *shc);


/**
 * @ingroup logging
 * Convert a UUID to a string (for printing debug messages).
 * This is one of the very few calls in the entire API that is
 * NOT reentrant!
 *
 * @param uuid the UUID
 * @return string
 */
const char *
GNUNET_uuid2s (const struct GNUNET_Uuid *uuid);


/**
 * @ingroup logging
 * Convert a hash value to a string (for printing debug messages).
 * This is one of the very few calls in the entire API that is
 * NOT reentrant!
 *
 * @param hc the hash code
 * @return string
 */
const char *
GNUNET_h2s (const struct GNUNET_HashCode *hc);


/**
 * @ingroup logging
 * Convert a hash value to a string (for printing debug messages).
 * This is one of the very few calls in the entire API that is
 * NOT reentrant! Identical to #GNUNET_h2s(), except that another
 * buffer is used so both #GNUNET_h2s() and #GNUNET_h2s2() can be
 * used within the same log statement.
 *
 * @param hc the hash code
 * @return string
 */
const char *
GNUNET_h2s2 (const struct GNUNET_HashCode *hc);


/**
 * @ingroup logging
 * Convert a hash value to a string (for printing debug messages).
 * This prints all 104 characters of a hashcode!
 * This is one of the very few calls in the entire API that is
 * NOT reentrant!
 *
 * @param hc the hash code
 * @return string
 */
const char *
GNUNET_h2s_full (const struct GNUNET_HashCode *hc);


/**
 * Public key. Details in gnunet_util_crypto.h.
 */
struct GNUNET_CRYPTO_EddsaPublicKey;


/**
 * Public key. Details in gnunet_util_crypto.h.
 */
struct GNUNET_CRYPTO_EcdhePublicKey;


/**
 * @ingroup logging
 * Convert a public key value to a string (for printing debug messages).
 * This is one of the very few calls in the entire API that is
 * NOT reentrant!
 *
 * @param hc the hash code
 * @return string
 */
const char *
GNUNET_p2s (const struct GNUNET_CRYPTO_EddsaPublicKey *p);


/**
 * @ingroup logging
 * Convert a public key value to a string (for printing debug messages).
 * This is one of the very few calls in the entire API that is
 * NOT reentrant!
 *
 * @param hc the hash code
 * @return string
 */
const char *
GNUNET_p2s2 (const struct GNUNET_CRYPTO_EddsaPublicKey *p);


/**
 * @ingroup logging
 * Convert a public key value to a string (for printing debug messages).
 * This is one of the very few calls in the entire API that is
 * NOT reentrant!
 *
 * @param hc the hash code
 * @return string
 */
const char *
GNUNET_e2s (const struct GNUNET_CRYPTO_EcdhePublicKey *p);


/**
 * @ingroup logging
 * Convert a public key value to a string (for printing debug messages).
 * This is one of the very few calls in the entire API that is
 * NOT reentrant!
 *
 * @param hc the hash code
 * @return string
 */
const char *
GNUNET_e2s2 (const struct GNUNET_CRYPTO_EcdhePublicKey *p);


/**
 * Forward declaration to make compiler happy depending on include order.
 */
struct GNUNET_PeerIdentity;


/**
 * @ingroup logging
 * Convert a peer identity to a string (for printing debug messages).
 * This is one of the very few calls in the entire API that is
 * NOT reentrant!
 *
 * @param pid the peer identity
 * @return string form of the pid; will be overwritten by next
 *         call to #GNUNET_i2s().
 */
const char *
GNUNET_i2s (const struct GNUNET_PeerIdentity *pid);


/**
 * @ingroup logging
 * Convert a peer identity to a string (for printing debug messages).
 * This is one of the very few calls in the entire API that is
 * NOT reentrant!  Identical to #GNUNET_i2s(), except that another
 * buffer is used so both #GNUNET_i2s() and #GNUNET_i2s2() can be
 * used within the same log statement.
 *
 * @param pid the peer identity
 * @return string form of the pid; will be overwritten by next
 *         call to #GNUNET_i2s().
 */
const char *
GNUNET_i2s2 (const struct GNUNET_PeerIdentity *pid);


/**
 * @ingroup logging
 * Convert a peer identity to a string (for printing debug messages).
 * This is one of the very few calls in the entire API that is
 * NOT reentrant!
 *
 * @param pid the peer identity
 * @return string form of the pid; will be overwritten by next
 *         call to #GNUNET_i2s_full().
 */
const char *
GNUNET_i2s_full (const struct GNUNET_PeerIdentity *pid);


/**
 * @ingroup logging
 * Convert a "struct sockaddr*" (IPv4 or IPv6 address) to a string
 * (for printing debug messages).  This is one of the very few calls
 * in the entire API that is NOT reentrant!
 *
 * @param addr the address
 * @param addrlen the length of the @a addr
 * @return nicely formatted string for the address
 *  will be overwritten by next call to #GNUNET_a2s().
 */
const char *
GNUNET_a2s (const struct sockaddr *addr, socklen_t addrlen);

/**
 * @ingroup logging
 * Parse an ascii-encoded hexadecimal string into the
 * buffer. The buffer must be (strlen (src) / 2) bytes
 * in length.
 *
 * @param src the string
 * @param dst the destination buffer
 * @param dst_len the length of the @a dst buffer
 * @param invert read from @a src in inverted direction.
 * @return number of bytes written.
 */
size_t
GNUNET_hex2b (const char *src, void *dst, size_t dstlen, int invert);

/**
 * @ingroup logging
 * Print a byte string in hexadecimal ascii notation
 *
 * @param buf the byte string
 * @param buf_len the length of the @a buf buffer
 * @param fold insert newline after this number of bytes
               (0 for no folding)
 * @param in_be Output byte string in NBO
 */
void
GNUNET_print_bytes (const void *buf,
                    size_t buf_len,
                    int fold,
                    int in_be);

/**
 * @ingroup logging
 * Convert error type to string.
 *
 * @param kind type to convert
 * @return string corresponding to the type
 */
const char *
GNUNET_error_type_to_string (enum GNUNET_ErrorType kind);


/**
 * @ingroup logging
 * Use this for fatal errors that cannot be handled
 */
#if __GNUC__ >= 6 || __clang_major__ >= 6
#define GNUNET_assert(cond)                                     \
        do                                                            \
        {                                                             \
          _Pragma("GCC diagnostic push")                                 \
          _Pragma("GCC diagnostic ignored \"-Wtautological-compare\"")   \
          if (! (cond))                                               \
          {                                                           \
          GNUNET_log (GNUNET_ERROR_TYPE_ERROR,                      \
          dgettext ("gnunet", "Assertion failed at %s:%d. Aborting.\n"), \
          __FILE__,                                     \
          __LINE__);                                    \
          GNUNET_abort_ ();                                         \
          }                                                           \
          _Pragma("GCC diagnostic pop")                                  \
          } while (0)
#else
/* older GCC/clangs do not support -Wtautological-compare */
#define GNUNET_assert(cond)                                     \
        do                                                            \
        {                                                             \
          if (! (cond))                                               \
          {                                                           \
            GNUNET_log (GNUNET_ERROR_TYPE_ERROR,                      \
                        dgettext ("gnunet", \
                                  "Assertion failed at %s:%d. Aborting.\n"), \
                        __FILE__,                                     \
                        __LINE__);                                    \
            GNUNET_abort_ ();                                         \
          }                                                           \
        } while (0)
#endif

/**
 * @ingroup logging
 * Use this for fatal errors that cannot be handled
 */
#define GNUNET_assert_at(cond, f, l)                            \
        do                                                            \
        {                                                             \
          if (! (cond))                                               \
          {                                                           \
            GNUNET_log (GNUNET_ERROR_TYPE_ERROR,                      \
                        dgettext ("gnunet", \
                                  "Assertion failed at %s:%d. Aborting.\n"), \
                        f,                                            \
                        l);                                           \
            GNUNET_abort_ ();                                         \
          }                                                           \
        } while (0)


/**
 * @ingroup logging
 * Use this for fatal errors that cannot be handled
 *
 * @param cond Condition to evaluate
 * @param comp Component string to use for logging
 */
#define GNUNET_assert_from(cond, comp)                               \
        do                                                                 \
        {                                                                  \
          if (! (cond))                                                    \
          {                                                                \
            GNUNET_log_from (GNUNET_ERROR_TYPE_ERROR,                      \
                             comp,                                         \
                             dgettext ("gnunet", \
                                       "Assertion failed at %s:%d. Aborting.\n") \
                             , \
                             __FILE__,                                     \
                             __LINE__);                                    \
            GNUNET_abort_ ();                                              \
          }                                                                \
        } while (0)


#ifdef _Static_assert
/**
 * Assertion to be checked (if supported by C compiler) at
 * compile time, otherwise checked at runtime and resulting
 * in an abort() on failure.
 *
 * @param cond condition to test, 0 implies failure
 */
#define GNUNET_static_assert(cond) _Static_assert (cond, "")
#else
/**
 * Assertion to be checked (if supported by C compiler) at
 * compile time, otherwise checked at runtime and resulting
 * in an abort() on failure.  This is the case where the
 * compiler does not support static assertions.
 *
 * @param cond condition to test, 0 implies failure
 */
#define GNUNET_static_assert(cond) GNUNET_assert (cond)
#endif


/**
 * @ingroup logging
 * Use this for internal assertion violations that are
 * not fatal (can be handled) but should not occur.
 */
#define GNUNET_break(cond)                            \
        do                                                  \
        {                                                   \
          if (! (cond))                                     \
          {                                                 \
            GNUNET_log (GNUNET_ERROR_TYPE_ERROR,            \
                        dgettext ("gnunet", "Assertion failed at %s:%d.\n"),   \
                        __FILE__,                           \
                        __LINE__);                          \
          }                                                 \
        } while (0)


/**
 * @ingroup logging
 * Use this for assertion violations caused by other
 * peers (i.e. protocol violations).  We do not want to
 * confuse end-users (say, some other peer runs an
 * older, broken or incompatible GNUnet version), but
 * we still want to see these problems during
 * development and testing.  "OP == other peer".
 */
#define GNUNET_break_op(cond)                                             \
        do                                                                      \
        {                                                                       \
          if (! (cond))                                                         \
          {                                                                     \
            GNUNET_log (GNUNET_ERROR_TYPE_WARNING | GNUNET_ERROR_TYPE_BULK,     \
                        dgettext ("gnunet", \
                                  "External protocol violation detected at %s:%d.\n"), \
                        __FILE__,                                               \
                        __LINE__);                                              \
          }                                                                     \
        } while (0)


/**
 * @ingroup logging
 * Log an error message at log-level 'level' that indicates
 * a failure of the command 'cmd' with the message given
 * by strerror(errno).
 */
#define GNUNET_log_strerror(level, cmd)                      \
        do                                                         \
        {                                                          \
          GNUNET_log (level,                                       \
                      dgettext ("gnunet", \
                                "`%s' failed at %s:%d with error: %s\n"), \
                      cmd,                                         \
                      __FILE__,                                    \
                      __LINE__,                                    \
                      strerror (errno));                           \
        } while (0)


/**
 * @ingroup logging
 * Log an error message at log-level 'level' that indicates
 * a failure of the command 'cmd' with the message given
 * by strerror(errno).
 */
#define GNUNET_log_from_strerror(level, component, cmd)           \
        do                                                              \
        {                                                               \
          GNUNET_log_from (level,                                       \
                           component,                                   \
                           dgettext ("gnunet", \
                                     "`%s' failed at %s:%d with error: %s\n"), \
                           cmd,                                         \
                           __FILE__,                                    \
                           __LINE__,                                    \
                           strerror (errno));                           \
        } while (0)


/**
 * @ingroup logging
 * Log an error message at log-level 'level' that indicates
 * a failure of the command 'cmd' with the message given
 * by strerror(errno).
 */
#define GNUNET_log_strerror_file(level, cmd, filename)                    \
        do                                                                      \
        {                                                                       \
          GNUNET_log (level,                                                    \
                      dgettext ("gnunet", \
                                "`%s' failed on file `%s' at %s:%d with error: %s\n"), \
                      cmd,                                                      \
                      filename,                                                 \
                      __FILE__,                                                 \
                      __LINE__,                                                 \
                      strerror (errno));                                        \
        } while (0)


/**
 * @ingroup logging
 * Log an error message at log-level 'level' that indicates
 * a failure of the command 'cmd' with the message given
 * by strerror(errno).
 */
#define GNUNET_log_from_strerror_file(level, component, cmd, filename)         \
        do                                                                           \
        {                                                                            \
          GNUNET_log_from (level,                                                    \
                           component,                                                \
                           dgettext ("gnunet", \
                                     "`%s' failed on file `%s' at %s:%d with error: %s\n"), \
                           cmd,                                                      \
                           filename,                                                 \
                           __FILE__,                                                 \
                           __LINE__,                                                 \
                           strerror (errno));                                        \
        } while (0)

/* ************************* endianness conversion ****************** */

#ifdef htobe64

#define GNUNET_htonll(n) htobe64 (n)

#else
/**
 * Convert unsigned 64-bit integer to network byte order.
 *
 * @param n
 *        The value in host byte order.
 *
 * @return The same value in network byte order.
 */
uint64_t
GNUNET_htonll (uint64_t n);

#endif


#ifdef be64toh

#define GNUNET_ntohll(n) be64toh (n)

#else
/**
 * Convert unsigned 64-bit integer to host byte order.
 *
 * @param n
 *        The value in network byte order.
 *
 * @return The same value in host byte order.
 */
uint64_t
GNUNET_ntohll (uint64_t n);

#endif


/**
 * Convert double to network byte order.
 *
 * @param d
 *        The value in host byte order.
 *
 * @return The same value in network byte order.
 */
double
GNUNET_hton_double (double d);


/**
 * Convert double to host byte order
 *
 * @param d
 *        The value in network byte order.
 *
 * @return The same value in host byte order.
 */
double
GNUNET_ntoh_double (double d);


/* ************************* allocation functions ****************** */

/**
 * @ingroup memory
 * Maximum allocation with GNUNET_malloc macro.
 */
#define GNUNET_MAX_MALLOC_CHECKED (1024 * 1024 * 40)

/**
 * @ingroup memory
 * Allocate a struct or union of the given @a type.
 * Wrapper around #GNUNET_malloc that returns a pointer
 * to the newly created object of the correct type.
 *
 * @param type name of the struct or union, i.e. pass 'struct Foo'.
 */
#define GNUNET_new(type) (type *) GNUNET_malloc (sizeof(type))


/**
 * Compare memory in @a a and @a b, where both must be of
 * the same pointer type.
 *
 * Do NOT use this function on arrays, it would only compare
 * the first element!
 */
#define GNUNET_memcmp(a, b)       \
        ({                              \
    const typeof (*b) * _a = (a);  \
    const typeof (*a) * _b = (b);  \
    memcmp (_a, _b, sizeof(*a)); \
  })


/**
 * Compare memory in @a b1 and @a b2 in constant time, suitable for private
 * data.
 *
 * @param b1 some buffer of size @a len
 * @param b2 another buffer of size @a len
 * @param len number of bytes in @a b1 and @a b2
 * @return 0 if buffers are equal,
 */
int
GNUNET_memcmp_ct_ (const void *b1,
                   const void *b2,
                   size_t len);

/**
 * Compare memory in @a a and @a b in constant time, suitable for private
 * data. Both @a a and @a b must be of the same pointer type.
 *
 * Do NOT use this function on arrays, it would only compare
 * the first element!
 */
#define GNUNET_memcmp_priv(a, b)       \
        ({                              \
    const typeof (*b) * _a = (a);  \
    const typeof (*a) * _b = (b);  \
    GNUNET_memcmp_ct_ (_a, _b, sizeof(*a)); \
  })


/**
 * Check that memory in @a a is all zeros. @a a must be a pointer.
 *
 * @param a pointer to @a n bytes which should be tested for the
 *          entire memory being zero'ed out.
 * @param n number of bytes in @a to be tested
 * @return true if @a a is zero, false_NO otherwise
 */
bool
GNUNET_is_zero_ (const void *a,
                 size_t n);


/**
 * Check that memory in @a a is all zeros. @a a must be a pointer.
 *
 * @param a pointer to a struct which should be tested for the
 *          entire memory being zero'ed out.
 * @return GNUNET_YES if a is zero, GNUNET_NO otherwise
 */
#define GNUNET_is_zero(a)           \
        GNUNET_is_zero_ ((a), sizeof (*(a)))


/**
 * Call memcpy() but check for @a n being 0 first. In the latter
 * case, it is now safe to pass NULL for @a src or @a dst.
 * Unlike traditional memcpy(), returns nothing.
 *
 * @param dst destination of the copy, may be NULL if @a n is zero
 * @param src source of the copy, may be NULL if @a n is zero
 * @param n number of bytes to copy
 */
#define GNUNET_memcpy(dst, src, n) \
        do                               \
        {                                \
          if (0 != n)                    \
          {                              \
            (void) memcpy (dst, src, n); \
          }                              \
        } while (0)


/* *INDENT-OFF* */
/**
 * @ingroup memory
 * Allocate a size @a n array with structs or unions of the given @a type.
 * Wrapper around #GNUNET_malloc that returns a pointer
 * to the newly created objects of the correct type.
 *
 * @param n number of elements in the array
 * @param type name of the struct or union, i.e. pass 'struct Foo'.
 */
#define GNUNET_new_array(n, type) ({ \
    GNUNET_assert (SIZE_MAX / sizeof (type) >= n);     \
    (type *) GNUNET_malloc ((n) * sizeof(type));   \
  })
/* *INDENT-ON* */

/**
 * @ingroup memory
 * Wrapper around malloc. Allocates size bytes of memory.
 * The memory will be zero'ed out.
 *
 * @param size the number of bytes to allocate, must be
 *        smaller than 40 MB.
 * @return pointer to size bytes of memory, never NULL (!)
 */
#define GNUNET_malloc(size) GNUNET_xmalloc_ (size, __FILE__, __LINE__)

/**
 * @ingroup memory
 * Allocate and initialize a block of memory.
 *
 * @param buf data to initialize the block with
 * @param size the number of bytes in buf (and size of the allocation)
 * @return pointer to size bytes of memory, never NULL (!)
 */
#define GNUNET_memdup(buf, size) GNUNET_xmemdup_ (buf, size, __FILE__, __LINE__)

/**
 * @ingroup memory
 * Wrapper around malloc. Allocates size bytes of memory.
 * The memory will be zero'ed out.
 *
 * @param size the number of bytes to allocate
 * @return pointer to size bytes of memory, NULL if we do not have enough memory
 */
#define GNUNET_malloc_large(size) \
        GNUNET_xmalloc_unchecked_ (size, __FILE__, __LINE__)


/**
 * @ingroup memory
 * Wrapper around realloc. Reallocates size bytes of memory.
 * The content of the intersection of the new and old size will be unchanged.
 *
 * @param ptr the pointer to reallocate
 * @param size the number of bytes to reallocate
 * @return pointer to size bytes of memory
 */
#define GNUNET_realloc(ptr, size) \
        GNUNET_xrealloc_ (ptr, size, __FILE__, __LINE__)


/**
 * @ingroup memory
 * Wrapper around free. Frees the memory referred to by ptr.
 * Note that it is generally better to free memory that was
 * allocated with #GNUNET_array_grow using #GNUNET_array_grow(mem, size, 0) instead of #GNUNET_free_nz.
 *
 * @param ptr location where to free the memory. ptr must have
 *     been returned by #GNUNET_strdup, #GNUNET_strndup, #GNUNET_malloc or #GNUNET_array_grow earlier.  NULL is allowed.
 */
#define GNUNET_free_nz(ptr) GNUNET_xfree_ (ptr, __FILE__, __LINE__)


/**
 * @ingroup memory
 * Wrapper around free. Frees the memory referred to by ptr and sets ptr to NULL.
 * Note that it is generally better to free memory that was
 * allocated with #GNUNET_array_grow using #GNUNET_array_grow(mem, size, 0) instead of #GNUNET_free.
 *
 * @a ptr will be set to NULL. Use #GNUNET_free_nz() if @a ptr is not an L-value.
 *
 * @param ptr location where to free the memory. ptr must have
 *     been returned by #GNUNET_strdup, #GNUNET_strndup, #GNUNET_malloc or #GNUNET_array_grow earlier.  NULL is allowed.
 */
#define GNUNET_free(ptr) do { \
          GNUNET_xfree_ (ptr, __FILE__, __LINE__); \
          ptr = NULL; \
} while (0)


/**
 * @ingroup memory
 * Wrapper around #GNUNET_xstrdup_.  Makes a copy of the zero-terminated string
 * pointed to by a.
 *
 * @param a pointer to a zero-terminated string
 * @return a copy of the string including zero-termination
 */
#define GNUNET_strdup(a) GNUNET_xstrdup_ (a, __FILE__, __LINE__)


/**
 * @ingroup memory
 * Wrapper around #GNUNET_xstrndup_.  Makes a partial copy of the string
 * pointed to by a.
 *
 * @param a pointer to a string
 * @param length of the string to duplicate
 * @return a partial copy of the string including zero-termination
 */
#define GNUNET_strndup(a, length) \
        GNUNET_xstrndup_ (a, length, __FILE__, __LINE__)

/**
 * @ingroup memory
 * Grow a well-typed (!) array.  This is a convenience
 * method to grow a vector @a arr of size @a size
 * to the new (target) size @a tsize.
 * <p>
 *
 * Example (simple, well-typed stack):
 *
 * <pre>
 * static struct foo * myVector = NULL;
 * static int myVecLen = 0;
 *
 * static void push(struct foo * elem) {
 *   GNUNET_array_grow(myVector, myVecLen, myVecLen+1);
 *   GNUNET_memcpy(&myVector[myVecLen-1], elem, sizeof(struct foo));
 * }
 *
 * static void pop(struct foo * elem) {
 *   if (myVecLen == 0) die();
 *   GNUNET_memcpy(elem, myVector[myVecLen-1], sizeof(struct foo));
 *   GNUNET_array_grow(myVector, myVecLen, myVecLen-1);
 * }
 * </pre>
 *
 * @param arr base-pointer of the vector, may be NULL if size is 0;
 *        will be updated to reflect the new address. The TYPE of
 *        arr is important since size is the number of elements and
 *        not the size in bytes
 * @param size the number of elements in the existing vector (number
 *        of elements to copy over), will be updated with the new
 *        array size
 * @param tsize the target size for the resulting vector, use 0 to
 *        free the vector (then, arr will be NULL afterwards).
 */
#define GNUNET_array_grow(arr, size, tsize) \
        GNUNET_xgrow_ ((void **) &(arr),          \
                       sizeof((arr)[0]),         \
                       &size,                     \
                       tsize,                     \
                       __FILE__,                  \
                       __LINE__)

/**
 * @ingroup memory
 * Append an element to an array (growing the array by one).
 *
 * @param arr base-pointer of the vector, may be NULL if @a len is 0;
 *        will be updated to reflect the new address. The TYPE of
 *        arr is important since size is the number of elements and
 *        not the size in bytes
 * @param len the number of elements in the existing vector (number
 *        of elements to copy over), will be updated with the new
 *        array length
 * @param element the element that will be appended to the array
 */
#define GNUNET_array_append(arr, len, element) \
        do                                            \
        {                                             \
          GNUNET_assert ((len) + 1 > (len)); \
          GNUNET_array_grow (arr, len, len + 1);    \
          (arr) [len - 1] = element;                  \
        } while (0)


/**
 * @ingroup memory
 * Append @a arr2 to @a arr1 (growing @a arr1
 * as needed).  The @a arr2 array is left unchanged. Naturally
 * this function performs a shallow copy. Both arrays must have
 * the same type for their elements.
 *
 * @param arr1 base-pointer of the vector, may be NULL if @a len is 0;
 *        will be updated to reflect the new address. The TYPE of
 *        arr is important since size is the number of elements and
 *        not the size in bytes
 * @param len1 the number of elements in the existing vector (number
 *        of elements to copy over), will be updated with the new
 *        array size
 * @param arr2 base-pointer a second array to concatenate, may be NULL if @a len2 is 0;
 *        will be updated to reflect the new address. The TYPE of
 *        arr is important since size is the number of elements and
 *        not the size in bytes
 * @param len2 the number of elements in the existing vector (number
 *        of elements to copy over), will be updated with the new
 *        array size

 */
#define GNUNET_array_concatenate(arr1, len1, arr2, len2)   \
        do                                            \
        {                                             \
          const typeof (*arr2) * _a1 = (arr1);  \
          const typeof (*arr1) * _a2 = (arr2);  \
          GNUNET_assert ((len1) + (len2) >= (len1));                    \
          GNUNET_assert (SIZE_MAX / sizeof (*_a1) >= ((len1) + (len2))); \
          GNUNET_array_grow (arr1, len1, (len1) + (len2));            \
          memcpy (&(arr1) [(len1) - (len2)], _a2, (len2) * sizeof (*arr1));    \
        } while (0)


/**
 * @ingroup memory
 * Like snprintf, just aborts if the buffer is of insufficient size.
 *
 * @param buf pointer to buffer that is written to
 * @param size number of bytes in @a buf
 * @param format format strings
 * @param ... data for format string
 * @return number of bytes written to buf or negative value on error
 */
int
GNUNET_snprintf (char *buf,
                 size_t size,
                 const char *format,
                 ...)
__attribute__ ((format (printf, 3, 4)));


/**
 * @ingroup memory
 * Like asprintf, just portable.
 *
 * @param buf set to a buffer of sufficient size (allocated, caller must free)
 * @param format format string (see printf, fprintf, etc.)
 * @param ... data for format string
 * @return number of bytes in "*buf" excluding 0-termination
 */
int
GNUNET_asprintf (char **buf,
                 const char *format,
                 ...)
__attribute__ ((format (printf, 2, 3)));


/* ************** internal implementations, use macros above! ************** */

/**
 * Allocate memory. Checks the return value, aborts if no more
 * memory is available.  Don't use GNUNET_xmalloc_ directly. Use the
 * #GNUNET_malloc macro.
 * The memory will be zero'ed out.
 *
 * @param size number of bytes to allocate
 * @param filename where is this call being made (for debugging)
 * @param linenumber line where this call is being made (for debugging)
 * @return allocated memory, never NULL
 */
void *
GNUNET_xmalloc_ (size_t size,
                 const char *filename,
                 int linenumber);


/**
 * Allocate and initialize memory. Checks the return value, aborts if no more
 * memory is available.  Don't use GNUNET_xmemdup_ directly. Use the
 * #GNUNET_memdup macro.
 *
 * @param buf buffer to initialize from (must contain size bytes)
 * @param size number of bytes to allocate
 * @param filename where is this call being made (for debugging)
 * @param linenumber line where this call is being made (for debugging)
 * @return allocated memory, never NULL
 */
void *
GNUNET_xmemdup_ (const void *buf,
                 size_t size,
                 const char *filename,
                 int linenumber);


/**
 * Allocate memory.  This function does not check if the allocation
 * request is within reasonable bounds, allowing allocations larger
 * than 40 MB.  If you don't expect the possibility of very large
 * allocations, use #GNUNET_malloc instead.  The memory will be zero'ed
 * out.
 *
 * @param size number of bytes to allocate
 * @param filename where is this call being made (for debugging)
 * @param linenumber line where this call is being made (for debugging)
 * @return pointer to size bytes of memory, NULL if we do not have enough memory
 */
void *
GNUNET_xmalloc_unchecked_ (size_t size,
                           const char *filename,
                           int linenumber);


/**
 * Reallocate memory. Checks the return value, aborts if no more
 * memory is available.
 */
void *
GNUNET_xrealloc_ (void *ptr,
                  size_t n,
                  const char *filename,
                  int linenumber);


/**
 * Free memory. Merely a wrapper for the case that we
 * want to keep track of allocations.  Don't use GNUNET_xfree_
 * directly. Use the #GNUNET_free macro.
 *
 * @param ptr pointer to memory to free
 * @param filename where is this call being made (for debugging)
 * @param linenumber line where this call is being made (for debugging)
 */
void
GNUNET_xfree_ (void *ptr,
               const char *filename,
               int linenumber);


/**
 * Dup a string. Don't call GNUNET_xstrdup_ directly. Use the #GNUNET_strdup macro.
 * @param str string to duplicate
 * @param filename where is this call being made (for debugging)
 * @param linenumber line where this call is being made (for debugging)
 * @return the duplicated string
 */
char *
GNUNET_xstrdup_ (const char *str,
                 const char *filename,
                 int linenumber);

/**
 * Dup partially a string. Don't call GNUNET_xstrndup_ directly. Use the #GNUNET_strndup macro.
 *
 * @param str string to duplicate
 * @param len length of the string to duplicate
 * @param filename where is this call being made (for debugging)
 * @param linenumber line where this call is being made (for debugging)
 * @return the duplicated string
 */
char *
GNUNET_xstrndup_ (const char *str,
                  size_t len,
                  const char *filename,
                  int linenumber);

/**
 * Grow an array, the new elements are zeroed out.
 * Grows old by (*oldCount-newCount)*elementSize
 * bytes and sets *oldCount to newCount.
 *
 * Don't call GNUNET_xgrow_ directly. Use the #GNUNET_array_grow macro.
 *
 * @param old address of the pointer to the array
 *        *old may be NULL
 * @param elementSize the size of the elements of the array
 * @param oldCount address of the number of elements in the *old array
 * @param newCount number of elements in the new array, may be 0 (then *old will be NULL afterwards)
 * @param filename where is this call being made (for debugging)
 * @param linenumber line where this call is being made (for debugging)
 */
void
GNUNET_xgrow_ (void **old,
               size_t elementSize,
               unsigned int *oldCount,
               unsigned int newCount,
               const char *filename,
               int linenumber);


/**
 * @ingroup memory
 * Create a copy of the given message.
 *
 * @param msg message to copy
 * @return duplicate of the message
 */
struct GNUNET_MessageHeader *
GNUNET_copy_message (const struct GNUNET_MessageHeader *msg);


/**
 * Set the async scope for the current thread.
 *
 * @param aid the async scope identifier
 * @param[out] old_scope location to save the old scope
 */
void
GNUNET_async_scope_enter (const struct GNUNET_AsyncScopeId *aid,
                          struct GNUNET_AsyncScopeSave *old_scope);


/**
 * Clear the current thread's async scope.
 *
 * @param old_scope scope to restore
 */
void
GNUNET_async_scope_restore (struct GNUNET_AsyncScopeSave *old_scope);


/**
 * Get the current async scope.
 *
 * @param[out] scope_ret pointer to where the result is stored
 */
void
GNUNET_async_scope_get (struct GNUNET_AsyncScopeSave *scope_ret);


/**
 * Generate a fresh async scope identifier.
 *
 * @param[out] aid_ret pointer to where the result is stored
 */
void
GNUNET_async_scope_fresh (struct GNUNET_AsyncScopeId *aid_ret);


#if __STDC_VERSION__ < 199901L
#if __GNUC__ >= 2
#define __func__ __FUNCTION__
#else
#define __func__ "<unknown>"
#endif
#endif


/**
 * Valid task priorities.  Use these, do not pass random integers!
 * For various reasons (#3862 -- building with QT Creator, and
 * our restricted cross-compilation with emscripten) this cannot
 * be in gnunet_scheduler_lib.h, but it works if we declare it here.
 * Naturally, logically this is part of the scheduler.
 */
enum GNUNET_SCHEDULER_Priority
{
  /**
   * Run with the same priority as the current job.
   */
  GNUNET_SCHEDULER_PRIORITY_KEEP = 0,

  /**
   * Run when otherwise idle.
   */
  GNUNET_SCHEDULER_PRIORITY_IDLE = 1,

  /**
   * Run as background job (higher than idle,
   * lower than default).
   */
  GNUNET_SCHEDULER_PRIORITY_BACKGROUND = 2,

  /**
   * Run with the default priority (normal
   * P2P operations).  Any task that is scheduled
   * without an explicit priority being specified
   * will run with this priority.
   */
  GNUNET_SCHEDULER_PRIORITY_DEFAULT = 3,

  /**
   * Run with high priority (important requests).
   * Higher than DEFAULT.
   */
  GNUNET_SCHEDULER_PRIORITY_HIGH = 4,

  /**
   * Run with priority for interactive tasks.
   * Higher than "HIGH".
   */
  GNUNET_SCHEDULER_PRIORITY_UI = 5,

  /**
   * Run with priority for urgent tasks.  Use
   * for things like aborts and shutdowns that
   * need to preempt "UI"-level tasks.
   * Higher than "UI".
   */
  GNUNET_SCHEDULER_PRIORITY_URGENT = 6,

  /**
   * This is an internal priority level that is only used for tasks
   * that are being triggered due to shutdown (they have automatically
   * highest priority).  User code must not use this priority level
   * directly.  Tasks run with this priority level that internally
   * schedule other tasks will see their original priority level
   * be inherited (unless otherwise specified).
   */
  GNUNET_SCHEDULER_PRIORITY_SHUTDOWN = 7,

  /**
   * Number of priorities (must be the last priority).
   * This priority must not be used by clients.
   */
  GNUNET_SCHEDULER_PRIORITY_COUNT = 8
};


/* *INDENT-OFF* */

#if 0 /* keep Emacsens' auto-indent happy */
{
#endif
#ifdef __cplusplus
}
#endif

#endif /* GNUNET_COMMON_H */
