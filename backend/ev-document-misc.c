
#include "ev-document-misc.h"
#include <string.h>

/* Returns a new GdkPixbuf that is suitable for placing in the thumbnail view.
 * It is four pixels wider and taller than the source.  If source_pixbuf is not
 * NULL, then it will fill the return pixbuf with the contents of
 * source_pixbuf.
 */

GdkPixbuf *
ev_document_misc_get_thumbnail_frame (int        width,
				      int        height,
				      GdkPixbuf *source_pixbuf)
{
	GdkPixbuf *retval;
	guchar *data;
	gint rowstride;
	int i;

	if (source_pixbuf)
		g_return_val_if_fail (GDK_IS_PIXBUF (source_pixbuf), NULL);

	if (source_pixbuf) {
		width = gdk_pixbuf_get_width (source_pixbuf);
		height = gdk_pixbuf_get_height (source_pixbuf);
	}

	/* make sure no one is passing us garbage */
	g_assert (width > 0 && height > 0);

	retval = gdk_pixbuf_new (GDK_COLORSPACE_RGB,
				 TRUE, 8,
				 width + 4,
				 height + 4);

	/* make it black and fill in the middle */
	data = gdk_pixbuf_get_pixels (retval);
	rowstride = gdk_pixbuf_get_rowstride (retval);

	gdk_pixbuf_fill (retval, 0x000000ff);
	for (i = 1; i < height + 1; i++)
		memset (data + (rowstride * i) + 4, 0xffffffff, width * 4);

	/* copy the source pixbuf */
	if (source_pixbuf)
		gdk_pixbuf_copy_area (source_pixbuf, 0, 0,
				      width,
				      height,
				      retval,
				      1, 1);
	/* Add the corner */
	data [(width + 2) * 4 + 3] = 0;
	data [(width + 3) * 4 + 3] = 0;
	data [(width + 2) * 4 + (rowstride * 1) + 3] = 0;
	data [(width + 3) * 4 + (rowstride * 1) + 3] = 0;

	data [(height + 2) * rowstride + 3] = 0;
	data [(height + 3) * rowstride + 3] = 0;
	data [(height + 2) * rowstride + 4 + 3] = 0;
	data [(height + 3) * rowstride + 4 + 3] = 0;

	return retval;
}
