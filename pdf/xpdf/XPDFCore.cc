//========================================================================
//
// XPDFCore.cc
//
// Copyright 2002-2003 Glyph & Cog, LLC
//
//========================================================================

#include <aconf.h>

#ifdef USE_GCC_PRAGMAS
#pragma implementation
#endif

#include <X11/keysym.h>
#include <X11/cursorfont.h>
#include <string.h>
#include "gmem.h"
#include "GString.h"
#include "GList.h"
#include "Error.h"
#include "GlobalParams.h"
#include "PDFDoc.h"
#include "ErrorCodes.h"
#include "GfxState.h"
#include "PSOutputDev.h"
#include "TextOutputDev.h"
#include "XPixmapOutputDev.h"
#include "XPDFCore.h"

// these macro defns conflict with xpdf's Object class
#ifdef LESSTIF_VERSION
#undef XtDisplay
#undef XtScreen
#undef XtWindow
#undef XtParent
#undef XtIsRealized
#endif

// hack around old X includes which are missing these symbols
#ifndef XK_Page_Up
#define XK_Page_Up              0xFF55
#endif
#ifndef XK_Page_Down
#define XK_Page_Down            0xFF56
#endif
#ifndef XK_KP_Home
#define XK_KP_Home              0xFF95
#endif
#ifndef XK_KP_Left
#define XK_KP_Left              0xFF96
#endif
#ifndef XK_KP_Up
#define XK_KP_Up                0xFF97
#endif
#ifndef XK_KP_Right
#define XK_KP_Right             0xFF98
#endif
#ifndef XK_KP_Down
#define XK_KP_Down              0xFF99
#endif
#ifndef XK_KP_Prior
#define XK_KP_Prior             0xFF9A
#endif
#ifndef XK_KP_Page_Up
#define XK_KP_Page_Up           0xFF9A
#endif
#ifndef XK_KP_Next
#define XK_KP_Next              0xFF9B
#endif
#ifndef XK_KP_Page_Down
#define XK_KP_Page_Down         0xFF9B
#endif
#ifndef XK_KP_End
#define XK_KP_End               0xFF9C
#endif
#ifndef XK_KP_Begin
#define XK_KP_Begin             0xFF9D
#endif
#ifndef XK_KP_Insert
#define XK_KP_Insert            0xFF9E
#endif
#ifndef XK_KP_Delete
#define XK_KP_Delete            0xFF9F
#endif

//------------------------------------------------------------------------

#define highlightNone     0
#define highlightNormal   1
#define highlightSelected 2

//------------------------------------------------------------------------

GString *XPDFCore::currentSelection = NULL;
XPDFCore *XPDFCore::currentSelectionOwner = NULL;
Atom XPDFCore::targetsAtom;

//------------------------------------------------------------------------
// XPDFCore
//------------------------------------------------------------------------

XPDFCore::XPDFCore(Widget shellA, Widget parentWidgetA,
		   Gulong paperColorA, GBool fullScreenA, GBool reverseVideo,
		   GBool installCmap, int rgbCubeSize) {
  GString *initialZoom;
  int i;

  shell = shellA;
  parentWidget = parentWidgetA;
  display = XtDisplay(parentWidget);
  screenNum = XScreenNumberOfScreen(XtScreen(parentWidget));
  targetsAtom = XInternAtom(display, "TARGETS", False);

  paperColor = paperColorA;
  fullScreen = fullScreenA;

  // for some reason, querying XmNvisual doesn't work (even if done
  // after the window is mapped)
  visual = DefaultVisual(display, screenNum);
  XtVaGetValues(shell, XmNcolormap, &colormap, NULL);

  scrolledWin = NULL;
  hScrollBar = NULL;
  vScrollBar = NULL;
  drawAreaFrame = NULL;
  drawArea = NULL;
  out = NULL;

  doc = NULL;
  page = 0;
  rotate = 0;

  // get the initial zoom value
  initialZoom = globalParams->getInitialZoom();
  if (!initialZoom->cmp("page")) {
    zoom = zoomPage;
  } else if (!initialZoom->cmp("width")) {
    zoom = zoomWidth;
  } else {
    zoom = atoi(initialZoom->getCString());
    if (zoom <= 0) {
      zoom = defZoom;
    }
  }
  delete initialZoom;

  scrollX = 0;
  scrollY = 0;
  linkAction = NULL;
  selectXMin = selectXMax = 0;
  selectYMin = selectYMax = 0;
  dragging = gFalse;
  lastDragLeft = lastDragTop = gTrue;

  panning = gFalse;


  updateCbk = NULL;
  actionCbk = NULL;
  keyPressCbk = NULL;
  mouseCbk = NULL;
  reqPasswordCbk = NULL;

  // no history yet
  historyCur = xpdfHistorySize - 1;
  historyBLen = historyFLen = 0;
  for (i = 0; i < xpdfHistorySize; ++i) {
    history[i].fileName = NULL;
  }

  // optional features default to on
  hyperlinksEnabled = gTrue;
  selectEnabled = gTrue;

  // do X-specific initialization and create the widgets
  initWindow();

  // create the OutputDev
  out = new XPixmapOutputDev(display, screenNum, visual, colormap,
			     reverseVideo, paperColor,
			     installCmap, rgbCubeSize, gTrue,
			     &outputDevRedrawCbk, this);
  out->startDoc(NULL);
}

XPDFCore::~XPDFCore() {
  int i;

  if (out) {
    delete out;
  }
  if (doc) {
    delete doc;
  }
  if (currentSelectionOwner == this && currentSelection) {
    delete currentSelection;
    currentSelection = NULL;
    currentSelectionOwner = NULL;
  }
  for (i = 0; i < xpdfHistorySize; ++i) {
    if (history[i].fileName) {
      delete history[i].fileName;
    }
  }
  if (selectGC) {
    XFreeGC(display, selectGC);
    XFreeGC(display, highlightGC);
  }
  if (drawAreaGC) {
    XFreeGC(display, drawAreaGC);
  }
  if (scrolledWin) {
    XtDestroyWidget(scrolledWin);
  }
  if (busyCursor) {
    XFreeCursor(display, busyCursor);
  }
  if (linkCursor) {
    XFreeCursor(display, linkCursor);
  }
  if (selectCursor) {
    XFreeCursor(display, selectCursor);
  }
}

//------------------------------------------------------------------------
// loadFile / displayPage / displayDest
//------------------------------------------------------------------------

int XPDFCore::loadFile(GString *fileName, GString *ownerPassword,
		       GString *userPassword) {
  PDFDoc *newDoc;
  GString *password;
  GBool again;
  int err;

  // busy cursor
  setCursor(busyCursor);

  // open the PDF file
  newDoc = new PDFDoc(fileName->copy(), ownerPassword, userPassword);
  if (!newDoc->isOk()) {
    err = newDoc->getErrorCode();
    delete newDoc;
    if (err != errEncrypted || !reqPasswordCbk) {
      setCursor(None);
      return err;
    }

    // try requesting a password
    again = ownerPassword != NULL || userPassword != NULL;
    while (1) {
      if (!(password = (*reqPasswordCbk)(reqPasswordCbkData, again))) {
	setCursor(None);
	return errEncrypted;
      }
      newDoc = new PDFDoc(fileName->copy(), password, password);
      if (newDoc->isOk()) {
	break;
      }
      err = newDoc->getErrorCode();
      delete newDoc;
      if (err != errEncrypted) {
	setCursor(None);
	return err;
      }
      again = gTrue;
    }
  }

  // replace old document
  if (doc) {
    delete doc;
  }
  doc = newDoc;
  if (out) {
    out->startDoc(doc->getXRef());
  }

  // nothing displayed yet
  page = -99;

  // save the modification time
  modTime = getModTime(doc->getFileName()->getCString());

  // update the parent window
  if (updateCbk) {
    (*updateCbk)(updateCbkData, doc->getFileName(), -1,
		 doc->getNumPages(), NULL);
  }

  // back to regular cursor
  setCursor(None);

  return errNone;
}

