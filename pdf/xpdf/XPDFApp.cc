//========================================================================
//
// XPDFApp.cc
//
// Copyright 2002-2003 Glyph & Cog, LLC
//
//========================================================================

#include <aconf.h>

#ifdef USE_GCC_PRAGMAS
#pragma implementation
#endif

#include "GString.h"
#include "GList.h"
#include "Error.h"
#include "XPDFViewer.h"
#include "XPDFApp.h"
#include "config.h"

// these macro defns conflict with xpdf's Object class
#ifdef LESSTIF_VERSION
#undef XtDisplay
#undef XtScreen
#undef XtWindow
#undef XtParent
#undef XtIsRealized
#endif

//------------------------------------------------------------------------

#define remoteCmdSize 512

//------------------------------------------------------------------------

static String fallbackResources[] = {
  "*XmTextField.fontList: -*-courier-medium-r-normal--12-*-*-*-*-*-iso8859-1",
  "*.fontList: -*-helvetica-medium-r-normal--12-*-*-*-*-*-iso8859-1",
  "*XmTextField.translations: #override\\n"
  "  Ctrl<Key>a:beginning-of-line()\\n"
  "  Ctrl<Key>b:backward-character()\\n"
  "  Ctrl<Key>d:delete-next-character()\\n"
  "  Ctrl<Key>e:end-of-line()\\n"
  "  Ctrl<Key>f:forward-character()\\n"
  "  Ctrl<Key>u:beginning-of-line()delete-to-end-of-line()\\n"
  "  Ctrl<Key>k:delete-to-end-of-line()\\n",
  NULL
};

static XrmOptionDescRec xOpts[] = {
  {"-display",       ".display",         XrmoptionSepArg,  NULL},
  {"-foreground",    "*Foreground",      XrmoptionSepArg,  NULL},
  {"-fg",            "*Foreground",      XrmoptionSepArg,  NULL},
  {"-background",    "*Background",      XrmoptionSepArg,  NULL},
  {"-bg",            "*Background",      XrmoptionSepArg,  NULL},
  {"-geometry",      ".geometry",        XrmoptionSepArg,  NULL},
  {"-g",             ".geometry",        XrmoptionSepArg,  NULL},
  {"-font",          "*.fontList",       XrmoptionSepArg,  NULL},
  {"-fn",            "*.fontList",       XrmoptionSepArg,  NULL},
  {"-title",         ".title",           XrmoptionSepArg,  NULL},
  {"-cmap",          ".installCmap",     XrmoptionNoArg,   (XPointer)"on"},
  {"-rgb",           ".rgbCubeSize",     XrmoptionSepArg,  NULL},
  {"-rv",            ".reverseVideo",    XrmoptionNoArg,   (XPointer)"true"},
  {"-papercolor",    ".paperColor",      XrmoptionSepArg,  NULL},
  {"-z",             ".initialZoom",     XrmoptionSepArg,  NULL}
};

#define nXOpts (sizeof(xOpts) / sizeof(XrmOptionDescRec))

struct XPDFAppResources {
  String geometry;
  String title;
  Bool installCmap;
  int rgbCubeSize;
  Bool reverseVideo;
  String paperColor;
  String initialZoom;
  Bool viKeys;
};

static Bool defInstallCmap = False;
static int defRGBCubeSize = defaultRGBCube;
static Bool defReverseVideo = False;
static Bool defViKeys = False;

static XtResource xResources[] = {
  { "geometry",     "Geometry",     XtRString, sizeof(String), XtOffsetOf(XPDFAppResources, geometry),     XtRString, (XtPointer)NULL             },
  { "title",        "Title",        XtRString, sizeof(String), XtOffsetOf(XPDFAppResources, title),        XtRString, (XtPointer)NULL             },
  { "installCmap",  "InstallCmap",  XtRBool,   sizeof(Bool),   XtOffsetOf(XPDFAppResources, installCmap),  XtRBool,   (XtPointer)&defInstallCmap  },
  { "rgbCubeSize",  "RgbCubeSize",  XtRInt,    sizeof(int),    XtOffsetOf(XPDFAppResources, rgbCubeSize),  XtRInt,    (XtPointer)&defRGBCubeSize  },
  { "reverseVideo", "ReverseVideo", XtRBool,   sizeof(Bool),   XtOffsetOf(XPDFAppResources, reverseVideo), XtRBool,   (XtPointer)&defReverseVideo },
  { "paperColor",   "PaperColor",   XtRString, sizeof(String), XtOffsetOf(XPDFAppResources, paperColor),   XtRString, (XtPointer)NULL             },
  { "initialZoom",  "InitialZoom",  XtRString, sizeof(String), XtOffsetOf(XPDFAppResources, initialZoom),  XtRString, (XtPointer)NULL             },
  { "viKeys",       "ViKeys",       XtRBool,   sizeof(Bool),   XtOffsetOf(XPDFAppResources, viKeys),       XtRBool,   (XtPointer)&defViKeys       }
};

