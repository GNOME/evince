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

#define PDF_DEBUG 0

GBool printCommands = gFalse;

CORBA_Environment ev;
CORBA_ORB orb;

/*
 * BonoboObject data
 */
typedef struct {
  GnomeEmbeddable *embeddable;

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
  GdkWindow            *win;
  GOutputDev           *out;
  GdkColor              paper;
  gint                  w, h;
  gdouble               zoom;
  gint                  page;
} view_data_t;

extern "C" {
  static void realize_drawing_areas (bed_t *bed);
  static void setup_pixmap (bed_t *doc, view_data_t *view, GdkWindow *window);
}

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
render_page (view_data_t *view_data)
{
  setup_pixmap (view_data->bed, view_data,
		view_data->win);
  view_data->bed->pdf->displayPage(view_data->out,
				   view_data->page, view_data->zoom,
				   0, gTrue);
}

static void
redraw_view_all (view_data_t *view_data)
{
  GdkRectangle rect;
  g_return_if_fail (view_data != NULL);

#if PDF_DEBUG > 0
  printf ("Redraw view of page %d\n", view_data->page);
#endif
  render_page (view_data);
  rect.x = 0;
  rect.y = 0;
  rect.width  = view_data->w;
  rect.height = view_data->h;
  redraw_view (view_data, &rect);
  gtk_widget_queue_draw (GTK_WIDGET (view_data->drawing_area));
}

static void
configure_size (view_data_t *view_data)
{
  gtk_widget_set_usize (
    view_data->drawing_area,
    view_data->w,
    view_data->h);
}

static void
redraw_all (bed_t *bed)
{
	GList *l;
	
	for (l = bed->views; l; l = l->next) {
	  GdkRectangle rect;
	  view_data_t *view_data = (view_data_t *)l->data;
	  configure_size (view_data);
	  redraw_view_all (view_data);
	}
}

static int
save_image (GnomePersistStream *ps, GNOME_Stream stream, void *data)
{
  g_warning ("Unimplemented");
  return -1;
}

/*
 * different size ?
 */
static gboolean
setup_size (bed_t *doc, view_data_t *view)
{
  int      w, h;
  gboolean same;

  if (!doc || !view || !doc->pdf) {
    view->w = 320;
    view->h = 200;
    return FALSE;
  }
  w = (int)((doc->pdf->getPageWidth  (view->page) * view->zoom) / 72.0);
  h = (int)((doc->pdf->getPageHeight (view->page) * view->zoom) / 72.0);
  if (view->w == w && view->h == h)
    same = TRUE;
  else
    same = FALSE;
  view->w = w;
  view->h = h;

  return same;
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

#if PDF_DEBUG > 0
	printf ("Loading PDF from persiststream\n");
#endif
	bed->stream = stream;
	BonoboStream *bs = new BonoboStream (stream);
	GString *st = new GString ("Bonobo.pdf");
	bed->pdf = new PDFDoc (bs, st);
					      
#if PDF_DEBUG > 0
	printf ("Done load\n");
#endif
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

  static void
  setup_pixmap (bed_t *doc, view_data_t *view_data, GdkWindow *window)
  {
    GdkGCValues  gcValues;
    GdkGC       *strokeGC;
    PDFDoc      *pdf;
    int          w, h;
    GdkPixmap   *pixmap = NULL;
    
    g_return_if_fail (doc != NULL);
    g_return_if_fail (doc->pdf != NULL);
    g_return_if_fail (view_data != NULL);
    
    pdf = doc->pdf;

    if (setup_size (doc, view_data) &&
	view_data->pixmap) {
#if PDF_DEBUG > 0
      printf ("No need to re-init output device\n");
#endif
      return;
    }

    w = view_data->w;
    h = view_data->h;

    pixmap = gdk_pixmap_new (window, w, h, -1);
    
    gdk_color_white (gtk_widget_get_default_colormap(), &view_data->paper);
    view_data->out = new GOutputDev (pixmap, view_data->paper, window);
    
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
    
    view_data->pixmap = pixmap;
  }

  static gboolean
  view_is_good (view_data_t *view_data)
  {
    if (!view_data ||
	!view_data->bed ||
	!view_data->bed->pdf)
      return FALSE;

    return TRUE;
  }

  static gboolean
  first_page (view_data_t *view_data)
  {
    g_return_val_if_fail (view_is_good (view_data), FALSE);
    if (view_data->page != 1) {
      view_data->page = 1;
      return TRUE;
    } else
      return FALSE;
  }

  static gboolean
  last_page (view_data_t *view_data)
  {
    g_return_val_if_fail (view_is_good (view_data), FALSE);
    if (view_data->page < view_data->bed->pdf->getNumPages()) {
      view_data->page = view_data->bed->pdf->getNumPages();
      return TRUE;
    } else
      return FALSE;
  }

  static gboolean
  next_page (view_data_t *view_data)
  {
    g_return_val_if_fail (view_is_good (view_data), FALSE);
    if (view_data->page < view_data->bed->pdf->getNumPages()) {
      view_data->page++;
      return TRUE;
    } else
      return FALSE;
  }

  static gboolean
  prev_page (view_data_t *view_data)
  {
    g_return_val_if_fail (view_is_good (view_data), FALSE);
    if (view_data->page > 1) {
      view_data->page--;
      return TRUE;
    } else
      return FALSE;
  }

  static void
  page_first_cb (GnomeUIHandler *uih, void *data, char *path)
  {
    first_page ((view_data_t *)data);
    redraw_view_all ((view_data_t *)data);
  }
  
  static void
  page_next_cb  (GnomeUIHandler *uih, void *data, char *path)
  {
    next_page ((view_data_t *)data);
    redraw_view_all ((view_data_t *)data);
  }
  
  static void
  page_prev_cb  (GnomeUIHandler *uih, void *data, char *path)
  {
    prev_page ((view_data_t *)data);
    redraw_view_all ((view_data_t *)data);
  }

  static void
  page_last_cb  (GnomeUIHandler *uih, void *data, char *path)
  {
    last_page ((view_data_t *)data);
    redraw_view_all ((view_data_t *)data);
  }
}

