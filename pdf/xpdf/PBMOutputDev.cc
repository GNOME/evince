//========================================================================
//
// PBMOutputDev.cc
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
#include "Object.h"
#include "Stream.h"
#include "GfxState.h"
#include "GfxFont.h"
#include "Error.h"
#include "Params.h"
#include "PBMOutputDev.h"

//------------------------------------------------------------------------

PBMOutputDev *PBMOutputDev::makePBMOutputDev(char *displayName,
					     char *fileRoot1) {
  Display *display;
  Pixmap pixmap;
  Window dummyWin;
  int screen;
  int invert;
  unsigned long black, white;
  PBMOutputDev *out;

  if (!(display = XOpenDisplay(displayName))) {
    fprintf(stderr, "Couldn't open display '%s'\n", displayName);
    exit(1);
  }
  screen = DefaultScreen(display);

  black = BlackPixel(display, screen);
  white = WhitePixel(display, screen);
  if ((black & 1) == (white & 1)) {
    fprintf(stderr, "Weird black/white pixel colors\n");
    XCloseDisplay(display);
    return NULL;
  } 
  invert = (white & 1) == 1 ? 0xff : 0x00;

  dummyWin = XCreateSimpleWindow(display, RootWindow(display, screen),
				 0, 0, 1, 1, 0,
				 black, white);
  pixmap = XCreatePixmap(display, dummyWin, 1, 1, 1);
  out = new PBMOutputDev(display, screen, pixmap, dummyWin,
			 invert, fileRoot1);
  out->startDoc();
  return out;
}

void PBMOutputDev::killPBMOutputDev(PBMOutputDev *out) {
  Display *display;
  Pixmap pixmap;
  Window dummyWin;

  display = out->display;
  pixmap = out->pixmap;
  dummyWin = out->dummyWin;

  delete out;

  // these have to be done *after* the XOutputDev (parent of the
  // PBMOutputDev) is deleted, since XOutputDev::~XOutputDev() needs
  // them
  XFreePixmap(display, pixmap);
  XDestroyWindow(display, dummyWin);
  XCloseDisplay(display);
}

PBMOutputDev::PBMOutputDev(Display *display1, int screen1,
			   Pixmap pixmap1, Window dummyWin1,
			   int invert1, char *fileRoot1):
  XOutputDev(display1, pixmap1, 1,
	     DefaultColormap(display1, screen1),
	     WhitePixel(display1, DefaultScreen(display1)))
{
  display = display1;
  screen = screen1;
  pixmap = pixmap1;
  dummyWin = dummyWin1;
  invert = invert1;
  fileRoot = fileRoot1;
  fileName = (char *)gmalloc(strlen(fileRoot) + 20);
}

PBMOutputDev::~PBMOutputDev() {
  gfree(fileName);
}

void PBMOutputDev::startPage(int pageNum, GfxState *state) {

  curPage = pageNum;
  width = (int)(state->getPageWidth() + 0.5);
  height = (int)(state->getPageHeight() + 0.5);
  XFreePixmap(display, pixmap);
  pixmap = XCreatePixmap(display, dummyWin, width, height, 1);
  setPixmap(pixmap, width, height);
  XOutputDev::startPage(pageNum, state);
}

void PBMOutputDev::endPage() {
  XImage *image;
  FILE *f;
  int p;
  int x, y, i;

  image = XCreateImage(display, DefaultVisual(display, screen),
		       1, ZPixmap, 0, NULL, width, height, 8, 0);
  image->data = (char *)gmalloc(height * image->bytes_per_line);
  XGetSubImage(display, pixmap, 0, 0, width, height, 1, ZPixmap,
	       image, 0, 0);

  sprintf(fileName, "%s-%06d.pbm", fileRoot, curPage);
  if (!(f = fopen(fileName, "wb"))) {
    fprintf(stderr, "Couldn't open output file '%s'\n", fileName);
    goto err;
  }
  fprintf(f, "P4\n");
  fprintf(f, "%d %d\n", width, height);

  for (y = 0; y < height; ++y) {
    for (x = 0; x+8 <= width; x += 8) {
      p = 0;
      for (i = 0; i < 8; ++i)
	p = (p << 1) + (XGetPixel(image, x+i, y) & 1);
      p ^= invert;
      fputc((char)p, f);
    }
    if (width & 7) {
      p = 0;
      for (i = 0; i < (width & 7); ++i)
	p = (p << 1) + (XGetPixel(image, x+i, y) & 1);
      p <<= 8 - (width & 7);
      p ^= invert;
      fputc((char)p, f);
    }
  }

  fclose(f);

 err:
  gfree(image->data);
  image->data = NULL;
  XDestroyImage(image);

  XOutputDev::endPage();
}
