//========================================================================
//
// GDKSplashOutputDev.h
//
// Copyright 2003 Glyph & Cog, LLC
// Copyright 2004 Red Hat, Inc. (GDK port)
//
//========================================================================

#ifndef XSPLASHOUTPUTDEV_H
#define XSPLASHOUTPUTDEV_H

#ifdef USE_GCC_PRAGMAS
#pragma interface
#endif

#include "gpdf-g-switch.h"
#  include <gdk/gdk.h>
#include "gpdf-g-switch.h"
#include "SplashTypes.h"
#include "SplashOutputDev.h"
#include "TextOutputDev.h"

//------------------------------------------------------------------------

#define xOutMaxRGBCube 6	// max size of RGB color cube

//------------------------------------------------------------------------
// GDKSplashOutputDev
//------------------------------------------------------------------------

class GDKSplashOutputDev: public SplashOutputDev {
public:

  GDKSplashOutputDev(GdkScreen *screen,
                     void (*redrawCbkA)(void *data),
                     void *redrawCbkDataA);
  
  virtual ~GDKSplashOutputDev();

  //----- initialization and control

  // Start a page.
  virtual void startPage(int pageNum, GfxState *state);

  // End a page.
  virtual void endPage();

  // Dump page contents to display.
  virtual void dump();

  //----- update text state
  virtual void updateFont(GfxState *state);

  //----- text drawing
  virtual void drawChar(GfxState *state, double x, double y,
			double dx, double dy,
			double originX, double originY,
			CharCode code, Unicode *u, int uLen);
  virtual GBool beginType3Char(GfxState *state, double x, double y,
			       double dx, double dy,
			       CharCode code, Unicode *u, int uLen);

  //----- special access

  // Clear out the document (used when displaying an empty window).
  void clear();

  // Copy the rectangle (srcX, srcY, width, height) to (destX, destY)
  // in destDC.
  void redraw(int srcX, int srcY,
              GdkDrawable *drawable,
	      int destX, int destY,
	      int width, int height);

  // Find a string.  If <startAtTop> is true, starts looking at the
  // top of the page; else if <startAtLast> is true, starts looking
  // immediately after the last find result; else starts looking at
  // <xMin>,<yMin>.  If <stopAtBottom> is true, stops looking at the
  // bottom of the page; else if <stopAtLast> is true, stops looking
  // just before the last find result; else stops looking at
  // <xMax>,<yMax>.
  GBool findText(Unicode *s, int len,
		 GBool startAtTop, GBool stopAtBottom,
		 GBool startAtLast, GBool stopAtLast,
		 int *xMin, int *yMin,
		 int *xMax, int *yMax);

  // Get the text which is inside the specified rectangle.
  GString *getText(int xMin, int yMin, int xMax, int yMax);

private:

  int incrementalUpdate;
  void (*redrawCbk)(void *data);
  void *redrawCbkData;
  TextPage *text;               // text from the current page
};

#endif