static void
view_create_menus (view_data_t *view_data)
{
  GNOME_UIHandler remote_uih;
  GnomeView *view = view_data->view;
  GnomeUIHandler *uih;
  
  uih = gnome_view_get_ui_handler (view);
  remote_uih = gnome_view_get_remote_ui_handler (view);
  
  if (remote_uih == CORBA_OBJECT_NIL) {
    g_warning ("server has no UI hander");
    return;
  }
  
  gnome_ui_handler_set_container (uih, remote_uih);

  gnome_ui_handler_menu_new_subtree (uih, "/Page",
				     N_("Page..."),
				     N_("Set the currently displayed page"),
				     1,
				     GNOME_UI_HANDLER_PIXMAP_NONE, NULL,
				     0, (GdkModifierType)0);

  gnome_ui_handler_menu_new_item (uih, "/Page/First",
				  N_("First"), N_("View the first page"), -1,
				  GNOME_UI_HANDLER_PIXMAP_NONE, NULL, 0,
				  (GdkModifierType)0, page_first_cb, (gpointer)view_data);
				 
  gnome_ui_handler_menu_new_item (uih, "/Page/Prev",
				  N_("Previous"), N_("View the previous page"), -1,
				  GNOME_UI_HANDLER_PIXMAP_NONE, NULL, 0,
				  (GdkModifierType)0, page_prev_cb, (gpointer)view_data);
				 
  gnome_ui_handler_menu_new_item (uih, "/Page/Next",
				  N_("Next"), N_("View the next page"), -1,
				  GNOME_UI_HANDLER_PIXMAP_NONE, NULL, 0,
				  (GdkModifierType)0, page_next_cb, (gpointer)view_data);
				 
  gnome_ui_handler_menu_new_item (uih, "/Page/Last",
				  N_("Last"), N_("View the last page"), -1,
				  GNOME_UI_HANDLER_PIXMAP_NONE, NULL, 0,
				  (GdkModifierType)0, page_last_cb, (gpointer)view_data);
  
				 
}

/*
 * When this view is deactivated, we must remove our menu items.
 */
static void
view_remove_menus (view_data_t *view_data)
{
	GnomeView *view = view_data->view;
	GnomeUIHandler *uih;

	uih = gnome_view_get_ui_handler (view);

	gnome_ui_handler_unset_container (uih);
}

extern "C" {

  static void
  drawing_area_realize (GtkWidget *drawing_area, view_data_t *view_data)
  {
    g_return_if_fail (view_data != NULL);
    g_return_if_fail (drawing_area != NULL);

    view_data->win = gtk_widget_get_parent_window (drawing_area);
  }

  static int
  drawing_area_exposed (GtkWidget *widget, GdkEventExpose *event, view_data_t *view_data)
  {
    if (!view_data ||
	!view_data->bed->pdf)
      return TRUE;
    
    redraw_view (view_data, &event->area);
    
    return TRUE;
  }

  static void
  view_activate (GnomeView *view, gboolean activate, gpointer data)
  {
    view_data_t *view_data = (view_data_t *) data;
    g_return_if_fail (view != NULL);
    g_return_if_fail (view_data != NULL);
    
    gnome_view_activate_notify (view, activate);
    
#if PDF_DEBUG > 0
    printf ("View change activation to %d\n", activate);
#endif
    /*
     * If we were just activated, we merge in our menu entries.
     * If we were just deactivated, we remove them.
     */
    if (activate)
      view_create_menus (view_data);
    else
      view_remove_menus (view_data);
  }

  static void
  view_size_query (GnomeView *view, int *desired_width, int *desired_height,
		   gpointer data)
  {
    view_data_t *view_data = (view_data_t *) data;
    
    *desired_width  = view_data->w;
    *desired_height = view_data->h;
  }

  static void
  realize_drawing_areas (bed_t *bed)
  {
    GList *l;
    
    for (l = bed->views; l; l = l->next) {
      view_data_t *view_data = (view_data_t *)l->data;
      g_return_if_fail (view_data->win);
      render_page (view_data);
      configure_size (view_data);
    }  
  }

  static void
  view_switch_page (GnomeView *view, const char *verb_name, void *user_data)
  {
    view_data_t  *view_data = (view_data_t *) user_data;
    GdkRectangle  rect;
    gboolean      changed = FALSE;

    if (!g_strcasecmp (verb_name, "firstpage")) {
      changed = first_page (view_data);
    } else if (!g_strcasecmp (verb_name, "prevpage")) {
      changed = prev_page (view_data);
    } else if (!g_strcasecmp (verb_name, "nextpage")) {
      changed = next_page (view_data);
    } else if (!g_strcasecmp (verb_name, "lastpage")) {
      changed = last_page (view_data);
    } else
      g_warning ("Unknown verb");

    if (changed) {
      render_page (view_data);
      redraw_view (view_data, &rect);
      gtk_widget_queue_draw (GTK_WIDGET (view_data->drawing_area));
    }
  }
}

