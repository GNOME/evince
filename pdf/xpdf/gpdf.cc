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

#define DOC_ROOT_MAGIC 0xad3f556d
struct DOC_ROOT {
  guint32        magic;
  GString        *title;
  PDFDoc         *pdf;
  GtkWidget      *toplevel;
  GtkWidget      *table;
  GnomeAppBar    *appbar;
  GtkDrawingArea *area;
  GdkPixmap      *pixmap;
  OutputDev      *out;
  GdkColor        paper;
};

DOC_ROOT *hack_global = NULL;

static void
crummy_cmd (GtkWidget *widget, DOC_ROOT *tmp)
{
  printf ("Crummy\n");
}


const struct poptOption gpdf_popt_options [] = {
  { "debug", '\0', POPT_ARG_INT, &gpdf_debug, 0,
    N_("Enables some debugging functions"), N_("LEVEL") },
  { NULL, '\0', 0, NULL, 0 }
};


static GnomeUIInfo dummy_menu [] = {
	{ GNOME_APP_UI_ITEM, N_("_dummy"),
	  N_("What a dummy!"), crummy_cmd },
	GNOMEUIINFO_END
};

static GnomeUIInfo main_menu [] = {
	{ GNOME_APP_UI_SUBTREE, N_("_Dummy"), NULL, dummy_menu },
	GNOMEUIINFO_END
};

//------------------------------------------------------------------------
// loadFile / displayPage
//------------------------------------------------------------------------

static gint
doc_config_event (GtkWidget *widget, void *ugly)
{
  DOC_ROOT *doc = hack_global;

  g_return_val_if_fail (doc, FALSE);
  g_return_val_if_fail (doc->magic == DOC_ROOT_MAGIC, FALSE);

  if (doc->pixmap)
    gdk_pixmap_unref(doc->pixmap);

  doc->pixmap = gdk_pixmap_new(widget->window,
                               widget->allocation.width,
                               widget->allocation.height,
                               -1);

  printf ("Creating pixmap of size %d %d\n",
	  widget->allocation.width, widget->allocation.height);
  gdk_color_white (gtk_widget_get_default_colormap(), &doc->paper);
  doc->out    = new GOutputDev (doc->pixmap, doc->paper);


  {
    GdkGCValues gcValues;
    GdkGC *strokeGC;
    
    gdk_color_white (gtk_widget_get_default_colormap (), &gcValues.foreground);
    gdk_color_black (gtk_widget_get_default_colormap (), &gcValues.background);
    gcValues.line_width = 1;
    gcValues.line_style = GDK_LINE_SOLID;
    strokeGC = gdk_gc_new_with_values (
      doc->pixmap, &gcValues, 
      (enum GdkGCValuesMask)(GDK_GC_FOREGROUND | GDK_GC_BACKGROUND | GDK_GC_LINE_WIDTH | GDK_GC_LINE_STYLE));

    gdk_draw_rectangle (doc->pixmap,
			strokeGC,
			TRUE,
			0, 0,
			widget->allocation.width,
			widget->allocation.height);
  }
  return TRUE;
}

GdkFont *magic_font;
GdkGC   *magic_black;

static gint
doc_redraw_event (GtkWidget *widget, GdkEventExpose *event)
{
  DOC_ROOT *doc = hack_global;

  g_return_val_if_fail (doc, FALSE);
  g_return_val_if_fail (doc->magic == DOC_ROOT_MAGIC, FALSE);

  if (doc->out && doc->pdf) {
    GtkStyle *style = gtk_widget_get_default_style();
    printf ("There are %d pages\n", doc->pdf->getNumPages());

    magic_font  = widget->style->font;
    magic_black = widget->style->black_gc;
    gdk_draw_line (doc->pixmap,
		   widget->style->black_gc,
		   event->area.x, event->area.y,
		   event->area.width, event->area.height);
    doc->pdf->displayPage(doc->out, 1, 86, 0, gTrue); /* 86 zoom */
    printf ("Draw pixmap %p\n", doc->pixmap);
    gdk_draw_string (doc->pixmap, magic_font, magic_black,
		     300, 300, "Hello");
    gdk_draw_pixmap(widget->window,
		    widget->style->fg_gc[GTK_WIDGET_STATE (widget)],
		    doc->pixmap,
		    0, 0,
		    event->area.x, event->area.y,
		    event->area.width, event->area.height);
  } else
    printf ("Null pointer error %p %p\n", doc->out, doc->pdf);
  
  return FALSE;
}

static GBool
loadFile(GString *fileName)
{
  DOC_ROOT *doc = new DOC_ROOT();
  char s[20];
  char *p;

  hack_global = doc;

  doc->magic = DOC_ROOT_MAGIC;
  // open PDF file
  doc->pdf = new PDFDoc(fileName);
  if (!doc->pdf->isOk()) {
    delete doc->pdf;
    delete doc;
    return gFalse;
  }

  g_assert (doc->pdf->getCatalog());

  doc->toplevel = gnome_app_new ("gpdf", "gpdf");
  gtk_window_set_policy(GTK_WINDOW(doc->toplevel), 1, 1, 0);
  gtk_window_set_default_size (GTK_WINDOW(doc->toplevel), 600, 400);
  doc->table  = GTK_WIDGET (gtk_table_new (0, 0, 0));
  doc->appbar = GNOME_APPBAR (gnome_appbar_new (FALSE, TRUE,
						GNOME_PREFERENCES_USER));
  gnome_app_set_statusbar (GNOME_APP (doc->toplevel),
			   GTK_WIDGET (doc->appbar));
  gnome_app_set_contents (GNOME_APP (doc->toplevel), doc->table);
  gnome_app_create_menus_with_data (GNOME_APP (doc->toplevel), main_menu, doc);
  gnome_app_install_menu_hints(GNOME_APP (doc->toplevel), main_menu);

  doc->pixmap = NULL;
  doc->area   = GTK_DRAWING_AREA (gtk_drawing_area_new ());
  gtk_signal_connect (GTK_OBJECT(doc->area),"configure_event",
		      (GtkSignalFunc) doc_config_event, doc);
  gtk_signal_connect (GTK_OBJECT (doc->area), "expose_event",
		      (GtkSignalFunc) doc_redraw_event, doc);

  gtk_table_attach (GTK_TABLE (doc->table), GTK_WIDGET (doc->area),
		    0, 1, 1, 2,
		    GTK_FILL | GTK_EXPAND, GTK_FILL | GTK_EXPAND,
		    0, 0);

  gtk_widget_show_all (doc->toplevel);

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

int
main (int argc, char *argv [])
{
  char **view_files = NULL;
  int lp;

  gnome_init_with_popt_table (
    "gpdf", "0.1", argc, argv,
    gpdf_popt_options, 0, &ctx);

  errorInit();
  
  initParams (xpdfConfigFile); /* Init font path */

  view_files = poptGetArgs (ctx);
  /* Load files */
  if (view_files) {
    for (lp=0;view_files[lp];lp++) {
      GString *name = new GString (view_files[lp]);
      if (!name || !loadFile(name))
	printf ("Error loading '%s'\n", view_files[lp]);
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
