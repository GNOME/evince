//========================================================================
//
// XPDFCore.h
//
// Copyright 2002-2003 Glyph & Cog, LLC
//
//========================================================================

#ifndef XPDFCORE_H
#define XPDFCORE_H

#include <aconf.h>

#ifdef USE_GCC_PRAGMAS
#pragma interface
#endif

#define Object XtObject
#include <Xm/XmAll.h>
#undef Object
#include <aconf.h>
#include "gtypes.h"
#include "gfile.h" // for time_t

class GString;
class GList;
class BaseStream;
class PDFDoc;
class LinkAction;
class LinkDest;
class XPixmapOutputDev;

//------------------------------------------------------------------------
// zoom factor
//------------------------------------------------------------------------

#define minZoom    -5
#define maxZoom     5
#define zoomPage  100
#define zoomWidth 101
#define defZoom     1

//------------------------------------------------------------------------
// XPDFHistory
//------------------------------------------------------------------------

struct XPDFHistory {
  GString *fileName;
  int page;
};

#define xpdfHistorySize 50

//------------------------------------------------------------------------
// XPDFRegion
//------------------------------------------------------------------------

struct XPDFRegion {
  int page;
  double xMin, yMin, xMax, yMax;
  Gulong color;
  Gulong selectColor;
  GBool selectable;
};

//------------------------------------------------------------------------
// callbacks
//------------------------------------------------------------------------

typedef void (*XPDFUpdateCbk)(void *data, GString *fileName,
				int pageNum, int numPages, char *linkLabel);

typedef void (*XPDFActionCbk)(void *data, char *action);

typedef void (*XPDFKeyPressCbk)(void *data, char *s, KeySym key,
				Guint modifiers);

typedef void (*XPDFMouseCbk)(void *data, XEvent *event);

typedef GString *(*XPDFReqPasswordCbk)(void *data, GBool again);

//------------------------------------------------------------------------
// XPDFCore
//------------------------------------------------------------------------

class XPDFCore {
public:

  // Create viewer core inside <parentWidgetA>.
  XPDFCore(Widget shellA, Widget parentWidgetA,
	   Gulong paperColorA, GBool fullScreenA, GBool reverseVideo,
	   GBool installCmap, int rgbCubeSize);

  ~XPDFCore();

  //----- loadFile / displayPage / displayDest

  // Load a new file.  Returns pdfOk or error code.
  int loadFile(GString *fileName, GString *ownerPassword = NULL,
	       GString *userPassword = NULL);

  // Load a new file, via a Stream instead of a file name.  Returns
  // pdfOk or error code.
  int loadFile(BaseStream *stream, GString *ownerPassword = NULL,
	       GString *userPassword = NULL);

  // Resize the window to fit page <pg> of the current document.
  void resizeToPage(int pg);

  // Clear out the current document, if any.
  void clear();

  // Display (or redisplay) the specified page.  If <scrollToTop> is
  // set, the window is vertically scrolled to the top; otherwise, no
  // scrolling is done.  If <addToHist> is set, this page change is
  // added to the history list.
  void displayPage(int pageA, int zoomA, int rotateA,
		   GBool scrollToTop, GBool addToHist);

  // Display a link destination.
  void displayDest(LinkDest *dest, int zoomA, int rotateA,
		   GBool addToHist);

  //----- page/position changes

  void gotoNextPage(int inc, GBool top);
  void gotoPrevPage(int dec, GBool top, GBool bottom);
  void goForward();
  void goBackward();
  void scrollLeft(int nCols = 1);
  void scrollRight(int nCols = 1);
  void scrollUp(int nLines = 1);
  void scrollDown(int nLines = 1);
  void scrollPageUp();
  void scrollPageDown();
  void scrollTo(int x, int y);

  //----- selection

  void setSelection(int newXMin, int newYMin, int newXMax, int newYMax);
  void moveSelection(int mx, int my);
  void copySelection();
  GBool getSelection(int *xMin, int *yMin, int *xMax, int *yMax);
  GString *extractText(int xMin, int yMin, int xMax, int yMax);
  GString *extractText(int pageNum, int xMin, int yMin, int xMax, int yMax);

  //----- hyperlinks

  void doAction(LinkAction *action);


  //----- find

  void find(char *s);

  //----- simple modal dialogs

  GBool doQuestionDialog(char *title, GString *msg);
  void doInfoDialog(char *title, GString *msg);
  void doErrorDialog(char *title, GString *msg);

  //----- misc access