static GnomeView *
view_factory (GnomeEmbeddable *embeddable,
	      const GNOME_ViewFrame view_frame,
	      void *data)
{
        GnomeView *view;
	GnomeUIHandler *uih;
	bed_t *bed = (bed_t *)data;
	view_data_t *view_data = g_new (view_data_t, 1);

#if PDF_DEBUG > 0
	printf ("Created new bonobo object view %p\n", view_data);
#endif
	
	view_data->scale  = 1.0;
	view_data->bed    = bed;
	view_data->drawing_area = gtk_drawing_area_new ();
	view_data->pixmap = NULL;
	view_data->win    = NULL;
	view_data->out    = NULL;
	view_data->w      = 320;
	view_data->h      = 320;
	view_data->zoom   = 43.0; /* 86.0; Must be small for demos :-) */
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
	view_data->view = view;

	gtk_signal_connect (
		GTK_OBJECT (view), "destroy",
		GTK_SIGNAL_FUNC (destroy_view), view_data);

	/* UI handling */
	uih = gnome_ui_handler_new ();
	gnome_view_set_ui_handler (view, uih);

	gtk_signal_connect (GTK_OBJECT (view), "view_activate",
			    GTK_SIGNAL_FUNC (view_activate), view_data);

	gtk_signal_connect (GTK_OBJECT (view), "size_query",
			    GTK_SIGNAL_FUNC (view_size_query), view_data);

	bed->views = g_list_prepend (bed->views, view_data);

	/* Verb handling */
	gnome_view_register_verb (view, "FirstPage",
				  view_switch_page, view_data);
	gnome_view_register_verb (view, "PrevPage",
				  view_switch_page, view_data);
	gnome_view_register_verb (view, "NextPage",
				  view_switch_page, view_data);
	gnome_view_register_verb (view, "LastPage",
				  view_switch_page, view_data);

        return view;
}

static GnomeObject *
embeddable_factory (GnomeEmbeddableFactory *This, void *data)
{
	GnomeEmbeddable *embeddable;
	GnomePersistStream *stream;
	bed_t *bed = (bed_t *)data;

	bed = g_new0 (bed_t, 1);
	if (!bed)
		return NULL;

#if PDF_DEBUG > 0
	printf ("Created new bonobo object %p\n", bed);
#endif
	/*
	 * Creates the BonoboObject server
	 */
	embeddable = gnome_embeddable_new (view_factory, bed);
	if (embeddable == NULL){
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
	if (stream == NULL) {
		gtk_object_unref (GTK_OBJECT (embeddable));
		g_free (bed);
		return NULL;
	}

	bed->embeddable = embeddable;

	/*
	 * Bind the interfaces
	 */
	gnome_object_add_interface (GNOME_OBJECT (embeddable),
				    GNOME_OBJECT (stream));
	gtk_signal_connect (
	  GTK_OBJECT (embeddable), "destroy",
	  GTK_SIGNAL_FUNC (destroy_embed), bed);

	/* Setup some verbs */
	gnome_embeddable_add_verb (embeddable,
				   "FirstPage",
				   _("_First page"),
				   _("goto the first page"));
	gnome_embeddable_add_verb (embeddable,
				   "PrevPage",
				   _("_Previous page"),
				   _("goto the previous page"));
	gnome_embeddable_add_verb (embeddable,
				   "NextPage",
				   _("_Next page"),
				   _("goto the next page"));
	gnome_embeddable_add_verb (embeddable,
				   "LastPage",
				   _("_Last page"),
				   _("goto the last page"));
	
	return (GnomeObject *) embeddable;
}

static void
init_bonobo_image_x_png_factory (void)
{
	GnomeEmbeddableFactory *factory;
	
	factory = gnome_embeddable_factory_new (
		"bonobo-object-factory:image-x-pdf",
		embeddable_factory, NULL);
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
