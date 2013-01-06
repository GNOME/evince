/*
 *  Copyright (C) 2002 Jorn Baayen
 *
 *  This program is free software; you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation; either version 2, or (at your option)
 *  any later version.
 *
 *  This program is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License
 *  along with this program; if not, write to the Free Software
 *  Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301, USA.
 *
 */

#if !defined (__EV_EVINCE_DOCUMENT_H_INSIDE__) && !defined (EVINCE_COMPILATION)
#error "Only <evince-document.h> can be included directly."
#endif

#ifndef EV_FILE_HELPERS_H
#define EV_FILE_HELPERS_H

#include <glib.h>
#include <gio/gio.h>

G_BEGIN_DECLS

typedef enum {
	EV_COMPRESSION_NONE,
	EV_COMPRESSION_BZIP2,
	EV_COMPRESSION_GZIP,
        EV_COMPRESSION_LZMA
} EvCompressionType;

void        _ev_file_helpers_init     (void);

void        _ev_file_helpers_shutdown (void);

int          ev_mkstemp               (const char        *tmpl,
                                       char             **file_name,
                                       GError           **error);
GFile       *ev_mkstemp_file          (const char        *tmpl,
                                       GError           **error);
gchar       *ev_mkdtemp               (const char        *tmpl,
                                       GError           **error);
void         ev_tmp_filename_unlink   (const gchar       *filename);
void         ev_tmp_file_unlink       (GFile             *file);
void         ev_tmp_uri_unlink        (const gchar       *uri);
gboolean     ev_file_is_temp          (GFile             *file);
gboolean     ev_xfer_uri_simple       (const char        *from,
				       const char        *to,
				       GError           **error);
gboolean     ev_file_copy_metadata    (const char        *from,
                                       const char        *to,
                                       GError           **error);

gchar       *ev_file_get_mime_type    (const gchar       *uri,
				       gboolean           fast,
				       GError           **error);

gchar       *ev_file_uncompress       (const gchar       *uri,
				       EvCompressionType  type,
				       GError           **error);
gchar       *ev_file_compress         (const gchar       *uri,
				       EvCompressionType  type,
				       GError           **error);


G_END_DECLS

#endif /* EV_FILE_HELPERS_H */
