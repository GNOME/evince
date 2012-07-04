/* this file is part of evince, a gnome document viewer
 *
 *  Copyright (C) 2006 Carlos Garcia Campos <carlosgc@gnome.org>
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

#include <config.h>
#include <glib/gi18n-lib.h>
#include <glib/gstdio.h>
#include <gtk/gtk.h>
#include "ev-file-helpers.h"
#include "ev-attachment.h"

enum
{
	PROP_0,
	PROP_NAME,
	PROP_DESCRIPTION,
	PROP_MTIME,
	PROP_CTIME,
	PROP_SIZE,
	PROP_DATA
};

struct _EvAttachmentPrivate {
	gchar                   *name;
	gchar                   *description;
	GTime                    mtime;
	GTime                    ctime;
	gsize                    size;
	gchar                   *data;
	gchar                   *mime_type;

	GAppInfo                *app;
	GFile                   *tmp_file;
};

#define EV_ATTACHMENT_GET_PRIVATE(object) \
                (G_TYPE_INSTANCE_GET_PRIVATE ((object), EV_TYPE_ATTACHMENT, EvAttachmentPrivate))

G_DEFINE_TYPE (EvAttachment, ev_attachment, G_TYPE_OBJECT)

GQuark
ev_attachment_error_quark (void)
{
	static GQuark error_quark = 0;
	
	if (error_quark == 0)
		error_quark =
			g_quark_from_static_string ("ev-attachment-error-quark");
	
	return error_quark;
}

static void
ev_attachment_finalize (GObject *object)
{
	EvAttachment *attachment = EV_ATTACHMENT (object);

	if (attachment->priv->name) {
		g_free (attachment->priv->name);
		attachment->priv->name = NULL;
	}

	if (attachment->priv->description) {
		g_free (attachment->priv->description);
		attachment->priv->description = NULL;
	}

	if (attachment->priv->data) {
		g_free (attachment->priv->data);
		attachment->priv->data = NULL;
	}

	if (attachment->priv->mime_type) {
		g_free (attachment->priv->mime_type);
		attachment->priv->mime_type = NULL;
	}

	if (attachment->priv->app) {
		g_object_unref (attachment->priv->app);
		attachment->priv->app = NULL;
	}

	if (attachment->priv->tmp_file) {
		ev_tmp_file_unlink (attachment->priv->tmp_file);
		g_object_unref (attachment->priv->tmp_file);
		attachment->priv->tmp_file = NULL;
	}

	G_OBJECT_CLASS (ev_attachment_parent_class)->finalize (object);
}

static void
ev_attachment_set_property (GObject      *object,
			    guint         prop_id,
			    const GValue *value,
			    GParamSpec   *param_spec)
{
	EvAttachment *attachment = EV_ATTACHMENT (object);

	switch (prop_id) {
	case PROP_NAME:
		attachment->priv->name = g_value_dup_string (value);
		break;
	case PROP_DESCRIPTION:
		attachment->priv->description = g_value_dup_string (value);
		break;
	case PROP_MTIME:
		attachment->priv->mtime = g_value_get_ulong (value);
		break;
	case PROP_CTIME:
		attachment->priv->ctime = g_value_get_ulong (value);
		break;
	case PROP_SIZE:
		attachment->priv->size = g_value_get_uint (value);
		break;
	case PROP_DATA:
		attachment->priv->data = g_value_get_pointer (value);
		attachment->priv->mime_type = g_content_type_guess (attachment->priv->name,
								    (guchar *) attachment->priv->data,
								    attachment->priv->size,
								    NULL);
		break;
	default:
		G_OBJECT_WARN_INVALID_PROPERTY_ID (object,
						   prop_id,
						   param_spec);
		break;
	}
}

static void
ev_attachment_class_init (EvAttachmentClass *klass)
{
	GObjectClass *g_object_class;

	g_object_class = G_OBJECT_CLASS (klass);

	g_object_class->set_property = ev_attachment_set_property;

	g_type_class_add_private (g_object_class, sizeof (EvAttachmentPrivate));

	/* Properties */
	g_object_class_install_property (g_object_class,
					 PROP_NAME,
					 g_param_spec_string ("name",
							      "Name",
							      "The attachment name",
							      NULL,
							      G_PARAM_WRITABLE |
							      G_PARAM_CONSTRUCT_ONLY |
                                                              G_PARAM_STATIC_STRINGS));
	g_object_class_install_property (g_object_class,
					 PROP_DESCRIPTION,
					 g_param_spec_string ("description",
							      "Description",
							      "The attachment description",
							      NULL,
							      G_PARAM_WRITABLE |
							      G_PARAM_CONSTRUCT_ONLY |
                                                              G_PARAM_STATIC_STRINGS));
	g_object_class_install_property (g_object_class,
					 PROP_MTIME,
					 g_param_spec_ulong ("mtime",
							     "ModifiedTime", 
							     "The attachment modification date",
							     0, G_MAXULONG, 0,
							     G_PARAM_WRITABLE |
							     G_PARAM_CONSTRUCT_ONLY |
                                                             G_PARAM_STATIC_STRINGS));
	g_object_class_install_property (g_object_class,
					 PROP_CTIME,
					 g_param_spec_ulong ("ctime",
							     "CreationTime",
							     "The attachment creation date",
							     0, G_MAXULONG, 0,
							     G_PARAM_WRITABLE |
							     G_PARAM_CONSTRUCT_ONLY |
                                                             G_PARAM_STATIC_STRINGS));
	g_object_class_install_property (g_object_class,
					 PROP_SIZE,
					 g_param_spec_uint ("size",
							    "Size",
							    "The attachment size",
							    0, G_MAXUINT, 0,
							    G_PARAM_WRITABLE |
							    G_PARAM_CONSTRUCT_ONLY |
                                                            G_PARAM_STATIC_STRINGS));
	g_object_class_install_property (g_object_class,
					 PROP_DATA,
					 g_param_spec_pointer ("data",
							       "Data",
							       "The attachment data",
							       G_PARAM_WRITABLE |
							       G_PARAM_CONSTRUCT_ONLY |
                                                               G_PARAM_STATIC_STRINGS));
	
	g_object_class->finalize = ev_attachment_finalize;
}

