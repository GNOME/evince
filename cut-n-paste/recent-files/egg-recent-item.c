/* -*- Mode: C; tab-width: 8; indent-tabs-mode: t; c-basic-offset: 8 -*- */
/*
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as
 * published by the Free Software Foundation; either version 2 of the
 * License, or (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA 02111-1307, USA.
 *
 * Authors:
 *   James Willcox <jwillcox@cs.indiana.edu>
 */


#include <stdio.h>
#include <string.h>
#include <glib.h>
#include <libgnomevfs/gnome-vfs.h>
#include <libgnomevfs/gnome-vfs-mime-utils.h>
#include "egg-recent-item.h"



EggRecentItem *
egg_recent_item_new (void)
{
	EggRecentItem *item;

	item = g_new (EggRecentItem, 1);

	item->groups = NULL;
	item->private_data = FALSE;
	item->uri = NULL;
	item->mime_type = NULL;
	item->mime_type_is_explicit = FALSE;

	item->refcount = 1;

	return item;
}

static void
egg_recent_item_free (EggRecentItem *item)
{
	if (item->uri)
		g_free (item->uri);

	if (item->mime_type)
		g_free (item->mime_type);

	if (item->groups) {
		g_list_foreach (item->groups, (GFunc)g_free, NULL);
		g_list_free (item->groups);
		item->groups = NULL;
	}

	g_free (item);
}

EggRecentItem *
egg_recent_item_ref (EggRecentItem *item)
{
	item->refcount++;
	return item;
}

EggRecentItem *
egg_recent_item_unref (EggRecentItem *item)
{
	item->refcount--;

	if (item->refcount == 0) {
		egg_recent_item_free (item);
	}

	return item;
}


EggRecentItem * 
egg_recent_item_new_from_uri (const gchar *uri)
{
	EggRecentItem *item;

	g_return_val_if_fail (uri != NULL, NULL);

	item = egg_recent_item_new ();

	if (!egg_recent_item_set_uri (item ,uri)) {
		egg_recent_item_free (item);
		return NULL;
	}
	
	return item;
}

static void
egg_recent_item_update_mime_type (EggRecentItem *item)
{
	if (!item->mime_type_is_explicit) {
		g_free (item->mime_type);
		item->mime_type = NULL;

		if (item->uri)
			item->mime_type = gnome_vfs_get_mime_type (item->uri);

		if (!item->mime_type)
			item->mime_type = g_strdup (GNOME_VFS_MIME_TYPE_UNKNOWN);
	}
}

gboolean
egg_recent_item_set_uri (EggRecentItem *item, const gchar *uri)
{
	gchar *utf8_uri;

	/* if G_BROKEN_FILENAMES is not set, this should succede */
	if (g_utf8_validate (uri, -1, NULL)) {
		item->uri = gnome_vfs_make_uri_from_input (uri);
	} else {
		utf8_uri = g_filename_to_utf8 (uri, -1, NULL, NULL, NULL);

		if (utf8_uri == NULL) {
			g_warning ("Couldn't convert URI to UTF-8");
			return FALSE;
		}

		if (g_utf8_validate (utf8_uri, -1, NULL)) {
			item->uri = gnome_vfs_make_uri_from_input (utf8_uri);
		} else {
			g_free (utf8_uri);
			return FALSE;
		}

		g_free (utf8_uri);
	}

	return TRUE;
}

gchar * 
egg_recent_item_get_uri (const EggRecentItem *item)
{
	return g_strdup (item->uri);
}

G_CONST_RETURN gchar * 
egg_recent_item_peek_uri (const EggRecentItem *item)
{
	return item->uri;
}

gchar * 
egg_recent_item_get_uri_utf8 (const EggRecentItem *item)
{
	/* this could fail, but it's not likely, since we've already done it
	 * once in set_uri()
	 */
	return g_filename_to_utf8 (item->uri, -1, NULL, NULL, NULL);
}

gchar *
egg_recent_item_get_uri_for_display (const EggRecentItem *item)
{
	return gnome_vfs_format_uri_for_display (item->uri);
}

/* Stolen from gnome_vfs_make_valid_utf8() */
static char *
make_valid_utf8 (const char *name)
{
	GString *string;
	const char *remainder, *invalid;
	int remaining_bytes, valid_bytes;

	string = NULL;
	remainder = name;
	remaining_bytes = name ? strlen (name) : 0;

	while (remaining_bytes != 0) {
		if (g_utf8_validate (remainder, remaining_bytes, &invalid))
			break;

		valid_bytes = invalid - remainder;

		if (string == NULL)
			string = g_string_sized_new (remaining_bytes);

		g_string_append_len (string, remainder, valid_bytes);
		g_string_append_c (string, '?');

		remaining_bytes -= valid_bytes + 1;
		remainder = invalid + 1;
	}

	if (string == NULL)
		return g_strdup (name);

	g_string_append (string, remainder);
/* 	g_string_append (string, _(" (invalid file name)")); */
	g_assert (g_utf8_validate (string->str, -1, NULL));

	return g_string_free (string, FALSE);
}

