/*
     This file is part of GNUnet.
     Copyright (C) 2010, 2011, 2017 GNUnet e.V.

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
 * @file fs/fs_misc.c
 * @brief misc. functions related to file-sharing in general
 * @author Christian Grothoff
 */
#include "platform.h"
#include "gnunet_constants.h"

#include "gnunet_fs_service.h"
#include "fs_api.h"


/**
 * Suggest a filename based on given metadata.
 *
 * @param md given meta data
 * @return NULL if meta data is useless for suggesting a filename
 */
char *
GNUNET_FS_meta_data_suggest_filename (const struct GNUNET_FS_MetaData
                                      *md)
{
  static const char *mimeMap[][2] = {
    { "application/bz2", ".bz2" },
    { "application/gnunet-directory", ".gnd" },
    { "application/java", ".class" },
    { "application/msword", ".doc" },
    { "application/nar", ".nar" },
    { "application/narinfo", ".narinfo" },
    { "application/ogg", ".ogg" },
    { "application/pdf", ".pdf" },
    { "application/pgp-keys", ".key" },
    { "application/pgp-signature", ".pgp" },
    { "application/postscript", ".ps" },
    { "application/rar", ".rar" },
    { "application/rtf", ".rtf" },
    { "application/xml", ".xml" },
    { "application/x-debian-package", ".deb" },
    { "application/x-dvi", ".dvi" },
    { "application/x-flac", ".flac" },
    { "application/x-gzip", ".gz" },
    { "application/x-java-archive", ".jar" },
    { "application/x-java-vm", ".class" },
    { "application/x-python-code", ".pyc" },
    { "application/x-redhat-package-manager", ".rpm" },
    { "application/x-rpm", ".rpm" },
    { "application/x-tar", ".tar" },
    { "application/x-tex-pk", ".pk" },
    { "application/x-texinfo", ".texinfo" },
    { "application/x-xcf", ".xcf" },
    { "application/x-xfig", ".xfig" },
    { "application/zip", ".zip" },

    { "audio/midi", ".midi" },
    { "audio/mpeg", ".mp3" },
    { "audio/real", ".rm" },
    { "audio/x-wav", ".wav" },

    { "image/gif", ".gif" },
    { "image/jpeg", ".jpg" },
    { "image/pcx", ".pcx" },
    { "image/png", ".png" },
    { "image/tiff", ".tiff" },
    { "image/x-ms-bmp", ".bmp" },
    { "image/x-xpixmap", ".xpm" },

    { "text/css", ".css" },
    { "text/html", ".html" },
    { "text/plain", ".txt" },
    { "text/rtf", ".rtf" },
    { "text/x-c++hdr", ".h++" },
    { "text/x-c++src", ".c++" },
    { "text/x-chdr", ".h" },
    { "text/x-csrc", ".c" },
    { "text/x-java", ".java" },
    { "text/x-moc", ".moc" },
    { "text/x-pascal", ".pas" },
    { "text/x-perl", ".pl" },
    { "text/x-python", ".py" },
    { "text/x-tex", ".tex" },

    { "video/avi", ".avi" },
    { "video/mpeg", ".mpeg" },
    { "video/quicktime", ".qt" },
    { "video/real", ".rm" },
    { "video/x-msvideo", ".avi" },
    { NULL, NULL },
  };
  char *ret;
  unsigned int i;
  char *mime;
  char *base;
  const char *ext;

  ret =
    GNUNET_FS_meta_data_get_by_type (md,
                                     EXTRACTOR_METATYPE_GNUNET_ORIGINAL_FILENAME);
  if (ret != NULL)
    return ret;
  ext = NULL;
  mime =
    GNUNET_FS_meta_data_get_by_type (md, EXTRACTOR_METATYPE_MIMETYPE);
  if (mime != NULL)
  {
    i = 0;
    while ((mimeMap[i][0] != NULL) && (0 != strcmp (mime, mimeMap[i][0])))
      i++;
    if (mimeMap[i][1] == NULL)
      GNUNET_log (GNUNET_ERROR_TYPE_DEBUG | GNUNET_ERROR_TYPE_BULK,
                  _ ("Did not find mime type `%s' in extension list.\n"), mime);
    else
      ext = mimeMap[i][1];
    GNUNET_free (mime);
  }
  base =
    GNUNET_FS_meta_data_get_first_by_types (md,
                                            EXTRACTOR_METATYPE_TITLE,
                                            EXTRACTOR_METATYPE_BOOK_TITLE,
                                            EXTRACTOR_METATYPE_ORIGINAL_TITLE,
                                            EXTRACTOR_METATYPE_PACKAGE_NAME,
                                            EXTRACTOR_METATYPE_URL,
                                            EXTRACTOR_METATYPE_URI,
                                            EXTRACTOR_METATYPE_DESCRIPTION,
                                            EXTRACTOR_METATYPE_ISRC,
                                            EXTRACTOR_METATYPE_JOURNAL_NAME,
                                            EXTRACTOR_METATYPE_AUTHOR_NAME,
                                            EXTRACTOR_METATYPE_SUBJECT,
                                            EXTRACTOR_METATYPE_ALBUM,
                                            EXTRACTOR_METATYPE_ARTIST,
                                            EXTRACTOR_METATYPE_KEYWORDS,
                                            EXTRACTOR_METATYPE_COMMENT,
                                            EXTRACTOR_METATYPE_UNKNOWN,
                                            -1);
  if ((base == NULL) && (ext == NULL))
    return NULL;
  if (base == NULL)
    return GNUNET_strdup (ext);
  if (ext == NULL)
    return base;
  GNUNET_asprintf (&ret, "%s%s", base, ext);
  GNUNET_free (base);
  return ret;
}


/* end of fs_misc.c */
