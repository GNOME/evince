#include "pixbuf-device.h"
#include <gtk/gtk.h>

typedef struct _DviPixbufDevice
{
    GdkPixbuf *pixbuf;
    
    gboolean valid;
    
    gint xmargin;
    gint ymargin;
    
    Ulong fg;
    Ulong bg;
    
} DviPixbufDevice;

static void dvi_pixbuf_draw_rule(DviContext *dvi, int x, int y, Uint w, Uint h, int fill);

static void dvi_pixbuf_draw_glyph(DviContext *dvi, DviFontChar *ch, int x0, int y0)
{
	DviPixbufDevice *c_device = (DviPixbufDevice *) dvi->device.device_data;
	
	int	x, y, w, h;	
	int	isbox;
	DviGlyph *glyph;
	
	glyph = &ch->grey;

	isbox = (glyph->data == NULL || (dvi->params.flags & MDVI_PARAM_CHARBOXES));
	
	x = - glyph->x + x0 + c_device->xmargin;
	y = - glyph->y + y0 + c_device->ymargin;
	w = glyph->w;
	h = glyph->h;
	
	if (x < 0 || y < 0 
	    || x + w > gdk_pixbuf_get_width (c_device->pixbuf)
	    || y + h > gdk_pixbuf_get_height (c_device->pixbuf))
	    return;
		
	if (isbox) {
		dvi_pixbuf_draw_rule(dvi, x - c_device->xmargin, y - c_device->ymargin, w, h, FALSE);    
	}
	else {
		gdk_pixbuf_copy_area (GDK_PIXBUF (glyph->data),
				      0, 0, 
				      w, h,
				      c_device->pixbuf, x, y);
	}
}

static void dvi_pixbuf_draw_rule(DviContext *dvi, int x, int y, Uint w, Uint h, int fill)
{
	DviPixbufDevice *c_device = (DviPixbufDevice *) dvi->device.device_data;
	gint rowstride;
	guchar *p;
	gint i, j;    
	gint red, green, blue;
	
	red = (c_device->fg >> 16) & 0xff;
	green = (c_device->fg >> 8) & 0xff;
	blue = c_device->fg & 0xff;
	
	x += c_device->xmargin; y += c_device->ymargin;
	
	if (x < 0 || y < 0 
	    || x + w > gdk_pixbuf_get_width (c_device->pixbuf)
	    || y + h > gdk_pixbuf_get_height (c_device->pixbuf))
	    return;
	
	rowstride = gdk_pixbuf_get_rowstride (c_device->pixbuf);
	p = gdk_pixbuf_get_pixels (c_device->pixbuf) + rowstride * y + 4 * x;

	for (i = 0; i < h; i++) {
	    if (i == 0 || i == h - 1 || fill) {
		  for (j = 0; j < w; j++) {
			p[j * 4] = red;
		        p[j * 4 + 1] = green;
			p[j * 4 + 2] = blue;
			p[j * 4 + 3] = 0xff;
		  }
    	    } else {
		p[0] = red;
		p[1] = green;
		p[2] = blue;
		p[3] = 0xff;
		p[(w - 1) * 4] = red;
		p[(w - 1) * 4 + 1] = green;
		p[(w - 1) * 4 + 2] = blue;
		p[(w - 1) * 4 + 3] = 0xff;
    	    }
	    p += rowstride;
      }
}

static int dvi_pixbuf_interpolate_colors(void *device_data,
	Ulong *pixels, int nlevels, Ulong fg, Ulong bg, double g, int density)
{
	double	frac;
	GdkColor color, color_fg, color_bg;
	int	i, n;
	
	color_bg.red = (bg >> 16) & 0xff;
	color_bg.green = (bg >> 8) & 0xff;
	color_bg.blue = bg & 0xff;

	color_fg.red = fg >> 16 & 0xff;
	color_fg.green = fg >> 8 & 0xff;
	color_fg.blue = fg & 0xff;

	n = nlevels - 1;
	for(i = 0; i < nlevels; i++) {
		if(g > 0)
			frac = pow((double)i / n, 1 / g);
		else
			frac = 1 - pow((double)(n - i) / n, -g);
		color.red = frac * ((double)color_fg.red - color_bg.red) + color_bg.red;
		color.green = frac * ((double)color_fg.green - color_bg.green) + color_bg.green;
		color.blue = frac * ((double)color_fg.blue - color_bg.blue) + color_bg.blue;
		
		pixels[i] = (color.red << 16) + (color.green << 8) + color.blue + 0xff000000;
	}

	return nlevels;
}

