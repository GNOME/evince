//========================================================================
//
// TextOutputDev.cc
//
// Copyright 1997 Derek B. Noonburg
//
//========================================================================

#ifdef __GNUC__
#pragma implementation
#endif

#include <stdio.h>
#include <stdlib.h>
#include <stddef.h>
#include <ctype.h>
#include "GString.h"
#include "gmem.h"
#include "config.h"
#include "Error.h"
#include "GfxState.h"
#include "GfxFont.h"
#include "TextOutputDev.h"

#include "TextOutputFontInfo.h"

//------------------------------------------------------------------------
// Character substitutions
//------------------------------------------------------------------------

static char *isoLatin1Subst[] = {
  "L",				// Lslash
  "OE",				// OE
  "S",				// Scaron
  "Y",				// Ydieresis
  "Z",				// Zcaron
  "fi",				// fi
  "fl",				// fl
  "i",				// dotlessi
  "l",				// lslash
  "oe",				// oe
  "s",				// scaron
  "z",				// zcaron
  "*",				// bullet
  "...",			// ellipsis
  "-", "-",			// emdash, hyphen
  "\"", "\"",			// quotedblleft, quotedblright
  "'",				// quotesingle
  "TM"				// trademark
};

static char *ascii7Subst[] = {
  "A", "A", "A", "A",		// A{acute,circumflex,dieresis,grave}
  "A", "A",			// A{ring,tilde}
  "AE",				// AE
  "C",				// Ccedilla
  "E", "E", "E", "E",		// E{acute,circumflex,dieresis,grave}
  "I", "I", "I", "I",		// I{acute,circumflex,dieresis,grave}
  "L",				// Lslash
  "N",				// Ntilde
  "O", "O", "O", "O",		// O{acute,circumflex,dieresis,grave}
  "O", "O",			// O{slash,tilde}
  "OE",				// OE
  "S",				// Scaron
  "U", "U", "U", "U",		// U{acute,circumflex,dieresis,grave}
  "Y", "Y",			// T{acute,dieresis}
  "Z",				// Zcaron
  "a", "a", "a", "a",		// a{acute,circumflex,dieresis,grave}
  "a", "a",			// a{ring,tilde}
  "ae",				// ae
  "c",				// ccedilla
  "e", "e", "e", "e",		// e{acute,circumflex,dieresis,grave}
  "fi",				// fi
  "fl",				// fl
  "i",				// dotlessi
  "i", "i", "i", "i",		// i{acute,circumflex,dieresis,grave}
  "l",				// lslash
  "n",				// ntilde
  "o", "o", "o", "o",		// o{acute,circumflex,dieresis,grave}
  "o", "o",			// o{slash,tilde}
  "oe",				// oe
  "s",				// scaron
  "u", "u", "u", "u",		// u{acute,circumflex,dieresis,grave}
  "y", "y",			// t{acute,dieresis}
  "z",				// zcaron
  "|",				// brokenbar
  "*",				// bullet
  "...",			// ellipsis
  "-", "-", "-",		// emdash, endash, hyphen
  "\"", "\"",			// quotedblleft, quotedblright
  "'",				// quotesingle
  "(R)",			// registered
  "TM"				// trademark
};

//------------------------------------------------------------------------
// TextString
//------------------------------------------------------------------------

TextString::TextString(GfxState *state, GBool hexCodes1) {
  double x, y, h;

  state->transform(state->getCurX(), state->getCurY(), &x, &y);
  h = state->getTransformedFontSize();
  //~ yMin/yMax computation should use font ascent/descent values
  yMin = y - 0.95 * h;
  yMax = yMin + 1.3 * h;
  col = 0;
  text = new GString();
  xRight = NULL;
  yxNext = NULL;
  xyNext = NULL;
  hexCodes = hexCodes1;
}

TextString::~TextString() {
  delete text;
  gfree(xRight);
}

