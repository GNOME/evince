/* ev-convert-metadata.c
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

//#include <config.h>

#include <gtk/gtk.h>
#include <glib.h>
#include <gio/gio.h>
#include <glib/gi18n.h>
#include <string.h>
#include <libxml/tree.h>

#define EV_METADATA_NAMESPACE "metadata::evince"

typedef struct {
	xmlNodePtr cur;
	xmlChar *uri;
} DocItem;

typedef struct {
	GtkWidget *progress;
	GtkWidget *label;

	xmlDocPtr doc;
	GList *items;
	GList *current;
	guint n_item;
} ConvertData;

static void
free_doc_item (DocItem *item)
{
	xmlFree (item->uri);
	g_free (item);
}

static void
convert_finish (ConvertData *data)
{
	g_list_foreach (data->items, (GFunc)free_doc_item, NULL);
	g_list_free (data->items);
	xmlFreeDoc (data->doc);
	g_free (data);

	gtk_main_quit ();
}

static gboolean
convert_file (ConvertData *data)
{
	GFile *file;
	DocItem *item;
	const gchar *uri;
	xmlNodePtr node;
	xmlNodePtr cur;
	gint total, current;
	gchar *text;

	if (!data->current)
		return FALSE;

	item = (DocItem *) data->current->data;
	uri = (const gchar *)item->uri;
	node = item->cur;
	data->current = g_list_next (data->current);

	/* Update progress information */
	total = g_list_length (data->items);
	current = ++(data->n_item);

	text = g_strdup_printf (_("Converting %s"), uri);
	gtk_label_set_text (GTK_LABEL (data->label), text);
	g_free (text);

	text = g_strdup_printf (_("%d of %d documents converted"), current, total);
	gtk_progress_bar_set_text (GTK_PROGRESS_BAR (data->progress), text);
	g_free (text);
	gtk_progress_bar_set_fraction (GTK_PROGRESS_BAR (data->progress),
				       (gdouble)(current - 1) / total);

	file = g_file_new_for_uri (uri);
	if (!g_file_query_exists (file, NULL)) {
		g_printerr ("Uri %s does not exist\n", uri);
		g_object_unref (file);

		return data->current != NULL;
	}

	for (cur = node->xmlChildrenNode; cur != NULL; cur = cur->next) {
		xmlChar *key;
		xmlChar *value;

		if (xmlStrcmp (cur->name, (const xmlChar *)"entry") != 0)
			continue;

		key = xmlGetProp (cur, (const xmlChar *)"key");
		value = xmlGetProp (cur, (const xmlChar *)"value");
		if (key && value) {
			GFileInfo *info;
			gchar *gio_key;
			GError *error = NULL;

			info = g_file_info_new ();

			gio_key = g_strconcat (EV_METADATA_NAMESPACE"::", key, NULL);
			g_file_info_set_attribute_string (info, gio_key, (const gchar *)value);
			g_free (gio_key);

			if (!g_file_set_attributes_from_info (file, info, 0, NULL, &error)) {
				g_printerr ("Error setting metadata for %s: %s\n",
					    uri, error->message);
				g_error_free (error);
			}

			g_object_unref (info);
		}

		if (key)
			xmlFree (key);
		if (value)
			xmlFree (value);
	}

	g_object_unref (file);

	return data->current != NULL;
}

static void
convert_metadata_cancel (GtkDialog   *dialog,
			 gint         response_id,
			 ConvertData *data)
{
	convert_finish (data);
	exit (1);
}

