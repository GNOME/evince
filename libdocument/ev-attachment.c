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
#include "ev-file-helpers.h"
#include "ev-attachment.h"

enum
{
	PROP_0,
	PROP_NAME,
	PROP_DESCRIPTION,
	PROP_MDATETIME,
	PROP_CDATETIME,
	PROP_SIZE,
	PROP_DATA
};

typedef struct {
	gchar                   *name;
	gchar                   *description;
	GDateTime               *mdatetime;
	GDateTime               *cdatetime;
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

	g_clear_pointer (&priv->name, g_free);
	g_clear_pointer (&priv->description, g_free);
	g_clear_pointer (&priv->data, g_free);
	g_clear_pointer (&priv->mime_type, g_free);
	g_clear_object (&priv->app);

	g_clear_pointer (&priv->mdatetime, g_date_time_unref);
	g_clear_pointer (&priv->cdatetime, g_date_time_unref);

	if (priv->tmp_file) {
		ev_tmp_file_unlink (priv->tmp_file);
		g_clear_object (&priv->tmp_file);
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
	case PROP_MDATETIME:
		priv->mdatetime = g_value_get_boxed (value);
		if (priv->mdatetime)
			g_date_time_ref (priv->mdatetime);
		break;
	case PROP_CDATETIME:
		priv->cdatetime = g_value_get_boxed (value);
		if (priv->cdatetime)
			g_date_time_ref (priv->cdatetime);
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
					 PROP_MDATETIME,
					 g_param_spec_boxed ("mdatetime",
							     "ModifiedTime",
							     "The attachment modification date",
							     G_TYPE_DATE_TIME,
							     G_PARAM_WRITABLE |
							     G_PARAM_CONSTRUCT_ONLY |
                                                             G_PARAM_STATIC_STRINGS));
	g_object_class_install_property (g_object_class,
					 PROP_CDATETIME,
					 g_param_spec_boxed ("cdatetime",
							     "CreationTime",
							     "The attachment creation date",
							     G_TYPE_DATE_TIME,
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
}

EvAttachment *
ev_attachment_new (const gchar *name,
		   const gchar *description,
		   GDateTime   *mdatetime,
		   GDateTime   *cdatetime,
		   gsize        size,
		   gpointer     data)
{
	return (EvAttachment *)g_object_new (EV_TYPE_ATTACHMENT,
					    "name", name,
					    "description", description,
					    "mdatetime", mdatetime,
					    "cdatetime", cdatetime,
					    "size", size,
					    "data", data,
					    NULL);
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

GDateTime*
ev_attachment_get_modification_datetime (EvAttachment *attachment)
{
	EvAttachmentPrivate *priv;

	g_return_val_if_fail (EV_IS_ATTACHMENT (attachment), 0);

	priv = GET_PRIVATE (attachment);

	return priv->mdatetime;
}

GDateTime*
ev_attachment_get_creation_datetime (EvAttachment *attachment)
{
	EvAttachmentPrivate *priv;

	g_return_val_if_fail (EV_IS_ATTACHMENT (attachment), 0);

	priv = GET_PRIVATE (attachment);

	return priv->cdatetime;
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
ev_attachment_launch_app (EvAttachment		 *attachment,
			  GAppLaunchContext	 *context,
			  GError		**error)
{
	gboolean             result;
	GList               *files = NULL;
	GError              *ioerror = NULL;
	EvAttachmentPrivate *priv = GET_PRIVATE (attachment);

	g_assert (G_IS_FILE (priv->tmp_file));
	g_assert (G_IS_APP_INFO (priv->app));

	files = g_list_prepend (files, priv->tmp_file);

	result = g_app_info_launch (priv->app, files, context, &ioerror);

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
ev_attachment_open (EvAttachment	 *attachment,
		    GAppLaunchContext	 *context,
		    GError		**error)
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
		retval = ev_attachment_launch_app (attachment, context, error);
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

			retval = ev_attachment_launch_app (attachment, context, error);
		}

		g_object_unref (file);
	}

	return retval;
}