int XPDFCore::loadFile(BaseStream *stream, GString *ownerPassword,
		       GString *userPassword) {
  PDFDoc *newDoc;
  GString *password;
  GBool again;
  int err;

  // busy cursor
  setCursor(busyCursor);

  // open the PDF file
  newDoc = new PDFDoc(stream, ownerPassword, userPassword);
  if (!newDoc->isOk()) {
    err = newDoc->getErrorCode();
    delete newDoc;
    if (err != errEncrypted || !reqPasswordCbk) {
      setCursor(None);
      return err;
    }

    // try requesting a password
    again = ownerPassword != NULL || userPassword != NULL;
    while (1) {
      if (!(password = (*reqPasswordCbk)(reqPasswordCbkData, again))) {
	setCursor(None);
	return errEncrypted;
      }
      newDoc = new PDFDoc(stream, password, password);
      if (newDoc->isOk()) {
	break;
      }
      err = newDoc->getErrorCode();
      delete newDoc;
      if (err != errEncrypted) {
	setCursor(None);
	return err;
      }
      again = gTrue;
    }
  }

  // replace old document
  if (doc) {
    delete doc;
  }
  doc = newDoc;
  if (out) {
    out->startDoc(doc->getXRef());
  }

  // nothing displayed yet
  page = -99;

  // save the modification time
  modTime = getModTime(doc->getFileName()->getCString());

  // update the parent window
  if (updateCbk) {
    (*updateCbk)(updateCbkData, doc->getFileName(), -1,
		 doc->getNumPages(), NULL);
  }

  // back to regular cursor
  setCursor(None);

  return errNone;
}

void XPDFCore::resizeToPage(int pg) {
  Dimension width, height;
  double width1, height1;
  Dimension topW, topH, topBorder, daW, daH;
  Dimension displayW, displayH;

  displayW = DisplayWidth(display, screenNum);
  displayH = DisplayHeight(display, screenNum);
  if (fullScreen) {
    width = displayW;
    height = displayH;
  } else {
    if (pg < 0 || pg > doc->getNumPages()) {
      width1 = 612;
      height1 = 792;
    } else if (doc->getPageRotate(pg) == 90 ||
	       doc->getPageRotate(pg) == 270) {
      width1 = doc->getPageHeight(pg);
      height1 = doc->getPageWidth(pg);
    } else {
      width1 = doc->getPageWidth(pg);
      height1 = doc->getPageHeight(pg);
    }
    if (zoom == zoomPage || zoom == zoomWidth) {
      width = (Dimension)(width1 * 0.01 * defZoom + 0.5);
      height = (Dimension)(height1 * 0.01 * defZoom + 0.5);
    } else {
      width = (Dimension)(width1 * 0.01 * zoom + 0.5);
      height = (Dimension)(height1 * 0.01 * zoom + 0.5);
    }
    if (width > displayW - 100) {
      width = displayW - 100;
    }
    if (height > displayH - 150) {
      height = displayH - 150;
    }
  }

  if (XtIsRealized(shell)) {
    XtVaGetValues(shell, XmNwidth, &topW, XmNheight, &topH,
		  XmNborderWidth, &topBorder, NULL);
    XtVaGetValues(drawArea, XmNwidth, &daW, XmNheight, &daH, NULL);
    XtVaSetValues(shell, XmNwidth, width + (topW - daW),
		  XmNheight, height + (topH - daH), NULL);
  } else {
    XtVaSetValues(drawArea, XmNwidth, width, XmNheight, height, NULL);
  }
}

void XPDFCore::clear() {
  if (!doc) {
    return;
  }

  // no document
  delete doc;
  doc = NULL;
  out->clear();

  // no page displayed
  page = -99;

  // redraw
  scrollX = scrollY = 0;
  updateScrollBars();
  redrawRectangle(scrollX, scrollY, drawAreaWidth, drawAreaHeight);
}

void XPDFCore::displayPage(int pageA, double zoomA, int rotateA,
			   GBool scrollToTop, GBool addToHist) {
  double hDPI, vDPI;
  int rot;
  XPDFHistory *h;
  GBool newZoom;
  XGCValues gcValues;
  time_t newModTime;
  int oldScrollX, oldScrollY;

  // update the zoom and rotate values
  newZoom = zoomA != zoom;
  zoom = zoomA;
  rotate = rotateA;

  // check for document and valid page number
  if (!doc || pageA <= 0 || pageA > doc->getNumPages()) {
    return;
  }

  // busy cursor
  setCursor(busyCursor);


  // check for changes to the file
  newModTime = getModTime(doc->getFileName()->getCString());
  if (newModTime != modTime) {
    if (loadFile(doc->getFileName()) == errNone) {
      if (pageA > doc->getNumPages()) {
	pageA = doc->getNumPages();
      }
    }
    modTime = newModTime;
  }

  // free the old GCs
  if (selectGC) {
    XFreeGC(display, selectGC);
    XFreeGC(display, highlightGC);
  }

  // new page number
  page = pageA;

  // scroll to top
  if (scrollToTop) {
    scrollY = 0;
  }

  // if zoom level changed, scroll to the top-left corner
  if (newZoom) {
    scrollX = scrollY = 0;
  }

  // initialize mouse-related stuff
  linkAction = NULL;
  selectXMin = selectXMax = 0;
  selectYMin = selectYMax = 0;
  dragging = gFalse;
  lastDragLeft = lastDragTop = gTrue;

  // draw the page
  rot = rotate + doc->getPageRotate(page);
  if (rot >= 360) {
    rot -= 360;
  } else if (rotate < 0) {
    rot += 360;
  }
  if (zoom == zoomPage) {
    if (rot == 90 || rot == 270) {
      hDPI = (drawAreaWidth / doc->getPageHeight(page)) * 72;
      vDPI = (drawAreaHeight / doc->getPageWidth(page)) * 72;
    } else {
      hDPI = (drawAreaWidth / doc->getPageWidth(page)) * 72;
      vDPI = (drawAreaHeight / doc->getPageHeight(page)) * 72;
    }
    dpi = (hDPI < vDPI) ? hDPI : vDPI;
  } else if (zoom == zoomWidth) {
    if (rot == 90 || rot == 270) {
      dpi = (drawAreaWidth / doc->getPageHeight(page)) * 72;
    } else {
      dpi = (drawAreaWidth / doc->getPageWidth(page)) * 72;
    }
  } else {
    dpi = 0.01 * zoom * 72;
  }
  out->setWindow(XtWindow(drawArea));
  doc->displayPage(out, page, dpi, dpi, rotate, gTrue);
  oldScrollX = scrollX;
  oldScrollY = scrollY;
  updateScrollBars();
  if (scrollX != oldScrollX || scrollY != oldScrollY) {
    redrawRectangle(scrollX, scrollY, drawAreaWidth, drawAreaHeight);
  }

  // allocate new GCs
  gcValues.foreground = BlackPixel(display, screenNum) ^
                        WhitePixel(display, screenNum);
  gcValues.function = GXxor;
  selectGC = XCreateGC(display, out->getPixmap(),
		       GCForeground | GCFunction, &gcValues);
  highlightGC = XCreateGC(display, out->getPixmap(),
		       GCForeground | GCFunction, &gcValues);


  // add to history
  if (addToHist) {
    if (++historyCur == xpdfHistorySize) {
      historyCur = 0;
    }
    h = &history[historyCur];
    if (h->fileName) {
      delete h->fileName;
    }
    if (doc->getFileName()) {
      h->fileName = doc->getFileName()->copy();
    } else {
      h->fileName = NULL;
    }
    h->page = page;
    if (historyBLen < xpdfHistorySize) {
      ++historyBLen;
    }
    historyFLen = 0;
  }

  // update the parent window
  if (updateCbk) {
    (*updateCbk)(updateCbkData, NULL, page, -1, "");
  }

  // back to regular cursor
  setCursor(None);
}

