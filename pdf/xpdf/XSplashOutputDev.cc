//========================================================================
//
// XSplashOutputDev.cc
//
// Copyright 2003 Glyph & Cog, LLC
//
//========================================================================

#include <aconf.h>

#ifdef USE_GCC_PRAGMAS
#pragma implementation
#endif

#include <X11/Xlib.h>
#include <X11/Xutil.h>
#include "gmem.h"
#include "SplashTypes.h"
#include "SplashBitmap.h"
#include "Object.h"
#include "GfxState.h"
#include "TextOutputDev.h"
#include "XSplashOutputDev.h"

//------------------------------------------------------------------------
// Constants and macros
//------------------------------------------------------------------------

#define xoutRound(x) ((int)(x + 0.5))

//------------------------------------------------------------------------
// XSplashOutputDev
//------------------------------------------------------------------------

XSplashOutputDev::XSplashOutputDev(Display *displayA, int screenNumA,
				   Visual *visualA, Colormap colormapA,
				   GBool reverseVideoA,
				   SplashColor paperColorA,
				   GBool installCmapA, int rgbCubeSizeA,
				   GBool incrementalUpdateA,
				   void (*redrawCbkA)(void *data),
				   void *redrawCbkDataA):
  SplashOutputDev(splashModeRGB8, reverseVideoA, paperColorA)
{
  XVisualInfo visualTempl;
  XVisualInfo *visualList;
  Gulong mask;
  int nVisuals;
  XColor xcolor;
  XColor *xcolors;
  int r, g, b, n, m;
  GBool ok;

  incrementalUpdate = incrementalUpdateA;
  redrawCbk = redrawCbkA;
  redrawCbkData = redrawCbkDataA;

  // create text object
  text = new TextPage(gFalse);

  //----- set up the X color stuff

  display = displayA;
  visual = visualA;

  // check for TrueColor visual
  //~ this should scan the list, not just look at the first one
  visualTempl.visualid = XVisualIDFromVisual(visual);
  visualList = XGetVisualInfo(display, VisualIDMask,
			      &visualTempl, &nVisuals);
  if (nVisuals < 1) {
    // this shouldn't happen
    XFree((XPointer)visualList);
    visualList = XGetVisualInfo(display, VisualNoMask, &visualTempl,
				&nVisuals);
  }
  depth = visualList->depth;
  if (visualList->c_class == TrueColor) {
    trueColor = gTrue;
    for (mask = visualList->red_mask, rShift = 0;
	 mask && !(mask & 1);
	 mask >>= 1, ++rShift) ;
    for (rDiv = 8; mask; mask >>= 1, --rDiv) ;
    for (mask = visualList->green_mask, gShift = 0;
	 mask && !(mask & 1);
	 mask >>= 1, ++gShift) ;
    for (gDiv = 8; mask; mask >>= 1, --gDiv) ;
    for (mask = visualList->blue_mask, bShift = 0;
	 mask && !(mask & 1);
	 mask >>= 1, ++bShift) ;
    for (bDiv = 8; mask; mask >>= 1, --bDiv) ;
  } else {
    trueColor = gFalse;
  }
  XFree((XPointer)visualList);

  // allocate a color cube
  if (!trueColor) {

    // set colors in private colormap
    if (installCmapA) {
      for (rgbCubeSize = xOutMaxRGBCube; rgbCubeSize >= 2; --rgbCubeSize) {
	m = rgbCubeSize * rgbCubeSize * rgbCubeSize;
	if (XAllocColorCells(display, colormapA, False, NULL, 0, colors, m)) {
	  break;
	}
      }
      if (rgbCubeSize >= 2) {
	m = rgbCubeSize * rgbCubeSize * rgbCubeSize;
	xcolors = (XColor *)gmalloc(m * sizeof(XColor));
	n = 0;
	for (r = 0; r < rgbCubeSize; ++r) {
	  for (g = 0; g < rgbCubeSize; ++g) {
	    for (b = 0; b < rgbCubeSize; ++b) {
	      xcolors[n].pixel = colors[n];
	      xcolors[n].red = (r * 65535) / (rgbCubeSize - 1);
	      xcolors[n].green = (g * 65535) / (rgbCubeSize - 1);
	      xcolors[n].blue = (b * 65535) / (rgbCubeSize - 1);
	      xcolors[n].flags = DoRed | DoGreen | DoBlue;
	      ++n;
	    }
	  }
	}
	XStoreColors(display, colormapA, xcolors, m);
	gfree(xcolors);
      } else {
	rgbCubeSize = 1;
	colors[0] = BlackPixel(display, screenNumA);
	colors[1] = WhitePixel(display, screenNumA);
      }

    // allocate colors in shared colormap
    } else {
      if (rgbCubeSize > xOutMaxRGBCube) {
	rgbCubeSize = xOutMaxRGBCube;
      }
      ok = gFalse;
      for (rgbCubeSize = rgbCubeSizeA; rgbCubeSize >= 2; --rgbCubeSize) {
	ok = gTrue;
	n = 0;
	for (r = 0; r < rgbCubeSize && ok; ++r) {
	  for (g = 0; g < rgbCubeSize && ok; ++g) {
	    for (b = 0; b < rgbCubeSize && ok; ++b) {
	      if (n == 0) {
		colors[n] = BlackPixel(display, screenNumA);
		++n;
	      } else {
		xcolor.red = (r * 65535) / (rgbCubeSize - 1);
		xcolor.green = (g * 65535) / (rgbCubeSize - 1);
		xcolor.blue = (b * 65535) / (rgbCubeSize - 1);
		if (XAllocColor(display, colormapA, &xcolor)) {
		  colors[n++] = xcolor.pixel;
		} else {
		  ok = gFalse;
		}
	      }
	    }
	  }
	}
	if (ok) {
	  break;
	}
	XFreeColors(display, colormapA, &colors[1], n-1, 0);
      }
      if (!ok) {
	rgbCubeSize = 1;
	colors[0] = BlackPixel(display, screenNumA);
	colors[1] = WhitePixel(display, screenNumA);
      }
    }
  }
}

