//========================================================================
//
// XSplashOutputDev.h
//
// Copyright 2003 Glyph & Cog, LLC
//
//========================================================================

#ifndef XSPLASHOUTPUTDEV_H
#define XSPLASHOUTPUTDEV_H

#ifdef USE_GCC_PRAGMAS
#pragma interface
#endif

#include <X11/Xlib.h>
#include "SplashTypes.h"
#include "SplashOutputDev.h"

//------------------------------------------------------------------------

#define xOutMaxRGBCube 6	// max size of RGB color cube

//------------------------------------------------------------------------
// XSplashOutputDev
//------------------------------------------------------------------------

class XSplashOutputDev: public SplashOutputDev {
public:

  XSplashOutputDev(Display *displayA, int screenNumA,
		   Visual *visualA, Colormap colormapA,
		   GBool reverseVideoA, SplashColor paperColorA,
		   GBool installCmapA, int rgbCubeSizeA,
		   GBool incrementalUpdateA,
		   void (*redrawCbkA)(void *data),
		   void *redrawCbkDataA);

  virtual ~XSplashOutputDev();

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
	      Drawable destDrawable, GC destGC,
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

  GBool incrementalUpdate;      // incrementally update the display?
  void (*redrawCbk)(void *data);
  void *redrawCbkData;
  TextPage *text;               // text from the current page

  Display *display;		// X display pointer
  Visual *visual;		// X visual
  Guint depth;			// visual depth
  GBool trueColor;		// set if using a TrueColor visual
  int rDiv, gDiv, bDiv;		// RGB right shifts (for TrueColor)
  int rShift, gShift, bShift;	// RGB left shifts (for TrueColor)
  int rgbCubeSize;		// size of color cube (for non-TrueColor)
  Gulong			// color cube (for non-TrueColor)
    colors[xOutMaxRGBCube * xOutMaxRGBCube * xOutMaxRGBCube];
};

#endif