  Widget getWidget() { return scrolledWin; }
  Widget getDrawAreaWidget() { return drawArea; }
  PDFDoc *getDoc() { return doc; }
  XPixmapOutputDev *getOutputDev() { return out; }
  int getPageNum() { return page; }
  int getZoom() { return zoom; }
  double getZoomDPI() { return dpi; }
  int getRotate() { return rotate; }
  GBool canGoBack() { return historyBLen > 1; }
  GBool canGoForward() { return historyFLen > 0; }
  int getScrollX() { return scrollX; }
  int getScrollY() { return scrollY; }
  int getDrawAreaWidth() { return drawAreaWidth; }
  int getDrawAreaHeight() { return drawAreaHeight; }
  void setBusyCursor(GBool busy);
  Cursor getBusyCursor() { return busyCursor; }
  void takeFocus();
  void enableHyperlinks(GBool on) { hyperlinksEnabled = on; }
  void enableSelect(GBool on) { selectEnabled = on; }
  void setUpdateCbk(XPDFUpdateCbk cbk, void *data)
    { updateCbk = cbk; updateCbkData = data; }
  void setActionCbk(XPDFActionCbk cbk, void *data)
    { actionCbk = cbk; actionCbkData = data; }
  void setKeyPressCbk(XPDFKeyPressCbk cbk, void *data)
    { keyPressCbk = cbk; keyPressCbkData = data; }
  void setMouseCbk(XPDFMouseCbk cbk, void *data)
    { mouseCbk = cbk; mouseCbkData = data; }
  void setReqPasswordCbk(XPDFReqPasswordCbk cbk, void *data)
    { reqPasswordCbk = cbk; reqPasswordCbkData = data; }

private:

  //----- hyperlinks
  void doLink(int mx, int my);
  void runCommand(GString *cmdFmt, GString *arg);

  //----- selection
  static Boolean convertSelectionCbk(Widget widget, Atom *selection,
				     Atom *target, Atom *type,
				     XtPointer *value, unsigned long *length,
				     int *format);


  //----- GUI code
  void initWindow();
  static void hScrollChangeCbk(Widget widget, XtPointer ptr,
			       XtPointer callData);
  static void hScrollDragCbk(Widget widget, XtPointer ptr,
			     XtPointer callData);
  static void vScrollChangeCbk(Widget widget, XtPointer ptr,
			       XtPointer callData);
  static void vScrollDragCbk(Widget widget, XtPointer ptr,
			     XtPointer callData);
  static void resizeCbk(Widget widget, XtPointer ptr, XtPointer callData);
  static void redrawCbk(Widget widget, XtPointer ptr, XtPointer callData);
  static void outputDevRedrawCbk(void *data);
  static void inputCbk(Widget widget, XtPointer ptr, XtPointer callData);
  void keyPress(char *s, KeySym key, Guint modifiers);
  void redrawRectangle(int x, int y, int w, int h);
  void updateScrollBars();
  void setCursor(Cursor cursor);
  GBool doDialog(int type, GBool hasCancel,
		 char *title, GString *msg);
  static void dialogOkCbk(Widget widget, XtPointer ptr,
			  XtPointer callData);
  static void dialogCancelCbk(Widget widget, XtPointer ptr,
			      XtPointer callData);

  Gulong paperColor;
  GBool fullScreen;

  Display *display;
  int screenNum;
  Visual *visual;
  Colormap colormap;
  Widget shell;			// top-level shell containing the widget
  Widget parentWidget;		// parent widget (not created by XPDFCore)
  Widget scrolledWin;
  Widget hScrollBar;
  Widget vScrollBar;
  Widget drawAreaFrame;
  Widget drawArea;
  Cursor busyCursor, linkCursor, selectCursor;
  Cursor currentCursor;
  GC drawAreaGC;		// GC for blitting into drawArea
  GC selectGC;
  GC highlightGC;

  int drawAreaWidth, drawAreaHeight;
  int scrollX, scrollY;		// current upper-left corner

  int selectXMin, selectYMin,	// coordinates of current selection:
      selectXMax, selectYMax;	//   (xMin==xMax || yMin==yMax) means there
				//   is no selection
  GBool dragging;		// set while selection is being dragged
  GBool lastDragLeft;		// last dragged selection edge was left/right
  GBool lastDragTop;		// last dragged selection edge was top/bottom
  static GString *currentSelection;  // selected text
  static XPDFCore *currentSelectionOwner;
  static Atom targetsAtom;

  GBool panning;
  int panMX, panMY;

  XPDFHistory			// page history queue
    history[xpdfHistorySize];
  int historyCur;               // currently displayed page
  int historyBLen;              // number of valid entries backward from
                                //   current entry
  int historyFLen;              // number of valid entries forward from
                                //   current entry

  PDFDoc *doc;			// current PDF file
  int page;			// current page number
  int zoom;			// current zoom level
  double dpi;			// current zoom level, in DPI
  int rotate;			// current page rotation
  time_t modTime;		// last modification time of PDF file

  LinkAction *linkAction;	// mouse cursor is over this link


  XPDFUpdateCbk updateCbk;
  void *updateCbkData;
  XPDFActionCbk actionCbk;
  void *actionCbkData;
  XPDFKeyPressCbk keyPressCbk;
  void *keyPressCbkData;
  XPDFMouseCbk mouseCbk;
  void *mouseCbkData;
  XPDFReqPasswordCbk reqPasswordCbk;
  void *reqPasswordCbkData;

  GBool hyperlinksEnabled;
  GBool selectEnabled;

  XPixmapOutputDev *out;

  int dialogDone;
};

#endif
