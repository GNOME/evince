//========================================================================
//
// xpdf.cc
//
// Copyright 1996 Derek B. Noonburg
//
//========================================================================

#include <stdio.h>
#include <stdlib.h>
#include <stddef.h>
#include <string.h>
#include <X11/X.h>
#include <X11/cursorfont.h>
#include <X11/keysym.h>
#include "gtypes.h"
#include "GString.h"
#include "parseargs.h"
#include "gfile.h"
#include "gmem.h"
#include "LTKAll.h"
#include "Object.h"
#include "Stream.h"
#include "Array.h"
#include "Dict.h"
#include "XRef.h"
#include "Catalog.h"
#include "Page.h"
#include "Link.h"
#include "PDFDoc.h"
#include "XOutputDev.h"
#include "LTKOutputDev.h"
#include "PSOutputDev.h"
#include "TextOutputDev.h"
#include "Params.h"
#include "Error.h"
#include "config.h"

#ifdef XlibSpecificationRelease
#if XlibSpecificationRelease < 5
typedef char *XPointer;
#endif
#else
typedef char *XPointer;
#endif

// hack around old X includes which are missing these symbols
#ifndef XK_Page_Up
#define XK_Page_Up              0xFF55
#endif
#ifndef XK_Page_Down
#define XK_Page_Down            0xFF56
#endif

//------------------------------------------------------------------------
// misc constants / enums
//------------------------------------------------------------------------

#define remoteCmdLength 256

enum XpdfMenuItem {
  menuOpen,
  menuSavePDF,
  menuRotateLeft,
  menuRotateRight,
  menuQuit
};

//------------------------------------------------------------------------
// prototypes
//------------------------------------------------------------------------

// loadFile / displayPage
static GBool loadFile(GString *fileName);
static void displayPage(int page1, int zoom1, int rotate1);

// key press and menu callbacks
static void keyPressCbk(LTKWindow *win1, KeySym key, Guint modifiers,
			char *s, int n);
static void menuCbk(LTKMenuItem *item);

// mouse callbacks
static void buttonPressCbk(LTKWidget *canvas1, int n,
			   int mx, int my, int button, GBool dblClick);
static void buttonReleaseCbk(LTKWidget *canvas1, int n,
			     int mx, int my, int button, GBool click);
static void doLink(int mx, int my);
static void mouseMoveCbk(LTKWidget *widget, int widgetNum, int mx, int my);
static void mouseDragCbk(LTKWidget *widget, int widgetNum,
			 int mx, int my, int button);

// button callbacks
static void nextPageCbk(LTKWidget *button, int n, GBool on);
static void nextTenPageCbk(LTKWidget *button, int n, GBool on);
static void prevPageCbk(LTKWidget *button, int n, GBool on);
static void prevTenPageCbk(LTKWidget *button, int n, GBool on);
static void pageNumCbk(LTKWidget *textIn, int n, GString *text);
static void zoomInCbk(LTKWidget *button, int n, GBool on);
static void zoomOutCbk(LTKWidget *button, int n, GBool on);
static void postScriptCbk(LTKWidget *button, int n, GBool on);
static void aboutCbk(LTKWidget *button, int n, GBool on);
static void quitCbk(LTKWidget *button, int n, GBool on);

// scrollbar callbacks
static void scrollVertCbk(LTKWidget *scrollbar, int n, int val);
static void scrollHorizCbk(LTKWidget *scrollbar, int n, int val);

// misc callbacks
static void layoutCbk(LTKWindow *win1);
static void propChangeCbk(LTKWindow *win1, Atom atom);

// selection
static void setSelection(int newXMin, int newYMin, int newXMax, int newYMax);

// "Open" dialog
static void mapOpenDialog();
static void openButtonCbk(LTKWidget *button, int n, GBool on);
static void openSelectCbk(LTKWidget *widget, int n, GString *name);

// "Save PDF" dialog
static void mapSaveDialog();
static void saveButtonCbk(LTKWidget *button, int n, GBool on);
static void saveSelectCbk(LTKWidget *widget, int n, GString *name);

// "PostScript" dialog
static void mapPSDialog();
static void psButtonCbk(LTKWidget *button, int n, GBool on);

// "About" window
static void mapAboutWin();
static void closeAboutCbk(LTKWidget *button, int n, GBool on);

// "Find" window
static void findCbk(LTKWidget *button, int n, GBool on);
static void mapFindWin();
static void findButtonCbk(LTKWidget *button, int n, GBool on);
static void doFind(char *s);

// app kill callback
static void killCbk(LTKWindow *win1);

//------------------------------------------------------------------------
// GUI includes
//------------------------------------------------------------------------

#include "xpdfIcon.xpm"
#include "leftArrow.xbm"
#include "dblLeftArrow.xbm"
#include "rightArrow.xbm"
#include "dblRightArrow.xbm"
#include "zoomIn.xbm"
#include "zoomOut.xbm"
#include "find.xbm"
#include "postscript.xbm"
#include "about.xbm"
#include "xpdf-ltk.h"

//------------------------------------------------------------------------
// command line options
//------------------------------------------------------------------------

static XrmOptionDescRec opts[] = {
  {"-display",       ".display",       XrmoptionSepArg,  NULL},
  {"-foreground",    ".foreground",    XrmoptionSepArg,  NULL},
  {"-fg",            ".foreground",    XrmoptionSepArg,  NULL},
  {"-background",    ".background",    XrmoptionSepArg,  NULL},
  {"-bg",            ".background",    XrmoptionSepArg,  NULL},
  {"-geometry",      ".geometry",      XrmoptionSepArg,  NULL},
  {"-g",             ".geometry",      XrmoptionSepArg,  NULL},
  {"-font",          ".font",          XrmoptionSepArg,  NULL},
  {"-fn",            ".font",          XrmoptionSepArg,  NULL},
  {"-cmap",          ".installCmap",   XrmoptionNoArg,   (XPointer)"on"},
  {"-rgb",           ".rgbCubeSize",   XrmoptionSepArg,  NULL},
  {"-papercolor",    ".paperColor",    XrmoptionSepArg,  NULL},
  {"-z",             ".initialZoom",   XrmoptionSepArg,  NULL},
  {"-ps",            ".psFile",        XrmoptionSepArg,  NULL},
  {"-paperw",        ".psPaperWidth",  XrmoptionSepArg,  NULL},
  {"-paperh",        ".psPaperHeight", XrmoptionSepArg,  NULL},
  {"-level1",        ".psLevel1",      XrmoptionNoArg,   (XPointer)"false"},
  {NULL}
};

GBool printCommands = gFalse;
static GBool printHelp = gFalse;
static char remoteName[100] = "xpdf_";
static GBool doRemoteRaise = gFalse;
static GBool doRemoteQuit = gFalse;

