//========================================================================
//
// XPDFViewer.h
//
// Copyright 2002-2003 Glyph & Cog, LLC
//
//========================================================================

#ifndef XPDFVIEWER_H
#define XPDFVIEWER_H

#include <aconf.h>

#ifdef USE_GCC_PRAGMAS
#pragma interface
#endif

#define Object XtObject
#include <Xm/XmAll.h>
#undef Object
#include "gtypes.h"
#include "XPDFCore.h"

#if (XmVERSION <= 1) && !defined(__sgi)
#define DISABLE_OUTLINE
#endif

#if (XmVERSION >= 2 && !defined(LESSTIF_VERSION))
#  define USE_COMBO_BOX 1
#else
#  undef USE_COMBO_BOX
#endif

class GString;
class GList;
class UnicodeMap;
class LinkDest;
class XPDFApp;

//------------------------------------------------------------------------

// NB: this must match the defn of zoomMenuBtnInfo in XPDFViewer.cc
#define nZoomMenuItems 10

//------------------------------------------------------------------------
// XPDFViewer
//------------------------------------------------------------------------

class XPDFViewer {
public:

  XPDFViewer(XPDFApp *appA, GString *fileName,
	     int pageA, GString *destName,
	     GString *ownerPassword, GString *userPassword);
  GBool isOk() { return ok; }
  ~XPDFViewer();

  void open(GString *fileName, int pageA, GString *destName);
  void clear();
  void reloadFile();

  Widget getWindow() { return win; }

private:

  //----- load / display
  GBool loadFile(GString *fileName, GString *ownerPassword = NULL,
		 GString *userPassword = NULL);
  void displayPage(int pageA, double zoomA, int rotateA,
                   GBool scrollToTop, GBool addToHist);
  void displayDest(LinkDest *dest, double zoomA, int rotateA,
		   GBool addToHist);
  void getPageAndDest(int pageA, GString *destName,
		      int *pageOut, LinkDest **destOut);

  //----- password dialog
  static GString *reqPasswordCbk(void *data, GBool again);

  //----- actions
  static void actionCbk(void *data, char *action);

  //----- keyboard/mouse input
  static void keyPressCbk(void *data, char *s, KeySym key,
			  Guint modifiers);
  static void mouseCbk(void *data, XEvent *event);

  //----- GUI code: main window
  void initWindow();
  void mapWindow();
  void closeWindow();
  int getZoomIdx();
  void setZoomIdx(int idx);
  void setZoomVal(double z);
  static void prevPageCbk(Widget widget, XtPointer ptr,
			  XtPointer callData);
  static void prevTenPageCbk(Widget widget, XtPointer ptr,
			     XtPointer callData);
  static void nextPageCbk(Widget widget, XtPointer ptr,
			  XtPointer callData);
  static void nextTenPageCbk(Widget widget, XtPointer ptr,
			     XtPointer callData);
  static void backCbk(Widget widget, XtPointer ptr,
		      XtPointer callData);
  static void forwardCbk(Widget widget, XtPointer ptr,
			 XtPointer callData);
#if USE_COMBO_BOX
  static void zoomComboBoxCbk(Widget widget, XtPointer ptr,
			      XtPointer callData);
#else
  static void zoomMenuCbk(Widget widget, XtPointer ptr,
			  XtPointer callData);
#endif
  static void findCbk(Widget widget, XtPointer ptr,
		      XtPointer callData);
  static void printCbk(Widget widget, XtPointer ptr,
		       XtPointer callData);
  static void aboutCbk(Widget widget, XtPointer ptr,
		       XtPointer callData);
  static void quitCbk(Widget widget, XtPointer ptr,
		      XtPointer callData);
  static void openCbk(Widget widget, XtPointer ptr,
		      XtPointer callData);
  static void openInNewWindowCbk(Widget widget, XtPointer ptr,
				 XtPointer callData);
  static void reloadCbk(Widget widget, XtPointer ptr,
			XtPointer callData);
  static void saveAsCbk(Widget widget, XtPointer ptr,
			XtPointer callData);
  static void rotateCCWCbk(Widget widget, XtPointer ptr,
			   XtPointer callData);
  static void rotateCWCbk(Widget widget, XtPointer ptr,
			  XtPointer callData);
  static void closeCbk(Widget widget, XtPointer ptr,
		       XtPointer callData);
  static void closeMsgCbk(Widget widget, XtPointer ptr,
			  XtPointer callData);
  static void pageNumCbk(Widget widget, XtPointer ptr,
			 XtPointer callData);
  static void updateCbk(void *data, GString *fileName,
			int pageNum, int numPages, char *linkString);

