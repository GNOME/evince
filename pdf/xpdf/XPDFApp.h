//========================================================================
//
// XPDFApp.h
//
// Copyright 2002-2003 Glyph & Cog, LLC
//
//========================================================================

#ifndef XPDFAPP_H
#define XPDFAPP_H

#include <aconf.h>

#ifdef USE_GCC_PRAGMAS
#pragma interface
#endif

#define Object XtObject
#include <Xm/XmAll.h>
#undef Object
#include "gtypes.h"
#include "SplashTypes.h"

class GString;
class GList;
class XPDFViewer;

//------------------------------------------------------------------------

#define xpdfAppName "Xpdf"

//------------------------------------------------------------------------
// XPDFApp
//------------------------------------------------------------------------

class XPDFApp {
public:

  XPDFApp(int *argc, char *argv[]);
  ~XPDFApp();

  XPDFViewer *open(GString *fileName, int page = 1,
		   GString *ownerPassword = NULL,
		   GString *userPassword = NULL);
  XPDFViewer *openAtDest(GString *fileName, GString *dest,
			 GString *ownerPassword = NULL,
			 GString *userPassword = NULL);
  void close(XPDFViewer *viewer, GBool closeLast);
  void quit();

  void run();

  //----- remote server
  void setRemoteName(char *remoteName);
  GBool remoteServerRunning();
  void remoteOpen(GString *fileName, int page, GBool raise);
  void remoteOpenAtDest(GString *fileName, GString *dest, GBool raise);
  void remoteReload(GBool raise);
  void remoteRaise();
  void remoteQuit();

  //----- resource/option values
  GString *getGeometry() { return geometry; }
  GString *getTitle() { return title; }
  GBool getInstallCmap() { return installCmap; }
  int getRGBCubeSize() { return rgbCubeSize; }
  GBool getReverseVideo() { return reverseVideo; }
  SplashRGB8 getPaperRGB() { return paperRGB; }
  Gulong getPaperColor() { return paperColor; }
  GString *getInitialZoom() { return initialZoom; }
  GBool getViKeys() { return viKeys; }
  void setFullScreen(GBool fullScreenA) { fullScreen = fullScreenA; }
  GBool getFullScreen() { return fullScreen; }

  XtAppContext getAppContext() { return appContext; }
  Widget getAppShell() { return appShell; }

private:

  void getResources();
  static void remoteMsgCbk(Widget widget, XtPointer ptr,
			   XEvent *event, Boolean *cont);

  Display *display;
  int screenNum;
  XtAppContext appContext;
  Widget appShell;
  GList *viewers;		// [XPDFViewer]

  Atom remoteAtom;
  Window remoteXWin;
  XPDFViewer *remoteViewer;
  Widget remoteWin;

  //----- resource/option values
  GString *geometry;
  GString *title;
  GBool installCmap;
  int rgbCubeSize;
  GBool reverseVideo;
  SplashRGB8 paperRGB;
  Gulong paperColor;
  GString *initialZoom;
  GBool viKeys;
  GBool fullScreen;
};

#endif
