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

#pragma once

#if !defined (__EV_EVINCE_DOCUMENT_H_INSIDE__) && !defined (EVINCE_COMPILATION)
#error "Only <evince-document.h> can be included directly."
#endif

#include <glib.h>
#include <gio/gio.h>

#include "ev-macros.h"

G_BEGIN_DECLS

typedef enum {
	EV_COMPRESSION_NONE,
	EV_COMPRESSION_BZIP2,
	EV_COMPRESSION_GZIP,
        EV_COMPRESSION_LZMA
} EvCompressionType;

void        _ev_file_helpers_init     (void);

void        _ev_file_helpers_shutdown (void);

EV_PUBLIC
int          ev_mkstemp               (const char        *tmpl,
                                       char             **file_name,
                                       GError           **error);
EV_PUBLIC
GFile       *ev_mkstemp_file          (const char        *tmpl,
                                       GError           **error);
EV_PUBLIC
gchar       *ev_mkdtemp               (const char        *tmpl,
                                       GError           **error);
EV_PUBLIC
void         ev_tmp_filename_unlink   (const gchar       *filename);
EV_PUBLIC
void         ev_tmp_file_unlink       (GFile             *file);
EV_PUBLIC
void         ev_tmp_uri_unlink        (const gchar       *uri);
EV_PUBLIC
gboolean     ev_file_is_temp          (GFile             *file);
EV_PUBLIC
gboolean     ev_xfer_uri_simple       (const char        *from,
				       const char        *to,
				       GError           **error);
EV_PUBLIC
gboolean     ev_file_copy_metadata    (const char        *from,
                                       const char        *to,
                                       GError           **error);

EV_PUBLIC
gchar       *ev_file_get_mime_type    (const gchar       *uri,
				       gboolean           fast,
				       GError           **error);

EV_PUBLIC
gchar       *ev_file_get_mime_type_from_fd (int           fd,
                                            GError      **error);

EV_PUBLIC
gchar       *ev_file_uncompress       (const gchar       *uri,
				       EvCompressionType  type,
				       GError           **error);
EV_PUBLIC
gchar       *ev_file_compress         (const gchar       *uri,
				       EvCompressionType  type,
				       GError           **error);

G_END_DECLS