static ArgDesc argDesc[] = {
  {"-err",        argFlag,        &errorsToTTY,   0,
   "send error messages to /dev/tty instead of stderr"},
  {"-z",          argIntDummy,    NULL,           0,
   "initial zoom level (-5..5)"},
  {"-g",          argStringDummy, NULL,           0,
   "initial window geometry"},
  {"-geometry",   argStringDummy, NULL,           0,
   "initial window geometry"},
  {"-remote",     argString,      remoteName + 5, sizeof(remoteName) - 5,
   "start/contact xpdf remote server with specified name"},
  {"-raise",      argFlag,        &doRemoteRaise, 0,
   "raise xpdf remote server window (with -remote only)"},
  {"-quit",       argFlag,        &doRemoteQuit,  0,
   "kill xpdf remote server (with -remote only)"},
  {"-cmap",       argFlagDummy,   NULL,           0,
   "install a private colormap"},
  {"-rgb",        argIntDummy,    NULL,           0,
   "biggest RGB cube to allocate (default is 5)"},
  {"-papercolor", argStringDummy, NULL,           0,
   "color of paper background"},
  {"-ps",         argStringDummy, NULL,           0,
   "default PostScript file/command name"},
  {"-paperw",     argIntDummy,    NULL,           0,
   "paper width, in points"},
  {"-paperh",     argIntDummy,    NULL,           0,
   "paper height, in points"},
  {"-level1",     argFlagDummy,   NULL,           0,
   "generate Level 1 PostScript"},
  {"-cmd",        argFlag,        &printCommands, 0,
   "print commands as they're executed"},
  {"-h",          argFlag,        &printHelp,     0,
   "print usage information"},
  {"-help",       argFlag,        &printHelp,     0,
   "print usage information"},
  {NULL}
};

//------------------------------------------------------------------------
// global variables
//------------------------------------------------------------------------

// zoom factor is 1.2 (similar to DVI magsteps)
#define minZoom -5
#define maxZoom  5
static int zoomDPI[maxZoom - minZoom + 1] = {
  29, 35, 42, 50, 60,
  72,
  86, 104, 124, 149, 179
};
#define defZoom 1

static PDFDoc *doc;

static LTKOutputDev *out;

static int page;
static int zoom;
static int rotate;
static GBool quit;

static LinkAction *linkAction;	// mouse pointer is over this link
static int			// coordinates of current selection:
  selectXMin, selectYMin,	//   (xMin==xMax || yMin==yMax) means there
  selectXMax, selectYMax;	//   is no selection
static GBool lastDragLeft;	// last dragged selection edge was left/right
static GBool lastDragTop;	// last dragged selection edge was top/bottom
static int panMX, panMY;	// last mouse position for pan

static GString *defPSFileName;
static GString *psFileName;
static int psFirstPage, psLastPage;

static GString *fileReqDir;	// current directory for file requesters

static GString *urlCommand;	// command to execute for URI links

static LTKApp *app;
static Display *display;
static LTKWindow *win;
static LTKScrollingCanvas *canvas;
static LTKScrollbar *hScrollbar, *vScrollbar;
static LTKTextIn *pageNumText;
static LTKLabel *numPagesLabel;
static LTKLabel *linkLabel;
static LTKWindow *aboutWin;
static LTKWindow *psDialog;
static LTKWindow *openDialog;
static LTKWindow *saveDialog;
static LTKWindow *findWin;
static Atom remoteAtom;
static GC selectGC;

//------------------------------------------------------------------------
// main program
//------------------------------------------------------------------------