void XPDFCore::displayDest(LinkDest *dest, double zoomA, int rotateA,
			   GBool addToHist) {
  Ref pageRef;
  int pg;
  int dx, dy;

  if (dest->isPageRef()) {
    pageRef = dest->getPageRef();
    pg = doc->findPage(pageRef.num, pageRef.gen);
  } else {
    pg = dest->getPageNum();
  }
  if (pg <= 0 || pg > doc->getNumPages()) {
    pg = 1;
  }
  if (pg != page) {
    displayPage(pg, zoomA, rotateA, gTrue, addToHist);
  }

  if (fullScreen) {
    return;
  }
  switch (dest->getKind()) {
  case destXYZ:
    out->cvtUserToDev(dest->getLeft(), dest->getTop(), &dx, &dy);
    if (dest->getChangeLeft() || dest->getChangeTop()) {
      scrollTo(dest->getChangeLeft() ? dx : scrollX,
	       dest->getChangeTop() ? dy : scrollY);
    }
    //~ what is the zoom parameter?
    break;
  case destFit:
  case destFitB:
    //~ do fit
    scrollTo(0, 0);
    break;
  case destFitH:
  case destFitBH:
    //~ do fit
    out->cvtUserToDev(0, dest->getTop(), &dx, &dy);
    scrollTo(0, dy);
    break;
  case destFitV:
  case destFitBV:
    //~ do fit
    out->cvtUserToDev(dest->getLeft(), 0, &dx, &dy);
    scrollTo(dx, 0);
    break;
  case destFitR:
    //~ do fit
    out->cvtUserToDev(dest->getLeft(), dest->getTop(), &dx, &dy);
    scrollTo(dx, dy);
    break;
  }
}

//------------------------------------------------------------------------
// page/position changes
//------------------------------------------------------------------------

void XPDFCore::gotoNextPage(int inc, GBool top) {
  int pg;

  if (!doc || doc->getNumPages() == 0) {
    return;
  }
  if (page < doc->getNumPages()) {
    if ((pg = page + inc) > doc->getNumPages()) {
      pg = doc->getNumPages();
    }
    displayPage(pg, zoom, rotate, top, gTrue);
  } else {
    XBell(display, 0);
  }
}

void XPDFCore::gotoPrevPage(int dec, GBool top, GBool bottom) {
  int pg;

  if (!doc || doc->getNumPages() == 0) {
    return;
  }
  if (page > 1) {
    if (!fullScreen && bottom) {
      scrollY = out->getPixmapHeight() - drawAreaHeight;
      if (scrollY < 0) {
	scrollY = 0;
      }
      // displayPage will call updateScrollBars()
    }
    if ((pg = page - dec) < 1) {
      pg = 1;
    }
    displayPage(pg, zoom, rotate, top, gTrue);
  } else {
    XBell(display, 0);
  }
}

void XPDFCore::goForward() {
  if (historyFLen == 0) {
    XBell(display, 0);
    return;
  }
  if (++historyCur == xpdfHistorySize) {
    historyCur = 0;
  }
  --historyFLen;
  ++historyBLen;
  if (!doc || history[historyCur].fileName->cmp(doc->getFileName()) != 0) {
    if (loadFile(history[historyCur].fileName) != errNone) {
      XBell(display, 0);
      return;
    }
  }
  displayPage(history[historyCur].page, zoom, rotate, gFalse, gFalse);
}

void XPDFCore::goBackward() {
  if (historyBLen <= 1) {
    XBell(display, 0);
    return;
  }
  if (--historyCur < 0) {
    historyCur = xpdfHistorySize - 1;
  }
  --historyBLen;
  ++historyFLen;
  if (!doc || history[historyCur].fileName->cmp(doc->getFileName()) != 0) {
    if (loadFile(history[historyCur].fileName) != errNone) {
      XBell(display, 0);
      return;
    }
  }
  displayPage(history[historyCur].page, zoom, rotate, gFalse, gFalse);
}

void XPDFCore::scrollLeft(int nCols) {
  scrollTo(scrollX - nCols * 16, scrollY);
}

void XPDFCore::scrollRight(int nCols) {
  scrollTo(scrollX + nCols * 16, scrollY);
}

void XPDFCore::scrollUp(int nLines) {
  scrollTo(scrollX, scrollY - nLines * 16);
}

void XPDFCore::scrollDown(int nLines) {
  scrollTo(scrollX, scrollY + nLines * 16);
}

void XPDFCore::scrollPageUp() {
  if (scrollY == 0) {
    gotoPrevPage(1, gFalse, gTrue);
  } else {
    scrollTo(scrollX, scrollY - drawAreaHeight);
  }
}

void XPDFCore::scrollPageDown() {
  if (scrollY >= out->getPixmapHeight() - drawAreaHeight) {
    gotoNextPage(1, gTrue);
  } else {
    scrollTo(scrollX, scrollY + drawAreaHeight);
  }
}

void XPDFCore::scrollTo(int x, int y) {
  GBool needRedraw;
  int maxPos, pos;

  needRedraw = gFalse;

  maxPos = out ? out->getPixmapWidth() : 1;
  if (maxPos < drawAreaWidth) {
    maxPos = drawAreaWidth;
  }
  if (x < 0) {
    pos = 0;
  } else if (x > maxPos - drawAreaWidth) {
    pos = maxPos - drawAreaWidth;
  } else {
    pos = x;
  }
  if (scrollX != pos) {
    scrollX = pos;
    XmScrollBarSetValues(hScrollBar, scrollX, drawAreaWidth, 16,
			 drawAreaWidth, False);
    needRedraw = gTrue;
  }

  maxPos = out ? out->getPixmapHeight() : 1;
  if (maxPos < drawAreaHeight) {
    maxPos = drawAreaHeight;
  }
  if (y < 0) {
    pos = 0;
  } else if (y > maxPos - drawAreaHeight) {
    pos = maxPos - drawAreaHeight;
  } else {
    pos = y;
  }
  if (scrollY != pos) {
    scrollY = pos;
    XmScrollBarSetValues(vScrollBar, scrollY, drawAreaHeight, 16,
			 drawAreaHeight, False);
    needRedraw = gTrue;
  }

  if (needRedraw) {
    redrawRectangle(scrollX, scrollY, drawAreaWidth, drawAreaHeight);
  }
}

//------------------------------------------------------------------------
// selection
//------------------------------------------------------------------------