void TextString::addChar(GfxState *state, double x, double y,
			 double dx, double dy,
			 Guchar c, GBool useASCII7) {
  char *charName, *sub;
  int c1;
  int i, j, n, m;

  // get current index
  i = text->getLength();

  // append translated character(s) to string
  sub = NULL;
  n = 1;
  if ((charName = state->getFont()->getCharName(c))) {
    if (useASCII7)
      c1 = ascii7Encoding.getCharCode(charName);
    else
      c1 = isoLatin1Encoding.getCharCode(charName);
    if (c1 < 0) {
      m = strlen(charName);
      if (hexCodes && m == 3 &&
	  (charName[0] == 'B' || charName[0] == 'C' ||
	   charName[0] == 'G') &&
	  isxdigit(charName[1]) && isxdigit(charName[2])) {
	sscanf(charName+1, "%x", &c1);
      } else if (!hexCodes && m >= 2 && m <= 3 &&
		 isdigit(charName[0]) && isdigit(charName[1])) {
	c1 = atoi(charName);
	if (c1 >= 256)
	  c1 = -1;
      } else if (!hexCodes && m >= 3 && m <= 5 && isdigit(charName[1])) {
	c1 = atoi(charName+1);
	if (c1 >= 256)
	  c1 = -1;
      }
      //~ this is a kludge -- is there a standard internal encoding
      //~ used by all/most Type 1 fonts?
      if (c1 == 262)		// hyphen
	c1 = 45;
      else if (c1 == 266)	// emdash
	c1 = 208;
      if (useASCII7)
	c1 = ascii7Encoding.getCharCode(isoLatin1Encoding.getCharName(c1));
    }
    if (useASCII7) {
      if (c1 >= 128) {
	sub = ascii7Subst[c1 - 128];
	n = strlen(sub);
      }
    } else {
      if (c1 >= 256) {
	sub = isoLatin1Subst[c1 - 256];
	n = strlen(sub);
      }
    }
  } else {
    c1 = -1;
  }
  if (sub)
    text->append(sub);
  else if (c1 >= 0)
    text->append((char)c1);
  else
    text->append(' ');

  // update position information
  if (i+n > ((i+15) & ~15))
    xRight = (double *)grealloc(xRight, ((i+n+15) & ~15) * sizeof(double));
  if (i == 0)
    xMin = x;
  for (j = 0; j < n; ++j)
    xRight[i+j] = x + ((j+1) * dx) / n;
  xMax = x + dx;
}

//------------------------------------------------------------------------
// TextPage
//------------------------------------------------------------------------

TextPage::TextPage(GBool useASCII71) {
  useASCII7 = useASCII71;
  curStr = NULL;
  yxStrings = NULL;
  xyStrings = NULL;
}

TextPage::~TextPage() {
  clear();
}

void TextPage::beginString(GfxState *state, GString *s, GBool hexCodes) {
  curStr = new TextString(state, hexCodes);
}

void TextPage::addChar(GfxState *state, double x, double y,
		       double dx, double dy, Guchar c) {
  double x1, y1, w1, h1;

  state->transform(x, y, &x1, &y1);
  state->transformDelta(dx, dy, &w1, &h1);
  curStr->addChar(state, x1, y1, w1, h1, c, useASCII7);
}

void TextPage::endString() {
  TextString *p1, *p2;
  double h, y1, y2;

  // throw away zero-length strings -- they don't have valid xMin/xMax
  // values, and they're useless anyway
  if (curStr->text->getLength() == 0) {
    delete curStr;
    curStr = NULL;
    return;
  }

#if 0 //~tmp
  if (curStr->yMax - curStr->yMin > 20) {
    delete curStr;
    curStr = NULL;
    return;
  }
#endif

  // insert string in y-major list
  h = curStr->yMax - curStr->yMin;
  y1 = curStr->yMin + 0.5 * h;
  y2 = curStr->yMin + 0.8 * h;
  for (p1 = NULL, p2 = yxStrings; p2; p1 = p2, p2 = p2->yxNext) {
    if (y1 < p2->yMin || (y2 < p2->yMax && curStr->xMax < p2->xMin))
      break;
  }
  if (p1)
    p1->yxNext = curStr;
  else
    yxStrings = curStr;
  curStr->yxNext = p2;
  curStr = NULL;
}

