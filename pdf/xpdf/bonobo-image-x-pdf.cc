/*
 * image/x-pdf BonoboObject.
 *
 * Author:
 *   Michael Meeks <michael@imaginator.com>
 *
 */
#include <stdio.h>
#include <stdlib.h>
#include <stddef.h>
#include <string.h>
#include <config.h>
extern "C" {
#define GString G_String
#include <gnome.h>
#include <libgnorba/gnorba.h>
#include <bonobo/gnome-bonobo.h>
#undef  GString 
}
#include "gtypes.h"
#include "GString.h"
#include "parseargs.h"
#include "gfile.h"
#include "gmem.h"
#include "Object.h"
#include "Stream.h"
#include "Array.h"
#include "Dict.h"
#include "XRef.h"
#include "Catalog.h"
#include "Page.h"
#include "Link.h"
#include "PDFDoc.h"
#include "GOutputDev.h"
#include "PSOutputDev.h"
#include "TextOutputDev.h"
#include "Params.h"
#include "Error.h"
#include "config.h"

GBool printCommands = gFalse;

CORBA_Environment ev;
CORBA_ORB orb;

/*
 * BonoboObject data
 */
typedef struct {
  GnomeEmbeddable *bonobo_object;

  PDFDoc *pdf;

  GList *views;
} bonobo_object_data_t;

/*
 * View data
 */
typedef struct {
  double                scale;
  bonobo_object_data_t *bonobo_object_data;
  GtkWidget            *drawing_area;
  GtkPixmap            *pixmap;
  GOutputDev           *out;
  GdkColor              paper;
  gint                  w, h;
  gdouble               zoom;
  gint                  page;
} view_data_t;

static void
redraw_view (view_data_t *view_data, GdkRectangle *rect)
{
  GdkPixmap *pixmap;
  GdkBitmap *dummy;
  gint width, height;
  bonobo_object_data_t *bonobo_object_data = view_data->bonobo_object_data;

  gtk_pixmap_get (view_data->pixmap, &pixmap, &dummy);

  g_return_if_fail (pixmap != NULL);

  /*
   * Do not draw outside the region that we know how to display
   */
  if (rect->x > view_data->w)
    return;
  
  if (rect->y > view_data->h)
    return;

  /*
   * Clip the draw region
   */
  if (rect->x + rect->width > view_data->w)
    rect->width = view_data->w - rect->x;
  
  if (rect->y + rect->height > view_data->h)
    rect->height = view_data->h - rect->y;
  
  /*
   * Draw the exposed region.
   */
  gdk_draw_pixmap (view_data->drawing_area->window,
		   view_data->drawing_area->style->white_gc,
		   pixmap,
		   rect->x, rect->y,
		   rect->width,
		   rect->height,
		   rect->x, rect->y);
}

static void
configure_size (view_data_t *view_data, GdkRectangle *rect)
{
/*	ArtPixBuf *pixbuf;
	
	if (view_data->scaled)
		pixbuf = view_data->scaled;
	else
		pixbuf = view_data->bonobo_object_data->image;

	gtk_widget_set_usize (
		view_data->drawing_area,
		pixbuf->width,
		pixbuf->height);

	rect->x = 0;
	rect->y = 0;
	rect->width = pixbuf->width;
	rect->height = pixbuf->height;*/
  g_warning ("Ahhh run away... scaling !");
}

static void
redraw_all (bonobo_object_data_t *bonobo_object_data)
{
	GList *l;
	
	for (l = bonobo_object_data->views; l; l = l->next){
		GdkRectangle rect;
		view_data_t *view_data = (view_data_t *)l->data;

		configure_size (view_data, &rect);
		
		redraw_view (view_data, &rect);
	}
}

static int
save_image (GnomePersistStream *ps, GNOME_Stream stream, void *data)
{
  g_warning ("Unimplemented");
  return -1;
}

/*
 * Loads a PDF from a GNOME_Stream
 */
static int
load_image_from_stream (GnomePersistStream *ps, GNOME_Stream stream, void *data)
{
	bonobo_object_data_t *bonobo_object_data = (bonobo_object_data_t *)data;
	CORBA_Environment ev;
	CORBA_long length;
	GNOME_Stream_iobuf *buffer;
	FILE *hack;
	char *name;

	buffer = GNOME_Stream_iobuf__alloc ();

	length = GNOME_Stream_length (stream, &ev);
	GNOME_Stream_read (stream, length, &buffer, &ev);

	name = tempnam (NULL, "xpdf-hack");
	if (!name)
	  return -1;
	hack = fopen (name, "w+");
	if (!hack)
	  return -1;

	fwrite (buffer->_buffer, 1, buffer->_length, hack);

	CORBA_free (buffer);

	bonobo_object_data->pdf = new PDFDoc (new GString (name));

	redraw_all (bonobo_object_data);
	return 0;
}

static void
destroy_view (GnomeView *view, view_data_t *view_data)
{
	view_data->bonobo_object_data->views = g_list_remove (view_data->bonobo_object_data->views, view_data);
	gtk_object_unref (GTK_OBJECT (view_data->drawing_area));

	g_free (view_data);
}

static int
drawing_area_exposed (GtkWidget *widget, GdkEventExpose *event, view_data_t *view_data)
{
/*	if (!view_data->bonobo_object_data->image)
	return TRUE;*/
	
	redraw_view (view_data, &event->area);

	return TRUE;
}


