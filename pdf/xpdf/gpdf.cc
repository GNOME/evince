//========================================================================
//
// gpdf.cc
//
// Copyright 1996 Derek B. Noonburg
// Copyright 1999 Michael Meeks.
// Copyright 1999 Miguel de Icaza
//
//========================================================================

#include <stdio.h>
#include <stdlib.h>
#include <stddef.h>
#include <string.h>
#define GString G_String
#include <gnome.h>
#include <glade/glade.h>
#undef  GString 
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
gint  gpdf_debug=1;
poptContext ctx;

#define DEV_DEBUG 0

#define DOC_ROOT_TAG "xpdf_doc_root"
struct DOC_ROOT {
  GString        *title;
  PDFDoc         *pdf;
  Catalog        *cat;
  GtkDrawingArea *area;
  GtkPixmap      *pixmap;
  OutputDev      *out;
  GdkColor        paper;
  GtkScrolledWindow *scroll;
  GtkWidget      *mainframe;
  GladeXML       *gui;
  guint           w, h;

  double          zoom;
  gint            page;
};

GList *documents = NULL;

const struct poptOption gpdf_popt_options [] = {
  { "debug", '\0', POPT_ARG_INT, &gpdf_debug, 0,
    N_("Enables some debugging functions"), N_("LEVEL") },
  { NULL, '\0', 0, NULL, 0 }
};

extern "C" {
  static void connect_signals (DOC_ROOT *doc);
}

//------------------------------------------------------------------------
// loadFile / displayPage
//------------------------------------------------------------------------

static GtkPixmap *
setup_pixmap (DOC_ROOT *doc, DOC_ROOT *view, GdkWindow *window)
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

static void
render_page (DOC_ROOT *doc, DOC_ROOT *view)
{
  doc->pdf->displayPage(doc->out, view->page, view->zoom, 0, gTrue);
}

/*static void displayPage(int page1, int zoom1, int rotate1) {
  char s[20];

  // check for document
  if (!doc)
    return;

  // busy cursor
  if (win)
    win->setBusyCursor(gTrue);

  // new page/zoom/rotate values
  page = page1;
  zoom = zoom1;
  rotate = rotate1;

  // initialize mouse-related stuff
  linkAction = NULL;
  win->setDefaultCursor();
  linkLabel->setText(NULL);
  selectXMin = selectXMax = 0;
  selectYMin = selectYMax = 0;
  lastDragLeft = lastDragTop = gTrue;

  // draw the page
  doc->displayPage(out, page, zoomDPI[zoom - minZoom], rotate, gTrue);
  layoutCbk(win);

  // update page number display
  sprintf(s, "%d", page);
  pageNumText->setText(s);

  // back to regular cursor
  win->setBusyCursor(gFalse);
  }*/

static PDFDoc *
getPDF (GString *fname)
{
  PDFDoc *pdf;
  pdf = new PDFDoc(fname);
  if (!pdf->isOk()) {
    delete pdf;
    return NULL;
  }
  g_return_val_if_fail (pdf->getCatalog(), NULL);
  return pdf;
}

static DOC_ROOT *
doc_root_new (GString *fileName)
{
  DOC_ROOT *doc = new DOC_ROOT();
  GtkVBox  *pane;

  // open PDF file
  doc->pdf = getPDF (fileName);
  if (!doc->pdf) {
    delete doc;
    return NULL;
  }

  doc->gui = glade_xml_new (GPDF_GLADE_DIR "/gpdf.glade", NULL);
  if (!doc->gui ||
      !(doc->mainframe = glade_xml_get_widget (doc->gui, "gpdf")) ||
      !(pane = GTK_VBOX (glade_xml_get_widget (doc->gui, "pane")))) {
    printf ("Couldn't find " GPDF_GLADE_DIR "/gpdf.glade\n");
    delete doc->pdf;
    delete doc;
    return NULL;
  }

  doc->zoom = 86.0;
  doc->page = 1;

  connect_signals (doc);

  gtk_object_set_data (GTK_OBJECT (doc->mainframe), DOC_ROOT_TAG, doc);

  doc->pixmap = setup_pixmap (doc, doc, gtk_widget_get_parent_window (GTK_WIDGET (pane)));
  
  doc->scroll = GTK_SCROLLED_WINDOW (gtk_scrolled_window_new (NULL, NULL));
  gtk_scrolled_window_set_policy (doc->scroll, GTK_POLICY_AUTOMATIC,
				  GTK_POLICY_AUTOMATIC);
  render_page (doc, doc);
  gtk_scrolled_window_add_with_viewport (doc->scroll, GTK_WIDGET (doc->pixmap));
  gtk_box_pack_start (GTK_BOX (pane), GTK_WIDGET (doc->scroll), TRUE, TRUE, 0);

  gtk_widget_show_all (doc->mainframe);

  documents = g_list_append (documents, doc);

  return doc;
}

static void
doc_root_destroy (DOC_ROOT *doc)
{
    gtk_widget_destroy (doc->mainframe);
    gtk_object_destroy (GTK_OBJECT (doc->gui));
    
    documents = g_list_remove (documents, doc);
    if (g_list_length (documents) == 0)
      gtk_main_quit ();
    delete (doc);
}

//------------------------------------------------------------------------
//                          Signal handlers
//------------------------------------------------------------------------