int main(int argc, char *argv[]) {
  Window xwin;
  XGCValues gcValues;
  char cmd[remoteCmdLength];
  LTKMenu *menu;
  GString *name;
  GString *title;
  unsigned long paperColor;
  int pg;
  int x, y;
  Guint width, height;
  GBool ok;
  char s[20];
  int ret;

  // initialize
  app = NULL;
  win = NULL;
  out = NULL;
  remoteAtom = None;
  doc = NULL;
  xref = NULL;
  psFileName = NULL;
  fileReqDir = getCurrentDir();
  ret = 0;

  // parse args
  paperWidth = paperHeight = -1;
  ok = parseArgs(argDesc, &argc, argv);

  // init error file
  errorInit();

  // read config file
  initParams(xpdfConfigFile);

  // create LTKApp (and parse X-related args)
  app = new LTKApp("xpdf", opts, &argc, argv);
  app->setKillCbk(&killCbk);
  display = app->getDisplay();

  // check command line
  if (doRemoteRaise)
    ok = ok && remoteName[5] && !doRemoteQuit && argc >= 1 && argc <= 3;
  else if (doRemoteQuit)
    ok = ok && remoteName[5] && argc == 1;
  else
    ok = ok && argc >= 1 && argc <= 3;
  if (!ok || printHelp) {
    fprintf(stderr, "xpdf version %s\n", xpdfVersion);
    fprintf(stderr, "%s\n", xpdfCopyright);
    printUsage("xpdf", "[<PDF-file> [<page>]]", argDesc);
    ret = 1;
    goto done2;
  }
  if (argc >= 2)
    name = new GString(argv[1]);
  else
    name = NULL;
  if (argc == 3)
    pg = atoi(argv[2]);
  else
    pg = 1;

  // look for already-running remote server
  if (remoteName[5]) {
    remoteAtom = XInternAtom(display, remoteName, False);
    xwin = XGetSelectionOwner(display, remoteAtom);
    if (xwin != None) {
      if (name) {
	sprintf(cmd, "%c %d %.200s", doRemoteRaise ? 'D' : 'd',
		pg, name->getCString());
	XChangeProperty(display, xwin, remoteAtom, remoteAtom, 8,
			PropModeReplace, (Guchar *)cmd, strlen(cmd) + 1);
	delete name;
      } else if (doRemoteRaise) {
	XChangeProperty(display, xwin, remoteAtom, remoteAtom, 8,
			PropModeReplace, (Guchar *)"r", 2);
      } else if (doRemoteQuit) {
	XChangeProperty(display, xwin, remoteAtom, remoteAtom, 8,
			PropModeReplace, (Guchar *)"q", 2);
      }
      goto done2;
    }
    if (doRemoteQuit)
      goto done2;
  }

  // print banner
  fprintf(errFile, "xpdf version %s\n", xpdfVersion);
  fprintf(errFile, "%s\n", xpdfCopyright);

  // open PDF file
  defPSFileName = app->getStringResource("psFile", NULL);
  if (name) {
    if (!loadFile(name)) {
      ret = 1;
      goto done1;
    }
    delete fileReqDir;
    fileReqDir = makePathAbsolute(grabPath(name->getCString()));
  }

  // check for legal page number
  if (doc && (pg < 1 || pg > doc->getNumPages()))
    pg = 1;

  // create window
  win = makeWindow(app);
  menu = makeMenu();
  win->setMenu(menu);
  canvas = (LTKScrollingCanvas *)win->findWidget("canvas");
  hScrollbar = (LTKScrollbar *)win->findWidget("hScrollbar");
  vScrollbar = (LTKScrollbar *)win->findWidget("vScrollbar");
  pageNumText = (LTKTextIn *)win->findWidget("pageNum");
  numPagesLabel = (LTKLabel *)win->findWidget("numPages");
  linkLabel = (LTKLabel *)win->findWidget("link");
  win->setKeyCbk(&keyPressCbk);
  win->setLayoutCbk(&layoutCbk);
  canvas->setButtonPressCbk(&buttonPressCbk);
  canvas->setButtonReleaseCbk(&buttonReleaseCbk);
  canvas->setMouseMoveCbk(&mouseMoveCbk);
  canvas->setMouseDragCbk(&mouseDragCbk);
  hScrollbar->setRepeatPeriod(0);
  vScrollbar->setRepeatPeriod(0);

  // get X resources
  paperWidth = app->getIntResource("psPaperWidth", defPaperWidth);
  paperHeight = app->getIntResource("psPaperHeight", defPaperHeight);
  psOutLevel1 = app->getBoolResource("psLevel1", gFalse);
  urlCommand = app->getStringResource("urlCommand", NULL);
  installCmap = app->getBoolResource("installCmap", gFalse);
  if (installCmap)
    win->setInstallCmap(gTrue);
  rgbCubeSize = app->getIntResource("rgbCubeSize", defaultRGBCube);
  paperColor = app->getColorResource("paperColor", "white",
				     WhitePixel(display, app->getScreenNum()),
				     NULL);
  zoom = app->getIntResource("initialZoom", defZoom);
  if (zoom < minZoom)
    zoom = minZoom;
  else if (zoom > maxZoom)
    zoom = maxZoom;

  // get geometry
  x = -1;
  y = -1;
  if (!doc) {
    width = 612;
    height = 792;
  } else if (doc->getPageRotate(pg) == 90 || doc->getPageRotate(pg) == 270) {
    width = (int)(doc->getPageHeight(pg) + 0.5);
    height = (int)(doc->getPageWidth(pg) + 0.5);
  } else {
    width = (int)(doc->getPageWidth(pg) + 0.5);
    height = (int)(doc->getPageHeight(pg) + 0.5);
  }
  width = (width * zoomDPI[zoom - minZoom]) / 72 + 28;
  if (width > (Guint)app->getDisplayWidth() - 100)
    width = app->getDisplayWidth() - 100;
  height = (height * zoomDPI[zoom - minZoom]) / 72 + 56;
  if (height > (Guint)app->getDisplayHeight() - 100)
    height = app->getDisplayHeight() - 100;
  app->getGeometryResource("geometry", &x, &y, &width, &height);

  // finish setting up window
  sprintf(s, "of %d", doc ? doc->getNumPages() : 0);
  numPagesLabel->setText(s);
  if (name) {
    title = new GString("xpdf: ");
    title->append(name);
  } else {
    title = new GString("xpdf");
  }
  win->setTitle(title);
  win->layout(x, y, width, height);
  win->map();
  aboutWin = NULL;
  psDialog = NULL;
  openDialog = NULL;
  saveDialog = NULL;
  findWin = NULL;
  gcValues.foreground = BlackPixel(display, win->getScreenNum()) ^
                        WhitePixel(display, win->getScreenNum());
  gcValues.function = GXxor;
  selectGC = XCreateGC(display, win->getXWindow(),
		       GCForeground | GCFunction, &gcValues);

  // set up remote server
  if (remoteAtom != None) {
    win->setPropChangeCbk(&propChangeCbk);
    xwin = win->getXWindow();
    XSetSelectionOwner(display, remoteAtom, xwin, CurrentTime);
  }

  // create output device
  out = new LTKOutputDev(win, paperColor);

  // display first page
  displayPage(pg, zoom, 0);

  // event loop
  quit = gFalse;
  do {
    app->doEvent(gTrue);
  } while (!quit);

 done1:
  // release remote control atom
  if (remoteAtom != None)
    XSetSelectionOwner(display, remoteAtom, None, CurrentTime);

 done2:
  // free stuff
  if (out)
    delete out;
  if (win)
    delete win;
  if (aboutWin)
    delete aboutWin;
  if (findWin)
    delete findWin;
  if (app)
    delete app;
  if (doc)
    delete doc;
  if (psFileName)
    delete psFileName;
  if (defPSFileName)
    delete defPSFileName;
  if (fileReqDir)
    delete fileReqDir;
  if (urlCommand)
    delete urlCommand;
  freeParams();

  // check for memory leaks
  Object::memCheck(errFile);
  gMemReport(errFile);

  return ret;
}

//------------------------------------------------------------------------
// loadFile / displayPage
//------------------------------------------------------------------------

static GBool loadFile(GString *fileName) {
  GString *title;
  PDFDoc *newDoc;
  char s[20];
  char *p;

  // busy cursor
  if (win)
    win->setBusyCursor(gTrue);

  // open PDF file
  newDoc = new PDFDoc(fileName);
  if (!newDoc->isOk()) {
    delete newDoc;
    if (win)
      win->setBusyCursor(gFalse);
    return gFalse;
  }

  // replace old document
  if (doc)
    delete doc;
  doc = newDoc;

  // nothing displayed yet
  page = -99;

  // init PostScript output params
  if (psFileName)
    delete psFileName;
  if (defPSFileName) {
    psFileName = defPSFileName->copy();
  } else {
    p = fileName->getCString() + fileName->getLength() - 4;
    if (!strcmp(p, ".pdf") || !strcmp(p, ".PDF"))
      psFileName = new GString(fileName->getCString(),
			       fileName->getLength() - 4);
    else
      psFileName = fileName->copy();
    psFileName->append(".ps");
  }
  psFirstPage = 1;
  psLastPage = doc->getNumPages();

  // set up title, number-of-pages display; back to normal cursor
  if (win) {
    title = new GString("xpdf: ");
    title->append(fileName);
    win->setTitle(title);
    sprintf(s, "of %d", doc->getNumPages());
    numPagesLabel->setText(s);
    win->setBusyCursor(gFalse);
  }

  // done
  return gTrue;
}

static void displayPage(int page1, int zoom1, int rotate1) {
  char s[20];

  // check for document
  if (!doc)
    return;

  // busy cursor
  if (win)
    win->setBusyCursor(gTrue);

  // new page/zoom/rotate values
  page = page1;
  zoom = zoom1;
  rotate = rotate1;

  // initialize mouse-related stuff
  linkAction = NULL;
  win->setDefaultCursor();
  linkLabel->setText(NULL);
  selectXMin = selectXMax = 0;
  selectYMin = selectYMax = 0;
  lastDragLeft = lastDragTop = gTrue;

  // draw the page
  doc->displayPage(out, page, zoomDPI[zoom - minZoom], rotate, gTrue);
  layoutCbk(win);

  // update page number display
  sprintf(s, "%d", page);
  pageNumText->setText(s);

  // back to regular cursor
  win->setBusyCursor(gFalse);
}