static GtkPixmap *
setup_pixmap (bonobo_object_data_t *doc, view_data_t *view, GdkWindow *window)
{
  GdkGCValues  gcValues;
  GdkGC       *strokeGC;
  PDFDoc      *pdf = doc->pdf;
  int          w, h;
  GdkPixmap   *pixmap = NULL;

  w = view->w = (int)((pdf->getPageWidth  (view->page) * view->zoom) / 72.0);
  h = view->h = (int)((pdf->getPageHeight (view->page) * view->zoom) / 72.0);

  pixmap = gdk_pixmap_new (window, w, h, -1);

  gdk_color_white (gtk_widget_get_default_colormap(), &view->paper);
  view->out    = new GOutputDev (pixmap, view->paper, window);

  gdk_color_white (gtk_widget_get_default_colormap (), &gcValues.foreground);
  gdk_color_black (gtk_widget_get_default_colormap (), &gcValues.background);
  gcValues.line_width = 1;
  gcValues.line_style = GDK_LINE_SOLID;
  strokeGC = gdk_gc_new_with_values (
    pixmap, &gcValues, 
    (enum GdkGCValuesMask)(GDK_GC_FOREGROUND | GDK_GC_BACKGROUND | GDK_GC_LINE_WIDTH | GDK_GC_LINE_STYLE));
  
  gdk_draw_rectangle (pixmap, strokeGC,
		      TRUE, 0, 0,
		      w, h);

  return GTK_PIXMAP (gtk_pixmap_new (pixmap, NULL));
}

static GnomeView *
view_factory (GnomeEmbeddable *bonobo_object,
	      const GNOME_ViewFrame view_frame,
	      void *data)
{
        GnomeView *view;
	bonobo_object_data_t *bonobo_object_data = (bonobo_object_data_t *)data;
	view_data_t *view_data = g_new (view_data_t, 1);

	view_data->scale = 1.0;
	view_data->bonobo_object_data = bonobo_object_data;
	view_data->drawing_area = gtk_drawing_area_new ();
	view_data->page = 1;
	view_data->zoom = 86.0;

	GdkWindow *win = gtk_widget_get_parent_window (GTK_WIDGET (view_data->drawing_area));
	view_data->pixmap = setup_pixmap (bonobo_object_data, view_data, win);
					  

	gtk_signal_connect (
		GTK_OBJECT (view_data->drawing_area),
		"expose_event",
		GTK_SIGNAL_FUNC (drawing_area_exposed), view_data);

        gtk_widget_show (view_data->drawing_area);
        view = gnome_view_new (view_data->drawing_area);

	gtk_signal_connect (
		GTK_OBJECT (view), "destroy",
		GTK_SIGNAL_FUNC (destroy_view), view_data);

	bonobo_object_data->views = g_list_prepend (bonobo_object_data->views,
						view_data);

        return view;
}

static GnomeObject *
bonobo_object_factory (GnomeEmbeddableFactory *This, void *data)
{
	GnomeEmbeddable *bonobo_object;
	GnomePersistStream *stream;
	bonobo_object_data_t *bonobo_object_data = (bonobo_object_data_t *)data;

	bonobo_object_data = g_new0 (bonobo_object_data_t, 1);
	if (!bonobo_object_data)
		return NULL;

	/*
	 * Creates the BonoboObject server
	 */
	bonobo_object = gnome_embeddable_new (view_factory, bonobo_object_data);
	if (bonobo_object == NULL){
		g_free (bonobo_object_data);
		return NULL;
	}

	/* Does the pdf init stuff */
	initParams (xpdfConfigFile); /* Init font path */
	bonobo_object_data->pdf = NULL;

	/*
	 * Interface GNOME::PersistStream 
	 */
	stream = gnome_persist_stream_new ("bonobo-object:image-x-pdf",
					   load_image_from_stream,
					   save_image,
					   bonobo_object_data);
	if (stream == NULL){
		gtk_object_unref (GTK_OBJECT (bonobo_object));
		g_free (bonobo_object_data);
		return NULL;
	}

	bonobo_object_data->bonobo_object = bonobo_object;

	/*
	 * Bind the interfaces
	 */
	gnome_object_add_interface (GNOME_OBJECT (bonobo_object),
				    GNOME_OBJECT (stream));
	return (GnomeObject *) bonobo_object;
}

static void
init_bonobo_image_x_png_factory (void)
{
	GnomeEmbeddableFactory *factory;
	
	factory = gnome_embeddable_factory_new (
		"bonobo-object-factory:image-x-pdf",
		bonobo_object_factory, NULL);
}

static void
init_server_factory (int argc, char **argv)
{
	gnome_CORBA_init_with_popt_table (
		"bonobo-image-x-pdf", "1.0",
		&argc, argv, NULL, 0, NULL, GNORBA_INIT_SERVER_FUNC, &ev);

	if (bonobo_init (CORBA_OBJECT_NIL, CORBA_OBJECT_NIL, CORBA_OBJECT_NIL) == FALSE)
		g_error (_("I could not initialize Bonobo"));
}

int
main (int argc, char *argv [])
{
	CORBA_exception_init (&ev);

	init_server_factory (argc, argv);
	init_bonobo_image_x_png_factory ();

	gtk_widget_set_default_colormap (gdk_rgb_get_cmap ());
	gtk_widget_set_default_visual (gdk_rgb_get_visual ());
	gtk_main ();
	
	CORBA_exception_free (&ev);

	return 0;
}
      
