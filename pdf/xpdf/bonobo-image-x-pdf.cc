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
#include "BonoboStream.h"

GBool printCommands = gFalse;

CORBA_Environment ev;
CORBA_ORB orb;

/*
 * BonoboObject data
 */
typedef struct {
  GnomeEmbeddable *embed_obj;

  PDFDoc       *pdf;
  GNOME_Stream  stream; /* To free it later */

  GList *views;
} bed_t;

/*
 * View data
 */
typedef struct {
  GnomeView            *view;
  bed_t *bed;

  double                scale;
  GtkWidget            *drawing_area;
  GdkPixmap            *pixmap;
  GOutputDev           *out;
  GdkColor              paper;
  gint                  w, h;
  gdouble               zoom;
  gint                  page;
} view_data_t;

static void realize_drawing_areas (bed_t *bed);

static void
redraw_view (view_data_t *view_data, GdkRectangle *rect)
{
  gint width, height;
  bed_t *bed = view_data->bed;

  g_return_if_fail (view_data->pixmap != NULL);

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
		   view_data->pixmap,
		   rect->x, rect->y,
		   rect->x, rect->y,
		   rect->width,
		   rect->height);
}

static void
configure_size (view_data_t *view_data, GdkRectangle *rect)
{
/*	ArtPixBuf *pixbuf;
	
	if (view_data->scaled)
		pixbuf = view_data->scaled;
	else
		pixbuf = view_data->bed->image;
*/
  gtk_widget_set_usize (
    view_data->drawing_area,
    view_data->w,
    view_data->h);
  
  rect->x = 0;
  rect->y = 0;
  rect->width = view_data->w;
  rect->height = view_data->h;
}