void XPDFCore::setSelection(int newXMin, int newYMin,
			    int newXMax, int newYMax) {
  Pixmap pixmap;
  int x, y;
  GBool needRedraw, needScroll;
  GBool moveLeft, moveRight, moveTop, moveBottom;

  pixmap = out->getPixmap();


  // erase old selection on off-screen bitmap
  needRedraw = gFalse;
  if (selectXMin < selectXMax && selectYMin < selectYMax) {
    XFillRectangle(display, pixmap,
		   selectGC, selectXMin, selectYMin,
		   selectXMax - selectXMin, selectYMax - selectYMin);
    needRedraw = gTrue;
  }

  // draw new selection on off-screen bitmap
  if (newXMin < newXMax && newYMin < newYMax) {
    XFillRectangle(display, pixmap,
		   selectGC, newXMin, newYMin,
		   newXMax - newXMin, newYMax - newYMin);
    needRedraw = gTrue;
  }

  // check which edges moved
  moveLeft = newXMin != selectXMin;
  moveTop = newYMin != selectYMin;
  moveRight = newXMax != selectXMax;
  moveBottom = newYMax != selectYMax;

  // redraw currently visible part of bitmap
  if (needRedraw) {
    if (moveLeft) {
      redrawRectangle((newXMin < selectXMin) ? newXMin : selectXMin,
		      (newYMin < selectYMin) ? newYMin : selectYMin,
		      (newXMin > selectXMin) ? newXMin : selectXMin,
		      (newYMax > selectYMax) ? newYMax : selectYMax);
    }
    if (moveRight) {
      redrawRectangle((newXMax < selectXMax) ? newXMax : selectXMax,
		      (newYMin < selectYMin) ? newYMin : selectYMin,
		      (newXMax > selectXMax) ? newXMax : selectXMax,
		      (newYMax > selectYMax) ? newYMax : selectYMax);
    }
    if (moveTop) {
      redrawRectangle((newXMin < selectXMin) ? newXMin : selectXMin,
		      (newYMin < selectYMin) ? newYMin : selectYMin,
		      (newXMax > selectXMax) ? newXMax : selectXMax,
		      (newYMin > selectYMin) ? newYMin : selectYMin);
    }
    if (moveBottom) {
      redrawRectangle((newXMin < selectXMin) ? newXMin : selectXMin,
		      (newYMax < selectYMax) ? newYMax : selectYMax,
		      (newXMax > selectXMax) ? newXMax : selectXMax,
		      (newYMax > selectYMax) ? newYMax : selectYMax);
    }
  }

  // switch to new selection coords
  selectXMin = newXMin;
  selectXMax = newXMax;
  selectYMin = newYMin;
  selectYMax = newYMax;

  // scroll if necessary
  if (fullScreen) {
    return;
  }
  needScroll = gFalse;
  x = scrollX;
  y = scrollY;
  if (moveLeft && selectXMin < x) {
    x = selectXMin;
    needScroll = gTrue;
  } else if (moveRight && selectXMax >= x + drawAreaWidth) {
    x = selectXMax - drawAreaWidth;
    needScroll = gTrue;
  } else if (moveLeft && selectXMin >= x + drawAreaWidth) {
    x = selectXMin - drawAreaWidth;
    needScroll = gTrue;
  } else if (moveRight && selectXMax < x) {
    x = selectXMax;
    needScroll = gTrue;
  }
  if (moveTop && selectYMin < y) {
    y = selectYMin;
    needScroll = gTrue;
  } else if (moveBottom && selectYMax >= y + drawAreaHeight) {
    y = selectYMax - drawAreaHeight;
    needScroll = gTrue;
  } else if (moveTop && selectYMin >= y + drawAreaHeight) {
    y = selectYMin - drawAreaHeight;
    needScroll = gTrue;
  } else if (moveBottom && selectYMax < y) {
    y = selectYMax;
    needScroll = gTrue;
  }
  if (needScroll) {
    scrollTo(x, y);
  }
}

void XPDFCore::moveSelection(int mx, int my) {
  int xMin, yMin, xMax, yMax;

  // clip mouse coords
  if (mx < 0) {
    mx = 0;
  } else if (mx >= out->getPixmapWidth()) {
    mx = out->getPixmapWidth() - 1;
  }
  if (my < 0) {
    my = 0;
  } else if (my >= out->getPixmapHeight()) {
    my = out->getPixmapHeight() - 1;
  }

  // move appropriate edges of selection
  if (lastDragLeft) {
    if (mx < selectXMax) {
      xMin = mx;
      xMax = selectXMax;
    } else {
      xMin = selectXMax;
      xMax = mx;
      lastDragLeft = gFalse;
    }
  } else {
    if (mx > selectXMin) {
      xMin = selectXMin;
      xMax = mx;
    } else {
      xMin = mx;
      xMax = selectXMin;
      lastDragLeft = gTrue;
    }
  }
  if (lastDragTop) {
    if (my < selectYMax) {
      yMin = my;
      yMax = selectYMax;
    } else {
      yMin = selectYMax;
      yMax = my;
      lastDragTop = gFalse;
    }
  } else {
    if (my > selectYMin) {
      yMin = selectYMin;
      yMax = my;
    } else {
      yMin = my;
      yMax = selectYMin;
      lastDragTop = gTrue;
    }
  }

  // redraw the selection
  setSelection(xMin, yMin, xMax, yMax);
}

// X's copy-and-paste mechanism is brain damaged.  Xt doesn't help
// any, but doesn't make it too much worse, either.  Motif, on the
// other hand, adds significant complexity to the mess.  So here we
// blow off the Motif junk and stick to plain old Xt.  The next two
// functions (copySelection and convertSelectionCbk) implement the
// magic needed to deal with Xt's mechanism.  Note that this requires
// global variables (currentSelection and currentSelectionOwner).

void XPDFCore::copySelection() {
  if (!doc->okToCopy()) {
    return;
  }
  if (currentSelection) {
    delete currentSelection;
  }
  //~ for multithreading: need a mutex here
  currentSelection = out->getText(selectXMin, selectYMin,
				  selectXMax, selectYMax);
  currentSelectionOwner = this;
  XtOwnSelection(drawArea, XA_PRIMARY, XtLastTimestampProcessed(display),
		 &convertSelectionCbk, NULL, NULL);
}

Boolean XPDFCore::convertSelectionCbk(Widget widget, Atom *selection,
				      Atom *target, Atom *type,
				      XtPointer *value, unsigned long *length,
				      int *format) {
  Atom *array;

  // send back a list of supported conversion targets
  if (*target == targetsAtom) {
    if (!(array = (Atom *)XtMalloc(sizeof(Atom)))) {
      return False;
    }
    array[0] = XA_STRING;
    *value = (XtPointer)array;
    *type = XA_ATOM;
    *format = 32;
    *length = 1;
    return True;

  // send the selected text
  } else if (*target == XA_STRING) {
    //~ for multithreading: need a mutex here
    *value = XtNewString(currentSelection->getCString());
    *length = currentSelection->getLength();
    *type = XA_STRING;
    *format = 8; // 8-bit elements
    return True;
  }

  return False;
}

GBool XPDFCore::getSelection(int *xMin, int *yMin, int *xMax, int *yMax) {
  if (selectXMin >= selectXMax || selectYMin >= selectYMax) {
    return gFalse;
  }
  *xMin = selectXMin;
  *yMin = selectYMin;
  *xMax = selectXMax;
  *yMax = selectYMax;
  return gTrue;
}

GString *XPDFCore::extractText(int xMin, int yMin, int xMax, int yMax) {
  if (!doc->okToCopy()) {
    return NULL;
  }
  return out->getText(xMin, yMin, xMax, yMax);
}

GString *XPDFCore::extractText(int pageNum,
			       int xMin, int yMin, int xMax, int yMax) {
  TextOutputDev *textOut;
  GString *s;

  if (!doc->okToCopy()) {
    return NULL;
  }
  textOut = new TextOutputDev(NULL, gTrue, gFalse, gFalse);
  if (!textOut->isOk()) {
    delete textOut;
    return NULL;
  }
  doc->displayPage(textOut, pageNum, dpi, dpi, rotate, gFalse);
  s = textOut->getText(xMin, yMin, xMax, yMax);
  delete textOut;
  return s;
}

//------------------------------------------------------------------------
// hyperlinks
//------------------------------------------------------------------------

GBool XPDFCore::doLink(int mx, int my) {
  double x, y;
  LinkAction *action;

  // look for a link
  out->cvtDevToUser(mx, my, &x, &y);
  if ((action = doc->findLink(x, y))) {
    doAction(action);
    return gTrue;
  }
  return gFalse;
}

