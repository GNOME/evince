//========================================================================
//
// XPixmapOutputDev.cc
//
// Copyright 2002-2003 Glyph & Cog, LLC
//
//========================================================================

#include <aconf.h>

#ifdef USE_GCC_PRAGMAS
#pragma implementation
#endif

#include "Object.h"
#include "GfxState.h"
#include "XPixmapOutputDev.h"

//------------------------------------------------------------------------

#define xoutRound(x) ((int)(x + 0.5))

//------------------------------------------------------------------------

XPixmapOutputDev::XPixmapOutputDev(Display *displayA, int screenNumA,
				   Visual *visualA, Colormap colormapA,
				   GBool reverseVideoA, Gulong paperColorA,
				   GBool installCmapA, int rgbCubeSizeA,
				   GBool incrementalUpdateA,
				   void (*redrawCbkA)(void *data),
				   void *redrawCbkDataA):
  XOutputDev(displayA, screenNumA, visualA, colormapA,
	     reverseVideoA, paperColorA, installCmapA, rgbCubeSizeA)
{
  incrementalUpdate = incrementalUpdateA;
  redrawCbk = redrawCbkA;
  redrawCbkData = redrawCbkDataA;
}

XPixmapOutputDev::~XPixmapOutputDev() {
  if (getPixmapWidth() > 0) {
    XFreePixmap(getDisplay(), getPixmap());
  }
}

void XPixmapOutputDev::clear() {
  startDoc(NULL);
  startPage(0, NULL);
}

void XPixmapOutputDev::startPage(int pageNum, GfxState *state) {
  int oldPixmapW, oldPixmapH, newPixmapW, newPixmapH;

  // resize the off-screen pixmap (if needed)
  oldPixmapW = getPixmapWidth();
  oldPixmapH = getPixmapHeight();
  newPixmapW = xoutRound(state ? state->getPageWidth() : 1);
  newPixmapH = xoutRound(state ? state->getPageHeight() : 1);
  if (oldPixmapW == 0 ||
      newPixmapW != oldPixmapW || newPixmapH != oldPixmapH) {
    if (oldPixmapW > 0) {
      XFreePixmap(getDisplay(), getPixmap());
    }
    setPixmap(XCreatePixmap(getDisplay(), win, newPixmapW, newPixmapH,
			    getDepth()),
	      newPixmapW, newPixmapH);
  }

  XOutputDev::startPage(pageNum, state);
}

void XPixmapOutputDev::endPage() {
  if (!incrementalUpdate) {
    (*redrawCbk)(redrawCbkData);
  }
  XOutputDev::endPage();
}

void XPixmapOutputDev::dump() {
  if (incrementalUpdate) {
    (*redrawCbk)(redrawCbkData);
  }
  XOutputDev::dump();
}