static void
ev_attachment_init (EvAttachment *attachment)
{
	attachment->priv = EV_ATTACHMENT_GET_PRIVATE (attachment);

	attachment->priv->name = NULL;
	attachment->priv->description = NULL;
	attachment->priv->data = NULL;
	attachment->priv->mime_type = NULL;

	attachment->priv->tmp_file = NULL;
}

EvAttachment *
ev_attachment_new (const gchar *name,
		   const gchar *description,
		   GTime        mtime,
		   GTime        ctime,
		   gsize        size,
		   gpointer     data)
{
	EvAttachment *attachment;

	attachment = g_object_new (EV_TYPE_ATTACHMENT,
				   "name", name,
				   "description", description,
				   "mtime", mtime,
				   "ctime", ctime,
				   "size", size,
				   "data", data,
				   NULL);

	return attachment;
}

const gchar *
ev_attachment_get_name (EvAttachment *attachment)
{
	g_return_val_if_fail (EV_IS_ATTACHMENT (attachment), NULL);

	return attachment->priv->name;
}

const gchar *
ev_attachment_get_description (EvAttachment *attachment)
{
	g_return_val_if_fail (EV_IS_ATTACHMENT (attachment), NULL);

	return attachment->priv->description;
}

GTime
ev_attachment_get_modification_date (EvAttachment *attachment)
{
	g_return_val_if_fail (EV_IS_ATTACHMENT (attachment), 0);

	return attachment->priv->mtime;
}

GTime
ev_attachment_get_creation_date (EvAttachment *attachment)
{
	g_return_val_if_fail (EV_IS_ATTACHMENT (attachment), 0);

	return attachment->priv->ctime;
}

const gchar *
ev_attachment_get_mime_type (EvAttachment *attachment)
{
	g_return_val_if_fail (EV_IS_ATTACHMENT (attachment), NULL);

	return attachment->priv->mime_type;
}