void XPDFCore::doAction(LinkAction *action) {
  LinkActionKind kind;
  LinkDest *dest;
  GString *namedDest;
  char *s;
  GString *fileName, *fileName2;
  GString *cmd;
  GString *actionName;
  Object movieAnnot, obj1, obj2;
  GString *msg;
  int i;

  switch (kind = action->getKind()) {

  // GoTo / GoToR action
  case actionGoTo:
  case actionGoToR:
    if (kind == actionGoTo) {
      dest = NULL;
      namedDest = NULL;
      if ((dest = ((LinkGoTo *)action)->getDest())) {
	dest = dest->copy();
      } else if ((namedDest = ((LinkGoTo *)action)->getNamedDest())) {
	namedDest = namedDest->copy();
      }
    } else {
      dest = NULL;
      namedDest = NULL;
      if ((dest = ((LinkGoToR *)action)->getDest())) {
	dest = dest->copy();
      } else if ((namedDest = ((LinkGoToR *)action)->getNamedDest())) {
	namedDest = namedDest->copy();
      }
      s = ((LinkGoToR *)action)->getFileName()->getCString();
      //~ translate path name for VMS (deal with '/')
      if (isAbsolutePath(s)) {
	fileName = new GString(s);
      } else {
	fileName = appendToPath(grabPath(doc->getFileName()->getCString()), s);
      }
      if (loadFile(fileName) != errNone) {
	if (dest) {
	  delete dest;
	}
	if (namedDest) {
	  delete namedDest;
	}
	delete fileName;
	return;
      }
      delete fileName;
    }
    if (namedDest) {
      dest = doc->findDest(namedDest);
      delete namedDest;
    }
    if (dest) {
      displayDest(dest, zoom, rotate, gTrue);
      delete dest;
    } else {
      if (kind == actionGoToR) {
	displayPage(1, zoom, 0, gFalse, gTrue);
      }
    }
    break;

  // Launch action
  case actionLaunch:
    fileName = ((LinkLaunch *)action)->getFileName();
    s = fileName->getCString();
    if (!strcmp(s + fileName->getLength() - 4, ".pdf") ||
	!strcmp(s + fileName->getLength() - 4, ".PDF")) {
      //~ translate path name for VMS (deal with '/')
      if (isAbsolutePath(s)) {
	fileName = fileName->copy();
      } else {
	fileName = appendToPath(grabPath(doc->getFileName()->getCString()), s);
      }
      if (loadFile(fileName) != errNone) {
	delete fileName;
	return;
      }
      delete fileName;
      displayPage(1, zoom, rotate, gFalse, gTrue);
    } else {
      fileName = fileName->copy();
      if (((LinkLaunch *)action)->getParams()) {
	fileName->append(' ');
	fileName->append(((LinkLaunch *)action)->getParams());
      }
#ifdef VMS
      fileName->insert(0, "spawn/nowait ");
#elif defined(__EMX__)
      fileName->insert(0, "start /min /n ");
#else
      fileName->append(" &");
#endif
      msg = new GString("About to execute the command:\n");
      msg->append(fileName);
      if (doQuestionDialog("Launching external application", msg)) {
	system(fileName->getCString());
      }
      delete fileName;
      delete msg;
    }
    break;

  // URI action
  case actionURI:
    if (!(cmd = globalParams->getURLCommand())) {
      error(-1, "No urlCommand defined in config file");
      break;
    }
    runCommand(cmd, ((LinkURI *)action)->getURI());
    break;

  // Named action
  case actionNamed:
    actionName = ((LinkNamed *)action)->getName();
    if (!actionName->cmp("NextPage")) {
      gotoNextPage(1, gTrue);
    } else if (!actionName->cmp("PrevPage")) {
      gotoPrevPage(1, gTrue, gFalse);
    } else if (!actionName->cmp("FirstPage")) {
      if (page != 1) {
	displayPage(1, zoom, rotate, gTrue, gTrue);
      }
    } else if (!actionName->cmp("LastPage")) {
      if (page != doc->getNumPages()) {
	displayPage(doc->getNumPages(), zoom, rotate, gTrue, gTrue);
      }
    } else if (!actionName->cmp("GoBack")) {
      goBackward();
    } else if (!actionName->cmp("GoForward")) {
      goForward();
    } else if (!actionName->cmp("Quit")) {
      if (actionCbk) {
	(*actionCbk)(actionCbkData, "Quit");
      }
    } else {
      error(-1, "Unknown named action: '%s'", actionName->getCString());
    }
    break;

  // Movie action
  case actionMovie:
    if (!(cmd = globalParams->getMovieCommand())) {
      error(-1, "No movieCommand defined in config file");
      break;
    }
    if (((LinkMovie *)action)->hasAnnotRef()) {
      doc->getXRef()->fetch(((LinkMovie *)action)->getAnnotRef()->num,
			    ((LinkMovie *)action)->getAnnotRef()->gen,
			    &movieAnnot);
    } else {
      doc->getCatalog()->getPage(page)->getAnnots(&obj1);
      if (obj1.isArray()) {
	for (i = 0; i < obj1.arrayGetLength(); ++i) {
	  if (obj1.arrayGet(i, &movieAnnot)->isDict()) {
	    if (movieAnnot.dictLookup("Subtype", &obj2)->isName("Movie")) {
	      obj2.free();
	      break;
	    }
	    obj2.free();
	  }
	  movieAnnot.free();
	}
	obj1.free();
      }
    }
    if (movieAnnot.isDict()) {
      if (movieAnnot.dictLookup("Movie", &obj1)->isDict()) {
	if (obj1.dictLookup("F", &obj2)) {
	  if ((fileName = LinkAction::getFileSpecName(&obj2))) {
	    if (!isAbsolutePath(fileName->getCString())) {
	      fileName2 = appendToPath(
			      grabPath(doc->getFileName()->getCString()),
			      fileName->getCString());
	      delete fileName;
	      fileName = fileName2;
	    }
	    runCommand(cmd, fileName);
	    delete fileName;
	  }
	  obj2.free();
	}
	obj1.free();
      }
    }
    movieAnnot.free();
    break;

  // unknown action type
  case actionUnknown:
    error(-1, "Unknown link action type: '%s'",
	  ((LinkUnknown *)action)->getAction()->getCString());
    break;
  }
}

// Run a command, given a <cmdFmt> string with one '%s' in it, and an
// <arg> string to insert in place of the '%s'.
void XPDFCore::runCommand(GString *cmdFmt, GString *arg) {
  GString *cmd;
  char *s;

  if ((s = strstr(cmdFmt->getCString(), "%s"))) {
    cmd = mungeURL(arg);
    cmd->insert(0, cmdFmt->getCString(),
		s - cmdFmt->getCString());
    cmd->append(s + 2);
  } else {
    cmd = cmdFmt->copy();
  }
#ifdef VMS
  cmd->insert(0, "spawn/nowait ");
#elif defined(__EMX__)
  cmd->insert(0, "start /min /n ");
#else
  cmd->append(" &");
#endif
  system(cmd->getCString());
  delete cmd;
}

// Escape any characters in a URL which might cause problems when
// calling system().
GString *XPDFCore::mungeURL(GString *url) {
  static char *allowed = "ABCDEFGHIJKLMNOPQRSTUVWXYZ"
                         "abcdefghijklmnopqrstuvwxyz"
                         "0123456789"
                         "-_.~/?:@&=+,#%";
  GString *newURL;
  char c;
  char buf[4];
  int i;

  newURL = new GString();
  for (i = 0; i < url->getLength(); ++i) {
    c = url->getChar(i);
    if (strchr(allowed, c)) {
      newURL->append(c);
    } else {
      sprintf(buf, "%%%02x", c & 0xff);
      newURL->append(buf);
    }
  }
  return newURL;
}


//------------------------------------------------------------------------
// find
//------------------------------------------------------------------------

void XPDFCore::find(char *s, GBool next) {
  Unicode *u;
  TextOutputDev *textOut;
  int xMin, yMin, xMax, yMax;
  double xMin1, yMin1, xMax1, yMax1;
  int pg;
  GBool startAtTop;
  int len, i;

  // check for zero-length string
  if (!s[0]) {
    XBell(display, 0);
    return;
  }

  // set cursor to watch
  setCursor(busyCursor);

  // convert to Unicode
#if 1 //~ should do something more intelligent here
  len = strlen(s);
  u = (Unicode *)gmalloc(len * sizeof(Unicode));
  for (i = 0; i < len; ++i) {
    u[i] = (Unicode)(s[i] & 0xff);
  }
#endif

  // search current page starting at current selection or top of page
  startAtTop = !next && !(selectXMin < selectXMax && selectYMin < selectYMax);
  xMin = selectXMin + 1;
  yMin = selectYMin + 1;
  xMax = yMax = 0;
  if (out->findText(u, len, startAtTop, gTrue, next, gFalse,
		    &xMin, &yMin, &xMax, &yMax)) {
    goto found;
  }

  // search following pages
  textOut = new TextOutputDev(NULL, gTrue, gFalse, gFalse);
  if (!textOut->isOk()) {
    delete textOut;
    goto done;
  }
  for (pg = page+1; pg <= doc->getNumPages(); ++pg) {
    doc->displayPage(textOut, pg, 72, 72, 0, gFalse);
    if (textOut->findText(u, len, gTrue, gTrue, gFalse, gFalse,
			  &xMin1, &yMin1, &xMax1, &yMax1)) {
      goto foundPage;
    }
  }

  // search previous pages
  for (pg = 1; pg < page; ++pg) {
    doc->displayPage(textOut, pg, 72, 72, 0, gFalse);
    if (textOut->findText(u, len, gTrue, gTrue, gFalse, gFalse,
			  &xMin1, &yMin1, &xMax1, &yMax1)) {
      goto foundPage;
    }
  }
  delete textOut;

  // search current page ending at current selection
  if (!startAtTop) {
    xMin = yMin = 0;
    xMax = selectXMin;
    yMax = selectYMin;
    if (out->findText(u, len, gTrue, gFalse, gFalse, next,
		      &xMin, &yMin, &xMax, &yMax)) {
      goto found;
    }
  }

  // not found
  XBell(display, 0);
  goto done;

  // found on a different page
 foundPage:
  delete textOut;
  displayPage(pg, zoom, rotate, gTrue, gTrue);
  if (!out->findText(u, len, gTrue, gTrue, gFalse, gFalse,
		     &xMin, &yMin, &xMax, &yMax)) {
    // this can happen if coalescing is bad
    goto done;
  }

  // found: change the selection
 found:
  setSelection(xMin, yMin, xMax, yMax);
#ifndef NO_TEXT_SELECT
  copySelection();
#endif

 done:
  gfree(u);

  // reset cursors to normal
  setCursor(None);
}

