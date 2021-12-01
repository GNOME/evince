/* ev-metadata.h
 *  this file is part of evince, a gnome document viewer
 *
 * Copyright (C) 2009 Carlos Garcia Campos  <carlosgc@gnome.org>
 *
 * Evince is free software; you can redistribute it and/or modify it
 * under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * Evince is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301, USA.
 */

#pragma once

#include <glib-object.h>
#include <gio/gio.h>

G_BEGIN_DECLS

#define EV_TYPE_METADATA         (ev_metadata_get_type())
#define EV_METADATA(object)      (G_TYPE_CHECK_INSTANCE_CAST((object), EV_TYPE_METADATA, EvMetadata))
#define EV_METADATA_CLASS(klass) (G_TYPE_CHECK_CLASS_CAST((klass), EV_TYPE_METADATA, EvMetadataClass))
#define EV_IS_METADATA(object)   (G_TYPE_CHECK_INSTANCE_TYPE((object), EV_TYPE_METADATA))

typedef struct _EvMetadata      EvMetadata;
typedef struct _EvMetadataClass EvMetadataClass;

GType       ev_metadata_get_type              (void) G_GNUC_CONST;
EvMetadata *ev_metadata_new                   (GFile       *file);
gboolean    ev_metadata_is_empty              (EvMetadata  *metadata);

gboolean    ev_metadata_get_string            (EvMetadata  *metadata,
					       const gchar *key,
					       gchar     **value);
gboolean    ev_metadata_set_string            (EvMetadata  *metadata,
					       const gchar *key,
					       const gchar *value);
gboolean    ev_metadata_get_int               (EvMetadata  *metadata,
					       const gchar *key,
					       gint        *value);
gboolean    ev_metadata_set_int               (EvMetadata  *metadata,
					       const gchar *key,
					       gint         value);
gboolean    ev_metadata_get_double            (EvMetadata  *metadata,
					       const gchar *key,
					       gdouble     *value);
gboolean    ev_metadata_set_double            (EvMetadata  *metadata,
					       const gchar *key,
					       gdouble      value);
gboolean    ev_metadata_get_boolean           (EvMetadata  *metadata,
					       const gchar *key,
					       gboolean    *value);
gboolean    ev_metadata_set_boolean           (EvMetadata  *metadata,
					       const gchar *key,
					       gboolean     value);
gboolean    ev_metadata_has_key               (EvMetadata  *metadata,
                                               const gchar *key);

gboolean    ev_is_metadata_supported_for_file (GFile       *file);

G_END_DECLS