//------------------------------------------------------------------------
// key press and menu callbacks
//------------------------------------------------------------------------

static void keyPressCbk(LTKWindow *win1, KeySym key, Guint modifiers,
			char *s, int n) {
  if (n > 0) {
    switch (s[0]) {
    case 'O':
    case 'o':
      mapOpenDialog();
      break;
    case 'F':
    case 'f':
      mapFindWin();
      break;
    case 'N':
    case 'n':
      nextPageCbk(NULL, 0, gTrue);
      break;
    case 'P':
    case 'p':
      prevPageCbk(NULL, 0, gTrue);
      break;
    case ' ':
      if (vScrollbar->getPos() >=
	  canvas->getRealHeight() - canvas->getHeight()) {
	nextPageCbk(NULL, 0, gTrue);
      } else {
	vScrollbar->setPos(vScrollbar->getPos() + canvas->getHeight(),
			   canvas->getHeight());
	canvas->scroll(hScrollbar->getPos(), vScrollbar->getPos());
      }
      break;
    case '\b':			// bs
    case '\177':		// del
      if (vScrollbar->getPos() == 0) {
	prevPageCbk(NULL, 0, gTrue);
      } else {
	vScrollbar->setPos(vScrollbar->getPos() - canvas->getHeight(),
			   canvas->getHeight());
	canvas->scroll(hScrollbar->getPos(), vScrollbar->getPos());
      }
      break;
    case '\014':		// ^L
      win->redraw();
      displayPage(page, zoom, rotate);
      break;
    case 'Q':
    case 'q':
      quitCbk(NULL, 0, gTrue);
      break;
    }
  } else {
    switch (key) {
    case XK_Home:
      hScrollbar->setPos(0, canvas->getWidth());
      vScrollbar->setPos(0, canvas->getHeight());
      canvas->scroll(hScrollbar->getPos(), vScrollbar->getPos());
      break;
    case XK_End:
      hScrollbar->setPos(canvas->getRealWidth() - canvas->getWidth(),
			 canvas->getWidth());
      vScrollbar->setPos(canvas->getRealHeight() - canvas->getHeight(),
			 canvas->getHeight());
      canvas->scroll(hScrollbar->getPos(), vScrollbar->getPos());
      break;
    case XK_Page_Up:
      if (vScrollbar->getPos() == 0) {
	prevPageCbk(NULL, 0, gTrue);
      } else {
	vScrollbar->setPos(vScrollbar->getPos() - canvas->getHeight(),
			   canvas->getHeight());
	canvas->scroll(hScrollbar->getPos(), vScrollbar->getPos());
      }
      break;
    case XK_Page_Down:
      if (vScrollbar->getPos() >=
	  canvas->getRealHeight() - canvas->getHeight()) {
	nextPageCbk(NULL, 0, gTrue);
      } else {
	vScrollbar->setPos(vScrollbar->getPos() + canvas->getHeight(),
			   canvas->getHeight());
	canvas->scroll(hScrollbar->getPos(), vScrollbar->getPos());
      }
      break;
    case XK_Left:
      hScrollbar->setPos(hScrollbar->getPos() - 16, canvas->getWidth());
      canvas->scroll(hScrollbar->getPos(), vScrollbar->getPos());
      break;
    case XK_Right:
      hScrollbar->setPos(hScrollbar->getPos() + 16, canvas->getWidth());
      canvas->scroll(hScrollbar->getPos(), vScrollbar->getPos());
      break;
    case XK_Up:
      vScrollbar->setPos(vScrollbar->getPos() - 16, canvas->getHeight());
      canvas->scroll(hScrollbar->getPos(), vScrollbar->getPos());
      break;
    case XK_Down:
      vScrollbar->setPos(vScrollbar->getPos() + 16, canvas->getHeight());
      canvas->scroll(hScrollbar->getPos(), vScrollbar->getPos());
      break;
    }
  }
}

static void menuCbk(LTKMenuItem *item) {
  int r;

  switch (item->getItemNum()) {
  case menuOpen:
    mapOpenDialog();
    break;
  case menuSavePDF:
    if (doc)
      mapSaveDialog();
    break;
  case menuRotateLeft:
    if (doc) {
      r = (rotate == 0) ? 270 : rotate - 90;
      displayPage(page, zoom, r);
    }
    break;
  case menuRotateRight:
    if (doc) {
      r = (rotate == 270) ? 0 : rotate + 90;
      displayPage(page, zoom, r);
    }
    break;
  case menuQuit:
    quit = gTrue;
    break;
  }
}

//------------------------------------------------------------------------
// mouse callbacks
//------------------------------------------------------------------------

static void buttonPressCbk(LTKWidget *canvas1, int n,
			   int mx, int my, int button, GBool dblClick) {
  if (!doc)
    return;
  if (button == 1) {
    setSelection(mx, my, mx, my);
  } else if (button == 2) {
    panMX = mx - hScrollbar->getPos();
    panMY = my - vScrollbar->getPos();
  }
}

static void buttonReleaseCbk(LTKWidget *canvas1, int n,
			     int mx, int my, int button, GBool click) {
  GString *s;

  if (!doc)
    return;

  if (button == 1) {
    // selection
    if (selectXMin < selectXMax && selectYMin < selectYMax) {
#ifndef NO_TEXT_SELECT
      if (doc->okToCopy()) {
	s = out->getText(selectXMin, selectYMin, selectXMax, selectYMax);
	win->setSelection(NULL, s);
      }
#endif

    // link
    } else {
      setSelection(mx, my, mx, my);
      doLink(mx, my);
    }
  }
}

