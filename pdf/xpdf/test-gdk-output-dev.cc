/**
 * render a PDF to GDKSplashOutputDev
 *
 * Copyright 2004 Red Hat, Inc.
 */

#include <config.h>

#include <aconf.h>
#include <stdio.h>
#include <stdlib.h>

#include "gpdf-g-switch.h"
#include <gtk/gtk.h>
#include "gpdf-g-switch.h"

#include "GlobalParams.h"
#include "GDKSplashOutputDev.h"
#include "PDFDoc.h"
#include "ErrorCodes.h"

typedef struct
{
  GtkWidget *window;
  GtkWidget *sw;
  GtkWidget *drawing_area;
  GDKSplashOutputDev *out;
  PDFDoc *doc;
} View;

static void
drawing_area_expose (GtkWidget      *drawing_area,
                     GdkEventExpose *event,
                     void           *data)
{
  View *v = (View*) data;
  int x, y, w, h;
  GdkRectangle document;
  GdkRectangle draw;

  gdk_window_clear (drawing_area->window);
  
  document.x = 0;
  document.y = 0;
  document.width = v->out->getBitmapWidth();
  document.height = v->out->getBitmapHeight();

  if (gdk_rectangle_intersect (&document, &event->area, &draw))
    {
      v->out->redraw (draw.x, draw.y,
                      drawing_area->window,
                      draw.x, draw.y,
                      draw.width, draw.height);
    }
}

static int
view_load (View       *v,
           const char *filename)
{
  PDFDoc *newDoc;
  int err;
  GString *filename_g;
  GtkAdjustment *hadj;
  GtkAdjustment *vadj;
  int w, h;

  filename_g = new GString (filename);

  // open the PDF file
  newDoc = new PDFDoc(filename_g, 0, 0);

  delete filename_g;
  
  if (!newDoc->isOk())
    {
      err = newDoc->getErrorCode();
      delete newDoc;
      return err;
    }

  if (v->doc)
    delete v->doc;
  v->doc = newDoc;
  
  v->out->startDoc(v->doc->getXRef());

  v->doc->displayPage (v->out, 1, 72, 72, 0, gTrue, gTrue);
  
  w = v->out->getBitmapWidth();
  h = v->out->getBitmapHeight();
  
  gtk_widget_set_size_request (v->drawing_area, w, h);
}

static void
view_show (View *v)
{
  gtk_widget_show (v->window);
}

static void
redraw_callback (void *data)
{
  View *v = (View*) data;

  gtk_widget_queue_draw (v->drawing_area);
}

static View*
view_new (void)
{
  View *v;
  GtkWidget *window;
  GtkWidget *drawing_area;
  GtkWidget *sw;

  window = gtk_window_new (GTK_WINDOW_TOPLEVEL);

  drawing_area = gtk_drawing_area_new ();

  sw = gtk_scrolled_window_new (NULL, NULL);

  gtk_container_add (GTK_CONTAINER (window), sw);
  gtk_scrolled_window_add_with_viewport (GTK_SCROLLED_WINDOW (sw), drawing_area);

  gtk_widget_show_all (sw);

  v = g_new0 (View, 1);

  v->window = window;
  v->drawing_area = drawing_area;
  v->sw = sw;
  v->out = new GDKSplashOutputDev (gtk_widget_get_screen (window),
                                   redraw_callback, (void*) v);
  v->doc = 0;

  g_signal_connect (drawing_area,
                    "expose_event",
                    G_CALLBACK (drawing_area_expose),
                    (void*) v);
  
  return v;
}

int
main (int argc, char *argv [])
{
  View *v;
  int i;
  
  gtk_init (&argc, &argv);
  
  globalParams = new GlobalParams("/etc/xpdfrc");
  globalParams->setupBaseFonts(NULL);
  
  i = 1;
  while (i < argc)
    {
      int err;
      
      v = view_new ();

      err = view_load (v, argv[i]);

      if (err != errNone)
        g_printerr ("Error loading document!\n");
      
      view_show (v);

      ++i;
    }
  
  gtk_main ();
  
  delete globalParams;
  
  return 0;
}
