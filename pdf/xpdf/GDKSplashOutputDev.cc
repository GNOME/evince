//========================================================================
//
// GDKSplashOutputDev.cc
//
// Copyright 2003 Glyph & Cog, LLC
// Copyright 2004 Red Hat, Inc. (GDK port)
//
//========================================================================

#include <aconf.h>

#ifdef USE_GCC_PRAGMAS
#pragma implementation
#endif

#include "gmem.h"
#include "SplashTypes.h"
#include "SplashBitmap.h"
#include "Object.h"
#include "GfxState.h"
#include "TextOutputDev.h"
#include "GDKSplashOutputDev.h"

//------------------------------------------------------------------------
// Constants and macros
//------------------------------------------------------------------------

#define xoutRound(x) ((int)(x + 0.5))

//------------------------------------------------------------------------
// GDKSplashOutputDev
//------------------------------------------------------------------------

static SplashColor makeSplashColor(int r, int g, int b)
{
  SplashColor c;
  c.rgb8 = splashMakeRGB8 (r, g, b);
  return c;
}

GDKSplashOutputDev::GDKSplashOutputDev(GdkScreen *screen,
                                       void (*redrawCbkA)(void *data),
                                       void *redrawCbkDataA):
  SplashOutputDev(splashModeRGB8, gFalse, makeSplashColor (255,255,255)),
  incrementalUpdate (1)
{
  redrawCbk = redrawCbkA;
  redrawCbkData = redrawCbkDataA;

  // create text object
  text = new TextPage(gFalse);
}

GDKSplashOutputDev::~GDKSplashOutputDev() {
  delete text;
}

void GDKSplashOutputDev::drawChar(GfxState *state, double x, double y,
                                  double dx, double dy,
                                  double originX, double originY,
                                  CharCode code, Unicode *u, int uLen) {
  text->addChar(state, x, y, dx, dy, code, u, uLen);
  SplashOutputDev::drawChar(state, x, y, dx, dy, originX, originY,
			    code, u, uLen);
}

GBool GDKSplashOutputDev::beginType3Char(GfxState *state, double x, double y,
				       double dx, double dy,
				       CharCode code, Unicode *u, int uLen) {
  text->addChar(state, x, y, dx, dy, code, u, uLen);
  return SplashOutputDev::beginType3Char(state, x, y, dx, dy, code, u, uLen);
}

void GDKSplashOutputDev::clear() {
  startDoc(NULL);
  startPage(0, NULL);
}

void GDKSplashOutputDev::startPage(int pageNum, GfxState *state) {
  SplashOutputDev::startPage(pageNum, state);
  text->startPage(state);
}

void GDKSplashOutputDev::endPage() {
  SplashOutputDev::endPage();
  if (!incrementalUpdate) {
    (*redrawCbk)(redrawCbkData);
  }
  text->coalesce(gTrue);
}

void GDKSplashOutputDev::dump() {
  if (incrementalUpdate && redrawCbk) {
    (*redrawCbk)(redrawCbkData);
  }
}

void GDKSplashOutputDev::updateFont(GfxState *state) {
  SplashOutputDev::updateFont(state);
  text->updateFont(state);
}

void GDKSplashOutputDev::redraw(int srcX, int srcY,
                                GdkDrawable *drawable,
                                int destX, int destY,
                                int width, int height) {
  SplashColorPtr dataPtr;
  GdkGC *gc;
  guchar *gdk_buf;
  int gdk_rowstride;
  int bw, x, y, r, g, b;

  gdk_rowstride = width * 3;
  
  /* better to draw nothing than crash on a huge image */
  gdk_buf = (guchar*) g_try_malloc (height * gdk_rowstride);
  if (gdk_buf == NULL)
    return;
  
  bw = getBitmap()->getWidth();
  dataPtr = getBitmap()->getDataPtr();
  
  for (y = 0; y < height; ++y)
    {
      SplashRGB8 *p;
      SplashRGB8 rgb;
      guchar *gdk_p;
      
      p = dataPtr.rgb8 + (y + srcY) * bw + srcX;
      gdk_p = gdk_buf + y * gdk_rowstride;
      for (x = 0; x < width; ++x)
        {
          rgb = *p++;
          r = splashRGB8R(rgb);
          g = splashRGB8G(rgb);
          b = splashRGB8B(rgb);

          *gdk_p++ = r;
          *gdk_p++ = g;
          *gdk_p++ = b;
        }
    }
  
  gc = gdk_gc_new (drawable);
  
  gdk_draw_rgb_image (drawable, gc,
                      destX, destY,
                      width, height,
                      GDK_RGB_DITHER_NORMAL,
                      gdk_buf,
                      gdk_rowstride);

  g_object_unref (gc);

  g_free (gdk_buf);
}

void GDKSplashOutputDev::drawToPixbuf(GdkPixbuf *pixbuf, int pageNum) {
	
}

GBool GDKSplashOutputDev::findText(Unicode *s, int len,
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

GString *GDKSplashOutputDev::getText(int xMin, int yMin, int xMax, int yMax) {
  return text->getText((double)xMin, (double)yMin,
		       (double)xMax, (double)yMax);
}