  //----- GUI code: outline
#ifndef DISABLE_OUTLINE
  void setupOutline();
  void setupOutlineItems(GList *items, Widget parent, UnicodeMap *uMap);
  static void outlineSelectCbk(Widget widget, XtPointer ptr,
			       XtPointer callData);
#endif

  //----- GUI code: "about" dialog
  void initAboutDialog();

  //----- GUI code: "open" dialog
  void initOpenDialog();
  void setOpenDialogDir(char *dir);
  void mapOpenDialog(GBool openInNewWindowA);
  static void openOkCbk(Widget widget, XtPointer ptr,
			XtPointer callData);

  //----- GUI code: "find" dialog
  void initFindDialog();
  static void findFindCbk(Widget widget, XtPointer ptr,
			  XtPointer callData);
  void doFind(GBool next);
  static void findCloseCbk(Widget widget, XtPointer ptr,
			   XtPointer callData);

  //----- GUI code: "save as" dialog
  void initSaveAsDialog();
  void setSaveAsDialogDir(char *dir);
  void mapSaveAsDialog();
  static void saveAsOkCbk(Widget widget, XtPointer ptr,
			  XtPointer callData);

  //----- GUI code: "print" dialog
  void initPrintDialog();
  void setupPrintDialog();
  static void printWithCmdBtnCbk(Widget widget, XtPointer ptr,
				 XtPointer callData);
  static void printToFileBtnCbk(Widget widget, XtPointer ptr,
				XtPointer callData);
  static void printPrintCbk(Widget widget, XtPointer ptr,
			    XtPointer callData);

  //----- GUI code: password dialog
  void initPasswordDialog();
  static void passwordTextVerifyCbk(Widget widget, XtPointer ptr,
				    XtPointer callData);
  static void passwordOkCbk(Widget widget, XtPointer ptr,
			    XtPointer callData);
  static void passwordCancelCbk(Widget widget, XtPointer ptr,
				XtPointer callData);
  void getPassword(GBool again);

  //----- Motif support
  XmFontList createFontList(char *xlfd);

  XPDFApp *app;
  GBool ok;

  Display *display;
  int screenNum;
  Widget win;			// top-level window
  Widget form;
  Widget panedWin;
#ifndef DISABLE_OUTLINE
  Widget outlineScroll;
  Widget outlineTree;
  Widget *outlineLabels;
  int outlineLabelsLength;
  int outlineLabelsSize;
#endif
  XPDFCore *core;
  Widget toolBar;
  Widget backBtn;
  Widget prevTenPageBtn;
  Widget prevPageBtn;
  Widget nextPageBtn;
  Widget nextTenPageBtn;
  Widget forwardBtn;
  Widget pageNumText;
  Widget pageCountLabel;
#if USE_COMBO_BOX
  Widget zoomComboBox;
#else
  Widget zoomMenu;
  Widget zoomMenuBtns[nZoomMenuItems];
#endif
  Widget findBtn;
  Widget printBtn;
  Widget aboutBtn;
  Widget linkLabel;
  Widget quitBtn;
  Widget popupMenu;

  Widget aboutDialog;
  XmFontList aboutBigFont, aboutVersionFont, aboutFixedFont;

  Widget openDialog;
  GBool openInNewWindow;

  Widget findDialog;
  Widget findText;

  Widget saveAsDialog;

  Widget printDialog;
  Widget printWithCmdBtn;
  Widget printToFileBtn;
  Widget printCmdText;
  Widget printFileText;
  Widget printFirstPage;
  Widget printLastPage;

  Widget passwordDialog;
  Widget passwordMsg;
  Widget passwordText;
  int passwordDone;
  GString *password;
};

#endif