static void doLink(int mx, int my) {
  LinkActionKind kind;
  LinkAction *action = NULL;
  LinkDest *dest;
  GString *namedDest;
  char *s;
  GString *fileName;
  Ref pageRef;
  int pg;
  double x, y;
  int dx, dy;
  LTKButtonDialog *dialog;

  // look for a link
  out->cvtDevToUser(mx, my, &x, &y);
  if ((action = doc->findLink(x, y))) {
    switch (kind = action->getKind()) {

    // GoTo / GoToR action
    case actionGoTo:
    case actionGoToR:
      if (kind == actionGoTo) {
	dest = NULL;
	namedDest = NULL;
	if ((dest = ((LinkGoTo *)action)->getDest()))
	  dest = dest->copy();
	else if ((namedDest = ((LinkGoTo *)action)->getNamedDest()))
	  namedDest = namedDest->copy();
      } else {
	dest = NULL;
	namedDest = NULL;
	if ((dest = ((LinkGoToR *)action)->getDest()))
	  dest = dest->copy();
	else if ((namedDest = ((LinkGoToR *)action)->getNamedDest()))
	  namedDest = namedDest->copy();
	s = ((LinkGoToR *)action)->getFileName()->getCString();
	//~ translate path name for VMS (deal with '/')
	if (isAbsolutePath(s))
	  fileName = new GString(s);
	else
	  fileName = appendToPath(
			 grabPath(doc->getFileName()->getCString()), s);
	if (!loadFile(fileName)) {
	  if (dest)
	    delete dest;
	  if (namedDest)
	    delete namedDest;
	  return;
	}
      }
      if (namedDest) {
	dest = doc->findDest(namedDest);
	delete namedDest;
      }
      if (!dest) {
	if (kind == actionGoToR)
	  displayPage(1, zoom, 0);
      } else {
	if (dest->isPageRef()) {
	  pageRef = dest->getPageRef();
	  pg = doc->findPage(pageRef.num, pageRef.gen);
	} else {
	  pg = dest->getPageNum();
	}
	if (pg > 0 && pg != page)
	  displayPage(pg, zoom, rotate);
	else if (pg <= 0)
	  displayPage(1, zoom, rotate);
	switch (dest->getKind()) {
	case destXYZ:
	  out->cvtUserToDev(dest->getLeft(), dest->getTop(), &dx, &dy);
	  if (dest->getChangeLeft() || dest->getChangeTop()) {
	    if (dest->getChangeLeft())
	      hScrollbar->setPos(dx, canvas->getWidth());
	    if (dest->getChangeTop())
	      vScrollbar->setPos(dy, canvas->getHeight());
	    canvas->scroll(hScrollbar->getPos(), vScrollbar->getPos());
	  }
	  //~ what is the zoom parameter?
	  break;
	case destFit:
	case destFitB:
	  //~ do fit
	  hScrollbar->setPos(0, canvas->getWidth());
	  vScrollbar->setPos(0, canvas->getHeight());
	  canvas->scroll(hScrollbar->getPos(), vScrollbar->getPos());
	  break;
	case destFitH:
	case destFitBH:
	  //~ do fit
	  out->cvtUserToDev(0, dest->getTop(), &dx, &dy);
	  hScrollbar->setPos(0, canvas->getWidth());
	  vScrollbar->setPos(dy, canvas->getHeight());
	  canvas->scroll(hScrollbar->getPos(), vScrollbar->getPos());
	  break;
	case destFitV:
	case destFitBV:
	  //~ do fit
	  out->cvtUserToDev(dest->getLeft(), 0, &dx, &dy);
	  hScrollbar->setPos(dx, canvas->getWidth());
	  vScrollbar->setPos(0, canvas->getHeight());
	  canvas->scroll(hScrollbar->getPos(), vScrollbar->getPos());
	  break;
	case destFitR:
	  //~ do fit
	  out->cvtUserToDev(dest->getLeft(), dest->getTop(), &dx, &dy);
	  hScrollbar->setPos(dx, canvas->getWidth());
	  vScrollbar->setPos(dy, canvas->getHeight());
	  canvas->scroll(hScrollbar->getPos(), vScrollbar->getPos());
	  break;
	}
	delete dest;
      }
      break;

    // Launch action
    case actionLaunch:
      fileName = ((LinkLaunch *)action)->getFileName();
      s = fileName->getCString();
      if (!strcmp(s + fileName->getLength() - 4, ".pdf") ||
	  !strcmp(s + fileName->getLength() - 4, ".PDF")) {
	//~ translate path name for VMS (deal with '/')
	if (isAbsolutePath(s))
	  fileName = fileName->copy();
	else
	  fileName = appendToPath(
		         grabPath(doc->getFileName()->getCString()), s);
	if (!loadFile(fileName))
	  return;
	displayPage(1, zoom, rotate);
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
	dialog = new LTKButtonDialog(win, "xpdf: Launch",
				     "Execute the command:",
				     fileName->getCString(),
				     NULL, "Ok", "Cancel");
	if (dialog->go())
	  system(fileName->getCString());
	delete dialog;
	delete fileName;
      }
      break;

    // URI action
    case actionURI:
      if (urlCommand) {
	for (s = urlCommand->getCString(); *s; ++s) {
	  if (s[0] == '%' && s[1] == 's')
	    break;
	}
	if (s) {
	  fileName = new GString(urlCommand->getCString(),
				 s - urlCommand->getCString());
	  fileName->append(((LinkURI *)action)->getURI());
	  fileName->append(s+2);
	} else {
	  fileName = urlCommand->copy();
	}
#ifdef VMS
	fileName->insert(0, "spawn/nowait ");
#elif defined(__EMX__)
	fileName->insert(0, "start /min /n ");
#else
	fileName->append(" &");
#endif
	system(fileName->getCString());
	delete fileName;
      } else {
	fprintf(errFile, "URI: %s\n",
		((LinkURI *)action)->getURI()->getCString());
      }
      break;

    // unknown action type
    case actionUnknown:
      error(-1, "Unknown link action type: '%s'",
	    ((LinkUnknown *)action)->getAction()->getCString());
      break;
    }
  }
}

static void mouseMoveCbk(LTKWidget *widget, int widgetNum, int mx, int my) {
  double x, y;
  LinkAction *action;
  char *s;

  if (!doc)
    return;
  out->cvtDevToUser(mx, my, &x, &y);
  if ((action = doc->findLink(x, y))) {
    if (action != linkAction) {
      if (!linkAction)
	win->setCursor(XC_hand2);
      linkAction = action;
      s = NULL;
      switch (linkAction->getKind()) {
      case actionGoTo:
	s = "[internal link]";
	break;
      case actionGoToR:
	s = ((LinkGoToR *)linkAction)->getFileName()->getCString();
	break;
      case actionLaunch:
	s = ((LinkLaunch *)linkAction)->getFileName()->getCString();
	break;
      case actionURI:
	s = ((LinkURI *)action)->getURI()->getCString();
	break;
      case actionUnknown:
	s = "[unknown link]";
	break;
      }
      linkLabel->setText(s);
    }
  } else {
    if (linkAction) {
      linkAction = NULL;
      win->setDefaultCursor();
      linkLabel->setText(NULL);
    }
  }
}