XSplashOutputDev::~XSplashOutputDev() {
  delete text;
}

void XSplashOutputDev::drawChar(GfxState *state, double x, double y,
				double dx, double dy,
				double originX, double originY,
				CharCode code, Unicode *u, int uLen) {
  text->addChar(state, x, y, dx, dy, code, u, uLen);
  SplashOutputDev::drawChar(state, x, y, dx, dy, originX, originY,
			    code, u, uLen);
}

GBool XSplashOutputDev::beginType3Char(GfxState *state, double x, double y,
				       double dx, double dy,
				       CharCode code, Unicode *u, int uLen) {
  text->addChar(state, x, y, dx, dy, code, u, uLen);
  return SplashOutputDev::beginType3Char(state, x, y, dx, dy, code, u, uLen);
}

void XSplashOutputDev::clear() {
  startDoc(NULL);
  startPage(0, NULL);
}

void XSplashOutputDev::startPage(int pageNum, GfxState *state) {
  SplashOutputDev::startPage(pageNum, state);
  text->startPage(state);
}

void XSplashOutputDev::endPage() {
  SplashOutputDev::endPage();
  if (!incrementalUpdate) {
    (*redrawCbk)(redrawCbkData);
  }
  text->coalesce(gTrue);
}

void XSplashOutputDev::dump() {
  if (incrementalUpdate) {
    (*redrawCbk)(redrawCbkData);
  }
}

void XSplashOutputDev::updateFont(GfxState *state) {
  SplashOutputDev::updateFont(state);
  text->updateFont(state);
}

void XSplashOutputDev::redraw(int srcX, int srcY,
			      Drawable destDrawable, GC destGC,
			      int destX, int destY,
			      int width, int height) {
  XImage *image;
  SplashColorPtr dataPtr;
  SplashRGB8 *p;
  SplashRGB8 rgb;
  Gulong pixel;
  int bw, x, y, r, g, b, gray;

  //~ allocate this image once (whenever the window changes size)
  //~ use XShm
  image = XCreateImage(display, visual, depth, ZPixmap, 0, NULL,
		       width, height, 8, 0);
  image->data = (char *)gmalloc(height * image->bytes_per_line);

  //~ optimize for known XImage formats
  bw = getBitmap()->getWidth();
  dataPtr = getBitmap()->getDataPtr();

  if (trueColor) {
    for (y = 0; y < height; ++y) {
      p = dataPtr.rgb8 + (y + srcY) * bw + srcX;
      for (x = 0; x < width; ++x) {
	rgb = *p++;
	r = splashRGB8R(rgb) >> rDiv;
	g = splashRGB8G(rgb) >> gDiv;
	b = splashRGB8B(rgb) >> bDiv;
	pixel = ((Gulong)r << rShift) +
	        ((Gulong)g << gShift) +
	        ((Gulong)b << bShift);
	XPutPixel(image, x, y, pixel);
      }
    }
  } else if (rgbCubeSize == 1) {
    //~ this should really use splashModeMono, with non-clustered dithering
    for (y = 0; y < height; ++y) {
      p = dataPtr.rgb8 + (y + srcY) * bw + srcX;
      for (x = 0; x < width; ++x) {
	rgb = *p++;
	gray = xoutRound(0.299 * splashRGB8R(rgb) +
			 0.587 * splashRGB8G(rgb) +
			 0.114 * splashRGB8B(rgb));
	if (gray < 128) {
	  pixel = colors[0];
	} else {
	  pixel = colors[1];
	}
	XPutPixel(image, x, y, pixel);
      }
    }
  } else {
    for (y = 0; y < height; ++y) {
      p = dataPtr.rgb8 + (y + srcY) * bw + srcX;
      for (x = 0; x < width; ++x) {
	rgb = *p++;
	r = (splashRGB8R(rgb) * (rgbCubeSize - 1)) / 255;
	g = (splashRGB8G(rgb) * (rgbCubeSize - 1)) / 255;
	b = (splashRGB8B(rgb) * (rgbCubeSize - 1)) / 255;
	pixel = colors[(r * rgbCubeSize + g) * rgbCubeSize + b];
	XPutPixel(image, x, y, pixel);
      }
    }
  }

  XPutImage(display, destDrawable, destGC, image,
	    0, 0, destX, destY, width, height);

  gfree(image->data);
  image->data = NULL;
  XDestroyImage(image);
}

GBool XSplashOutputDev::findText(Unicode *s, int len,
				 GBool startAtTop, GBool stopAtBottom,
				 GBool startAtLast, GBool stopAtLast,
				 int *xMin, int *yMin,
				 int *xMax, int *yMax) {
  double xMin1, yMin1, xMax1, yMax1;
  
  xMin1 = (double)*xMin;
  yMin1 = (double)*yMin;
  xMax1 = (double)*xMax;
  yMax1 = (double)*yMax;
  if (text->findText(s, len, startAtTop, stopAtBottom,
		     startAtLast, stopAtLast,
		     &xMin1, &yMin1, &xMax1, &yMax1)) {
    *xMin = xoutRound(xMin1);
    *xMax = xoutRound(xMax1);
    *yMin = xoutRound(yMin1);
    *yMax = xoutRound(yMax1);
    return gTrue;
  }
  return gFalse;
}

GString *XSplashOutputDev::getText(int xMin, int yMin, int xMax, int yMax) {
  return text->getText((double)xMin, (double)yMin,
		       (double)xMax, (double)yMax);
}
