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

#include <glib/gstdio.h>
#include <unistd.h>

#include "ev-document-misc.h"
#include "ev-file-helpers.h"
#include "ev-image.h"

struct _EvImagePrivate {
	gint       page;
	gint       id;
	GdkPixbuf *pixbuf;
	gchar     *tmp_uri;
};

#define EV_IMAGE_GET_PRIVATE(object) \
                (G_TYPE_INSTANCE_GET_PRIVATE ((object), EV_TYPE_IMAGE, EvImagePrivate))

G_DEFINE_TYPE (EvImage, ev_image, G_TYPE_OBJECT)

static void
ev_image_finalize (GObject *object)
{
	EvImage *image = EV_IMAGE (object);

	if (image->priv->pixbuf) {
		g_object_unref (image->priv->pixbuf);
		image->priv->pixbuf = NULL;
	}

	if (image->priv->tmp_uri) {
		gchar *filename;

		filename = g_filename_from_uri (image->priv->tmp_uri, NULL, NULL);
		ev_tmp_filename_unlink (filename);
		g_free (filename);
		g_free (image->priv->tmp_uri);
		image->priv->tmp_uri = NULL;
	}

	(* G_OBJECT_CLASS (ev_image_parent_class)->finalize) (object);
}

static void
ev_image_class_init (EvImageClass *klass)
{
	GObjectClass *g_object_class;

	g_object_class = G_OBJECT_CLASS (klass);

	g_type_class_add_private (g_object_class, sizeof (EvImagePrivate));

	g_object_class->finalize = ev_image_finalize;
}

static void
ev_image_init (EvImage *image)
{
	image->priv = EV_IMAGE_GET_PRIVATE (image);
}

EvImage *
ev_image_new (gint page,
	      gint img_id)
{
	EvImage *image;

	image = EV_IMAGE (g_object_new (EV_TYPE_IMAGE, NULL));
	image->priv->page = page;
	image->priv->id = img_id;

	return image;
}

EvImage *
ev_image_new_from_pixbuf (GdkPixbuf *pixbuf)
{
	EvImage *image;

	g_return_val_if_fail (GDK_IS_PIXBUF (pixbuf), NULL);

	image = EV_IMAGE (g_object_new (EV_TYPE_IMAGE, NULL));
	image->priv->pixbuf = g_object_ref (pixbuf);

	return image;
}

gint
ev_image_get_page (EvImage *image)
{
	g_return_val_if_fail (EV_IS_IMAGE (image), -1);

	return image->priv->page;
}

gint
ev_image_get_id (EvImage *image)
{
	g_return_val_if_fail (EV_IS_IMAGE (image), -1);

	return image->priv->id;
}

/**
 * ev_image_get_pixbuf:
 * @image: an #EvImage
 *
 * Returns: (transfer none): a #GdkPixbuf
 */
GdkPixbuf *
ev_image_get_pixbuf (EvImage *image)
{
	g_return_val_if_fail (EV_IS_IMAGE (image), NULL);
	g_return_val_if_fail (GDK_IS_PIXBUF (image->priv->pixbuf), NULL);

	return image->priv->pixbuf;
}

const gchar *
ev_image_save_tmp (EvImage   *image,
		   GdkPixbuf *pixbuf)
{
	GError *error = NULL;
	gchar  *filename = NULL;
        int fd;
	
	g_return_val_if_fail (EV_IS_IMAGE (image), NULL);
	g_return_val_if_fail (GDK_IS_PIXBUF (pixbuf), NULL);

	if (image->priv->tmp_uri)
		return image->priv->tmp_uri;

	if ((fd = ev_mkstemp ("image.XXXXXX.png", &filename, &error)) == -1)
		goto had_error;

	gdk_pixbuf_save (pixbuf, filename,
			 "png", &error,
			 "compression", "3", NULL);
        close (fd);

	if (!error) {
		image->priv->tmp_uri = g_filename_to_uri (filename, NULL, &error);
                if (image->priv->tmp_uri == NULL)
                        goto had_error;

		g_free (filename);
		
		return image->priv->tmp_uri;
	}

    had_error:

	/* Erro saving image */
	g_warning ("Error saving image: %s", error->message);
	g_error_free (error);
	g_free (filename);

	return NULL;
}

const gchar *
ev_image_get_tmp_uri (EvImage *image)
{
	g_return_val_if_fail (EV_IS_IMAGE (image), NULL);

	return image->priv->tmp_uri;
}
