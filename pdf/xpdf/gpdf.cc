//========================================================================
//
// gpdf.cc
//
// Copyright 1996 Derek B. Noonburg
// Copyright 1999 Miguel de Icaza
// Copyright 1999 Michael Meeks.
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
gint  gpdf_debug;
poptContext ctx;

const struct poptOption gpdf_popt_options [] = {
  { "debug", '\0', POPT_ARG_INT, &gpdf_debug, 0,
    N_("Enables some debugging functions"), N_("LEVEL") },
  { NULL, '\0', 0, NULL, 0 }
};


//------------------------------------------------------------------------
// loadFile / displayPage
//------------------------------------------------------------------------

static GBool loadFile(GString *fileName) {
  GString *title;
  PDFDoc *newDoc;
  char s[20];
  char *p;

  // busy cursor
  if (win)
    win->setBusyCursor(gTrue);

  // open PDF file
  newDoc = new PDFDoc(fileName);
  if (!newDoc->isOk()) {
    delete newDoc;
    if (win)
      win->setBusyCursor(gFalse);
    return gFalse;
  }

  // replace old document
  if (doc)
    delete doc;
  doc = newDoc;

  // nothing displayed yet
  page = -99;

  // init PostScript output params
  if (psFileName)
    delete psFileName;
  if (defPSFileName) {
    psFileName = defPSFileName->copy();
  } else {
    p = fileName->getCString() + fileName->getLength() - 4;
    if (!strcmp(p, ".pdf") || !strcmp(p, ".PDF"))
      psFileName = new GString(fileName->getCString(),
			       fileName->getLength() - 4);
    else
      psFileName = fileName->copy();
    psFileName->append(".ps");
  }
  psFirstPage = 1;
  psLastPage = doc->getNumPages();

  // set up title, number-of-pages display; back to normal cursor
  if (win) {
    title = new GString("xpdf: ");
    title->append(fileName);
    win->setTitle(title);
    sprintf(s, "of %d", doc->getNumPages());
    numPagesLabel->setText(s);
    win->setBusyCursor(gFalse);
  }

  // done
  return gTrue;
}

static void displayPage(int page1, int zoom1, int rotate1) {
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
}

int
main (int argc, char *argv [])
{
  char **view_files = NULL;
  int lp;

  gnome_init_with_popt_table (
    "gpdf", "0.1", argc, argv,
    gpdf_popt_options, 0, &ctx);
  
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

  /* Destroy files */
}
