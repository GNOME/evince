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

#define GPDF_GLADE_DIR "/opt/gnome/src/xpdf/xpdf"

GBool printCommands = gFalse;
gint  gpdf_debug=1;
poptContext ctx;

#define DEV_DEBUG 0

double zoom = 86.0;
gint page = 2;

#define DOC_KEY "xpdf_doc_key"
struct DOC_ROOT {
  GString        *title;
  PDFDoc         *pdf;
  GtkDrawingArea *area;
  GtkPixmap      *pixmap;
  OutputDev      *out;
  GdkColor        paper;
  GtkScrolledWindow *scroll;
  GtkWidget      *mainframe;
  GladeXML       *gui;
};

const struct poptOption gpdf_popt_options [] = {
  { "debug", '\0', POPT_ARG_INT, &gpdf_debug, 0,
    N_("Enables some debugging functions"), N_("LEVEL") },
  { NULL, '\0', 0, NULL, 0 }
};

//------------------------------------------------------------------------
// loadFile / displayPage
//------------------------------------------------------------------------

static void
get_page_geom (int *w, int *h, Page *p)
{
  double pw = p->getWidth();
  double ph = p->getHeight();

  *w = 612;
  *h = 792;

  if (!p)
    return;

  *w = (int)((pw * zoom)/72.0 + 28.0);
  *h = (int)((ph * zoom)/72.0 + 56.0);
}

static GdkPixmap *
setup_pixmap (DOC_ROOT *doc, GdkWindow *window)
{
  GdkGCValues  gcValues;
  GdkGC       *strokeGC;
  PDFDoc      *pdf = doc->pdf;
  int          w, h;
  GdkPixmap   *pixmap = NULL;

  if (pixmap)
    gdk_pixmap_unref(pixmap);

  Catalog *cat = pdf->getCatalog();
  get_page_geom (&w, &h, cat->getPage (page));

  pixmap = gdk_pixmap_new (window, w, h, -1);
  gtk_widget_set_usize (GTK_WIDGET (doc->scroll), w, h);

  printf ("Creating pixmap of size %d %d\n", w, h);
  gdk_color_white (gtk_widget_get_default_colormap(), &doc->paper);
  doc->out    = new GOutputDev (pixmap, doc->paper, window);

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

/*static gint
doc_config_event (GtkWidget *widget, void *ugly)
{
  DOC_ROOT *doc;

  doc = (DOC_ROOT *)gtk_object_get_data (GTK_OBJECT (widget), DOC_KEY);
  
  g_return_val_if_fail (doc, FALSE);

  return TRUE;
}

static gint
doc_redraw_event (GtkWidget *widget, GdkEventExpose *event)
{
  DOC_ROOT *doc;
 
  g_return_val_if_fail (widget != NULL, FALSE);

  doc = (DOC_ROOT *)gtk_object_get_data (GTK_OBJECT (widget), DOC_KEY);

  g_return_val_if_fail (doc != NULL, FALSE);

  if (doc->out && doc->pdf) {
#if DEV_DEBUG > 0
    printf ("There are %d pages\n", doc->pdf->getNumPages());
#endif

    doc->pdf->displayPage(doc->out, 1, 86, 0, gTrue);
    gdk_draw_pixmap(widget->window,
		    widget->style->fg_gc[GTK_WIDGET_STATE (widget)],
		    doc->pixmap,
		    0, 0,
		    event->area.x, event->area.y,
		    event->area.width, event->area.height);
  } else
    printf ("Null pointer error %p %p\n", doc->out, doc->pdf);
  
  return FALSE;
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


static GBool
loadPDF(GString *fileName)
{
  DOC_ROOT *doc = new DOC_ROOT();
  GtkVBox  *pane;
  GtkAdjustment *hadj, *vadj;
  GdkPixmap *pix;

  // open PDF file
  doc->pdf = getPDF (fileName);
  if (!doc->pdf) {
    delete doc;
    return gFalse;
  }

  doc->gui = glade_xml_new (GPDF_GLADE_DIR "/gpdf.glade", NULL);
  if (!doc->gui ||
      !(doc->mainframe = glade_xml_get_widget (doc->gui, "gpdf")) ||
      !(pane = GTK_VBOX (glade_xml_get_widget (doc->gui, "pane")))) {
    printf ("Couldn't find " GPDF_GLADE_DIR "/gpdf.glade\n");
    delete doc->pdf;
    delete doc;
    return gFalse;
  }
/*  glade_xml_signal_autoconnect (doc->gui);*/
    
  pix = setup_pixmap (doc, gtk_widget_get_parent_window (GTK_WIDGET (pane)));
  doc->pixmap = GTK_PIXMAP (gtk_pixmap_new (pix, NULL));
  
  doc->scroll = GTK_SCROLLED_WINDOW (gtk_scrolled_window_new (NULL, NULL));
  gtk_scrolled_window_set_policy (doc->scroll, GTK_POLICY_AUTOMATIC,
				  GTK_POLICY_AUTOMATIC);
  doc->pdf->displayPage(doc->out, page, zoom, 0, gTrue);
  gtk_scrolled_window_add_with_viewport (doc->scroll, GTK_WIDGET (doc->pixmap));
  gtk_box_pack_start (GTK_BOX (pane), GTK_WIDGET (doc->scroll), TRUE, TRUE, 0);

  gtk_widget_show_all (doc->mainframe);
  return gTrue;
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

extern "C" {
  void
  on_close_activate (GtkWidget *window, void *data)
  {
    printf ("Bye...");
    gtk_widget_destroy (window);
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
      if (!name || !loadPDF (name))
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