gboolean
ev_attachment_save (EvAttachment *attachment,
		    GFile        *file,
		    GError      **error)
{
	GFileOutputStream *output_stream;
	GError *ioerror = NULL;
	gssize  written_bytes;

	g_return_val_if_fail (EV_IS_ATTACHMENT (attachment), FALSE);
	g_return_val_if_fail (G_IS_FILE (file), FALSE);

	output_stream = g_file_replace (file, NULL, FALSE, 0, NULL, &ioerror);
	if (output_stream == NULL) {
		char *uri;
		
		uri = g_file_get_uri (file);
		g_set_error (error,
			     EV_ATTACHMENT_ERROR, 
			     ioerror->code,
			     _("Couldn't save attachment “%s”: %s"),
			     uri, 
			     ioerror->message);

		g_error_free (ioerror);
		g_free (uri);
		
		return FALSE;
	}
	
	written_bytes = g_output_stream_write (G_OUTPUT_STREAM (output_stream),
					       attachment->priv->data,
					       attachment->priv->size,
					       NULL, &ioerror);
	if (written_bytes == -1) {
		char *uri;
		
		uri = g_file_get_uri (file);
		g_set_error (error,
			     EV_ATTACHMENT_ERROR,
			     ioerror->code,
			     _("Couldn't save attachment “%s”: %s"),
			     uri,
			     ioerror->message);
		
		g_output_stream_close (G_OUTPUT_STREAM (output_stream), NULL, NULL);
		g_error_free (ioerror);
		g_free (uri);

		return FALSE;
	}

	g_output_stream_close (G_OUTPUT_STREAM (output_stream), NULL, NULL);

	return TRUE;
	
}

static gboolean
ev_attachment_launch_app (EvAttachment *attachment,
			  GdkScreen    *screen,
			  guint32       timestamp,
			  GError      **error)
{
	gboolean             result;
	GList               *files = NULL;
	GdkAppLaunchContext *context;
        GdkDisplay          *display;
	GError              *ioerror = NULL;

	g_assert (G_IS_FILE (attachment->priv->tmp_file));
	g_assert (G_IS_APP_INFO (attachment->priv->app));

	files = g_list_prepend (files, attachment->priv->tmp_file);

        display = screen ? gdk_screen_get_display (screen) : gdk_display_get_default ();
	context = gdk_display_get_app_launch_context (display);
	gdk_app_launch_context_set_screen (context, screen);
	gdk_app_launch_context_set_timestamp (context, timestamp);

	result = g_app_info_launch (attachment->priv->app, files,
				    G_APP_LAUNCH_CONTEXT (context),
                                    &ioerror);
        g_object_unref (context);

	if (!result) {
		g_set_error (error,
			     EV_ATTACHMENT_ERROR,
			     (gint) result,
			     _("Couldn't open attachment “%s”: %s"),
			     attachment->priv->name,
			     ioerror->message);

		g_list_free (files);
		g_error_free (ioerror);
		
		return FALSE;
	}

	g_list_free (files);
	
	return TRUE;
}

gboolean
ev_attachment_open (EvAttachment *attachment,
		    GdkScreen    *screen,
		    guint32       timestamp,
		    GError      **error)
{
	GAppInfo *app_info;
	gboolean  retval = FALSE;

	g_return_val_if_fail (EV_IS_ATTACHMENT (attachment), FALSE);
	
	if (!attachment->priv->app) {
		app_info = g_app_info_get_default_for_type (attachment->priv->mime_type, FALSE);
		attachment->priv->app = app_info;
	}

	if (!attachment->priv->app) {
		g_set_error (error,
			     EV_ATTACHMENT_ERROR,
			     0,
			     _("Couldn't open attachment “%s”"),
			     attachment->priv->name);
		
		return FALSE;
	}

	if (attachment->priv->tmp_file) {
		retval = ev_attachment_launch_app (attachment, screen,
						   timestamp, error);
	} else {
                char *basename;
                char *template;
		GFile *file;

                /* FIXMEchpe: convert to filename encoding first! */
                basename = g_path_get_basename (ev_attachment_get_name (attachment));
                template = g_strdup_printf ("%s.XXXXXX", basename);
                file = ev_mkstemp_file (template, error);
                g_free (template);
                g_free (basename);

		if (file != NULL && ev_attachment_save (attachment, file, error)) {
			if (attachment->priv->tmp_file)
				g_object_unref (attachment->priv->tmp_file);
			attachment->priv->tmp_file = g_object_ref (file);

			retval = ev_attachment_launch_app (attachment, screen,
							   timestamp, error);
		}

		g_object_unref (file);
	}

	return retval;
}

