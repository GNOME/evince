
#include "ev-document-misc.h"
#include <string.h>
#include <gtk/gtk.h>

/* Returns a new GdkPixbuf that is suitable for placing in the thumbnail view.
 * It is four pixels wider and taller than the source.  If source_pixbuf is not
 * NULL, then it will fill the return pixbuf with the contents of
 * source_pixbuf.
 */

GdkPixbuf *
ev_document_misc_get_thumbnail_frame (int        width,
				      int        height,
				      int        rotation,
				      GdkPixbuf *source_pixbuf)
{
	GdkPixbuf *retval;
	guchar *data;
	gint rowstride;
	int i;
	int width_r, height_r;

	rotation = rotation % 360;


	if (source_pixbuf)
		g_return_val_if_fail (GDK_IS_PIXBUF (source_pixbuf), NULL);

	if (source_pixbuf) {
		width_r = gdk_pixbuf_get_width (source_pixbuf);
		height_r = gdk_pixbuf_get_height (source_pixbuf);
	} else {
		if (rotation == 0 || rotation == 180) {
			width_r = width;
			height_r = height;
		} else if (rotation == 90 || rotation == 270) {
			width_r = height;
			height_r = width;
		} else {
			g_assert_not_reached ();
		}
	}

	/* make sure no one is passing us garbage */
	g_assert (width_r >= 0 && height_r >= 0);

	retval = gdk_pixbuf_new (GDK_COLORSPACE_RGB,
				 TRUE, 8,
				 width_r + 4,
				 height_r + 4);

	/* make it black and fill in the middle */
	data = gdk_pixbuf_get_pixels (retval);
	rowstride = gdk_pixbuf_get_rowstride (retval);

	gdk_pixbuf_fill (retval, 0x000000ff);
	for (i = 1; i < height_r + 1; i++)
		memset (data + (rowstride * i) + 4, 0xffffffff, width_r * 4);

	/* copy the source pixbuf */
	if (source_pixbuf)
		gdk_pixbuf_copy_area (source_pixbuf, 0, 0,
				      width_r,
				      height_r,
				      retval,
				      1, 1);
	/* Add the corner */
	data [(width_r + 2) * 4 + 3] = 0;
	data [(width_r + 3) * 4 + 3] = 0;
	data [(width_r + 2) * 4 + (rowstride * 1) + 3] = 0;
	data [(width_r + 3) * 4 + (rowstride * 1) + 3] = 0;

	data [(height_r + 2) * rowstride + 3] = 0;
	data [(height_r + 3) * rowstride + 3] = 0;
	data [(height_r + 2) * rowstride + 4 + 3] = 0;
	data [(height_r + 3) * rowstride + 4 + 3] = 0;

	return retval;
}

void
ev_document_misc_get_page_border_size (gint       page_width,
				       gint       page_height,
				       GtkBorder *border)
{
	g_assert (border);

	border->left = 1;
	border->top = 1;
	if (page_width < 100) {
		border->right = 2;
		border->bottom = 2;
	} else if (page_width < 500) {
		border->right = 3;
		border->bottom = 3;
	} else {
		border->right = 4;
		border->bottom = 4;
	}
}


void
ev_document_misc_paint_one_page (GdkDrawable  *drawable,
				 GtkWidget    *widget,
				 GdkRectangle *area,
				 GtkBorder    *border,
				 gboolean highlight)
{
	gdk_draw_rectangle (drawable,
			    highlight ?
			    	    widget->style->text_gc[widget->state] : widget->style->dark_gc[widget->state],
			    TRUE,
			    area->x,
			    area->y,
			    area->width,
			    area->height);
	gdk_draw_rectangle (drawable,
			    widget->style->white_gc,
			    TRUE,
			    area->x + border->left,
			    area->y + border->top,
			    area->width - (border->left + border->right),
			    area->height - (border->top + border->bottom));
	gdk_draw_rectangle (drawable,
			    widget->style->mid_gc[widget->state],
			    TRUE,
			    area->x,
			    area->y + area->height - (border->bottom - border->top),
			    border->bottom - border->top,
			    border->bottom - border->top);
	gdk_draw_rectangle (drawable,
			    widget->style->mid_gc[widget->state],
			    TRUE,
			    area->x + area->width - (border->right - border->left),
			    area->y,
			    border->right - border->left,
			    border->right - border->left);

}
