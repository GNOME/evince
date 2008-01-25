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
 *  Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA 02111-1307, USA.
 *
 *  $Id$
 */

#ifndef EV_FILE_HELPERS_H
#define EV_FILE_HELPERS_H

#include <glib.h>
#include <gio/gio.h>

G_BEGIN_DECLS

typedef enum {
	EV_COMPRESSION_NONE,
	EV_COMPRESSION_BZIP2,
	EV_COMPRESSION_GZIP
} EvCompressionType;

const gchar *ev_dot_dir               (void);

const gchar *ev_tmp_dir               (void);

void         ev_file_helpers_init     (void);

void         ev_file_helpers_shutdown (void);

GFile       *ev_tmp_file_get          (const gchar       *prefix);
gchar       *ev_tmp_filename          (const char        *prefix);
void         ev_tmp_filename_unlink   (const gchar       *filename);
void         ev_tmp_file_unlink       (GFile             *file);
void         ev_tmp_uri_unlink        (const gchar       *uri);

gboolean     ev_xfer_uri_simple       (const char        *from,
				       const char        *to,
				       GError           **error);

gchar       *ev_file_uncompress       (const gchar       *uri,
				       EvCompressionType  type,
				       GError           **error);
gchar       *ev_file_compress         (const gchar       *uri,
				       EvCompressionType  type,
				       GError           **error);


G_END_DECLS

#endif /* EV_FILE_HELPERS_H */