static void mouseDragCbk(LTKWidget *widget, int widgetNum,
			 int mx, int my, int button) {
  int x, y;
  int xMin, yMin, xMax, yMax;

  // button 1: select
  if (button == 1) {

    // clip mouse coords
    x = mx;
    if (x < 0)
      x = 0;
    else if (x >= canvas->getRealWidth())
      x = canvas->getRealWidth() - 1;
    y = my;
    if (y < 0)
      y = 0;
    else if (y >= canvas->getRealHeight())
      y = canvas->getRealHeight() - 1;

    // move appropriate edges of selection
    if (lastDragLeft) {
      if (x < selectXMax) {
	xMin = x;
	xMax = selectXMax;
      } else {
	xMin = selectXMax;
	xMax = x;
	lastDragLeft = gFalse;
      }      
    } else {
      if (x > selectXMin) {
	xMin = selectXMin;
	xMax = x;
      } else {
	xMin = x;
	xMax = selectXMin;
	lastDragLeft = gTrue;
      }
    }
    if (lastDragTop) {
      if (y < selectYMax) {
	yMin = y;
	yMax = selectYMax;
      } else {
	yMin = selectYMax;
	yMax = y;
	lastDragTop = gFalse;
      }
    } else {
      if (y > selectYMin) {
	yMin = selectYMin;
	yMax = y;
      } else {
	yMin = y;
	yMax = selectYMin;
	lastDragTop = gTrue;
      }
    }

    // redraw the selection
    setSelection(xMin, yMin, xMax, yMax);

  // button 2: pan
  } else if (button == 2) {
    mx -= hScrollbar->getPos();
    my -= vScrollbar->getPos();
    hScrollbar->setPos(hScrollbar->getPos() - (mx - panMX),
		       canvas->getWidth());
    vScrollbar->setPos(vScrollbar->getPos() - (my - panMY),
		       canvas->getHeight());
    canvas->scroll(hScrollbar->getPos(), vScrollbar->getPos());
    panMX = mx;
    panMY = my;
  }
}

//------------------------------------------------------------------------
// button callbacks
//------------------------------------------------------------------------

static void nextPageCbk(LTKWidget *button, int n, GBool on) {
  if (!doc)
    return;
  if (page < doc->getNumPages()) {
    vScrollbar->setPos(0, canvas->getHeight());
    canvas->scroll(hScrollbar->getPos(), vScrollbar->getPos());
    displayPage(page + 1, zoom, rotate);
  } else {
    XBell(display, 0);
  }
}

static void nextTenPageCbk(LTKWidget *button, int n, GBool on) {
  int pg;

  if (!doc)
    return;
  if (page < doc->getNumPages()) {
    vScrollbar->setPos(0, canvas->getHeight());
    canvas->scroll(hScrollbar->getPos(), vScrollbar->getPos());
    if ((pg = page + 10) > doc->getNumPages())
      pg = doc->getNumPages();
    displayPage(pg, zoom, rotate);
  } else {
    XBell(display, 0);
  }
}

static void prevPageCbk(LTKWidget *button, int n, GBool on) {
  if (!doc)
    return;
  if (page > 1) {
    vScrollbar->setPos(0, canvas->getHeight());
    canvas->scroll(hScrollbar->getPos(), vScrollbar->getPos());
    displayPage(page - 1, zoom, rotate);
  } else {
    XBell(display, 0);
  }
}

static void prevTenPageCbk(LTKWidget *button, int n, GBool on) {
  int pg;

  if (!doc)
    return;
  if (page > 1) {
    vScrollbar->setPos(0, canvas->getHeight());
    canvas->scroll(hScrollbar->getPos(), vScrollbar->getPos());
    if ((pg = page - 10) < 1)
      pg = 1;
    displayPage(pg, zoom, rotate);
  } else {
    XBell(display, 0);
  }
}

static void pageNumCbk(LTKWidget *textIn, int n, GString *text) {
  int page1;
  char s[20];

  if (!doc)
    return;
  page1 = atoi(text->getCString());
  if (page1 >= 1 && page1 <= doc->getNumPages()) {
    if (page1 != page)
      displayPage(page1, zoom, rotate);
  } else {
    XBell(display, 0);
    sprintf(s, "%d", page);
    pageNumText->setText(s);
  }
}

static void zoomInCbk(LTKWidget *button, int n, GBool on) {
  if (!doc)
    return;
  if (zoom < maxZoom)
    displayPage(page, zoom + 1, rotate);
  else
    XBell(display, 0);
}

static void zoomOutCbk(LTKWidget *button, int n, GBool on) {
  if (!doc)
    return;
  if (zoom > minZoom)
    displayPage(page, zoom - 1, rotate);
  else
    XBell(display, 0);
}

static void postScriptCbk(LTKWidget *button, int n, GBool on) {
  if (!doc)
    return;
  mapPSDialog();
}

static void aboutCbk(LTKWidget *button, int n, GBool on) {
  mapAboutWin();
}

static void quitCbk(LTKWidget *button, int n, GBool on) {
  quit = gTrue;
}

//------------------------------------------------------------------------
// scrollbar callbacks
//------------------------------------------------------------------------

static void scrollVertCbk(LTKWidget *scrollbar, int n, int val) {
  canvas->scroll(hScrollbar->getPos(), val);
  XSync(display, False);
}

static void scrollHorizCbk(LTKWidget *scrollbar, int n, int val) {
  canvas->scroll(val, vScrollbar->getPos());
  XSync(display, False);
}

//------------------------------------------------------------------------
// misc callbacks
//------------------------------------------------------------------------

static void layoutCbk(LTKWindow *win1) {
  hScrollbar->setLimits(0, canvas->getRealWidth() - 1);
  hScrollbar->setPos(hScrollbar->getPos(), canvas->getWidth());
  hScrollbar->setScrollDelta(16);
  vScrollbar->setLimits(0, canvas->getRealHeight() - 1);
  vScrollbar->setPos(vScrollbar->getPos(), canvas->getHeight());
  vScrollbar->setScrollDelta(16);
  canvas->scroll(hScrollbar->getPos(), vScrollbar->getPos());
}

static void propChangeCbk(LTKWindow *win1, Atom atom) {
  Window xwin;
  char *cmd;
  Atom type;
  int format;
  Gulong size, remain;
  char *p;
  GString *newFileName;
  int newPage;

  // get command
  xwin = win1->getXWindow();
  if (XGetWindowProperty(display, xwin, remoteAtom,
			 0, remoteCmdLength/4, True, remoteAtom,
			 &type, &format, &size, &remain,
			 (Guchar **)&cmd) != Success)
    return;
  if (size == 0)
    return;

  // raise window
  if (cmd[0] == 'D' || cmd[0] == 'r'){
    win->raise();
    XFlush(display);
  }

  // display file / page
  if (cmd[0] == 'd' || cmd[0] == 'D') {
    p = cmd + 2;
    newPage = atoi(p);
    if (!(p = strchr(p, ' ')))
      return;
    newFileName = new GString(p + 1);
    XFree((XPointer)cmd);
    if (!doc || newFileName->cmp(doc->getFileName())) {
      if (!loadFile(newFileName))
	return;
    } else {
      delete newFileName;
    }
    if (newPage != page && newPage >= 1 && newPage <= doc->getNumPages())
      displayPage(newPage, zoom, rotate);

  // quit
  } else if (cmd[0] == 'q') {
    quit = gTrue;
  }
}