#define nXResources (sizeof(xResources) / sizeof(XtResource))

//------------------------------------------------------------------------
// XPDFApp
//------------------------------------------------------------------------

#if 0 //~ for debugging
static int xErrorHandler(Display *display, XErrorEvent *ev) {
  printf("X error:\n");
  printf("  resource ID = %08lx\n", ev->resourceid);
  printf("  serial = %lu\n", ev->serial);
  printf("  error_code = %d\n", ev->error_code);
  printf("  request_code = %d\n", ev->request_code);
  printf("  minor_code = %d\n", ev->minor_code);
  fflush(stdout);
  abort();
}
#endif

XPDFApp::XPDFApp(int *argc, char *argv[]) {
  appShell = XtAppInitialize(&appContext, xpdfAppName, xOpts, nXOpts,
			     argc, argv, fallbackResources, NULL, 0);
  display = XtDisplay(appShell);
  screenNum = XScreenNumberOfScreen(XtScreen(appShell));
#if XmVERSION > 1
  XtVaSetValues(XmGetXmDisplay(XtDisplay(appShell)),
		XmNenableButtonTab, True, NULL);
#endif
#if XmVERSION > 1
  // Drag-and-drop appears to be buggy -- I'm seeing weird crashes
  // deep in the Motif code when I destroy widgets in the XpdfForms
  // code.  Xpdf doesn't use it, so just turn it off.
  XtVaSetValues(XmGetXmDisplay(XtDisplay(appShell)),
		XmNdragInitiatorProtocolStyle, XmDRAG_NONE,
		XmNdragReceiverProtocolStyle, XmDRAG_NONE,
		NULL);
#endif

#if 0 //~ for debugging
  XSynchronize(display, True);
  XSetErrorHandler(&xErrorHandler);
#endif

  fullScreen = gFalse;
  remoteAtom = None;
  remoteViewer = NULL;
  remoteWin = None;

  getResources();

  viewers = new GList();

}

void XPDFApp::getResources() {
  XPDFAppResources resources;
  XColor xcol, xcol2;
  Colormap colormap;
  
  XtGetApplicationResources(appShell, &resources, xResources, nXResources,
			    NULL, 0);
  geometry = resources.geometry ? new GString(resources.geometry)
                                : (GString *)NULL;
  title = resources.title ? new GString(resources.title) : (GString *)NULL;
  installCmap = (GBool)resources.installCmap;
  rgbCubeSize = resources.rgbCubeSize;
  reverseVideo = (GBool)resources.reverseVideo;
  paperColor = reverseVideo ? BlackPixel(display, screenNum) :
                              WhitePixel(display, screenNum);
  if (resources.paperColor) {
    XtVaGetValues(appShell, XmNcolormap, &colormap, NULL);
    if (XAllocNamedColor(display, colormap, resources.paperColor,
			 &xcol, &xcol2)) {
      paperColor = xcol.pixel;
    } else {
      error(-1, "Couldn't allocate color '%s'", resources.paperColor);
    }
  }
  initialZoom = resources.initialZoom ? new GString(resources.initialZoom)
                                      : (GString *)NULL;
  viKeys = (GBool)resources.viKeys;
}

XPDFApp::~XPDFApp() {
  deleteGList(viewers, XPDFViewer);
  if (geometry) {
    delete geometry;
  }
  if (title) {
    delete title;
  }
  if (initialZoom) {
    delete initialZoom;
  }
}

XPDFViewer *XPDFApp::open(GString *fileName, int page,
			  GString *ownerPassword, GString *userPassword) {
  XPDFViewer *viewer;

  viewer = new XPDFViewer(this, fileName, page, NULL,
			  ownerPassword, userPassword);
  if (!viewer->isOk()) {
    delete viewer;
    return NULL;
  }
  if (remoteAtom != None) {
    remoteViewer = viewer;
    remoteWin = viewer->getWindow();
    XtAddEventHandler(remoteWin, PropertyChangeMask, False,
		      &remoteMsgCbk, this);
    XSetSelectionOwner(display, remoteAtom, XtWindow(remoteWin), CurrentTime);
  }
  viewers->append(viewer);
  return viewer;
}

XPDFViewer *XPDFApp::openAtDest(GString *fileName, GString *dest,
				GString *ownerPassword,
				GString *userPassword) {
  XPDFViewer *viewer;

  viewer = new XPDFViewer(this, fileName, 1, dest,
			  ownerPassword, userPassword);
  if (!viewer->isOk()) {
    delete viewer;
    return NULL;
  }
  if (remoteAtom != None) {
    remoteViewer = viewer;
    remoteWin = viewer->getWindow();
    XtAddEventHandler(remoteWin, PropertyChangeMask, False,
		      &remoteMsgCbk, this);
    XSetSelectionOwner(display, remoteAtom, XtWindow(remoteWin), CurrentTime);
  }
  viewers->append(viewer);
  return viewer;
}

