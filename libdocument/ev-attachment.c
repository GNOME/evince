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

typedef struct {
	gchar                   *name;
	gchar                   *description;
	GTime                    mtime;
	GTime                    ctime;
	gsize                    size;
	gchar                   *data;
	gchar                   *mime_type;

	GAppInfo                *app;
	GFile                   *tmp_file;
} EvAttachmentPrivate;

G_DEFINE_TYPE_WITH_PRIVATE (EvAttachment, ev_attachment, G_TYPE_OBJECT)

#define GET_PRIVATE(o) ev_attachment_get_instance_private (o);

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
	EvAttachmentPrivate *priv = GET_PRIVATE (attachment);

	if (priv->name) {
		g_free (priv->name);
		priv->name = NULL;
	}

	if (priv->description) {
		g_free (priv->description);
		priv->description = NULL;
	}

	if (priv->data) {
		g_free (priv->data);
		priv->data = NULL;
	}

	if (priv->mime_type) {
		g_free (priv->mime_type);
		priv->mime_type = NULL;
	}

	if (priv->app) {
		g_object_unref (priv->app);
		priv->app = NULL;
	}

	if (priv->tmp_file) {
		ev_tmp_file_unlink (priv->tmp_file);
		g_object_unref (priv->tmp_file);
		priv->tmp_file = NULL;
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
	EvAttachmentPrivate *priv = GET_PRIVATE (attachment);

	switch (prop_id) {
	case PROP_NAME:
		priv->name = g_value_dup_string (value);
		break;
	case PROP_DESCRIPTION:
		priv->description = g_value_dup_string (value);
		break;
	case PROP_MTIME:
		priv->mtime = g_value_get_ulong (value);
		break;
	case PROP_CTIME:
		priv->ctime = g_value_get_ulong (value);
		break;
	case PROP_SIZE:
		priv->size = g_value_get_uint (value);
		break;
	case PROP_DATA:
		priv->data = g_value_get_pointer (value);
		priv->mime_type = g_content_type_guess (priv->name,
								    (guchar *) priv->data,
								    priv->size,
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
	EvAttachmentPrivate *priv = GET_PRIVATE (attachment);

	priv->name = NULL;
	priv->description = NULL;
	priv->data = NULL;
	priv->mime_type = NULL;

	priv->tmp_file = NULL;
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
	EvAttachmentPrivate *priv;

	g_return_val_if_fail (EV_IS_ATTACHMENT (attachment), NULL);

	priv = GET_PRIVATE (attachment);

	return priv->name;
}

const gchar *
ev_attachment_get_description (EvAttachment *attachment)
{
	EvAttachmentPrivate *priv;

	g_return_val_if_fail (EV_IS_ATTACHMENT (attachment), NULL);

	priv = GET_PRIVATE (attachment);

	return priv->description;
}

GTime
ev_attachment_get_modification_date (EvAttachment *attachment)
{
	EvAttachmentPrivate *priv;

	g_return_val_if_fail (EV_IS_ATTACHMENT (attachment), 0);

	priv = GET_PRIVATE (attachment);

	return priv->mtime;
}

GTime
ev_attachment_get_creation_date (EvAttachment *attachment)
{
	EvAttachmentPrivate *priv;

	g_return_val_if_fail (EV_IS_ATTACHMENT (attachment), 0);

	priv = GET_PRIVATE (attachment);

	return priv->ctime;
}

const gchar *
ev_attachment_get_mime_type (EvAttachment *attachment)
{
	EvAttachmentPrivate *priv;

	g_return_val_if_fail (EV_IS_ATTACHMENT (attachment), NULL);

	priv = GET_PRIVATE (attachment);

	return priv->mime_type;
}

gboolean
ev_attachment_save (EvAttachment *attachment,
		    GFile        *file,
		    GError      **error)
{
	GFileOutputStream *output_stream;
	GError *ioerror = NULL;
	gssize  written_bytes;
	EvAttachmentPrivate *priv = GET_PRIVATE (attachment);

	g_return_val_if_fail (EV_IS_ATTACHMENT (attachment), FALSE);
	g_return_val_if_fail (G_IS_FILE (file), FALSE);

	output_stream = g_file_replace (file, NULL, FALSE, 0, NULL, &ioerror);
	if (output_stream == NULL) {
		char *uri;
		
		uri = g_file_get_uri (file);
		g_set_error (error,
			     EV_ATTACHMENT_ERROR, 
			     ioerror->code,
			     _("Couldn’t save attachment “%s”: %s"),
			     uri, 
			     ioerror->message);

		g_error_free (ioerror);
		g_free (uri);
		
		return FALSE;
	}
	
	written_bytes = g_output_stream_write (G_OUTPUT_STREAM (output_stream),
					       priv->data,
					       priv->size,
					       NULL, &ioerror);
	if (written_bytes == -1) {
		char *uri;
		
		uri = g_file_get_uri (file);
		g_set_error (error,
			     EV_ATTACHMENT_ERROR,
			     ioerror->code,
			     _("Couldn’t save attachment “%s”: %s"),
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
	EvAttachmentPrivate *priv = GET_PRIVATE (attachment);

	g_assert (G_IS_FILE (priv->tmp_file));
	g_assert (G_IS_APP_INFO (priv->app));

	files = g_list_prepend (files, priv->tmp_file);

        display = screen ? gdk_screen_get_display (screen) : gdk_display_get_default ();
	context = gdk_display_get_app_launch_context (display);
	gdk_app_launch_context_set_screen (context, screen);
	gdk_app_launch_context_set_timestamp (context, timestamp);

	result = g_app_info_launch (priv->app, files,
				    G_APP_LAUNCH_CONTEXT (context),
                                    &ioerror);
        g_object_unref (context);

	if (!result) {
		g_set_error (error,
			     EV_ATTACHMENT_ERROR,
			     (gint) result,
			     _("Couldn’t open attachment “%s”: %s"),
			     priv->name,
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
	EvAttachmentPrivate *priv;

	g_return_val_if_fail (EV_IS_ATTACHMENT (attachment), FALSE);

	priv = GET_PRIVATE (attachment);

	if (!priv->app) {
		app_info = g_app_info_get_default_for_type (priv->mime_type, FALSE);
		priv->app = app_info;
	}

	if (!priv->app) {
		g_set_error (error,
			     EV_ATTACHMENT_ERROR,
			     0,
			     _("Couldn’t open attachment “%s”"),
			     priv->name);
		
		return FALSE;
	}

	if (priv->tmp_file) {
		retval = ev_attachment_launch_app (attachment, screen,
						   timestamp, error);
	} else {
                char *basename;
                char *temp_dir;
                char *file_path;
		GFile *file;

                /* FIXMEchpe: convert to filename encoding first!
                 * Store the file inside a temporary XXXXXX subdirectory to
                 * keep the filename "as is".
                 */
                basename = g_path_get_basename (ev_attachment_get_name (attachment));
                temp_dir = g_dir_make_tmp ("evince.XXXXXX", error);
                file_path = g_build_filename (temp_dir, basename, NULL);
                file = g_file_new_for_path (file_path);

                g_free (temp_dir);
                g_free (file_path);
                g_free (basename);

		if (file != NULL && ev_attachment_save (attachment, file, error)) {
			if (priv->tmp_file)
				g_object_unref (priv->tmp_file);
			priv->tmp_file = g_object_ref (file);

			retval = ev_attachment_launch_app (attachment, screen,
							   timestamp, error);
		}

		g_object_unref (file);
	}

	return retval;
}