//------------------------------------------------------------------------
// misc access
//------------------------------------------------------------------------

void XPDFCore::setBusyCursor(GBool busy) {
  setCursor(busy ? busyCursor : None);
}

void XPDFCore::takeFocus() {
  XmProcessTraversal(drawArea, XmTRAVERSE_CURRENT);
}

//------------------------------------------------------------------------
// GUI code
//------------------------------------------------------------------------

void XPDFCore::initWindow() {
  Arg args[20];
  int n;

  // create the cursors
  busyCursor = XCreateFontCursor(display, XC_watch);
  linkCursor = XCreateFontCursor(display, XC_hand2);
  selectCursor = XCreateFontCursor(display, XC_cross);
  currentCursor = 0;

  // create the scrolled window and scrollbars
  n = 0;
  XtSetArg(args[n], XmNscrollingPolicy, XmAPPLICATION_DEFINED); ++n;
  XtSetArg(args[n], XmNvisualPolicy, XmVARIABLE); ++n;
  scrolledWin = XmCreateScrolledWindow(parentWidget, "scroll", args, n);
  XtManageChild(scrolledWin);
  n = 0;
  XtSetArg(args[n], XmNorientation, XmHORIZONTAL); ++n;
  XtSetArg(args[n], XmNminimum, 0); ++n;
  XtSetArg(args[n], XmNmaximum, 1); ++n;
  XtSetArg(args[n], XmNsliderSize, 1); ++n;
  XtSetArg(args[n], XmNvalue, 0); ++n;
  XtSetArg(args[n], XmNincrement, 1); ++n;
  XtSetArg(args[n], XmNpageIncrement, 1); ++n;
  hScrollBar = XmCreateScrollBar(scrolledWin, "hScrollBar", args, n);
  XtManageChild(hScrollBar);
  XtAddCallback(hScrollBar, XmNvalueChangedCallback,
		&hScrollChangeCbk, (XtPointer)this);
#ifndef DISABLE_SMOOTH_SCROLL
  XtAddCallback(hScrollBar, XmNdragCallback,
		&hScrollDragCbk, (XtPointer)this);
#endif
  n = 0;
  XtSetArg(args[n], XmNorientation, XmVERTICAL); ++n;
  XtSetArg(args[n], XmNminimum, 0); ++n;
  XtSetArg(args[n], XmNmaximum, 1); ++n;
  XtSetArg(args[n], XmNsliderSize, 1); ++n;
  XtSetArg(args[n], XmNvalue, 0); ++n;
  XtSetArg(args[n], XmNincrement, 1); ++n;
  XtSetArg(args[n], XmNpageIncrement, 1); ++n;
  vScrollBar = XmCreateScrollBar(scrolledWin, "vScrollBar", args, n);
  XtManageChild(vScrollBar);
  XtAddCallback(vScrollBar, XmNvalueChangedCallback,
		&vScrollChangeCbk, (XtPointer)this);
#ifndef DISABLE_SMOOTH_SCROLL
  XtAddCallback(vScrollBar, XmNdragCallback,
		&vScrollDragCbk, (XtPointer)this);
#endif

  // create the drawing area
  n = 0;
  XtSetArg(args[n], XmNshadowType, XmSHADOW_IN); ++n;
  XtSetArg(args[n], XmNmarginWidth, 0); ++n;
  XtSetArg(args[n], XmNmarginHeight, 0); ++n;
  if (fullScreen) {
    XtSetArg(args[n], XmNshadowThickness, 0); ++n;
  }
  drawAreaFrame = XmCreateFrame(scrolledWin, "drawAreaFrame", args, n);
  XtManageChild(drawAreaFrame);
  n = 0;
  XtSetArg(args[n], XmNresizePolicy, XmRESIZE_ANY); ++n;
  XtSetArg(args[n], XmNbackground, paperColor); ++n;
  XtSetArg(args[n], XmNwidth, 700); ++n;
  XtSetArg(args[n], XmNheight, 500); ++n;
  drawArea = XmCreateDrawingArea(drawAreaFrame, "drawArea", args, n);
  XtManageChild(drawArea);
  XtAddCallback(drawArea, XmNresizeCallback, &resizeCbk, (XtPointer)this);
  XtAddCallback(drawArea, XmNexposeCallback, &redrawCbk, (XtPointer)this);
  XtAddCallback(drawArea, XmNinputCallback, &inputCbk, (XtPointer)this);
  resizeCbk(drawArea, this, NULL);

  // set up mouse motion translations
  XtOverrideTranslations(drawArea, XtParseTranslationTable(
      "<Btn1Down>:DrawingAreaInput()\n"
      "<Btn1Up>:DrawingAreaInput()\n"
      "<Btn1Motion>:DrawingAreaInput()\n"
      "<Motion>:DrawingAreaInput()"));

  // can't create a GC until the window gets mapped
  drawAreaGC = NULL;
  selectGC = NULL;
  highlightGC = NULL;
}

void XPDFCore::hScrollChangeCbk(Widget widget, XtPointer ptr,
			     XtPointer callData) {
  XPDFCore *core = (XPDFCore *)ptr;
  XmScrollBarCallbackStruct *data = (XmScrollBarCallbackStruct *)callData;

  core->scrollTo(data->value, core->scrollY);
}

void XPDFCore::hScrollDragCbk(Widget widget, XtPointer ptr,
			      XtPointer callData) {
  XPDFCore *core = (XPDFCore *)ptr;
  XmScrollBarCallbackStruct *data = (XmScrollBarCallbackStruct *)callData;

  core->scrollTo(data->value, core->scrollY);
}

void XPDFCore::vScrollChangeCbk(Widget widget, XtPointer ptr,
			     XtPointer callData) {
  XPDFCore *core = (XPDFCore *)ptr;
  XmScrollBarCallbackStruct *data = (XmScrollBarCallbackStruct *)callData;

  core->scrollTo(core->scrollX, data->value);
}

void XPDFCore::vScrollDragCbk(Widget widget, XtPointer ptr,
			      XtPointer callData) {
  XPDFCore *core = (XPDFCore *)ptr;
  XmScrollBarCallbackStruct *data = (XmScrollBarCallbackStruct *)callData;

  core->scrollTo(core->scrollX, data->value);
}

