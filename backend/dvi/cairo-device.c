/*
 * Copyright (C) 2007 Carlos Garcia Campos <carlosgc@gnome.org>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2, or (at your option)
 * any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301, USA.
 */

#include <config.h>

#include <stdlib.h>
#include <gdk/gdk.h>
#ifdef HAVE_SPECTRE
#include <libspectre/spectre.h>
#endif

#include "cairo-device.h"

typedef struct {
	cairo_t *cr;

	gint xmargin;
	gint ymargin;

	gdouble scale;
	
	Ulong fg;
	Ulong bg;

} DviCairoDevice;

static void
dvi_cairo_draw_glyph (DviContext  *dvi,
		      DviFontChar *ch,
		      int          x0,
		      int          y0)
{
	DviCairoDevice  *cairo_device;
	int              x, y, w, h;
	gboolean         isbox;
	DviGlyph        *glyph;
	cairo_surface_t *surface;

	cairo_device = (DviCairoDevice *) dvi->device.device_data;

	glyph = &ch->grey;

	isbox = (glyph->data == NULL ||
	         (dvi->params.flags & MDVI_PARAM_CHARBOXES) ||
	         MDVI_GLYPH_ISEMPTY (glyph->data));

	x = - glyph->x + x0 + cairo_device->xmargin;
	y = - glyph->y + y0 + cairo_device->ymargin;
	w = glyph->w;
	h = glyph->h;

	surface = cairo_get_target (cairo_device->cr);
	if (x < 0 || y < 0
	    || x + w > cairo_image_surface_get_width (surface)
	    || y + h > cairo_image_surface_get_height (surface))
		return;

	cairo_save (cairo_device->cr);
	if (isbox) {
		cairo_rectangle (cairo_device->cr,
				 x - cairo_device->xmargin,
				 y - cairo_device->ymargin,
				 w, h);
		cairo_stroke (cairo_device->cr);
	} else {
		cairo_translate (cairo_device->cr, x, y);
		cairo_set_source_surface (cairo_device->cr,
					  (cairo_surface_t *) glyph->data,
					  0, 0);
		cairo_paint (cairo_device->cr);
	}

	cairo_restore (cairo_device->cr);
}

static void
dvi_cairo_draw_rule (DviContext *dvi,
		     int         x,
		     int         y,
		     Uint        width,
		     Uint        height,
		     int         fill)
{
	DviCairoDevice *cairo_device;
	Ulong           color;

	cairo_device = (DviCairoDevice *) dvi->device.device_data;

	color = cairo_device->fg;
	
	cairo_save (cairo_device->cr);

	cairo_set_line_width (cairo_device->cr,
			      cairo_get_line_width (cairo_device->cr) * cairo_device->scale);
	cairo_set_source_rgb (cairo_device->cr,
			      ((color >> 16) & 0xff) / 255.,
			      ((color >> 8) & 0xff) / 255.,
			      ((color >> 0) & 0xff) / 255.);

	cairo_rectangle (cairo_device->cr,
			 x + cairo_device->xmargin,
			 y + cairo_device->ymargin,
			 width, height);
	if (fill == 0) {
		cairo_stroke (cairo_device->cr);
	} else {
		cairo_fill (cairo_device->cr);
	}

	cairo_restore (cairo_device->cr);
}

#ifdef HAVE_SPECTRE
static void
dvi_cairo_draw_ps (DviContext *dvi,
		   const char *filename,
		   int         x,
		   int         y,
		   Uint        width,
		   Uint        height)
{
	DviCairoDevice       *cairo_device;
	unsigned char        *data = NULL;
	int                   row_length;
	SpectreDocument      *psdoc;
	SpectreRenderContext *rc;
	int                   w, h;
	SpectreStatus         status;
	cairo_surface_t      *image;

	cairo_device = (DviCairoDevice *) dvi->device.device_data;

	psdoc = spectre_document_new ();
	spectre_document_load (psdoc, filename);
	if (spectre_document_status (psdoc)) {
		spectre_document_free (psdoc);
		return;
	}

	spectre_document_get_page_size (psdoc, &w, &h);

	rc = spectre_render_context_new ();
	spectre_render_context_set_scale (rc,
					  (double)width / w,
					  (double)height / h);
	spectre_document_render_full (psdoc, rc, &data, &row_length);	
	status = spectre_document_status (psdoc);

	spectre_render_context_free (rc);
	spectre_document_free (psdoc);

	if (status) {
		g_warning ("Error rendering PS document %s: %s\n",
			   filename, spectre_status_to_string (status));
		free (data);
		
		return;
	}

	image = cairo_image_surface_create_for_data ((unsigned char *)data,
						     CAIRO_FORMAT_RGB24,
						     width, height,
						     row_length);

	cairo_save (cairo_device->cr);

	cairo_translate (cairo_device->cr,
			 x + cairo_device->xmargin,
			 y + cairo_device->ymargin);
	cairo_set_source_surface (cairo_device->cr, image, 0, 0); 
	cairo_paint (cairo_device->cr);

	cairo_restore (cairo_device->cr);

	cairo_surface_destroy (image);
	free (data);
}
#endif /* HAVE_SPECTRE */

