/* ev-metadata.c
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

#include <gio/gio.h>
#include <string.h>

#include "ev-metadata.h"
#include "ev-file-helpers.h"

struct _EvMetadata {
	GObject base;

	GFile      *file;
	GHashTable *items;
};

struct _EvMetadataClass {
	GObjectClass base_class;
};

G_DEFINE_TYPE (EvMetadata, ev_metadata, G_TYPE_OBJECT)

#define EV_METADATA_NAMESPACE "metadata::evince"

static void
ev_metadata_finalize (GObject *object)
{
	EvMetadata *metadata = EV_METADATA (object);

	g_clear_pointer (&metadata->items, g_hash_table_destroy);
	g_clear_object (&metadata->file);

	G_OBJECT_CLASS (ev_metadata_parent_class)->finalize (object);
}

static void
ev_metadata_init (EvMetadata *metadata)
{
	metadata->items = g_hash_table_new_full (g_str_hash,
						 g_str_equal,
						 g_free,
						 g_free);
}

static void
ev_metadata_class_init (EvMetadataClass *klass)
{
	GObjectClass *gobject_class = G_OBJECT_CLASS (klass);

	gobject_class->finalize = ev_metadata_finalize;
}

static void
ev_metadata_load (EvMetadata *metadata)
{
	GFileInfo *info;
	gchar    **attrs;
	gint       i;
	GError    *error = NULL;

	info = g_file_query_info (metadata->file, "metadata::*", 0, NULL, &error);
	if (!info) {
		g_warning ("%s", error->message);
		g_error_free (error);

		return;
	}

	if (!g_file_info_has_namespace (info, "metadata")) {
		g_object_unref (info);

		return;
	}

	attrs = g_file_info_list_attributes (info, "metadata");
	for (i = 0; attrs[i]; i++) {
		GFileAttributeType type;
		gpointer           value;
		const gchar       *key;

		if (!g_str_has_prefix (attrs[i], EV_METADATA_NAMESPACE))
			continue;

		if (!g_file_info_get_attribute_data (info, attrs[i],
						     &type, &value, NULL)) {
			continue;
		}

		key = attrs[i] + strlen (EV_METADATA_NAMESPACE"::");

		if (type == G_FILE_ATTRIBUTE_TYPE_STRING) {
			g_hash_table_insert (metadata->items,
					     g_strdup (key),
					     g_strdup (value));
		}
	}
	g_strfreev (attrs);
	g_object_unref (info);
}

EvMetadata *
ev_metadata_new (GFile *file)
{
	EvMetadata *metadata;

	g_return_val_if_fail (G_IS_FILE (file), NULL);

	metadata = EV_METADATA (g_object_new (EV_TYPE_METADATA, NULL));
        if (!ev_file_is_temp (file)) {
                metadata->file = g_object_ref (file);
                ev_metadata_load (metadata);
        }

	return metadata;
}

gboolean
ev_metadata_is_empty (EvMetadata *metadata)
{
	return g_hash_table_size (metadata->items) == 0;
}

gboolean
ev_metadata_get_string (EvMetadata  *metadata,
			const gchar *key,
			gchar     **value)
{
	gchar *v;

	v = g_hash_table_lookup (metadata->items, key);
	if (!v)
		return FALSE;

	*value = v;
	return TRUE;
}

static void
metadata_set_callback (GObject      *file,
		       GAsyncResult *result,
		       EvMetadata   *metadata)
{
	GError *error = NULL;

	if (!g_file_set_attributes_finish (G_FILE (file), result, NULL, &error)) {
		g_warning ("%s", error->message);
		g_error_free (error);
	}
}

gboolean
ev_metadata_set_string (EvMetadata  *metadata,
			const gchar *key,
			const gchar *value)
{
	GFileInfo *info;
	gchar     *gio_key;

        g_hash_table_insert (metadata->items, g_strdup (key), g_strdup (value));
        if (!metadata->file)
                return TRUE;

	info = g_file_info_new ();

	gio_key = g_strconcat (EV_METADATA_NAMESPACE"::", key, NULL);
	if (value) {
		g_file_info_set_attribute_string (info, gio_key, value);
	} else {
		g_file_info_set_attribute (info, gio_key,
					   G_FILE_ATTRIBUTE_TYPE_INVALID,
					   NULL);
	}
	g_free (gio_key);

	g_file_set_attributes_async (metadata->file,
				     info,
				     0,
				     G_PRIORITY_DEFAULT,
				     NULL,
				     (GAsyncReadyCallback)metadata_set_callback,
				     metadata);
	g_object_unref (info);

	return TRUE;
}

gboolean
ev_metadata_get_int (EvMetadata  *metadata,
		     const gchar *key,
		     gint        *value)
{
	gchar *string_value;
	gchar *endptr;
	gint   int_value;

	if (!ev_metadata_get_string (metadata, key, &string_value))
		return FALSE;

	int_value = g_ascii_strtoull (string_value, &endptr, 0);
	if (int_value == 0 && string_value == endptr)
		return FALSE;

	*value = int_value;
	return TRUE;
}

gboolean
ev_metadata_set_int (EvMetadata  *metadata,
		     const gchar *key,
		     gint         value)
{
	gchar string_value[32];

	g_snprintf (string_value, sizeof (string_value), "%d", value);

	return ev_metadata_set_string (metadata, key, string_value);
}

gboolean
ev_metadata_get_double (EvMetadata  *metadata,
			const gchar *key,
			gdouble     *value)
{
	gchar  *string_value;
	gchar  *endptr;
	gdouble double_value;

	if (!ev_metadata_get_string (metadata, key, &string_value))
		return FALSE;

	double_value = g_ascii_strtod (string_value, &endptr);
	if (double_value == 0. && string_value == endptr)
		return FALSE;

	*value = double_value;
	return TRUE;
}

gboolean
ev_metadata_set_double (EvMetadata  *metadata,
			const gchar *key,
			gdouble      value)
{
	gchar string_value[G_ASCII_DTOSTR_BUF_SIZE];

	g_ascii_dtostr (string_value, G_ASCII_DTOSTR_BUF_SIZE, value);

	return ev_metadata_set_string (metadata, key, string_value);
}

gboolean
ev_metadata_get_boolean (EvMetadata  *metadata,
			 const gchar *key,
			 gboolean    *value)
{
	gint int_value;

	if (!ev_metadata_get_int (metadata, key, &int_value))
		return FALSE;

	*value = int_value;
	return TRUE;
}

gboolean
ev_metadata_set_boolean (EvMetadata  *metadata,
			 const gchar *key,
			 gboolean     value)
{
	return ev_metadata_set_string (metadata, key, value ? "1" : "0");
}

gboolean
ev_metadata_has_key (EvMetadata  *metadata,
                     const gchar *key)
{
        return g_hash_table_lookup (metadata->items, key) != NULL;
}

gboolean
ev_is_metadata_supported_for_file (GFile *file)
{
	GFileAttributeInfoList *namespaces;
	gint i;
	gboolean retval = FALSE;

	namespaces = g_file_query_writable_namespaces (file, NULL, NULL);
	if (!namespaces)
		return retval;

	for (i = 0; i < namespaces->n_infos; i++) {
		if (strcmp (namespaces->infos[i].name, "metadata") == 0) {
			retval = TRUE;
			break;
		}
	}

	g_file_attribute_info_list_unref (namespaces);

	return retval;
}