void TextPage::coalesce() {
  TextString *str1, *str2;
  double space, d;
  int n, i;

#if 0 //~ for debugging
  for (str1 = yxStrings; str1; str1 = str1->yxNext) {
    printf("x=%3d..%3d  y=%3d..%3d  size=%2d '%s'\n",
	   (int)str1->xMin, (int)str1->xMax, (int)str1->yMin, (int)str1->yMax,
	   (int)(str1->yMax - str1->yMin), str1->text->getCString());
  }
  printf("\n------------------------------------------------------------\n\n");
#endif
  str1 = yxStrings;
  while (str1 && (str2 = str1->yxNext)) {
    space = str1->yMax - str1->yMin;
    d = str2->xMin - str1->xMax;
#if 0 //~tmp
    if (str2->yMin < str1->yMax && d > -0.1 * space && d < 0.2 * space) {
#else
    if (str2->yMin < str1->yMax && d > -0.5 * space && d < space) {
#endif
      n = str1->text->getLength();
      if (d > 0.1 * space)
	str1->text->append(' ');
      str1->text->append(str2->text);
      str1->xRight = (double *)
	grealloc(str1->xRight, str1->text->getLength() * sizeof(double));
      if (d > 0.1 * space)
	str1->xRight[n++] = str2->xMin;
      for (i = 0; i < str2->text->getLength(); ++i)
	str1->xRight[n++] = str2->xRight[i];
      if (str2->xMax > str1->xMax)
	str1->xMax = str2->xMax;
      if (str2->yMax > str1->yMax)
	str1->yMax = str2->yMax;
      str1->yxNext = str2->yxNext;
      delete str2;
    } else {
      str1 = str2;
    }
  }
}

GBool TextPage::findText(char *s, GBool top, GBool bottom,
			 double *xMin, double *yMin,
			 double *xMax, double *yMax) {
  TextString *str;
  char *p, *p1, *q;
  int n, m, i;
  double x;

  // scan all strings on page
  n = strlen(s);
  for (str = yxStrings; str; str = str->yxNext) {

    // check: above top limit?
    if (!top && (str->yMax < *yMin ||
		 (str->yMin < *yMin && str->xMax <= *xMin)))
      continue;

    // check: below bottom limit?
    if (!bottom && (str->yMin > *yMax ||
		    (str->yMax > *yMax && str->xMin >= *xMax)))
      return gFalse;

    // search each position in this string
    m = str->text->getLength();
    for (i = 0, p = str->text->getCString(); i <= m - n; ++i, ++p) {

      // check: above top limit?
      if (!top && str->yMin < *yMin) {
	x = (((i == 0) ? str->xMin : str->xRight[i-1]) + str->xRight[i]) / 2;
	if (x < *xMin)
	  continue;
      }

      // check: below bottom limit?
      if (!bottom && str->yMax > *yMax) {
	x = (((i == 0) ? str->xMin : str->xRight[i-1]) + str->xRight[i]) / 2;
	if (x > *xMax)
	  return gFalse;
      }

      // compare the strings
      for (p1 = p, q = s; *q; ++p1, ++q) {
	if (tolower(*p1) != tolower(*q))
	  break;
      }

      // found it
      if (!*q) {
	*xMin = (i == 0) ? str->xMin : str->xRight[i-1];
	*xMax = str->xRight[i+n-1];
	*yMin = str->yMin;
	*yMax = str->yMax;
	return gTrue;
      }
    }
  }
  return gFalse;
}

GString *TextPage::getText(double xMin, double yMin,
			   double xMax, double yMax) {
  GString *s;
  TextString *str1;
  double x0, x1, x2, y;
  double xPrev, yPrev;
  int i1, i2;
  GBool multiLine;

  s = new GString();
  xPrev = yPrev = 0;
  multiLine = gFalse;
  for (str1 = yxStrings; str1; str1 = str1->yxNext) {
    y = 0.5 * (str1->yMin + str1->yMax);
    if (y > yMax)
      break;
    if (y > yMin && str1->xMin < xMax && str1->xMax > xMin) {
      x0 = x1 = x2 = str1->xMin;
      for (i1 = 0; i1 < str1->text->getLength(); ++i1) {
	x0 = (i1==0) ? str1->xMin : str1->xRight[i1-1];
	x1 = str1->xRight[i1];
	if (0.5 * (x0 + x1) >= xMin)
	  break;
      }
      for (i2 = str1->text->getLength() - 1; i2 > i1; --i2) {
	x1 = (i2==0) ? str1->xMin : str1->xRight[i2-1];
	x2 = str1->xRight[i2];
	if (0.5 * (x1 + x2) <= xMax)
	  break;
      }
      if (s->getLength() > 0) {
	if (x0 < xPrev || str1->yMin > yPrev) {
	  s->append('\n');
	  multiLine = gTrue;
	} else {
	  s->append("    ");
	}
      }
      s->append(str1->text->getCString() + i1, i2 - i1 + 1);
      xPrev = x2;
      yPrev = str1->yMax;
    }
  }
  if (multiLine)
    s->append('\n');
  return s;
}

void TextPage::dump(FILE *f) {
  TextString *str1, *str2, *str3;
  double yMin, yMax;
  int col1, col2;
  double d;

  // build x-major list
  xyStrings = NULL;
  for (str1 = yxStrings; str1; str1 = str1->yxNext) {
    for (str2 = NULL, str3 = xyStrings;
	 str3;
	 str2 = str3, str3 = str3->xyNext) {
      if (str1->xMin < str3->xMin ||
	  (str1->xMin == str3->xMin && str1->yMin < str3->yMin))
	break;
    }
    if (str2)
      str2->xyNext = str1;
    else
      xyStrings = str1;
    str1->xyNext = str3;
  }

  // do column assignment
  for (str1 = xyStrings; str1; str1 = str1->xyNext) {
    col1 = 0;
    for (str2 = xyStrings; str2 != str1; str2 = str2->xyNext) {
      if (str1->xMin >= str2->xMax) {
	col2 = str2->col + str2->text->getLength() + 4;
	if (col2 > col1)
	  col1 = col2;
      } else if (str1->xMin > str2->xMin) {
	col2 = str2->col +
	       (int)(((str1->xMin - str2->xMin) / (str2->xMax - str2->xMin)) *
		     str2->text->getLength());
	if (col2 > col1) {
	  col1 = col2;
	}
      }
    }
    str1->col = col1;
  }

#if 0 //~ for debugging
  fprintf(f, "~~~~~~~~~~\n");
  for (str1 = yxStrings; str1; str1 = str1->yxNext) {
    fprintf(f, "(%4d,%4d) - (%4d,%4d) [%3d] %s\n",
	    (int)str1->xMin, (int)str1->yMin, (int)str1->xMax, (int)str1->yMax,
	    str1->col, str1->text->getCString());
  }
  fprintf(f, "~~~~~~~~~~\n");
#endif

  // output
  col1 = 0;
  yMax = yxStrings ? yxStrings->yMax : 0;
  for (str1 = yxStrings; str1; str1 = str1->yxNext) {

    // line this string up with the correct column
    for (; col1 < str1->col; ++col1)
      fputc(' ', f);

    // print the string
    fputs(str1->text->getCString(), f);

    // increment column
    col1 += str1->text->getLength();

    // update yMax for this line
    if (str1->yMax > yMax)
      yMax = str1->yMax;

    // if we've hit the end of the line...
#if 0 //~
    if (!(str1->yxNext && str1->yxNext->yMin < str1->yMax &&
	  str1->yxNext->xMin >= str1->xMax)) {
#else
    if (!(str1->yxNext &&
	  str1->yxNext->yMin < 0.2*str1->yMin + 0.8*str1->yMax &&
	  str1->yxNext->xMin >= str1->xMax)) {
#endif

      // print a return
      fputc('\n', f);

      // print extra vertical space if necessary
      if (str1->yxNext) {

	// find yMin for next line
	yMin = str1->yxNext->yMin;
	for (str2 = str1->yxNext; str2; str2 = str2->yxNext) {
	  if (str2->yMin < yMin)
	    yMin = str2->yMin;
	  if (!(str2->yxNext && str2->yxNext->yMin < str2->yMax &&
		str2->yxNext->xMin >= str2->xMax))
	    break;
	}
	  
	// print the space
	d = (int)((yMin - yMax) / (str1->yMax - str1->yMin) + 0.5);
	for (; d > 0; --d)
	  fputc('\n', f);
      }

      // set up for next line
      col1 = 0;
      yMax = str1->yxNext ? str1->yxNext->yMax : 0;
    }
  }
}

void TextPage::clear() {
  TextString *p1, *p2;

  if (curStr) {
    delete curStr;
    curStr = NULL;
  }
  for (p1 = yxStrings; p1; p1 = p2) {
    p2 = p1->yxNext;
    delete p1;
  }
  yxStrings = NULL;
  xyStrings = NULL;
}

//------------------------------------------------------------------------
// TextOutputDev
//------------------------------------------------------------------------

TextOutputDev::TextOutputDev(char *fileName, GBool useASCII7) {
  text = NULL;
  ok = gTrue;

  // open file
  needClose = gFalse;
  if (fileName) {
    if (!strcmp(fileName, "-")) {
      f = stdout;
    } else if ((f = fopen(fileName, "w"))) {
      needClose = gTrue;
    } else {
      error(-1, "Couldn't open text file '%s'", fileName);
      ok = gFalse;
      return;
    }
  } else {
    f = NULL;
  }

  // set up text object
  text = new TextPage(useASCII7);
}

TextOutputDev::~TextOutputDev() {
  if (needClose)
    fclose(f);
  if (text)
    delete text;
}

void TextOutputDev::startPage(int pageNum, GfxState *state) {
  text->clear();
}

void TextOutputDev::endPage() {
  text->coalesce();
  if (f) {
    text->dump(f);
    fputc('\n', f);
    fputs("\f\n", f);
    fputc('\n', f);
  }
}

void TextOutputDev::updateFont(GfxState *state) {
  GfxFont *font;
  char *charName;
  int c;

  // look for hex char codes in subsetted font
  hexCodes = gFalse;
  if ((font = state->getFont())) {
    for (c = 0; c < 256; ++c) {
      if ((charName = font->getCharName(c))) {
	if ((charName[0] == 'B' || charName[0] == 'C' ||
	     charName[0] == 'G') &&
	    strlen(charName) == 3 &&
	    ((charName[1] >= 'a' && charName[1] <= 'f') ||
	     (charName[1] >= 'A' && charName[1] <= 'F') ||
	     (charName[2] >= 'a' && charName[2] <= 'f') ||
	     (charName[2] >= 'A' && charName[2] <= 'F'))) {
	  hexCodes = gTrue;
	  break;
	}
      }
    }
  }
}

void TextOutputDev::beginString(GfxState *state, GString *s) {
  text->beginString(state, s, hexCodes);
}

void TextOutputDev::endString(GfxState *state) {
  text->endString();
}

void TextOutputDev::drawChar(GfxState *state, double x, double y,
			     double dx, double dy, Guchar c) {
  text->addChar(state, x, y, dx, dy, c);
}

GBool TextOutputDev::findText(char *s, GBool top, GBool bottom,
			      double *xMin, double *yMin,
			      double *xMax, double *yMax) {
  return text->findText(s, top, bottom, xMin, yMin, xMax, yMax);
}
