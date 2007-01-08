#ifndef MDVI_PIXBUF_DEVICE
#define MDVI_PIXBUF_DEVICE

#include "mdvi.h"
#include <gtk/gtk.h>

void 
mdvi_pixbuf_device_init (DviDevice *device);

void 
mdvi_pixbuf_device_free (DviDevice *device);

GdkPixbuf * 
mdvi_pixbuf_device_get_pixbuf (DviDevice *device);

void 
mdvi_pixbuf_device_render (DviContext *dvi);

void 
mdvi_pixbuf_device_set_margins (DviDevice *device, gint xmargin, gint ymargin);

#endif /* MDVI_PIXBUF_DEVICE */