//------------------------------------------------------------------------
// selection
//------------------------------------------------------------------------

static void setSelection(int newXMin, int newYMin, int newXMax, int newYMax) {
  int x, y, w, h;
  GBool needRedraw, needScroll;
  GBool moveLeft, moveRight, moveTop, moveBottom;

  // erase old selection on canvas pixmap
  needRedraw = gFalse;
  if (selectXMin < selectXMax && selectYMin < selectYMax) {
    XFillRectangle(canvas->getDisplay(), canvas->getPixmap(),
		   selectGC, selectXMin, selectYMin,
		   selectXMax - selectXMin, selectYMax - selectYMin);
    needRedraw = gTrue;
  }

  // draw new selection on canvas pixmap
  if (newXMin < newXMax && newYMin < newYMax) {
    XFillRectangle(canvas->getDisplay(), canvas->getPixmap(),
		   selectGC, newXMin, newYMin,
		   newXMax - newXMin, newYMax - newYMin);
    needRedraw = gTrue;
  }

  // check which edges moved
  moveLeft = newXMin != selectXMin;
  moveTop = newYMin != selectYMin;
  moveRight = newXMax != selectXMax;
  moveBottom = newYMax != selectYMax;

  // redraw currently visible part of canvas
  if (needRedraw) {
    if (moveLeft) {
      canvas->redrawRect((newXMin < selectXMin) ? newXMin : selectXMin,
			 (newYMin < selectYMin) ? newYMin : selectYMin,
			 (newXMin > selectXMin) ? newXMin : selectXMin,
			 (newYMax > selectYMax) ? newYMax : selectYMax);
    }
    if (moveRight) {
      canvas->redrawRect((newXMax < selectXMax) ? newXMax : selectXMax,
			 (newYMin < selectYMin) ? newYMin : selectYMin,
			 (newXMax > selectXMax) ? newXMax : selectXMax,
			 (newYMax > selectYMax) ? newYMax : selectYMax);
    }
    if (moveTop) {
      canvas->redrawRect((newXMin < selectXMin) ? newXMin : selectXMin,
			 (newYMin < selectYMin) ? newYMin : selectYMin,
			 (newXMax > selectXMax) ? newXMax : selectXMax,
			 (newYMin > selectYMin) ? newYMin : selectYMin);
    }
    if (moveBottom) {
      canvas->redrawRect((newXMin < selectXMin) ? newXMin : selectXMin,
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

  // scroll canvas if necessary
  needScroll = gFalse;
  w = canvas->getWidth();
  h = canvas->getHeight();
  x = hScrollbar->getPos();
  y = vScrollbar->getPos();
  if (moveLeft && selectXMin < x) {
    x = selectXMin;
    needScroll = gTrue;
  } else if (moveRight && selectXMax >= x + w) {
    x = selectXMax - w;
    needScroll = gTrue;
  } else if (moveLeft && selectXMin >= x + w) {
    x = selectXMin - w;
    needScroll = gTrue;
  } else if (moveRight && selectXMax < x) {
    x = selectXMax;
    needScroll = gTrue;
  }
  if (moveTop && selectYMin < y) {
    y = selectYMin;
    needScroll = gTrue;
  } else if (moveBottom && selectYMax >= y + h) {
    y = selectYMax - h;
    needScroll = gTrue;
  } else if (moveTop && selectYMin >= y + h) {
    y = selectYMin - h;
    needScroll = gTrue;
  } else if (moveBottom && selectYMax < y) {
    y = selectYMax;
    needScroll = gTrue;
  }
  if (needScroll) {
    hScrollbar->setPos(x, w);
    vScrollbar->setPos(y, h);
    canvas->scroll(x, y);
  }
}

//------------------------------------------------------------------------
// "Open" dialog
//------------------------------------------------------------------------

static void mapOpenDialog() {
  openDialog = makeOpenDialog(app);
  ((LTKFileReq *)openDialog->findWidget("fileReq"))->setDir(fileReqDir);
  openDialog->layoutDialog(win, -1, -1);
  openDialog->map();
}

static void openButtonCbk(LTKWidget *button, int n, GBool on) {
  LTKFileReq *fileReq;
  GString *sel;

  sel = NULL;
  if (n == 1) {
    fileReq = (LTKFileReq *)openDialog->findWidget("fileReq");
    if ((sel = fileReq->getSelection()))
      openSelectCbk(fileReq, 0, sel);
    else
      XBell(display, 0);
  }
  if (openDialog) {
    if (sel) {
      delete fileReqDir;
      fileReqDir = ((LTKFileReq *)openDialog->findWidget("fileReq"))->getDir();
    }
    delete openDialog;
    openDialog = NULL;
  }
}

static void openSelectCbk(LTKWidget *widget, int n, GString *name) {
  GString *name1;

  name1 = name->copy();
  if (openDialog) {
    delete fileReqDir;
    fileReqDir = ((LTKFileReq *)openDialog->findWidget("fileReq"))->getDir();
    delete openDialog;
    openDialog = NULL;
  }
  if (loadFile(name1))
    displayPage(1, zoom, rotate);
}

//------------------------------------------------------------------------
// "Save PDF" dialog
//------------------------------------------------------------------------

static void mapSaveDialog() {
  saveDialog = makeSaveDialog(app);
  ((LTKFileReq *)saveDialog->findWidget("fileReq"))->setDir(fileReqDir);
  saveDialog->layoutDialog(win, -1, -1);
  saveDialog->map();
}

static void saveButtonCbk(LTKWidget *button, int n, GBool on) {
  LTKFileReq *fileReq;
  GString *sel;

  if (!doc)
    return;
  sel = NULL;
  if (n == 1) {
    fileReq = (LTKFileReq *)saveDialog->findWidget("fileReq");
    if ((sel = fileReq->getSelection()))
      saveSelectCbk(fileReq, 0, sel);
    else
      XBell(display, 0);
  }
  if (saveDialog) {
    if (sel) {
      delete fileReqDir;
      fileReqDir = ((LTKFileReq *)saveDialog->findWidget("fileReq"))->getDir();
    }
    delete saveDialog;
    saveDialog = NULL;
  }
}

static void saveSelectCbk(LTKWidget *widget, int n, GString *name) {
  GString *name1;

  name1 = name->copy();
  if (saveDialog) {
    delete fileReqDir;
    fileReqDir = ((LTKFileReq *)saveDialog->findWidget("fileReq"))->getDir();
    delete saveDialog;
    saveDialog = NULL;
  }
  win->setBusyCursor(gTrue);
  doc->saveAs(name1);
  delete name1;
  win->setBusyCursor(gFalse);
}

//------------------------------------------------------------------------
// "PostScript" dialog
//------------------------------------------------------------------------

static void mapPSDialog() {
  LTKTextIn *widget;
  char s[20];

  psDialog = makePostScriptDialog(app);
  sprintf(s, "%d", psFirstPage);
  widget = (LTKTextIn *)psDialog->findWidget("firstPage");
  widget->setText(s);
  sprintf(s, "%d", psLastPage);
  widget = (LTKTextIn *)psDialog->findWidget("lastPage");
  widget->setText(s);
  widget = (LTKTextIn *)psDialog->findWidget("fileName");
  widget->setText(psFileName->getCString());
  psDialog->layoutDialog(win, -1, -1);
  psDialog->map();
}

static void psButtonCbk(LTKWidget *button, int n, GBool on) {
  PSOutputDev *psOut;
  LTKTextIn *widget;

  if (!doc)
    return;

  // "Ok" button
  if (n == 1) {
    // extract params and close the dialog
    widget = (LTKTextIn *)psDialog->findWidget("firstPage");
    psFirstPage = atoi(widget->getText()->getCString());
    if (psFirstPage < 1)
      psFirstPage = 1;
    widget = (LTKTextIn *)psDialog->findWidget("lastPage");
    psLastPage = atoi(widget->getText()->getCString());
    if (psLastPage < psFirstPage)
      psLastPage = psFirstPage;
    else if (psLastPage > doc->getNumPages())
      psLastPage = doc->getNumPages();
    widget = (LTKTextIn *)psDialog->findWidget("fileName");
    if (psFileName)
      delete psFileName;
    psFileName = widget->getText()->copy();
    if (!(psFileName->getChar(0) == '|' ||
	  psFileName->cmp("-") == 0))
      makePathAbsolute(psFileName);

    // do the PostScript output
    psDialog->setBusyCursor(gTrue);
    win->setBusyCursor(gTrue);
    if (doc->okToPrint()) {
      psOut = new PSOutputDev(psFileName->getCString(), doc->getCatalog(),
			      psFirstPage, psLastPage, gTrue, gFalse);
      if (psOut->isOk()) {
	doc->displayPages(psOut, psFirstPage, psLastPage,
			  zoomDPI[zoom - minZoom], rotate);
      }
      delete psOut;
    }

    delete psDialog;
    win->setBusyCursor(gFalse);

  // "Cancel" button
  } else {
    delete psDialog;
  }
}

//------------------------------------------------------------------------
// "About" window
//------------------------------------------------------------------------

static void mapAboutWin() {
  if (aboutWin) {
    aboutWin->raise();
  } else {
    aboutWin = makeAboutWindow(app);
    aboutWin->layout(-1, -1, -1, -1);
    aboutWin->map();
  }
}

static void closeAboutCbk(LTKWidget *button, int n, GBool on) {
  delete aboutWin;
  aboutWin = NULL;
}

//------------------------------------------------------------------------
// "Find" window
//------------------------------------------------------------------------

static void findCbk(LTKWidget *button, int n, GBool on) {
  if (!doc)
    return;
  mapFindWin();
}

static void mapFindWin() {
  if (findWin) {
    findWin->raise();
  } else {
    findWin = makeFindWindow(app);
    findWin->layout(-1, -1, -1, -1);
    findWin->map();
  }
}

static void findButtonCbk(LTKWidget *button, int n, GBool on) {
  LTKTextIn *textIn;

  if (!doc)
    return;
  if (n == 1) {
    textIn = (LTKTextIn *)findWin->findWidget("text");
    doFind(textIn->getText()->getCString());
  } else {
    delete findWin;
    findWin = NULL;
  }
}

static void doFind(char *s) {
  TextOutputDev *textOut;
  int xMin, yMin, xMax, yMax;
  double xMin1, yMin1, xMax1, yMax1;
  int pg;
  GBool top;
  GString *s1;

  // check for zero-length string
  if (!s[0]) {
    XBell(display, 0);
    return;
  }

  // set cursors to watch
  win->setBusyCursor(gTrue);
  findWin->setBusyCursor(gTrue);

  // search current page starting at current selection or top of page
  xMin = yMin = xMax = yMax = 0;
  if (selectXMin < selectXMax && selectYMin < selectYMax) {
    xMin = selectXMax;
    yMin = (selectYMin + selectYMax) / 2;
    top = gFalse;
  } else {
    top = gTrue;
  }
  if (out->findText(s, top, gTrue, &xMin, &yMin, &xMax, &yMax))
    goto found;

  // search following pages
  textOut = new TextOutputDev(NULL, gFalse);
  if (!textOut->isOk()) {
    delete textOut;
    goto done;
  }
  for (pg = page+1; pg <= doc->getNumPages(); ++pg) {
    doc->displayPage(textOut, pg, 72, 0, gFalse);
    if (textOut->findText(s, gTrue, gTrue, &xMin1, &yMin1, &xMax1, &yMax1))
      goto foundPage;
  }

  // search previous pages
  for (pg = 1; pg < page; ++pg) {
    doc->displayPage(textOut, pg, 72, 0, gFalse);
    if (textOut->findText(s, gTrue, gTrue, &xMin1, &yMin1, &xMax1, &yMax1))
      goto foundPage;
  }
  delete textOut;

  // search current page ending at current selection
  if (selectXMin < selectXMax && selectYMin < selectYMax) {
    xMax = selectXMin;
    yMax = (selectYMin + selectYMax) / 2;
    if (out->findText(s, gTrue, gFalse, &xMin, &yMin, &xMax, &yMax))
      goto found;
  }

  // not found
  XBell(display, 0);
  goto done;

  // found on a different page
 foundPage:
  delete textOut;
  displayPage(pg, zoom, rotate);
  if (!out->findText(s, gTrue, gTrue, &xMin, &yMin, &xMax, &yMax))
    goto done; // this can happen if coalescing is bad

  // found: change the selection
 found:
  setSelection(xMin, yMin, xMax, yMax);
#ifndef NO_TEXT_SELECT
  if (doc->okToCopy()) {
    s1 = out->getText(selectXMin, selectYMin, selectXMax, selectYMax);
    win->setSelection(NULL, s1);
  }
#endif

 done:
  // reset cursors to normal
  win->setBusyCursor(gFalse);
  findWin->setBusyCursor(gFalse);
}

//------------------------------------------------------------------------
// app kill callback
//------------------------------------------------------------------------

static void killCbk(LTKWindow *win1) {
  if (win1 == win) {
    quit = gTrue;
  } else if (win1 == aboutWin) {
    delete aboutWin;
    aboutWin = NULL;
  } else if (win1 == psDialog) {
    delete psDialog;
    psDialog = NULL;
  } else if (win1 == openDialog) {
    delete openDialog;
    openDialog = NULL;
  } else if (win1 == saveDialog) {
    delete saveDialog;
    saveDialog = NULL;
  } else if (win1 == findWin) {
    delete findWin;
    findWin = NULL;
  }
}
