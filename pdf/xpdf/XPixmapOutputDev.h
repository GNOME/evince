//========================================================================
//
// XPixmapOutputDev.h
//
// Copyright 2002-2003 Glyph & Cog, LLC
//
//========================================================================

#ifndef XPIXMAPOUTPUTDEV_H
#define XPIXMAPOUTPUTDEV_H

#include <aconf.h>

#ifdef USE_GCC_PRAGMAS
#pragma interface
#endif

#include <X11/Xlib.h>
#include "XOutputDev.h"

//------------------------------------------------------------------------

class XPixmapOutputDev: public XOutputDev {
public:

  XPixmapOutputDev(Display *displayA, int screenNumA,
		   Visual *visualA, Colormap colormapA,
		   GBool reverseVideoA, Gulong paperColorA,
		   GBool installCmapA, int rgbCubeSizeA,
		   GBool incrementalUpdateA,
		   void (*redrawCbkA)(void *data),
		   void *redrawCbkDataA);

  ~XPixmapOutputDev();

  //----- initialization and control

  // Start a page.
  virtual void startPage(int pageNum, GfxState *state);

  // End a page.
  virtual void endPage();

  // Dump page contents to display.
  virtual void dump();

  //----- special access

  // Set the window - this is used only to create a compatible pixmap.
  void setWindow(Window winA) { win = winA; }

  // Clear out the document (used when displaying an empty window).
  void clear();

private:

  GBool incrementalUpdate;	// incrementally update the display?
  void (*redrawCbk)(void *data);
  void *redrawCbkData;
  Window win;
};

#endif