static gchar *
get_uri_shortname_for_display (GnomeVFSURI *uri)
{
	gchar    *name;	
	gboolean  validated;

	validated = FALSE;
	name = gnome_vfs_uri_extract_short_name (uri);
	
	if (name == NULL)
	{
		name = gnome_vfs_uri_to_string (uri, GNOME_VFS_URI_HIDE_PASSWORD);
	}
	else if (g_ascii_strcasecmp (uri->method_string, "file") == 0)
	{
		gchar *text_uri;
		gchar *local_file;
		text_uri = gnome_vfs_uri_to_string (uri, GNOME_VFS_URI_HIDE_PASSWORD);
		local_file = gnome_vfs_get_local_path_from_uri (text_uri);
		
		if (local_file != NULL)
		{
			g_free (name);
			name = g_filename_display_basename (local_file);
			validated = TRUE;
		}
		
		g_free (local_file);
		g_free (text_uri);
	} 
	else if (!gnome_vfs_uri_has_parent (uri)) 
	{
		const gchar *method;
		
		method = uri->method_string;
		
		if (name == NULL ||
		    strcmp (name, GNOME_VFS_URI_PATH_STR) == 0) 
		{
			g_free (name);
			name = g_strdup (method);
		} 
		else 
		{
			gchar *tmp;
			
			tmp = name;
			name = g_strdup_printf ("%s: %s", method, name);
			g_free (tmp);
		}
	}

	if (!validated && !g_utf8_validate (name, -1, NULL)) 
	{
		gchar *utf8_name;
		
		utf8_name = make_valid_utf8 (name);
		g_free (name);
		name = utf8_name;
	}

	return name;
}

/**
 * egg_recent_item_get_short_name:
 * @item: an #EggRecentItem
 *
 * Computes a valid UTF-8 string that can be used as the name of the item in a
 * menu or list.  For example, calling this function on an item that refers to
 * "file:///foo/bar.txt" will yield "bar.txt".
 *
 * Return value: A newly-allocated string in UTF-8 encoding; free it with
 * g_free().
 **/
gchar *
egg_recent_item_get_short_name (const EggRecentItem *item)
{
	GnomeVFSURI *uri;
	gchar *short_name;

	g_return_val_if_fail (item != NULL, NULL);

	if (item->uri == NULL)
		return NULL;

	uri = gnome_vfs_uri_new (item->uri);
	if (uri == NULL)
		return NULL;

	short_name = get_uri_shortname_for_display (uri);

	gnome_vfs_uri_unref (uri);

	return short_name;
}

void 
egg_recent_item_set_mime_type (EggRecentItem *item, const gchar *mime)
{
	g_free (item->mime_type);
	item->mime_type = NULL;

	if (mime && mime[0]) {
		item->mime_type_is_explicit = TRUE;
		item->mime_type             = g_strdup (mime);
	} else {
		item->mime_type_is_explicit = FALSE;
	}
}

gchar * 
egg_recent_item_get_mime_type (EggRecentItem *item)
{
	egg_recent_item_update_mime_type (item);

	return g_strdup (item->mime_type);
}

void 
egg_recent_item_set_timestamp (EggRecentItem *item, time_t timestamp)
{
	if (timestamp == (time_t) -1)
		time (&timestamp);

	item->timestamp = timestamp;
}

time_t 
egg_recent_item_get_timestamp (const EggRecentItem *item)
{
	return item->timestamp;
}

G_CONST_RETURN GList *
egg_recent_item_get_groups (const EggRecentItem *item)
{
	return item->groups;
}

gboolean
egg_recent_item_in_group (const EggRecentItem *item, const gchar *group_name)
{
	GList *tmp;

	tmp = item->groups;
	while (tmp != NULL) {
		gchar *val = (gchar *)tmp->data;
		
		if (strcmp (group_name, val) == 0)
			return TRUE;

		tmp = tmp->next;
	}
	
	return FALSE;
}

void
egg_recent_item_add_group (EggRecentItem *item, const gchar *group_name)
{
	g_return_if_fail (group_name != NULL);

	if (!egg_recent_item_in_group (item, group_name))
		item->groups = g_list_append (item->groups, g_strdup (group_name));
}

void
egg_recent_item_remove_group (EggRecentItem *item, const gchar *group_name)
{
	GList *tmp;

	g_return_if_fail (group_name != NULL);

	tmp = item->groups;
	while (tmp != NULL) {
		gchar *val = (gchar *)tmp->data;
		
		if (strcmp (group_name, val) == 0) {
			item->groups = g_list_remove (item->groups,
						      val);
			g_free (val);
			break;
		}

		tmp = tmp->next;
	}
}

void
egg_recent_item_set_private (EggRecentItem *item, gboolean priv)
{
	item->private_data = priv;
}

gboolean
egg_recent_item_get_private (const EggRecentItem *item)
{
	return item->private_data;
}

GType
egg_recent_item_get_type (void)
{
	static GType boxed_type = 0;
	
	if (!boxed_type) {
		boxed_type = g_boxed_type_register_static ("EggRecentItem",
					(GBoxedCopyFunc)egg_recent_item_ref,
					(GBoxedFreeFunc)egg_recent_item_unref);
	}
	
	return boxed_type;
}