static void
redraw_all (bed_t *bed)
{
	GList *l;
	
	for (l = bed->views; l; l = l->next){
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

static void
setup_size (bed_t *doc, view_data_t *view)
{
  if (!doc || !view || !doc->pdf) {
    view->w = 320;
    view->h = 200;
    return;
  }
  view->w = (int)((doc->pdf->getPageWidth  (view->page) * view->zoom) / 72.0);
  view->h = (int)((doc->pdf->getPageHeight (view->page) * view->zoom) / 72.0);
}

/*
 * Loads a PDF from a GNOME_Stream
 */
static int
load_image_from_stream (GnomePersistStream *ps, GNOME_Stream stream, void *data)
{
	bed_t *bed = (bed_t *)data;
	CORBA_long length;
	GNOME_Stream_iobuf *buffer;
	guint lp;
	#define CHUNK 512
	FILE *hack;
	char *name;

	if (bed->pdf ||
	    bed->stream) {
	  g_warning ("Won't overwrite pre-existing stream: you wierdo");
	  return 0;
	}

	/* We need this for later */
	CORBA_Object_duplicate (stream, &ev);
	g_return_val_if_fail (ev._major == CORBA_NO_EXCEPTION, 0);

/*	buffer = GNOME_Stream_iobuf__alloc ();
	length = GNOME_Stream_length (stream, &ev);

	name = tempnam (NULL, "xpdf-hack");
	if (!name)
	  return -1;
	hack = fopen (name, "wb+");
	if (!hack)
	  return -1;

	while (length > 0) {
	  guint getlen;
	  if (length > 128)
	    getlen = 128;
	  else
	    getlen = length;
	  GNOME_Stream_read (stream, getlen, &buffer, &ev);
	  fwrite (buffer->_buffer, 1, buffer->_length, hack);
	  length -= buffer->_length;
	}

	fclose (hack);

	CORBA_free (buffer);*/

	printf ("Loading PDF from persiststream\n");
	bed->stream = stream;
	BonoboStream *bs = new BonoboStream (stream);
	GString *st = new GString ("Bonobo.pdf");
	bed->pdf = new PDFDoc (bs, st);
					      
	printf ("Done load\n");
	if (!(bed->pdf->isOk())) {
	  g_warning ("Duff pdf data\n");
	  delete bed->pdf;
	  bed->pdf = NULL;
	}
	if (!bed->pdf->getCatalog()) {
	  g_warning ("Duff pdf catalog\n");
	  delete bed->pdf;
	  bed->pdf = NULL;
	}

	realize_drawing_areas (bed);
	redraw_all (bed);
	return 0;
}

extern "C" {

  static void
  destroy_view (GnomeView *view, view_data_t *view_data)
  {
    view_data->bed->views = g_list_remove (view_data->bed->views, view_data);
    gtk_object_unref (GTK_OBJECT (view_data->drawing_area));
    
    g_free (view_data);
  }

  static void
  destroy_embed (GnomeView *view, bed_t *bed)
  {
    while (bed->views)
      destroy_view (NULL, (view_data_t *)bed->views->data);

    delete bed->pdf;
    bed->pdf = NULL;
    gtk_object_unref (GTK_OBJECT (bed->stream));
    bed->stream = NULL;
    g_free (bed);
  }

}

static GdkPixmap *
setup_pixmap (bed_t *doc, view_data_t *view, GdkWindow *window)
{
    GdkGCValues  gcValues;
    GdkGC       *strokeGC;
    PDFDoc      *pdf;
    int          w, h;
    GdkPixmap   *pixmap = NULL;
    
    g_return_val_if_fail (doc != NULL, NULL);
    g_return_val_if_fail (view != NULL, NULL);
    g_return_val_if_fail (doc->pdf != NULL, NULL);
    
    pdf = doc->pdf;

    setup_size (doc, view);

    w = view->w;
    h = view->h;
   
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
    
    return pixmap;
}

extern "C" {

  static void
  drawing_area_realize (GtkWidget *drawing_area, view_data_t *view_data)
  {
    g_return_if_fail (view_data != NULL);
    g_return_if_fail (view_data->bed != NULL);
    g_return_if_fail (view_data->bed->pdf != NULL);

    if (!view_data->pixmap) {
      GdkWindow *win = gtk_widget_get_parent_window (drawing_area);
      GdkRectangle tmp;
      
      g_return_if_fail (win);
      
      view_data->pixmap = setup_pixmap (view_data->bed, view_data, win);
      view_data->bed->pdf->displayPage(view_data->out, view_data->page, view_data->zoom, 0, gTrue);

      configure_size (view_data, &tmp);
    }
  }

  static int
  drawing_area_exposed (GtkWidget *widget, GdkEventExpose *event, view_data_t *view_data)
  {
    if (!view_data ||
	!view_data->bed->pdf)
      return TRUE;
    
/* Hoisted from view_factory: ugly */
    redraw_view (view_data, &event->area);
    
    return TRUE;
  }
}

static void
realize_drawing_areas (bed_t *bed)
{
  GList *l;
  
  for (l = bed->views; l; l = l->next){
    GdkRectangle rect;
    view_data_t *view_data = (view_data_t *)l->data;

    drawing_area_realize (view_data->drawing_area, view_data);
  }  
}

static GnomeView *
view_factory (GnomeEmbeddable *embed_obj,
	      const GNOME_ViewFrame view_frame,
	      void *data)
{
        GnomeView *view;
	bed_t *bed = (bed_t *)data;
	view_data_t *view_data = g_new (view_data_t, 1);

	printf ("Created new bonobo object view %p\n", view_data);
	
	view_data->scale  = 1.0;
	view_data->bed    = bed;
	view_data->drawing_area = gtk_drawing_area_new ();
	view_data->pixmap = NULL;
	view_data->out    = NULL;
	view_data->w      = 320;
	view_data->h      = 320;
	view_data->zoom   = 24.0; /* 86.0; Must be small for demos :-) */
	view_data->page   = 1;

	gtk_signal_connect (
		GTK_OBJECT (view_data->drawing_area),
		"realize",
		GTK_SIGNAL_FUNC (drawing_area_realize), view_data);

	gtk_signal_connect (
		GTK_OBJECT (view_data->drawing_area),
		"expose_event",
		GTK_SIGNAL_FUNC (drawing_area_exposed), view_data);

        gtk_widget_show (view_data->drawing_area);

	setup_size (bed, view_data);

        view = gnome_view_new (view_data->drawing_area);

	gtk_signal_connect (
		GTK_OBJECT (view), "destroy",
		GTK_SIGNAL_FUNC (destroy_view), view_data);

	bed->views = g_list_prepend (bed->views, view_data);

        return view;
}

static GnomeObject *
embed_obj_factory (GnomeEmbeddableFactory *This, void *data)
{
	GnomeEmbeddable *embed_obj;
	GnomePersistStream *stream;
	bed_t *bed = (bed_t *)data;

	bed = g_new0 (bed_t, 1);
	if (!bed)
		return NULL;

	printf ("Created new bonobo object %p\n", bed);
	/*
	 * Creates the BonoboObject server
	 */
	embed_obj = gnome_embeddable_new (view_factory, bed);
	if (embed_obj == NULL){
		g_free (bed);
		return NULL;
	}

	bed->pdf = NULL;

	/*
	 * Interface GNOME::PersistStream 
	 */
	stream = gnome_persist_stream_new ("bonobo-object:image-x-pdf",
					   load_image_from_stream,
					   save_image,
					   bed);
	if (stream == NULL){
		gtk_object_unref (GTK_OBJECT (embed_obj));
		g_free (bed);
		return NULL;
	}

	bed->embed_obj = embed_obj;

	/*
	 * Bind the interfaces
	 */
	gnome_object_add_interface (GNOME_OBJECT (embed_obj),
				    GNOME_OBJECT (stream));
	gtk_signal_connect (
	  GTK_OBJECT (embed_obj), "destroy",
	  GTK_SIGNAL_FUNC (destroy_embed), bed);
	return (GnomeObject *) embed_obj;
}

static void
init_bonobo_image_x_png_factory (void)
{
	GnomeEmbeddableFactory *factory;
	
	factory = gnome_embeddable_factory_new (
		"bonobo-object-factory:image-x-pdf",
		embed_obj_factory, NULL);
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

	errorInit();

	initParams (xpdfConfigFile); /* Init font path */

	gtk_widget_set_default_colormap (gdk_rgb_get_cmap ());
	gtk_widget_set_default_visual (gdk_rgb_get_visual ());
	gtk_main ();
	
	CORBA_exception_free (&ev);

	return 0;
}
      