static int
dvi_cairo_alloc_colors (void  *device_data,
			Ulong *pixels,
			int    npixels,
			Ulong  fg,
			Ulong  bg,
			double gamma,
			int    density)
{
	double  frac;
	GdkColor color, color_fg, color_bg;
	int     i, n;

	color_bg.red = (bg >> 16) & 0xff;
	color_bg.green = (bg >> 8) & 0xff;
	color_bg.blue = (bg >> 0) & 0xff;

	color_fg.red = (fg >> 16) & 0xff;
	color_fg.green = (fg >> 8) & 0xff;
	color_fg.blue = (fg >> 0) & 0xff;

	n = npixels - 1;
	for (i = 0; i < npixels; i++) {
		frac = (gamma > 0) ?
			pow ((double)i / n, 1 / gamma) :
			1 - pow ((double)(n - i) / n, -gamma);
		
		color.red = frac * ((double)color_fg.red - color_bg.red) + color_bg.red;
		color.green = frac * ((double)color_fg.green - color_bg.green) + color_bg.green;
		color.blue = frac * ((double)color_fg.blue - color_bg.blue) + color_bg.blue;
		
		pixels[i] = (color.red << 16) + (color.green << 8) + color.blue + 0xff000000;
	}

	return npixels;
}

static void *
dvi_cairo_create_image (void *device_data,
			Uint  width,
			Uint  height,
			Uint  bpp)
{
	return cairo_image_surface_create (CAIRO_FORMAT_RGB24, width, height);
}

static void
dvi_cairo_free_image (void *ptr)
{
	cairo_surface_destroy ((cairo_surface_t *)ptr);
}

static void
dvi_cairo_put_pixel (void *image, int x, int y, Ulong color)
{
	cairo_surface_t *surface;
	gint             rowstride;
	guint32         *p;

	surface = (cairo_surface_t *) image;

	rowstride = cairo_image_surface_get_stride (surface);
	p = (guint32*) (cairo_image_surface_get_data (surface) + y * rowstride + x * 4);

	*p = color;
}

static void
dvi_cairo_set_color (void *device_data, Ulong fg, Ulong bg)
{
	DviCairoDevice *cairo_device = (DviCairoDevice *) device_data;

	cairo_device->fg = fg;
	cairo_device->bg = bg;
}

/* Public methods */
void
mdvi_cairo_device_init (DviDevice *device)
{
	device->device_data = g_new0 (DviCairoDevice, 1);

	device->draw_glyph = dvi_cairo_draw_glyph;
	device->draw_rule = dvi_cairo_draw_rule;
	device->alloc_colors = dvi_cairo_alloc_colors;
	device->create_image = dvi_cairo_create_image;
	device->free_image = dvi_cairo_free_image;
	device->put_pixel = dvi_cairo_put_pixel;
	device->set_color = dvi_cairo_set_color;
#ifdef HAVE_SPECTRE
	device->draw_ps = dvi_cairo_draw_ps;
#else
	device->draw_ps = NULL;
#endif
	device->refresh = NULL;
}

void
mdvi_cairo_device_free (DviDevice *device)
{
	DviCairoDevice *cairo_device;

	cairo_device = (DviCairoDevice *) device->device_data;

	if (cairo_device->cr)
		cairo_destroy (cairo_device->cr);

	g_free (cairo_device);
}

cairo_surface_t *
mdvi_cairo_device_get_surface (DviDevice *device)
{
	DviCairoDevice *cairo_device;

	cairo_device = (DviCairoDevice *) device->device_data;

	return cairo_surface_reference (cairo_get_target (cairo_device->cr));
}

void
mdvi_cairo_device_render (DviContext* dvi)
{
	DviCairoDevice  *cairo_device;
	gint             page_width;
	gint             page_height;
	cairo_surface_t *surface;
	guchar          *pixels;
	gint             rowstride;
	static const cairo_user_data_key_t key;

	cairo_device = (DviCairoDevice *) dvi->device.device_data;

	if (cairo_device->cr)
		cairo_destroy (cairo_device->cr);

	page_width = dvi->dvi_page_w * dvi->params.conv + 2 * cairo_device->xmargin;
	page_height = dvi->dvi_page_h * dvi->params.vconv + 2 * cairo_device->ymargin;

	rowstride = page_width * 4;
	pixels = (guchar *) g_malloc (page_height * rowstride);
	memset (pixels, 0xff, page_height * rowstride);

	surface = cairo_image_surface_create_for_data (pixels,
						       CAIRO_FORMAT_RGB24,
						       page_width, page_height,
						       rowstride);
	cairo_surface_set_user_data (surface, &key,
				     pixels, (cairo_destroy_func_t)g_free);

	cairo_device->cr = cairo_create (surface);
	cairo_surface_destroy (surface);

	mdvi_dopage (dvi, dvi->currpage);
}

void
mdvi_cairo_device_set_margins (DviDevice *device,
			       gint       xmargin,
			       gint       ymargin)
{
	DviCairoDevice *cairo_device;

	cairo_device = (DviCairoDevice *) device->device_data;

	cairo_device->xmargin = xmargin;
	cairo_device->ymargin = ymargin;
}

void
mdvi_cairo_device_set_scale (DviDevice *device,
			     gdouble    scale)
{
	DviCairoDevice *cairo_device;

	cairo_device = (DviCairoDevice *) device->device_data;

	cairo_device->scale = scale;
}