void XPDFCore::resizeCbk(Widget widget, XtPointer ptr, XtPointer callData) {
  XPDFCore *core = (XPDFCore *)ptr;
  Arg args[2];
  int n;
  Dimension w, h;
  int oldScrollX, oldScrollY;

  n = 0;
  XtSetArg(args[n], XmNwidth, &w); ++n;
  XtSetArg(args[n], XmNheight, &h); ++n;
  XtGetValues(core->drawArea, args, n);
  core->drawAreaWidth = (int)w;
  core->drawAreaHeight = (int)h;
  if (core->page >= 0 &&
      (core->zoom == zoomPage || core->zoom == zoomWidth)) {
    core->displayPage(core->page, core->zoom, core->rotate,
		      gFalse, gFalse);
  } else {
    oldScrollX = core->scrollX;
    oldScrollY = core->scrollY;
    core->updateScrollBars();
    if (core->scrollX != oldScrollX || core->scrollY != oldScrollY) {
      core->redrawRectangle(core->scrollX, core->scrollY,
			    core->drawAreaWidth, core->drawAreaHeight);
    }
  }
}

void XPDFCore::redrawCbk(Widget widget, XtPointer ptr, XtPointer callData) {
  XPDFCore *core = (XPDFCore *)ptr;
  XmDrawingAreaCallbackStruct *data = (XmDrawingAreaCallbackStruct *)callData;
  int x, y, w, h;

  if (data->reason == XmCR_EXPOSE) {
    x = core->scrollX + data->event->xexpose.x;
    y = core->scrollY + data->event->xexpose.y;
    w = data->event->xexpose.width;
    h = data->event->xexpose.height;
  } else {
    x = core->scrollX;
    y = core->scrollY;
    w = core->drawAreaWidth;
    h = core->drawAreaHeight;
  }
  core->redrawRectangle(x, y, w, h);
}

void XPDFCore::outputDevRedrawCbk(void *data) {
  XPDFCore *core = (XPDFCore *)data;

  core->redrawRectangle(core->scrollX, core->scrollY,
			core->drawAreaWidth, core->drawAreaHeight);
}

void XPDFCore::inputCbk(Widget widget, XtPointer ptr, XtPointer callData) {
  XPDFCore *core = (XPDFCore *)ptr;
  XmDrawingAreaCallbackStruct *data = (XmDrawingAreaCallbackStruct *)callData;
  LinkAction *action;
  int mx, my;
  double x, y;
  char *s;
  KeySym key;
  char buf[20];
  int n;

  switch (data->event->type) {
  case ButtonPress:
    if (data->event->xbutton.button == 1) {
      core->takeFocus();
      if (core->doc && core->doc->getNumPages() > 0) {
	if (core->selectEnabled) {
	  mx = core->scrollX + data->event->xbutton.x;
	  my = core->scrollY + data->event->xbutton.y;
	  core->setSelection(mx, my, mx, my);
	  core->setCursor(core->selectCursor);
	  core->dragging = gTrue;
	}
      }
    } else if (data->event->xbutton.button == 2) {
      if (!core->fullScreen) {
	core->panning = gTrue;
	core->panMX = data->event->xbutton.x;
	core->panMY = data->event->xbutton.y;
      }
    } else if (data->event->xbutton.button == 4) { // mouse wheel up
      if (core->fullScreen) {
	core->gotoPrevPage(1, gTrue, gFalse);
      } else if (core->scrollY == 0) {
	core->gotoPrevPage(1, gFalse, gTrue);
      } else {
	core->scrollUp(1);
      }
    } else if (data->event->xbutton.button == 5) { // mouse wheel down
      if (core->fullScreen ||
	  core->scrollY >=
	    core->out->getPixmapHeight() - core->drawAreaHeight) {
	core->gotoNextPage(1, gTrue);
      } else {
	core->scrollDown(1);
      }
    } else if (data->event->xbutton.button == 6) { // second mouse wheel right
      if (!core->fullScreen) {
	core->scrollRight(1);
      }
    } else if (data->event->xbutton.button == 7) { // second mouse wheel left
      if (!core->fullScreen) {
	core->scrollLeft(1);
      }
    } else {
      if (*core->mouseCbk) {
	(*core->mouseCbk)(core->mouseCbkData, data->event);
      }
    }
    break;
  case ButtonRelease:
    if (data->event->xbutton.button == 1) {
      if (core->doc && core->doc->getNumPages() > 0) {
	mx = core->scrollX + data->event->xbutton.x;
	my = core->scrollY + data->event->xbutton.y;
	if (core->dragging) {
	  core->dragging = gFalse;
	  core->setCursor(None);
	  core->moveSelection(mx, my);
#ifndef NO_TEXT_SELECT
	  if (core->selectXMin != core->selectXMax &&
	      core->selectYMin != core->selectYMax) {
	    if (core->doc->okToCopy()) {
	      core->copySelection();
	    } else {
	      error(-1, "Copying of text from this document is not allowed.");
	    }
	  }
#endif
	}
	if (core->hyperlinksEnabled) {
	  if (core->selectXMin == core->selectXMax ||
	    core->selectYMin == core->selectYMax) {
	    core->doLink(mx, my);
	  }
	}
      }
    } else if (data->event->xbutton.button == 2) {
      core->panning = gFalse;
    } else {
      if (*core->mouseCbk) {
	(*core->mouseCbk)(core->mouseCbkData, data->event);
      }
    }
    break;
  case MotionNotify:
    if (core->doc && core->doc->getNumPages() > 0) {
      mx = core->scrollX + data->event->xbutton.x;
      my = core->scrollY + data->event->xbutton.y;
      if (core->dragging) {
	core->moveSelection(mx, my);
      } else if (core->hyperlinksEnabled) {
	core->out->cvtDevToUser(mx, my, &x, &y);
	if ((action = core->doc->findLink(x, y))) {
	  core->setCursor(core->linkCursor);
	  if (action != core->linkAction) {
	    core->linkAction = action;
	    if (core->updateCbk) {
	      s = "";
	      switch (action->getKind()) {
	      case actionGoTo:
		s = "[internal link]";
		break;
	      case actionGoToR:
		s = ((LinkGoToR *)action)->getFileName()->getCString();
		break;
	      case actionLaunch:
		s = ((LinkLaunch *)action)->getFileName()->getCString();
		break;
	      case actionURI:
		s = ((LinkURI *)action)->getURI()->getCString();
		break;
	      case actionNamed:
		s = ((LinkNamed *)action)->getName()->getCString();
		break;
	      case actionMovie:
		s = "[movie]";
		break;
	      case actionUnknown:
		s = "[unknown link]";
		break;
	      }
	      (*core->updateCbk)(core->updateCbkData, NULL, -1, -1, s);
	    }
	  }
	} else {
	  core->setCursor(None);
	  if (core->linkAction) {
	    core->linkAction = NULL;
	    if (core->updateCbk) {
	      (*core->updateCbk)(core->updateCbkData, NULL, -1, -1, "");
	    }
	  }
	}
      }
    }
    if (core->panning) {
      core->scrollTo(core->scrollX - (data->event->xbutton.x - core->panMX),
		     core->scrollY - (data->event->xbutton.y - core->panMY));
      core->panMX = data->event->xbutton.x;
      core->panMY = data->event->xbutton.y;
    }
    break;
  case KeyPress:
    n = XLookupString(&data->event->xkey, buf, sizeof(buf) - 1,
		      &key, NULL);
    core->keyPress(buf, key, data->event->xkey.state);
    break;
  }
}

void XPDFCore::keyPress(char *s, KeySym key, Guint modifiers) {
  switch (key) {
  case XK_Home:
  case XK_KP_Home:
    if (modifiers & ControlMask) {
      displayPage(1, zoom, rotate, gTrue, gTrue);
    } else if (!fullScreen) {
      scrollTo(0, 0);
    }
    return;
  case XK_End:
  case XK_KP_End:
    if (modifiers & ControlMask) {
      displayPage(doc->getNumPages(), zoom, rotate, gTrue, gTrue);
    } else if (!fullScreen) {
      scrollTo(out->getPixmapWidth() - drawAreaWidth,
	       out->getPixmapHeight() - drawAreaHeight);
    }
    return;
  case XK_Page_Up:
  case XK_KP_Page_Up:
    if (fullScreen) {
      gotoPrevPage(1, gTrue, gFalse);
    } else {
      scrollPageUp();
    }
    return;
  case XK_Page_Down:
  case XK_KP_Page_Down:
    if (fullScreen) {
      gotoNextPage(1, gTrue);
    } else {
      scrollPageDown();
    }
    return;
  case XK_Left:
  case XK_KP_Left:
    if (!fullScreen) {
      scrollLeft();
    }
    return;
  case XK_Right:
  case XK_KP_Right:
    if (!fullScreen) {
      scrollRight();
    }
    return;
  case XK_Up:
  case XK_KP_Up:
    if (!fullScreen) {
      scrollUp();
    }
    return;
  case XK_Down:
  case XK_KP_Down:
    if (!fullScreen) {
      scrollDown();
    }
    return;
  }

  if (*keyPressCbk) {
    (*keyPressCbk)(keyPressCbkData, s, key, modifiers);
  }
}