static void
show_progress_dialog (ConvertData *data)
{
	GtkWidget *dialog;
	GtkWidget *action_area;
	GtkWidget *vbox, *pbox;
	GtkWidget *label;
	GtkWidget *progress;
	gchar     *text;

	dialog = gtk_dialog_new_with_buttons (_("Converting metadata"),
					      NULL,
					      GTK_DIALOG_NO_SEPARATOR,
					      GTK_STOCK_CANCEL, GTK_RESPONSE_CANCEL,
					      NULL);
	action_area = gtk_dialog_get_action_area (GTK_DIALOG (dialog));
	gtk_container_set_border_width (GTK_CONTAINER (dialog), 5);
	gtk_container_set_border_width (GTK_CONTAINER (action_area), 5);
	gtk_box_set_spacing (GTK_BOX (action_area), 6);

	vbox = gtk_dialog_get_content_area (GTK_DIALOG (dialog));
	gtk_box_set_spacing (GTK_BOX (vbox), 12);

	label = gtk_label_new (NULL);
	gtk_label_set_line_wrap (GTK_LABEL (label), TRUE);
	gtk_misc_set_alignment (GTK_MISC (label), 0.0, 0.0);
	text = g_strdup_printf ("<b>%s</b>", _("Converting metadata"));
	gtk_label_set_markup (GTK_LABEL (label), text);
	g_free (text);
	gtk_box_pack_start (GTK_BOX (vbox), label, FALSE, FALSE, 0);
	gtk_widget_show (label);

	label = gtk_label_new (_("The metadata format used by Evince "
				 "has changed, and hence it needs to be migrated. "
				 "If the migration is cancelled the metadata "
				 "storage will not work."));
	gtk_misc_set_alignment (GTK_MISC (label), 0.0, 0.0);
	gtk_label_set_line_wrap (GTK_LABEL (label), TRUE);
	gtk_box_pack_start (GTK_BOX (vbox), label, TRUE, TRUE, 0);
	gtk_widget_show (label);

	pbox = gtk_vbox_new (FALSE, 6);
	progress = gtk_progress_bar_new ();
	data->progress = progress;
	gtk_box_pack_start (GTK_BOX (pbox), progress, TRUE, TRUE, 0);
	gtk_widget_show (progress);

	label = gtk_label_new (NULL);
	data->label = label;
	gtk_misc_set_alignment (GTK_MISC (label), 0.0, 0.0);
	gtk_label_set_ellipsize (GTK_LABEL (label), PANGO_ELLIPSIZE_END);
	gtk_box_pack_start (GTK_BOX (pbox), label, FALSE, FALSE, 0);
	gtk_widget_show (label);

	gtk_box_pack_start (GTK_BOX (vbox), pbox, TRUE, TRUE, 0);
	gtk_widget_show (pbox);

	g_signal_connect (dialog, "response",
			  G_CALLBACK (convert_metadata_cancel),
			  data);

	gtk_widget_show (dialog);
}

static gboolean
convert_metadata_file (const gchar *filename)
{
	ConvertData *data;
	xmlDocPtr doc;
	xmlNodePtr cur;

	if (!g_file_test (filename, G_FILE_TEST_EXISTS))
		return FALSE;

	doc = xmlParseFile (filename);
	if (!doc) {
		g_printerr ("Error loading metadata file %s\n", filename);
		return FALSE;
	}

	cur = xmlDocGetRootElement (doc);
	if (!cur) {
		g_printerr ("Metadata file %s is empty\n", filename);
		xmlFreeDoc (doc);
		return TRUE;
	}

	if (xmlStrcmp (cur->name, (const xmlChar *) "metadata")) {
		g_printerr ("File %s is not a valid evince metadata file\n", filename);
		xmlFreeDoc (doc);
		return FALSE;
	}

	data = g_new0 (ConvertData, 1);
	data->doc = doc;

	for (cur = cur->xmlChildrenNode; cur != NULL; cur = cur->next) {
		xmlChar *uri;
		DocItem *item;

		if (xmlStrcmp (cur->name, (const xmlChar *)"document") != 0)
			continue;

		uri = xmlGetProp (cur, (const xmlChar *)"uri");
		if (!uri)
			continue;

		item = g_new (DocItem, 1);
		item->uri = uri;
		item->cur = cur;
		data->items = g_list_prepend (data->items, item);
	}

	if (!data->items) {
		xmlFreeDoc (data->doc);
		g_free (data);

		return TRUE;
	}

	show_progress_dialog (data);

	data->current = data->items;
	g_idle_add_full (G_PRIORITY_DEFAULT_IDLE,
			 (GSourceFunc)convert_file,
			 data,
			 (GDestroyNotify)convert_finish);

	return TRUE;
}

gint
main (gint argc, gchar **argv)
{
	if (argc != 2) {
		g_printerr ("%s\n", "Usage: evince-convert-metadata FILE");
		return 1;
	}

	gtk_init (&argc, &argv);

	if (!convert_metadata_file (argv[1]))
		return 1;

	gtk_main ();

	return 0;
}
