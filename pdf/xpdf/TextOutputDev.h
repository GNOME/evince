//========================================================================
//
// TextOutputDev.h
//
// Copyright 1997 Derek B. Noonburg
//
//========================================================================

#ifndef TEXTOUTPUTDEV_H
#define TEXTOUTPUTDEV_H

#ifdef __GNUC__
#pragma interface
#endif

#include <stdio.h>
#include "gtypes.h"
#include "GfxFont.h"
#include "OutputDev.h"

class GfxState;
class GString;

//------------------------------------------------------------------------
// TextString
//------------------------------------------------------------------------

class TextString {
public:

  // Constructor.
  TextString(GfxState *state, GBool hexCodes1);

  // Destructor.
  ~TextString();

  // Add a character to the string.
  void addChar(GfxState *state, double x, double y,
	       double dx, double dy,
	       Guchar c, GBool useASCII7);

  // Add a 16-bit character to the string.
  void addChar16(GfxState *state, double x, double y,
		 double dx, double dy,
		 int c, GfxFontCharSet16 charSet);

private:

  double xMin, xMax;		// bounding box x coordinates
  double yMin, yMax;		// bounding box y coordinates
  int col;			// starting column
  GString *text;		// the text
  double *xRight;		// right-hand x coord of each char
  TextString *yxNext;		// next string in y-major order
  TextString *xyNext;		// next string in x-major order
  GBool hexCodes;		// subsetted font with hex char codes

  friend class TextPage;
};

//------------------------------------------------------------------------
// TextPage
//------------------------------------------------------------------------

class TextPage {
public:

  // Constructor.
  TextPage(GBool useASCII7, GBool rawOrder);

  // Destructor.
  ~TextPage();

  // Begin a new string.
  void beginString(GfxState *state, GString *s, GBool hex1);

  // Add a character to the current string.
  void addChar(GfxState *state, double x, double y,
	       double dx, double dy, Guchar c);

  // Add a 16-bit character to the current string.
  void addChar16(GfxState *state, double x, double y,
		 double dx, double dy, int c,
		 GfxFontCharSet16 charSet);

  // End the current string, sorting it into the list of strings.
  void endString();

  // Coalesce strings that look like parts of the same line.
  void coalesce();

  // Find a string.  If <top> is true, starts looking at top of page;
  // otherwise starts looking at <xMin>,<yMin>.  If <bottom> is true,
  // stops looking at bottom of page; otherwise stops looking at
  // <xMax>,<yMax>.  If found, sets the text bounding rectange and
  // returns true; otherwise returns false.
  GBool findText(char *s, GBool top, GBool bottom,
		 double *xMin, double *yMin,
		 double *xMax, double *yMax);

  // Get the text which is inside the specified rectangle.
  GString *getText(double xMin, double yMin,
		   double xMax, double yMax);

  // Dump contents of page to a file.
  void dump(FILE *f);

  // Clear the page.
  void clear();

private:

  GBool useASCII7;		// use 7-bit ASCII?
  GBool rawOrder;		// keep strings in content stream order

  TextString *curStr;		// currently active string

  TextString *yxStrings;	// strings in y-major order
  TextString *xyStrings;	// strings in x-major order
  TextString *yxCur1, *yxCur2;	// cursors for yxStrings list
};

//------------------------------------------------------------------------
// TextOutputDev
//------------------------------------------------------------------------

class TextOutputDev: public OutputDev {
public:

  // Open a text output file.  If <fileName> is NULL, no file is written
  // (this is useful, e.g., for searching text).  If <useASCII7> is true,
  // text is converted to 7-bit ASCII; otherwise, text is converted to
  // 8-bit ISO Latin-1.  <useASCII7> should also be set for Japanese
  // (EUC-JP) text.  If <rawOrder> is true, the text is kept in content
  // stream order.
  TextOutputDev(char *fileName, GBool useASCII7, GBool rawOrder);

  // Destructor.
  virtual ~TextOutputDev();

  // Check if file was successfully created.
  virtual GBool isOk() { return ok; }

  //---- get info about output device

  // Does this device use upside-down coordinates?
  // (Upside-down means (0,0) is the top left corner of the page.)
  virtual GBool upsideDown() { return gTrue; }

  // Does this device use drawChar() or drawString()?
  virtual GBool useDrawChar() { return gTrue; }

  //----- initialization and control

  // Start a page.
  virtual void startPage(int pageNum, GfxState *state);

  // End a page.
  virtual void endPage();

  //----- update text state
  virtual void updateFont(GfxState *state);

  //----- text drawing
  virtual void beginString(GfxState *state, GString *s);
  virtual void endString(GfxState *state);
  virtual void drawChar(GfxState *state, double x, double y,
			double dx, double dy, Guchar c);
  virtual void drawChar16(GfxState *state, double x, double y,
			  double dx, double dy, int c);

  //----- special access

  // Find a string.  If <top> is true, starts looking at top of page;
  // otherwise starts looking at <xMin>,<yMin>.  If <bottom> is true,
  // stops looking at bottom of page; otherwise stops looking at
  // <xMax>,<yMax>.  If found, sets the text bounding rectange and
  // returns true; otherwise returns false.
  GBool findText(char *s, GBool top, GBool bottom,
		 double *xMin, double *yMin,
		 double *xMax, double *yMax);

private:

  FILE *f;			// text file
  GBool needClose;		// need to close the file?
  TextPage *text;		// text for the current page
  GBool rawOrder;		// keep text in content stream order
  GBool hexCodes;		// subsetted font with hex char codes
  GBool ok;			// set up ok?
};

#endif