void XPDFCore::redrawRectangle(int x, int y, int w, int h) {
  XGCValues gcValues;
  Window drawAreaWin;

  // clip to window
  if (x < scrollX) {
    w -= scrollX - x;
    x = scrollX;
  }
  if (x + w > scrollX + drawAreaWidth) {
    w = scrollX + drawAreaWidth - x;
  }
  if (y < scrollY) {
    h -= scrollY - y;
    y = scrollY;
  }
  if (y + h > scrollY + drawAreaHeight) {
    h = scrollY + drawAreaHeight - y;
  }

  // create a GC for the drawing area
  drawAreaWin = XtWindow(drawArea);
  if (!drawAreaGC) {
    gcValues.foreground = paperColor;
    drawAreaGC = XCreateGC(display, drawAreaWin, GCForeground, &gcValues);
  }

  // draw white background past the edges of the document
  if (x + w > out->getPixmapWidth()) {
    XFillRectangle(display, drawAreaWin, drawAreaGC,
		   out->getPixmapWidth() - scrollX, y - scrollY,
		   x + w - out->getPixmapWidth(), h);
    w = out->getPixmapWidth() - x;
  }
  if (y + h > out->getPixmapHeight()) {
    XFillRectangle(display, drawAreaWin, drawAreaGC,
		   x - scrollX, out->getPixmapHeight() - scrollY,
		   w, y + h - out->getPixmapHeight());
    h = out->getPixmapHeight() - y;
  }

  // redraw (checking to see if pixmap has been allocated yet)
  if (out->getPixmapWidth() > 0) {
    XCopyArea(display, out->getPixmap(), drawAreaWin, drawAreaGC,
	      x, y, w, h, x - scrollX, y - scrollY);
  }
}

void XPDFCore::updateScrollBars() {
  Arg args[20];
  int n;
  int maxPos;

  maxPos = out ? out->getPixmapWidth() : 1;
  if (maxPos < drawAreaWidth) {
    maxPos = drawAreaWidth;
  }
  if (scrollX > maxPos - drawAreaWidth) {
    scrollX = maxPos - drawAreaWidth;
  }
  n = 0;
  XtSetArg(args[n], XmNvalue, scrollX); ++n;
  XtSetArg(args[n], XmNmaximum, maxPos); ++n;
  XtSetArg(args[n], XmNsliderSize, drawAreaWidth); ++n;
  XtSetArg(args[n], XmNincrement, 16); ++n;
  XtSetArg(args[n], XmNpageIncrement, drawAreaWidth); ++n;
  XtSetValues(hScrollBar, args, n);

  maxPos = out ? out->getPixmapHeight() : 1;
  if (maxPos < drawAreaHeight) {
    maxPos = drawAreaHeight;
  }
  if (scrollY > maxPos - drawAreaHeight) {
    scrollY = maxPos - drawAreaHeight;
  }
  n = 0;
  XtSetArg(args[n], XmNvalue, scrollY); ++n;
  XtSetArg(args[n], XmNmaximum, maxPos); ++n;
  XtSetArg(args[n], XmNsliderSize, drawAreaHeight); ++n;
  XtSetArg(args[n], XmNincrement, 16); ++n;
  XtSetArg(args[n], XmNpageIncrement, drawAreaHeight); ++n;
  XtSetValues(vScrollBar, args, n);
}

void XPDFCore::setCursor(Cursor cursor) {
  Window topWin;

  if (cursor == currentCursor) {
    return;
  }
  if (!(topWin = XtWindow(shell))) {
    return;
  }
  if (cursor == None) {
    XUndefineCursor(display, topWin);
  } else {
    XDefineCursor(display, topWin, cursor);
  }
  XFlush(display);
  currentCursor = cursor;
}

GBool XPDFCore::doQuestionDialog(char *title, GString *msg) {
  return doDialog(XmDIALOG_QUESTION, gTrue, title, msg);
}

void XPDFCore::doInfoDialog(char *title, GString *msg) {
  doDialog(XmDIALOG_INFORMATION, gFalse, title, msg);
}

void XPDFCore::doErrorDialog(char *title, GString *msg) {
  doDialog(XmDIALOG_ERROR, gFalse, title, msg);
}

GBool XPDFCore::doDialog(int type, GBool hasCancel,
			 char *title, GString *msg) {
  Widget dialog, scroll, text;
  XtAppContext appContext;
  Arg args[20];
  int n;
  XmString s1, s2;
  XEvent event;

  n = 0;
  XtSetArg(args[n], XmNdialogType, type); ++n;
  XtSetArg(args[n], XmNdialogStyle, XmDIALOG_PRIMARY_APPLICATION_MODAL); ++n;
  s1 = XmStringCreateLocalized(title);
  XtSetArg(args[n], XmNdialogTitle, s1); ++n;
  s2 = NULL; // make gcc happy
  if (msg->getLength() <= 80) {
    s2 = XmStringCreateLocalized(msg->getCString());
    XtSetArg(args[n], XmNmessageString, s2); ++n;
  }
  dialog = XmCreateMessageDialog(drawArea, "questionDialog", args, n);
  XmStringFree(s1);
  if (msg->getLength() <= 80) {
    XmStringFree(s2);
  } else {
    n = 0;
    XtSetArg(args[n], XmNscrollingPolicy, XmAUTOMATIC); ++n;
    if (drawAreaWidth > 300) {
      XtSetArg(args[n], XmNwidth, drawAreaWidth - 100); ++n;
    }
    scroll = XmCreateScrolledWindow(dialog, "scroll", args, n);
    XtManageChild(scroll);
    n = 0;
    XtSetArg(args[n], XmNeditable, False); ++n;
    XtSetArg(args[n], XmNeditMode, XmMULTI_LINE_EDIT); ++n;
    XtSetArg(args[n], XmNvalue, msg->getCString()); ++n;
    XtSetArg(args[n], XmNshadowThickness, 0); ++n;
    text = XmCreateText(scroll, "text", args, n);
    XtManageChild(text);
  }
  XtUnmanageChild(XmMessageBoxGetChild(dialog, XmDIALOG_HELP_BUTTON));
  XtAddCallback(dialog, XmNokCallback,
		&dialogOkCbk, (XtPointer)this);
  if (hasCancel) {
    XtAddCallback(dialog, XmNcancelCallback,
		  &dialogCancelCbk, (XtPointer)this);
  } else {
    XtUnmanageChild(XmMessageBoxGetChild(dialog, XmDIALOG_CANCEL_BUTTON));
  }

  XtManageChild(dialog);

  appContext = XtWidgetToApplicationContext(dialog);
  dialogDone = 0;
  do {
    XtAppNextEvent(appContext, &event);
    XtDispatchEvent(&event);
  } while (!dialogDone);

  XtUnmanageChild(dialog);
  XtDestroyWidget(dialog);

  return dialogDone > 0;
}

void XPDFCore::dialogOkCbk(Widget widget, XtPointer ptr,
			   XtPointer callData) {
  XPDFCore *core = (XPDFCore *)ptr;

  core->dialogDone = 1;
}

void XPDFCore::dialogCancelCbk(Widget widget, XtPointer ptr,
			       XtPointer callData) {
  XPDFCore *core = (XPDFCore *)ptr;

  core->dialogDone = -1;
}