static void *dvi_pixbuf_create_image(void *device_data, Uint w, Uint h, Uint bpp)
{

    return gdk_pixbuf_new (GDK_COLORSPACE_RGB, TRUE, 8, w, h);
    
    return NULL;
}

static void dvi_pixbuf_free_image(void *ptr)
{
    g_object_unref (GDK_PIXBUF(ptr)); 
}

static void dvi_pixbuf_put_pixel(void *image, int x, int y, Ulong color)
{
    guchar *p;
    
    p = gdk_pixbuf_get_pixels (GDK_PIXBUF(image)) + y * gdk_pixbuf_get_rowstride(GDK_PIXBUF(image)) + x * 4;

    p[0] = (color >> 16) & 0xff;
    p[1] = (color >> 8) & 0xff;
    p[2] = color & 0xff;
    p[3] = (color >> 24) & 0xff;
}

static void dvi_pixbuf_set_color(void *device_data, Ulong fg, Ulong bg)
{
    DviPixbufDevice *c_device = (DviPixbufDevice *) device_data;
    
    c_device->fg = fg;
        
    return; 
}

void mdvi_pixbuf_device_init (DviDevice *device)
{
    device->device_data = 
	 g_new0 (DviPixbufDevice, 1);
	 
    device->draw_glyph   = dvi_pixbuf_draw_glyph;
    device->draw_rule    = dvi_pixbuf_draw_rule;
    device->alloc_colors = dvi_pixbuf_interpolate_colors;
    device->create_image = dvi_pixbuf_create_image;
    device->free_image   = dvi_pixbuf_free_image;
    device->put_pixel    = dvi_pixbuf_put_pixel;
    device->set_color    = dvi_pixbuf_set_color;
    device->refresh      = NULL; 
    
    return;
}

void mdvi_pixbuf_device_free (DviDevice *device)
{
    DviPixbufDevice *c_device = (DviPixbufDevice *) device->device_data;
    
    if (c_device->pixbuf)
	g_object_unref (c_device->pixbuf);
    
    g_free (c_device);
}

GdkPixbuf * 
mdvi_pixbuf_device_get_pixbuf (DviDevice *device)
{
    DviPixbufDevice *c_device = (DviPixbufDevice *) device->device_data;
    
    return g_object_ref (c_device->pixbuf);
}

void
mdvi_pixbuf_device_render (DviContext * dvi)
{
  DviPixbufDevice *c_device = (DviPixbufDevice *) dvi->device.device_data;
  gint page_width;
  gint page_height;

  if (c_device->pixbuf)
    g_object_unref (c_device->pixbuf);
    
  page_width = dvi->dvi_page_w * dvi->params.conv + 2 * c_device->xmargin;
  page_height = dvi->dvi_page_h * dvi->params.vconv + 2 * c_device->ymargin;
    
  c_device->pixbuf = gdk_pixbuf_new (GDK_COLORSPACE_RGB, TRUE, 8, page_width, page_height);
  gdk_pixbuf_fill (c_device->pixbuf, 0xffffffff);

  mdvi_dopage (dvi, dvi->currpage);
}


void 
mdvi_pixbuf_device_set_margins (DviDevice *device, gint xmargin, gint ymargin)
{
  DviPixbufDevice *c_device = (DviPixbufDevice *) device->device_data;
    
  c_device->xmargin = xmargin;
  c_device->ymargin = ymargin;
}