extern "C" {
  void
  do_close (GtkWidget *menuitem, DOC_ROOT *doc)
  {
    doc_root_destroy (doc);
  }

  void
  do_exit (GtkWidget *menuitem, DOC_ROOT *doc)
  {
    GList *l;
    while ((l=documents))
      doc_root_destroy ((DOC_ROOT *)l->data);
  }

  static void
  do_about_box (GtkWidget *w, DOC_ROOT *doc)
  {
    GladeXML *gui = glade_xml_new (GPDF_GLADE_DIR "/about.glade", NULL);
    g_return_if_fail (gui);
    GtkWidget *wi = glade_xml_get_widget (gui, "about_box");
    g_return_if_fail (wi);
    gtk_widget_show  (wi);
    gtk_object_destroy (GTK_OBJECT (gui));
  }

  static void
  do_forward_button (GtkWidget *w, DOC_ROOT *doc)
  {
    if (doc->page < doc->pdf->getNumPages()) {
      doc->page++;
      render_page (doc, doc);
      gtk_widget_queue_draw (GTK_WIDGET (doc->scroll));
    }
  }

  static void
  do_back_button (GtkWidget *w, DOC_ROOT *doc)
  {
    if (doc->page > 1) {
      doc->page--;
      render_page (doc, doc);
      gtk_widget_queue_draw (GTK_WIDGET (doc->pixmap));
    }
  }

  static void
  do_first_button (GtkWidget *w, DOC_ROOT *doc)
  {
    if (doc->page != 1) {
      doc->page = 1;
      render_page (doc, doc);
      gtk_widget_queue_draw (GTK_WIDGET (doc->pixmap));
    }
  }

  static void
  do_last_button (GtkWidget *w, DOC_ROOT *doc)
  {
    if (doc->page != doc->pdf->getNumPages()) {
      doc->page = doc->pdf->getNumPages();
      render_page (doc, doc);
      gtk_widget_queue_draw (GTK_WIDGET (doc->pixmap));
    }
  }

  static void
  do_larger_button (GtkWidget *w, DOC_ROOT *doc)
  {
    if (doc->zoom < 200) {
      doc->zoom *= 1.2;
      render_page (doc, doc);
      gtk_widget_queue_draw (GTK_WIDGET (doc->pixmap));
    }
  }

  static void
  do_smaller_button (GtkWidget *w, DOC_ROOT *doc)
  {
    if (doc->zoom < 200) {
      doc->zoom /= 1.2;
      render_page (doc, doc);
      gtk_widget_queue_draw (GTK_WIDGET (doc->pixmap));
    }
  }
  
  static void
  simple_menu_connect (DOC_ROOT *doc, const char *name, GtkSignalFunc func)
  {
    GtkWidget *w;
    w = glade_xml_get_widget (doc->gui, name);
    g_return_if_fail (w);
    gtk_signal_connect (GTK_OBJECT (w), "activate", func, doc);
  }
  
  static void
  simple_button_connect (DOC_ROOT *doc, const char *name, GtkSignalFunc func)
  {
    GtkWidget *w;
    w = glade_xml_get_widget (doc->gui, name);
    g_return_if_fail (w);
    gtk_signal_connect (GTK_OBJECT (w), "clicked", func, doc);
  }

  static void
  connect_signals (DOC_ROOT *doc)
  {
    GtkWidget *w;

    simple_menu_connect (doc, "about_menu", GTK_SIGNAL_FUNC (do_about_box)); 
    simple_menu_connect (doc, "close_menu", GTK_SIGNAL_FUNC (do_close));
    simple_menu_connect (doc, "exit_menu",  GTK_SIGNAL_FUNC (do_exit));

    simple_button_connect (doc, "forward", GTK_SIGNAL_FUNC (do_forward_button));
    simple_button_connect (doc, "back",    GTK_SIGNAL_FUNC (do_back_button));
    simple_button_connect (doc, "first",   GTK_SIGNAL_FUNC (do_first_button));
    simple_button_connect (doc, "last",    GTK_SIGNAL_FUNC (do_last_button));
/*    simple_button_connect (doc, "larger",  GTK_SIGNAL_FUNC (do_larger_button)); need to resize the gtkpixmap...
      simple_button_connect (doc, "smaller", GTK_SIGNAL_FUNC (do_smaller_button)); but bed first. */

    gtk_signal_connect (GTK_OBJECT (doc->mainframe), "destroy",
			GTK_SIGNAL_FUNC (do_close), doc);
 }
}

int
main (int argc, char *argv [])
{
  char **view_files = NULL;
  int    i;
  
  gnome_init_with_popt_table (
    "gpdf", "0.1", argc, argv,
    gpdf_popt_options, 0, &ctx);

  errorInit();
  
  initParams (xpdfConfigFile); /* Init font path */

  glade_gnome_init ();

  view_files = poptGetArgs (ctx);
  /* Load files */
  if (view_files) {
    for (i = 0; view_files[i]; i++) {
      GString *name = new GString (view_files[i]);
      if (!name || !doc_root_new (name))
	printf ("Error loading '%s'\n", view_files[i]);
    }
  } else {
    printf ("Need filenames...\n");
    exit (0);
  }

  poptFreeContext (ctx);

  gtk_main ();

  freeParams();

  /* Destroy files */
}
