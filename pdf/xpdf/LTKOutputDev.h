//========================================================================
//
// LTKOutputDev.h
//
// Copyright 1998 Derek B. Noonburg
//
//========================================================================

#ifndef LTKOUTPUTDEV_H
#define LTKOUTPUTDEV_H

#ifdef __GNUC__
#pragma interface
#endif

#include <stddef.h>
#include "config.h"
#include "XOutputDev.h"

class LTKApp;
class LTKWindow;

//------------------------------------------------------------------------

class LTKOutputDev: public XOutputDev {
public:

  LTKOutputDev(LTKWindow *win1, unsigned long paperColor);

  ~LTKOutputDev();

  //----- initialization and control

  // Start a page.
  virtual void startPage(int pageNum, GfxState *state);

  // Dump page contents to display.
  virtual void dump();

private:

  LTKWindow *win;		// window
  LTKScrollingCanvas *canvas;	// drawing canvas
};

#endif
