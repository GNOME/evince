//========================================================================
//
// LTKOutputDev.cc
//
// Copyright 1998 Derek B. Noonburg
//
//========================================================================

#ifdef __GNUC__
#pragma implementation
#endif

#include <stdio.h>
#include <stdlib.h>
#include <stddef.h>
#include <string.h>
#include "gmem.h"
#include "GString.h"
#include "LTKWindow.h"
#include "LTKScrollingCanvas.h"
#include "Object.h"
#include "Stream.h"
#include "GfxState.h"
#include "GfxFont.h"
#include "Error.h"
#include "Params.h"
#include "LTKOutputDev.h"

//------------------------------------------------------------------------

LTKOutputDev::LTKOutputDev(LTKWindow *win1, unsigned long paperColor):
  XOutputDev(win1->getDisplay(),
	     ((LTKScrollingCanvas *)win1->findWidget("canvas"))->getPixmap(),
	     0, win1->getColormap(), paperColor)
{
  win = win1;
  canvas = (LTKScrollingCanvas *)win->findWidget("canvas");
  setPixmap(canvas->getPixmap(),
	    canvas->getRealWidth(), canvas->getRealHeight());
}

LTKOutputDev::~LTKOutputDev() {
}

void LTKOutputDev::startPage(int pageNum, GfxState *state) {
  canvas->resize((int)(state->getPageWidth() + 0.5),
		 (int)(state->getPageHeight() + 0.5));
  setPixmap(canvas->getPixmap(),
	    canvas->getRealWidth(), canvas->getRealHeight());
  XOutputDev::startPage(pageNum, state);
  canvas->redraw();
}

void LTKOutputDev::dump() {
  canvas->redraw();
  XOutputDev::dump();
}