void XPDFApp::close(XPDFViewer *viewer, GBool closeLast) {
  int i;

  if (viewers->getLength() == 1) {
    if (viewer != (XPDFViewer *)viewers->get(0)) {
      return;
    }
    if (closeLast) {
      quit();
    } else {
      viewer->clear();
    }
  } else {
    for (i = 0; i < viewers->getLength(); ++i) {
      if (((XPDFViewer *)viewers->get(i)) == viewer) {
	viewers->del(i);
	if (remoteAtom != None && remoteViewer == viewer) {
	  remoteViewer = (XPDFViewer *)viewers->get(viewers->getLength() - 1);
	  remoteWin = remoteViewer->getWindow();
	  XSetSelectionOwner(display, remoteAtom, XtWindow(remoteWin),
			     CurrentTime);
	}
	delete viewer;
	return;
      }
    }
  }
}

void XPDFApp::quit() {
  if (remoteAtom != None) {
    XSetSelectionOwner(display, remoteAtom, None, CurrentTime);
  }
  while (viewers->getLength() > 0) {
    delete (XPDFViewer *)viewers->del(0);
  }
#if HAVE_XTAPPSETEXITFLAG
  exit(0);
#else
  XtAppSetExitFlag(appContext);
#endif
}

void XPDFApp::run() {
  XtAppMainLoop(appContext);
}

void XPDFApp::setRemoteName(char *remoteName) {
  remoteAtom = XInternAtom(display, remoteName, False);
  remoteXWin = XGetSelectionOwner(display, remoteAtom);
}

GBool XPDFApp::remoteServerRunning() {
  return remoteXWin != None;
}

void XPDFApp::remoteOpen(GString *fileName, int page, GBool raise) {
  char cmd[remoteCmdSize];

  sprintf(cmd, "%c %d %.200s",
	  raise ? 'D' : 'd', page, fileName->getCString());
  XChangeProperty(display, remoteXWin, remoteAtom, remoteAtom, 8,
		  PropModeReplace, (Guchar *)cmd, strlen(cmd) + 1);
  XFlush(display);
}

void XPDFApp::remoteOpenAtDest(GString *fileName, GString *dest, GBool raise) {
  char cmd[remoteCmdSize];

  sprintf(cmd, "%c +%.256s %.200s",
	  raise ? 'D' : 'd', dest->getCString(), fileName->getCString());
  XChangeProperty(display, remoteXWin, remoteAtom, remoteAtom, 8,
		  PropModeReplace, (Guchar *)cmd, strlen(cmd) + 1);
  XFlush(display);
}

void XPDFApp::remoteReload(GBool raise) {
  XChangeProperty(display, remoteXWin, remoteAtom, remoteAtom, 8,
		  PropModeReplace, raise ? (Guchar *)"L" : (Guchar *)"l", 2);
  XFlush(display);
}

void XPDFApp::remoteRaise() {
  XChangeProperty(display, remoteXWin, remoteAtom, remoteAtom, 8,
		  PropModeReplace, (Guchar *)"r", 2);
  XFlush(display);
}

void XPDFApp::remoteQuit() {
  XChangeProperty(display, remoteXWin, remoteAtom, remoteAtom, 8,
		  PropModeReplace, (Guchar *)"q", 2);
  XFlush(display);
}

void XPDFApp::remoteMsgCbk(Widget widget, XtPointer ptr,
			   XEvent *event, Boolean *cont) {
  XPDFApp *app = (XPDFApp *)ptr;
  char *cmd;
  Atom type;
  int format;
  Gulong size, remain;
  char *p, *q;
  GString *fileName;
  int page;
  GString *destName;

  if (event->xproperty.atom != app->remoteAtom) {
    *cont = True;
    return;
  }
  *cont = False;

  // get command
  if (XGetWindowProperty(app->display, XtWindow(app->remoteWin),
			 app->remoteAtom, 0, remoteCmdSize/4,
			 True, app->remoteAtom,
			 &type, &format, &size, &remain,
			 (Guchar **)&cmd) != Success) {
    return;
  }
  if (size == 0) {
    return;
  }

  // display file / page
  if (cmd[0] == 'd' || cmd[0] == 'D') {
    p = cmd + 2;
    q = strchr(p, ' ');
    if (!q) {
      return;
    }
    *q++ = '\0';
    page = 1;
    destName = NULL;
    if (*p == '+') {
      destName = new GString(p + 1);
    } else {
      page = atoi(p);
    }
    if (q) {
      fileName = new GString(q);
      app->remoteViewer->open(fileName, page, destName);
      delete fileName;
    }
    XFree((XPointer)cmd);
    if (destName) {
      delete destName;
    }

  // reload
  } else if (cmd[0] == 'l' || cmd[0] == 'L') {
    app->remoteViewer->reloadFile();

  // quit
  } else if (cmd[0] == 'q') {
    app->quit();
  }

  // raise window
  if (cmd[0] == 'D' || cmd[0] == 'L' || cmd[0] == 'r'){
    XMapRaised(app->display, XtWindow(app->remoteWin));
    XFlush(app->display);
  }
}
