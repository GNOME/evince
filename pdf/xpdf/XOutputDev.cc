//========================================================================
//
// XOutputDev.cc
//
// Copyright 1996 Derek B. Noonburg
//
//========================================================================

#ifdef __GNUC__
#pragma implementation
#endif

#include <stdio.h>
#include <stdlib.h>
#include <stddef.h>
#include <string.h>
#include <ctype.h>
#include <math.h>
#include "gmem.h"
#include "GString.h"
#include "Object.h"
#include "Stream.h"
#include "GfxState.h"
#include "GfxFont.h"
#include "Error.h"
#include "Params.h"
#include "TextOutputDev.h"
#include "XOutputDev.h"

#include "XOutputFontInfo.h"

#ifdef XlibSpecificationRelease
#if XlibSpecificationRelease < 5
typedef char *XPointer;
#endif
#else
typedef char *XPointer;
#endif

//------------------------------------------------------------------------
// Constants and macros
//------------------------------------------------------------------------

#define xoutRound(x) ((int)(x + 0.5))

#define maxCurveSplits 6	// max number of splits when recursively
				//   drawing Bezier curves

//------------------------------------------------------------------------
// Parameters
//------------------------------------------------------------------------

GBool installCmap;

int rgbCubeSize;

//------------------------------------------------------------------------
// Font map
//------------------------------------------------------------------------

struct FontMapEntry {
  char *pdfFont;
  char *xFont;
  GfxFontEncoding *encoding;
};

static FontMapEntry fontMap[] = {
  {"Courier",
   "-*-courier-medium-r-normal-*-%s-*-*-*-*-*-iso8859-1",
   &isoLatin1Encoding},
  {"Courier-Bold",
   "-*-courier-bold-r-normal-*-%s-*-*-*-*-*-iso8859-1",
   &isoLatin1Encoding},
  {"Courier-BoldOblique",
   "-*-courier-bold-o-normal-*-%s-*-*-*-*-*-iso8859-1",
   &isoLatin1Encoding},
  {"Courier-Oblique",
   "-*-courier-medium-o-normal-*-%s-*-*-*-*-*-iso8859-1",
   &isoLatin1Encoding},
  {"Helvetica",
   "-*-helvetica-medium-r-normal-*-%s-*-*-*-*-*-iso8859-1",
   &isoLatin1Encoding},
  {"Helvetica-Bold",
   "-*-helvetica-bold-r-normal-*-%s-*-*-*-*-*-iso8859-1",
   &isoLatin1Encoding},
  {"Helvetica-BoldOblique",
   "-*-helvetica-bold-o-normal-*-%s-*-*-*-*-*-iso8859-1",
   &isoLatin1Encoding},
  {"Helvetica-Oblique",
   "-*-helvetica-medium-o-normal-*-%s-*-*-*-*-*-iso8859-1",
   &isoLatin1Encoding},
  {"Symbol",
   "-*-symbol-medium-r-normal-*-%s-*-*-*-*-*-adobe-fontspecific",
   &symbolEncoding},
  {"Times-Bold",
   "-*-times-bold-r-normal-*-%s-*-*-*-*-*-iso8859-1",
   &isoLatin1Encoding},
  {"Times-BoldItalic",
   "-*-times-bold-i-normal-*-%s-*-*-*-*-*-iso8859-1",
   &isoLatin1Encoding},
  {"Times-Italic",
   "-*-times-medium-i-normal-*-%s-*-*-*-*-*-iso8859-1",
   &isoLatin1Encoding},
  {"Times-Roman",
   "-*-times-medium-r-normal-*-%s-*-*-*-*-*-iso8859-1",
   &isoLatin1Encoding},
  {"ZapfDingbats",
   "-*-zapfdingbats-medium-r-normal-*-%s-*-*-*-*-*-*-*",
   &zapfDingbatsEncoding},
  {NULL}
};

static FontMapEntry *userFontMap;

//------------------------------------------------------------------------
// Font substitutions
//------------------------------------------------------------------------

struct FontSubst {
  char *xFont;
  double mWidth;
};

// index: {symbolic:12, fixed:8, serif:4, sans-serif:0} + bold*2 + italic
static FontSubst fontSubst[16] = {
  {"-*-helvetica-medium-r-normal-*-%s-*-*-*-*-*-iso8859-1",       0.833},
  {"-*-helvetica-medium-o-normal-*-%s-*-*-*-*-*-iso8859-1",       0.833},
  {"-*-helvetica-bold-r-normal-*-%s-*-*-*-*-*-iso8859-1",         0.889},
  {"-*-helvetica-bold-o-normal-*-%s-*-*-*-*-*-iso8859-1",         0.889},
  {"-*-times-medium-r-normal-*-%s-*-*-*-*-*-iso8859-1",           0.788},
  {"-*-times-medium-i-normal-*-%s-*-*-*-*-*-iso8859-1",           0.722},
  {"-*-times-bold-r-normal-*-%s-*-*-*-*-*-iso8859-1",             0.833},
  {"-*-times-bold-i-normal-*-%s-*-*-*-*-*-iso8859-1",             0.778},
  {"-*-courier-medium-r-normal-*-%s-*-*-*-*-*-iso8859-1",         0.600},
  {"-*-courier-medium-o-normal-*-%s-*-*-*-*-*-iso8859-1",         0.600},
  {"-*-courier-bold-r-normal-*-%s-*-*-*-*-*-iso8859-1",           0.600},
  {"-*-courier-bold-o-normal-*-%s-*-*-*-*-*-iso8859-1",           0.600},
  {"-*-symbol-medium-r-normal-*-%s-*-*-*-*-*-adobe-fontspecific", 0.576},
  {"-*-symbol-medium-r-normal-*-%s-*-*-*-*-*-adobe-fontspecific", 0.576},
  {"-*-symbol-medium-r-normal-*-%s-*-*-*-*-*-adobe-fontspecific", 0.576},
  {"-*-symbol-medium-r-normal-*-%s-*-*-*-*-*-adobe-fontspecific", 0.576}
};

//------------------------------------------------------------------------
// 16-bit fonts
//------------------------------------------------------------------------

#if JAPANESE_SUPPORT

static char *japan12Font = "-*-fixed-medium-r-normal-*-%s-*-*-*-*-*-jisx0208.1983-0";

// CID 0 .. 96
static Gushort japan12Map[96] = {
  0x2120, 0x2120, 0x212a, 0x2149, 0x2174, 0x2170, 0x2173, 0x2175, // 00 .. 07
  0x2147, 0x214a, 0x214b, 0x2176, 0x215c, 0x2124, 0x213e, 0x2123, // 08 .. 0f
  0x213f, 0x2330, 0x2331, 0x2332, 0x2333, 0x2334, 0x2335, 0x2336, // 10 .. 17
  0x2337, 0x2338, 0x2339, 0x2127, 0x2128, 0x2163, 0x2161, 0x2164, // 18 .. 1f
  0x2129, 0x2177, 0x2341, 0x2342, 0x2343, 0x2344, 0x2345, 0x2346, // 20 .. 27
  0x2347, 0x2348, 0x2349, 0x234a, 0x234b, 0x234c, 0x234d, 0x234e, // 28 .. 2f
  0x234f, 0x2350, 0x2351, 0x2352, 0x2353, 0x2354, 0x2355, 0x2356, // 30 .. 37
  0x2357, 0x2358, 0x2359, 0x235a, 0x214e, 0x216f, 0x214f, 0x2130, // 38 .. 3f
  0x2132, 0x2146, 0x2361, 0x2362, 0x2363, 0x2364, 0x2365, 0x2366, // 40 .. 47
  0x2367, 0x2368, 0x2369, 0x236a, 0x236b, 0x236c, 0x236d, 0x236e, // 48 .. 4f
  0x236f, 0x2370, 0x2371, 0x2372, 0x2373, 0x2374, 0x2375, 0x2376, // 50 .. 57
  0x2377, 0x2378, 0x2379, 0x237a, 0x2150, 0x2143, 0x2151, 0x2141  // 58 .. 5f
};

// CID 325 .. 421
static Gushort japan12KanaMap1[97] = {
  0x2131, 0x2121, 0x2123, 0x2156, 0x2157, 0x2122, 0x2126, 0x2572,
  0x2521, 0x2523, 0x2525, 0x2527, 0x2529, 0x2563, 0x2565, 0x2567,
  0x2543, 0x213c, 0x2522, 0x2524, 0x2526, 0x2528, 0x252a, 0x252b,
  0x252d, 0x252f, 0x2531, 0x2533, 0x2535, 0x2537, 0x2539, 0x253b,
  0x253d, 0x253f, 0x2541, 0x2544, 0x2546, 0x2548, 0x254a, 0x254b,
  0x254c, 0x254d, 0x254e, 0x254f, 0x2552, 0x2555, 0x2558, 0x255b,
  0x255e, 0x255f, 0x2560, 0x2561, 0x2562, 0x2564, 0x2566, 0x2568,
  0x2569, 0x256a, 0x256b, 0x256c, 0x256d, 0x256f, 0x2573, 0x212b,
  0x212c, 0x212e, 0x2570, 0x2571, 0x256e, 0x2575, 0x2576, 0x2574,
  0x252c, 0x252e, 0x2530, 0x2532, 0x2534, 0x2536, 0x2538, 0x253a,
  0x253c, 0x253e, 0x2540, 0x2542, 0x2545, 0x2547, 0x2549, 0x2550,
  0x2551, 0x2553, 0x2554, 0x2556, 0x2557, 0x2559, 0x255a, 0x255c,
  0x255d
};

// CID 501 .. 598
static Gushort japan12KanaMap2[98] = {
  0x212d, 0x212f, 0x216d, 0x214c, 0x214d, 0x2152, 0x2153, 0x2154,
  0x2155, 0x2158, 0x2159, 0x215a, 0x215b, 0x213d, 0x2121, 0x2472,
  0x2421, 0x2423, 0x2425, 0x2427, 0x2429, 0x2463, 0x2465, 0x2467,
  0x2443, 0x2422, 0x2424, 0x2426, 0x2428, 0x242a, 0x242b, 0x242d,
  0x242f, 0x2431, 0x2433, 0x2435, 0x2437, 0x2439, 0x243b, 0x243d,
  0x243f, 0x2441, 0x2444, 0x2446, 0x2448, 0x244a, 0x244b, 0x244c,
  0x244d, 0x244e, 0x244f, 0x2452, 0x2455, 0x2458, 0x245b, 0x245e,
  0x245f, 0x2460, 0x2461, 0x2462, 0x2464, 0x2466, 0x2468, 0x2469,
  0x246a, 0x246b, 0x246c, 0x246d, 0x246f, 0x2473, 0x2470, 0x2471,
  0x246e, 0x242c, 0x242e, 0x2430, 0x2432, 0x2434, 0x2436, 0x2438,
  0x243a, 0x243c, 0x243e, 0x2440, 0x2442, 0x2445, 0x2447, 0x2449,
  0x2450, 0x2451, 0x2453, 0x2454, 0x2456, 0x2457, 0x2459, 0x245a,
  0x245c, 0x245d
};

static char *japan12Roman[10] = {
  "I", "II", "III", "IV", "V", "VI", "VII", "VIII", "IX", "X"
};

static char *japan12Abbrev1[6] = {
  "mm", "cm", "km", "mg", "kg", "cc"
};

#endif

//------------------------------------------------------------------------
// Constructed characters
//------------------------------------------------------------------------

#define lastRegularChar 0x0ff
#define firstSubstChar  0x100
#define lastSubstChar   0x104
#define firstConstrChar 0x105
#define lastConstrChar  0x106
#define firstMultiChar  0x107
#define lastMultiChar   0x10d

// substituted chars
static Guchar substChars[] = {
  0x27,				// 100: quotesingle --> quoteright
  0x2d,				// 101: emdash --> hyphen
  0xad,				// 102: hyphen --> endash
  0x2f,				// 103: fraction --> slash
  0xb0,				// 104: ring --> degree
};

// constructed chars
// 105: bullet
// 106: trademark

// built-up chars
static char *multiChars[] = {
  "fi",				// 107: fi
  "fl",				// 108: fl
  "OE",				// 109: OE
  "oe",				// 10a: oe
  "...",			// 10b: ellipsis
  "``",				// 10c: quotedblleft
  "''"				// 10d: quotedblright
};

// ignored chars
// 10c: Lslash
// 10d: Scaron
// 10e: Zcaron
// 10f: Ydieresis
// 110: breve
// 111: caron
// 112: circumflex
// 113: dagger
// 114: daggerdbl
// 115: dotaccent
// 116: dotlessi
// 117: florin
// 118: grave
// 119: guilsinglleft
// 11a: guilsinglright
// 11b: hungarumlaut
// 11c: lslash
// 11d: ogonek
// 11e: perthousand
// 11f: quotedblbase
// 120: quotesinglbase
// 121: scaron
// 122: tilde
// 123: zcaron

//------------------------------------------------------------------------
// XOutputFont
//------------------------------------------------------------------------

// Note: if real font is substantially narrower than substituted
// font, the size is reduced accordingly.
XOutputFont::XOutputFont(GfxFont *gfxFont, double m11, double m12,
			 double m21, double m22, Display *display1) {
  GString *pdfFont;
  FontMapEntry *p;
  GfxFontEncoding *encoding;
  char *fontNameFmt;
  char fontName[200], fontSize[100];
  GBool rotated;
  double size;
  int startSize, sz;
  int index;
  int code, code2;
  double w1, w2, v;
  double *fm;
  char *charName;
  int n;

  // init
  id = gfxFont->getID();
  mat11 = m11;
  mat12 = m12;
  mat21 = m21;
  mat22 = m22;
  display = display1;
  xFont = NULL;
  hex = gFalse;

  // construct X font name
  if (gfxFont->is16Bit()) {
    fontNameFmt = fontSubst[0].xFont;
    switch (gfxFont->getCharSet16()) {
    case font16AdobeJapan12:
#if JAPANESE_SUPPORT
      fontNameFmt = japan12Font;
#endif
      break;
    }
  } else {
    pdfFont = gfxFont->getName();
    if (pdfFont) {
      for (p = userFontMap; p->pdfFont; ++p) {
	if (!pdfFont->cmp(p->pdfFont))
	  break;
      }
      if (!p->pdfFont) {
	for (p = fontMap; p->pdfFont; ++p) {
	  if (!pdfFont->cmp(p->pdfFont))
	    break;
	}
      }
    } else {
      p = NULL;
    }
    if (p && p->pdfFont) {
      fontNameFmt = p->xFont;
      encoding = p->encoding;
    } else {
      encoding = &isoLatin1Encoding;
//~ Some non-symbolic fonts are tagged as symbolic.
//      if (gfxFont->isSymbolic()) {
//        index = 12;
//        encoding = symbolEncoding;
//      } else
      if (gfxFont->isFixedWidth()) {
	index = 8;
      } else if (gfxFont->isSerif()) {
	index = 4;
      } else {
	index = 0;
      }
      if (gfxFont->isBold())
	index += 2;
      if (gfxFont->isItalic())
	index += 1;
      if ((code = gfxFont->getCharCode("m")) >= 0)
	w1 = gfxFont->getWidth(code);
      else
	w1 = 0;
      w2 = fontSubst[index].mWidth;
      if (gfxFont->getType() == fontType3) {
	// This is a hack which makes it possible to substitute for some
	// Type 3 fonts.  The problem is that it's impossible to know what
	// the base coordinate system used in the font is without actually
	// rendering the font.  This code tries to guess by looking at the
	// width of the character 'm' (which breaks if the font is a
	// subset that doesn't contain 'm').
	if (w1 > 0 && (w1 > 1.1 * w2 || w1 < 0.9 * w2)) {
	  w1 /= w2;
	  mat11 *= w1;
	  mat12 *= w1;
	  mat21 *= w1;
	  mat22 *= w1;
	}
	fm = gfxFont->getFontMatrix();
	v = (fm[0] == 0) ? 1 : (fm[3] / fm[0]);
	mat12 *= v;
	mat22 *= v;
      } else if (!gfxFont->isSymbolic()) {
	if (w1 > 0.01 && w1 < 0.9 * w2) {
	  w1 /= w2;
	  if (w1 < 0.8)
	    w1 = 0.8;
	  mat11 *= w1;
	  mat12 *= w1;
	  mat21 *= w1;
	  mat22 *= w1;
	}
      }
      fontNameFmt = fontSubst[index].xFont;
    }

    // Construct forward and reverse map.
    // This tries to deal with font subset character names of the
    // form 'Bxx', 'Cxx', 'Gxx', with decimal or hex numbering.
    for (code = 0; code < 256; ++code)
      revMap[code] = 0;
    if (encoding) {
      for (code = 0; code < 256; ++code) {
	if ((charName = gfxFont->getCharName(code))) {
	  if ((charName[0] == 'B' || charName[0] == 'C' ||
	       charName[0] == 'G') &&
	      strlen(charName) == 3 &&
	      ((charName[1] >= 'a' && charName[1] <= 'f') ||
	       (charName[1] >= 'A' && charName[1] <= 'F') ||
	       (charName[2] >= 'a' && charName[2] <= 'f') ||
	       (charName[2] >= 'A' && charName[2] <= 'F'))) {
	    hex = gTrue;
	    break;
	  }
	}
      }
      for (code = 0; code < 256; ++code) {
	if ((charName = gfxFont->getCharName(code))) {
	  if ((code2 = encoding->getCharCode(charName)) < 0) {
	    n = strlen(charName);
	    if (hex && n == 3 &&
		(charName[0] == 'B' || charName[0] == 'C' ||
		 charName[0] == 'G') &&
		isxdigit(charName[1]) && isxdigit(charName[2])) {
	      sscanf(charName+1, "%x", &code2);
	    } else if (!hex && n >= 2 && n <= 3 &&
		       isdigit(charName[0]) && isdigit(charName[1])) {
	      code2 = atoi(charName);
	      if (code2 >= 256)
		code2 = -1;
	    } else if (!hex && n >= 3 && n <= 5 && isdigit(charName[1])) {
	      code2 = atoi(charName+1);
	      if (code2 >= 256)
		code2 = -1;
	    }
	    //~ this is a kludge -- is there a standard internal encoding
	    //~ used by all/most Type 1 fonts?
	    if (code2 == 262)		// hyphen
	      code2 = 45;
	    else if (code2 == 266)	// emdash
	      code2 = 208;
	  }
	  if (code2 >= 0) {
	    map[code] = (Gushort)code2;
	    if (code2 < 256)
	      revMap[code2] = (Guchar)code;
	  } else {
	    map[code] = 0;
	  }
	} else {
	  map[code] = 0;
	}
      }
    } else {
      code2 = 0; // to make gcc happy
      //~ this is a hack to get around the fact that X won't draw
      //~ chars 0..31; this works when the fonts have duplicate encodings
      //~ for those chars
      for (code = 0; code < 32; ++code) {
	if ((charName = gfxFont->getCharName(code)) &&
	    (code2 = gfxFont->getCharCode(charName)) >= 0) {
	  map[code] = (Gushort)code2;
	  if (code2 < 256)
	    revMap[code2] = (Guchar)code;
	}
      }
      for (code = 32; code < 256; ++code) {
	map[code] = (Gushort)code;
	revMap[code] = (Guchar)code;
      }
    }
  }

  // compute size, normalize matrix
  size = sqrt(mat21*mat21 + mat22*mat22);
  mat11 = mat11 / size;
  mat12 = -mat12 / size;
  mat21 = mat21 / size;
  mat22 = -mat22 / size;
  startSize = (int)size;

  // try to get a rotated font?
  rotated = !(mat11 > 0 && mat22 > 0 && fabs(mat11/mat22 - 1) < 0.2 &&
	      fabs(mat12) < 0.01 && fabs(mat21) < 0.01);

  // open X font -- if font is not found (which means the server can't
  // scale fonts), try progressively smaller and then larger sizes
  //~ This does a linear search -- it should get a list of fonts from
  //~ the server and pick the closest.
  if (rotated)
    sprintf(fontSize, "[%s%0.2f %s%0.2f %s%0.2f %s%0.2f]",
	    mat11<0 ? "~" : "", fabs(mat11 * startSize),
	    mat12<0 ? "~" : "", fabs(mat12 * startSize),
	    mat21<0 ? "~" : "", fabs(mat21 * startSize),
	    mat22<0 ? "~" : "", fabs(mat22 * startSize));
  else
    sprintf(fontSize, "%d", startSize);
  sprintf(fontName, fontNameFmt, fontSize);
  xFont = XLoadQueryFont(display, fontName);
  if (!xFont) {
    for (sz = startSize; sz >= startSize/2 && sz >= 1; --sz) {
      sprintf(fontSize, "%d", sz);
      sprintf(fontName, fontNameFmt, fontSize);
      if ((xFont = XLoadQueryFont(display, fontName)))
	break;
    }
    if (!xFont) {
      for (sz = startSize + 1; sz < startSize + 10; ++sz) {
	sprintf(fontSize, "%d", sz);
	sprintf(fontName, fontNameFmt, fontSize);
	if ((xFont = XLoadQueryFont(display, fontName)))
	  break;
      }
      if (!xFont) {
	sprintf(fontSize, "%d", startSize);
	sprintf(fontName, fontNameFmt, fontSize);
	error(-1, "Failed to open font: '%s'", fontName);
	return;
      }
    }
  }
}

XOutputFont::~XOutputFont() {
  if (xFont)
    XFreeFont(display, xFont);
}

//------------------------------------------------------------------------
// XOutputFontCache
//------------------------------------------------------------------------

XOutputFontCache::XOutputFontCache(Display *display1) {
  int i;

  display = display1;
  for (i = 0; i < fontCacheSize; ++i)
    fonts[i] = NULL;
  numFonts = 0;
}

XOutputFontCache::~XOutputFontCache() {
  int i;

  for (i = 0; i < numFonts; ++i)
    delete fonts[i];
}

XOutputFont *XOutputFontCache::getFont(GfxFont *gfxFont,
				       double m11, double m12,
				       double m21, double m22) {
  XOutputFont *font;
  int i, j;

  // is it the most recently used font?
  if (numFonts > 0 && fonts[0]->matches(gfxFont->getID(),
					m11, m12, m21, m22))
    return fonts[0];

  // is it in the cache?
  for (i = 1; i < numFonts; ++i) {
    if (fonts[i]->matches(gfxFont->getID(), m11, m12, m21, m22)) {
      font = fonts[i];
      for (j = i; j > 0; --j)
	fonts[j] = fonts[j-1];
      fonts[0] = font;
      return font;
    }
  }

  // make a new font
  font = new XOutputFont(gfxFont, m11, m12, m21, m22, display);
  if (!font->getXFont()) {
    delete font;
    return NULL;
  }

  // insert font in cache
  if (numFonts == fontCacheSize) {
    --numFonts;
    delete fonts[numFonts];
  }
  for (j = numFonts; j > 0; --j)
    fonts[j] = fonts[j-1];
  fonts[0] = font;
  ++numFonts;

  // return it
  return font;
}

//------------------------------------------------------------------------
// XOutputDev
//------------------------------------------------------------------------

XOutputDev::XOutputDev(Display *display1, Pixmap pixmap1, Guint depth1,
		       Colormap colormap, unsigned long paperColor) {
  XVisualInfo visualTempl;
  XVisualInfo *visualList;
  int nVisuals;
  Gulong mask;
  XGCValues gcValues;
  XColor xcolor;
  XColor *xcolors;
  int r, g, b, n, m, i;
  GBool ok;

  // get display/pixmap info
  display = display1;
  screenNum = DefaultScreen(display);
  pixmap = pixmap1;
  depth = depth1;

  // check for TrueColor visual
  trueColor = gFalse;
  if (depth == 0) {
    depth = DefaultDepth(display, screenNum);
    visualList = XGetVisualInfo(display, 0, &visualTempl, &nVisuals);
    for (i = 0; i < nVisuals; ++i) {
      if (visualList[i].visual == DefaultVisual(display, screenNum)) {
	if (visualList[i].c_class == TrueColor) {
	  trueColor = gTrue;
	  mask = visualList[i].red_mask;
	  rShift = 0;
	  while (mask && !(mask & 1)) {
	    ++rShift;
	    mask >>= 1;
	  }
	  rMul = (int)mask;
	  mask = visualList[i].green_mask;
	  gShift = 0;
	  while (mask && !(mask & 1)) {
	    ++gShift;
	    mask >>= 1;
	  }
	  gMul = (int)mask;
	  mask = visualList[i].blue_mask;
	  bShift = 0;
	  while (mask && !(mask & 1)) {
	    ++bShift;
	    mask >>= 1;
	  }
	  bMul = (int)mask;
	}
	break;
      }
    }
    XFree((XPointer)visualList);
  }

  // allocate a color cube
  if (!trueColor) {

    // set colors in private colormap
    if (installCmap) {
      for (numColors = 6; numColors >= 2; --numColors) {
	m = numColors * numColors * numColors;
	if (XAllocColorCells(display, colormap, False, NULL, 0, colors, m))
	  break;
      }
      if (numColors >= 2) {
	m = numColors * numColors * numColors;
	xcolors = (XColor *)gmalloc(m * sizeof(XColor));
	n = 0;
	for (r = 0; r < numColors; ++r) {
	  for (g = 0; g < numColors; ++g) {
	    for (b = 0; b < numColors; ++b) {
	      xcolors[n].pixel = colors[n];
	      xcolors[n].red = (r * 65535) / (numColors - 1);
	      xcolors[n].green = (g * 65535) / (numColors - 1);
	      xcolors[n].blue = (b * 65535) / (numColors - 1);
	      xcolors[n].flags = DoRed | DoGreen | DoBlue;
	      ++n;
	    }
	  }
	}
	XStoreColors(display, colormap, xcolors, m);
	gfree(xcolors);
      } else {
	numColors = 1;
	colors[0] = BlackPixel(display, screenNum);
	colors[1] = WhitePixel(display, screenNum);
      }

    // allocate colors in shared colormap
    } else {
      if (rgbCubeSize > maxRGBCube)
	rgbCubeSize = maxRGBCube;
      ok = gFalse;
      for (numColors = rgbCubeSize; numColors >= 2; --numColors) {
	ok = gTrue;
	n = 0;
	for (r = 0; r < numColors && ok; ++r) {
	  for (g = 0; g < numColors && ok; ++g) {
	    for (b = 0; b < numColors && ok; ++b) {
	      if (n == 0) {
		colors[n++] = BlackPixel(display, screenNum);
	      } else {
		xcolor.red = (r * 65535) / (numColors - 1);
		xcolor.green = (g * 65535) / (numColors - 1);
		xcolor.blue = (b * 65535) / (numColors - 1);
		if (XAllocColor(display, colormap, &xcolor))
		  colors[n++] = xcolor.pixel;
		else
		  ok = gFalse;
	      }
	    }
	  }
	}
	if (ok)
	  break;
	XFreeColors(display, colormap, &colors[1], n-1, 0);
      }
      if (!ok) {
	numColors = 1;
	colors[0] = BlackPixel(display, screenNum);
	colors[1] = WhitePixel(display, screenNum);
      }
    }
  }

  // allocate GCs
  gcValues.foreground = BlackPixel(display, screenNum);
  gcValues.background = WhitePixel(display, screenNum);
  gcValues.line_width = 0;
  gcValues.line_style = LineSolid;
  strokeGC = XCreateGC(display, pixmap,
		       GCForeground | GCBackground | GCLineWidth | GCLineStyle,
                       &gcValues);
  fillGC = XCreateGC(display, pixmap,
		     GCForeground | GCBackground | GCLineWidth | GCLineStyle,
		     &gcValues);
  gcValues.foreground = paperColor;
  paperGC = XCreateGC(display, pixmap,
		      GCForeground | GCBackground | GCLineWidth | GCLineStyle,
		      &gcValues);

  // no clip region yet
  clipRegion = NULL;

  // get user font map
  for (n = 0; devFontMap[n].pdfFont; ++n) ;
  userFontMap = (FontMapEntry *)gmalloc((n+1) * sizeof(FontMapEntry));
  for (i = 0; i < n; ++i) {
    userFontMap[i].pdfFont = devFontMap[i].pdfFont;
    userFontMap[i].xFont = devFontMap[i].devFont;
    m = strlen(userFontMap[i].xFont);
    if (m >= 10 && !strcmp(userFontMap[i].xFont + m - 10, "-iso8859-2"))
      userFontMap[i].encoding = &isoLatin2Encoding;
    else if (m >= 13 && !strcmp(userFontMap[i].xFont + m - 13,
				"-fontspecific"))
      userFontMap[i].encoding = NULL;
    else
      userFontMap[i].encoding = &isoLatin1Encoding;
  }
  userFontMap[n].pdfFont = NULL;

  // set up the font cache and fonts
  gfxFont = NULL;
  font = NULL;
  fontCache = new XOutputFontCache(display);

  // empty state stack
  save = NULL;

  // create text object
  text = new TextPage(gFalse);
}

XOutputDev::~XOutputDev() {
  gfree(userFontMap);
  delete fontCache;
  XFreeGC(display, strokeGC);
  XFreeGC(display, fillGC);
  XFreeGC(display, paperGC);
  if (clipRegion)
    XDestroyRegion(clipRegion);
  delete text;
}

void XOutputDev::startPage(int pageNum, GfxState *state) {
  XOutputState *s;
  XGCValues gcValues;
  XRectangle rect;

  // clear state stack
  while (save) {
    s = save;
    save = save->next;
    XFreeGC(display, s->strokeGC);
    XFreeGC(display, s->fillGC);
    XDestroyRegion(s->clipRegion);
    delete s;
  }
  save = NULL;

  // default line flatness
  flatness = 0;

  // reset GCs
  gcValues.foreground = BlackPixel(display, screenNum);
  gcValues.background = WhitePixel(display, screenNum);
  gcValues.line_width = 0;
  gcValues.line_style = LineSolid;
  XChangeGC(display, strokeGC,
	    GCForeground | GCBackground | GCLineWidth | GCLineStyle,
	    &gcValues);
  XChangeGC(display, fillGC,
	    GCForeground | GCBackground | GCLineWidth | GCLineStyle,
	    &gcValues);

  // clear clipping region
  if (clipRegion)
    XDestroyRegion(clipRegion);
  clipRegion = XCreateRegion();
  rect.x = rect.y = 0;
  rect.width = pixmapW;
  rect.height = pixmapH;
  XUnionRectWithRegion(&rect, clipRegion, clipRegion);
  XSetRegion(display, strokeGC, clipRegion);
  XSetRegion(display, fillGC, clipRegion);

  // clear font
  gfxFont = NULL;
  font = NULL;

  // clear window
  XFillRectangle(display, pixmap, paperGC, 0, 0, pixmapW, pixmapH);

  // clear text object
  text->clear();
}

void XOutputDev::endPage() {
  text->coalesce();
}

void XOutputDev::drawLinkBorder(double x1, double y1, double x2, double y2,
				double w) {
  GfxColor color;
  XPoint points[5];
  int x, y;

  color.setRGB(0, 0, 1);
  XSetForeground(display, strokeGC, findColor(&color));
  XSetLineAttributes(display, strokeGC, xoutRound(w),
		     LineSolid, CapRound, JoinRound);
  cvtUserToDev(x1, y1, &x, &y);
  points[0].x = points[4].x = x;
  points[0].y = points[4].y = y;
  cvtUserToDev(x2, y1, &x, &y);
  points[1].x = x;
  points[1].y = y;
  cvtUserToDev(x2, y2, &x, &y);
  points[2].x = x;
  points[2].y = y;
  cvtUserToDev(x1, y2, &x, &y);
  points[3].x = x;
  points[3].y = y;
  XDrawLines(display, pixmap, strokeGC, points, 5, CoordModeOrigin);
}

void XOutputDev::saveState(GfxState *state) {
  XOutputState *s;
  XGCValues values;

  // save current state
  s = new XOutputState;
  s->strokeGC = strokeGC;
  s->fillGC = fillGC;
  s->clipRegion = clipRegion;

  // push onto state stack
  s->next = save;
  save = s;

  // create a new current state by copying
  strokeGC = XCreateGC(display, pixmap, 0, &values);
  XCopyGC(display, s->strokeGC, 0xffffffff, strokeGC);
  fillGC = XCreateGC(display, pixmap, 0, &values);
  XCopyGC(display, s->fillGC, 0xffffffff, fillGC);
  clipRegion = XCreateRegion();
  XUnionRegion(s->clipRegion, clipRegion, clipRegion);
  XSetRegion(display, strokeGC, clipRegion);
  XSetRegion(display, fillGC, clipRegion);
}

void XOutputDev::restoreState(GfxState *state) {
  XOutputState *s;

  if (save) {
    // kill current state
    XFreeGC(display, strokeGC);
    XFreeGC(display, fillGC);
    XDestroyRegion(clipRegion);

    // restore state
    flatness = state->getFlatness();
    strokeGC = save->strokeGC;
    fillGC = save->fillGC;
    clipRegion = save->clipRegion;
    XSetRegion(display, strokeGC, clipRegion);
    XSetRegion(display, fillGC, clipRegion);

    // pop state stack
    s = save;
    save = save->next;
    delete s;
  }
}

void XOutputDev::updateAll(GfxState *state) {
  updateLineAttrs(state, gTrue);
  updateFlatness(state);
  updateMiterLimit(state);
  updateFillColor(state);
  updateStrokeColor(state);
  updateFont(state);
}

void XOutputDev::updateCTM(GfxState *state, double m11, double m12,
			   double m21, double m22, double m31, double m32) {
  updateLineAttrs(state, gTrue);
}

void XOutputDev::updateLineDash(GfxState *state) {
  updateLineAttrs(state, gTrue);
}

void XOutputDev::updateFlatness(GfxState *state) {
  flatness = state->getFlatness();
}

void XOutputDev::updateLineJoin(GfxState *state) {
  updateLineAttrs(state, gFalse);
}

void XOutputDev::updateLineCap(GfxState *state) {
  updateLineAttrs(state, gFalse);
}

// unimplemented
void XOutputDev::updateMiterLimit(GfxState *state) {
}

void XOutputDev::updateLineWidth(GfxState *state) {
  updateLineAttrs(state, gFalse);
}

void XOutputDev::updateLineAttrs(GfxState *state, GBool updateDash) {
  double width;
  int cap, join;
  double *dashPattern;
  int dashLength;
  double dashStart;
  char dashList[20];
  int i;

  width = state->getTransformedLineWidth();
  switch (state->getLineCap()) {
  case 0: cap = CapButt; break;
  case 1: cap = CapRound; break;
  case 2: cap = CapProjecting; break;
  default:
    error(-1, "Bad line cap style (%d)", state->getLineCap());
    cap = CapButt;
    break;
  }
  switch (state->getLineJoin()) {
  case 0: join = JoinMiter; break;
  case 1: join = JoinRound; break;
  case 2: join = JoinBevel; break;
  default:
    error(-1, "Bad line join style (%d)", state->getLineJoin());
    join = JoinMiter;
    break;
  }
  state->getLineDash(&dashPattern, &dashLength, &dashStart);
  XSetLineAttributes(display, strokeGC, xoutRound(width),
		     dashLength > 0 ? LineOnOffDash : LineSolid,
		     cap, join);
  if (updateDash && dashLength > 0) {
    if (dashLength > 20)
      dashLength = 20;
    for (i = 0; i < dashLength; ++i) {
      dashList[i] = xoutRound(state->transformWidth(dashPattern[i]));
      if (dashList[i] == 0)
	dashList[i] = 1;
    }
    XSetDashes(display, strokeGC, xoutRound(dashStart), dashList, dashLength);
  }
}

void XOutputDev::updateFillColor(GfxState *state) {
  XSetForeground(display, fillGC, findColor(state->getFillColor()));
}

void XOutputDev::updateStrokeColor(GfxState *state) {
  XSetForeground(display, strokeGC, findColor(state->getStrokeColor()));
}

void XOutputDev::updateFont(GfxState *state) {
  double m11, m12, m21, m22;

  if (!(gfxFont = state->getFont())) {
    font = NULL;
    return;
  }
  state->getFontTransMat(&m11, &m12, &m21, &m22);
  m11 *= state->getHorizScaling();
  m21 *= state->getHorizScaling();
  font = fontCache->getFont(gfxFont, m11, m12, m21, m22);
  if (font) {
    XSetFont(display, fillGC, font->getXFont()->fid);
    XSetFont(display, strokeGC, font->getXFont()->fid);
  }
}

void XOutputDev::stroke(GfxState *state) {
  XPoint *points;
  int *lengths;
  int n, size, numPoints, i, j;

  // transform points
  n = convertPath(state, &points, &size, &numPoints, &lengths, gFalse);

  // draw each subpath
  j = 0;
  for (i = 0; i < n; ++i) {
    XDrawLines(display, pixmap, strokeGC, points + j, lengths[i],
	       CoordModeOrigin);
    j += lengths[i];
  }

  // free points and lengths arrays
  if (points != tmpPoints)
    gfree(points);
  if (lengths != tmpLengths)
    gfree(lengths);
}

void XOutputDev::fill(GfxState *state) {
  doFill(state, WindingRule);
}

void XOutputDev::eoFill(GfxState *state) {
  doFill(state, EvenOddRule);
}

//
//  X doesn't color the pixels on the right-most and bottom-most
//  borders of a polygon.  This means that one-pixel-thick polygons
//  are not colored at all.  I think this is supposed to be a
//  feature, but I can't figure out why.  So after it fills a
//  polygon, it also draws lines around the border.  This is done
//  only for single-component polygons, since it's not very
//  compatible with the compound polygon kludge (see convertPath()).
//
void XOutputDev::doFill(GfxState *state, int rule) {
  XPoint *points;
  int *lengths;
  int n, size, numPoints, i, j;

  // set fill rule
  XSetFillRule(display, fillGC, rule);

  // transform points, build separate polygons
  n = convertPath(state, &points, &size, &numPoints, &lengths, gTrue);

  // fill them
  j = 0;
  for (i = 0; i < n; ++i) {
    XFillPolygon(display, pixmap, fillGC, points + j, lengths[i],
		 Complex, CoordModeOrigin);
    if (state->getPath()->getNumSubpaths() == 1) {
      XDrawLines(display, pixmap, fillGC, points + j, lengths[i],
		 CoordModeOrigin);
    }
    j += lengths[i] + 1;
  }

  // free points and lengths arrays
  if (points != tmpPoints)
    gfree(points);
  if (lengths != tmpLengths)
    gfree(lengths);
}

void XOutputDev::clip(GfxState *state) {
  doClip(state, WindingRule);
}

void XOutputDev::eoClip(GfxState *state) {
  doClip(state, EvenOddRule);
}

void XOutputDev::doClip(GfxState *state, int rule) {
  Region region, region2;
  XPoint *points;
  int *lengths;
  int n, size, numPoints, i, j;

  // transform points, build separate polygons
  n = convertPath(state, &points, &size, &numPoints, &lengths, gTrue);

  // construct union of subpath regions
  region = XPolygonRegion(points, lengths[0], rule);
  j = lengths[0] + 1;
  for (i = 1; i < n; ++i) {
    region2 = XPolygonRegion(points + j, lengths[i], rule);
    XUnionRegion(region2, region, region);
    XDestroyRegion(region2);
    j += lengths[i] + 1;
  }

  // intersect region with clipping region
  XIntersectRegion(region, clipRegion, clipRegion);
  XDestroyRegion(region);
  XSetRegion(display, strokeGC, clipRegion);
  XSetRegion(display, fillGC, clipRegion);

  // free points and lengths arrays
  if (points != tmpPoints)
    gfree(points);
  if (lengths != tmpLengths)
    gfree(lengths);
}

//
// Transform points in the path and convert curves to line segments.
// Builds a set of subpaths and returns the number of subpaths.
// If <fillHack> is set, close any unclosed subpaths and activate a
// kludge for polygon fills:  First, it divides up the subpaths into
// non-overlapping polygons by simply comparing bounding rectangles.
// Then it connects subaths within a single compound polygon to a single
// point so that X can fill the polygon (sort of).
//
int XOutputDev::convertPath(GfxState *state, XPoint **points, int *size,
			    int *numPoints, int **lengths, GBool fillHack) {
  GfxPath *path;
  BoundingRect *rects;
  BoundingRect rect;
  int n, i, ii, j, k, k0;

  // get path and number of subpaths
  path = state->getPath();
  n = path->getNumSubpaths();

  // allocate lengths array
  if (n < numTmpSubpaths)
    *lengths = tmpLengths;
  else
    *lengths = (int *)gmalloc(n * sizeof(int));

  // allocate bounding rectangles array
  if (fillHack) {
    if (n < numTmpSubpaths)
      rects = tmpRects;
    else
      rects = (BoundingRect *)gmalloc(n * sizeof(BoundingRect));
  } else {
    rects = NULL;
  }

  // do each subpath
  *points = tmpPoints;
  *size = numTmpPoints;
  *numPoints = 0;
  for (i = 0; i < n; ++i) {

    // transform the points
    j = *numPoints;
    convertSubpath(state, path->getSubpath(i), points, size, numPoints);

    // construct bounding rectangle
    if (fillHack) {
      rects[i].xMin = rects[i].xMax = (*points)[j].x;
      rects[i].yMin = rects[i].yMax = (*points)[j].y;
      for (k = j + 1; k < *numPoints; ++k) {
	if ((*points)[k].x < rects[i].xMin)
	  rects[i].xMin = (*points)[k].x;
	else if ((*points)[k].x > rects[i].xMax)
	  rects[i].xMax = (*points)[k].x;
	if ((*points)[k].y < rects[i].yMin)
	  rects[i].yMin = (*points)[k].y;
	else if ((*points)[k].y > rects[i].yMax)
	  rects[i].yMax = (*points)[k].y;
      }
    }

    // close subpath if necessary
    if (fillHack && ((*points)[*numPoints-1].x != (*points)[j].x ||
		   (*points)[*numPoints-1].y != (*points)[j].y)) {
      addPoint(points, size, numPoints, (*points)[j].x, (*points)[j].y);
    }

    // length of this subpath
    (*lengths)[i] = *numPoints - j;

    // leave an extra point for compound fill hack
    if (fillHack)
      addPoint(points, size, numPoints, 0, 0);
  }

  // combine compound polygons
  if (fillHack) {
    i = j = k = 0;
    while (i < n) {

      // start with subpath i
      rect = rects[i];
      (*lengths)[j] = (*lengths)[i];
      k0 = k;
      (*points)[k + (*lengths)[i]] = (*points)[k0];
      k += (*lengths)[i] + 1;
      ++i;

      // combine overlapping polygons
      do {

	// look for the first subsequent subpath, if any, which overlaps
	for (ii = i; ii < n; ++ii) {
	  if (((rects[ii].xMin > rect.xMin && rects[ii].xMin < rect.xMax) ||
	       (rects[ii].xMax > rect.xMin && rects[ii].xMax < rect.xMax) ||
	       (rects[ii].xMin < rect.xMin && rects[ii].xMax > rect.xMax)) &&
	      ((rects[ii].yMin > rect.yMin && rects[ii].yMin < rect.yMax) ||
	       (rects[ii].yMax > rect.yMin && rects[ii].yMax < rect.yMax) ||
	       (rects[ii].yMin < rect.yMin && rects[ii].yMax > rect.yMax)))
	    break;
	}

	// if there is an overlap, combine the polygons
	if (ii < n) {
	  for (; i <= ii; ++i) {
	    if (rects[i].xMin < rect.xMin)
	      rect.xMin = rects[j].xMin;
	    if (rects[i].xMax > rect.xMax)
	      rect.xMax = rects[j].xMax;
	    if (rects[i].yMin < rect.yMin)
	      rect.yMin = rects[j].yMin;
	    if (rects[i].yMax > rect.yMax)
	      rect.yMax = rects[j].yMax;
	    (*lengths)[j] += (*lengths)[i] + 1;
	    (*points)[k + (*lengths)[i]] = (*points)[k0];
	    k += (*lengths)[i] + 1;
	  }
	}
      } while (ii < n && i < n);

      ++j;
    }

    // free bounding rectangles
    if (rects != tmpRects)
      gfree(rects);

    n = j;
  }

  return n;
}

//
// Transform points in a single subpath and convert curves to line
// segments.
//
void XOutputDev::convertSubpath(GfxState *state, GfxSubpath *subpath,
				XPoint **points, int *size, int *n) {
  double x0, y0, x1, y1, x2, y2, x3, y3;
  int m, i;

  m = subpath->getNumPoints();
  i = 0;
  while (i < m) {
    if (i >= 1 && subpath->getCurve(i)) {
      state->transform(subpath->getX(i-1), subpath->getY(i-1), &x0, &y0);
      state->transform(subpath->getX(i), subpath->getY(i), &x1, &y1);
      state->transform(subpath->getX(i+1), subpath->getY(i+1), &x2, &y2);
      state->transform(subpath->getX(i+2), subpath->getY(i+2), &x3, &y3);
      doCurve(points, size, n, x0, y0, x1, y1, x2, y2, x3, y3);
      i += 3;
    } else {
      state->transform(subpath->getX(i), subpath->getY(i), &x1, &y1);
      addPoint(points, size, n, xoutRound(x1), xoutRound(y1));
      ++i;
    }
  }
}

//
// Subdivide a Bezier curve.  This uses floating point to avoid
// propagating rounding errors.  (The curves look noticeably more
// jagged with integer arithmetic.)
//
void XOutputDev::doCurve(XPoint **points, int *size, int *n,
			 double x0, double y0, double x1, double y1,
			 double x2, double y2, double x3, double y3) {
  double x[(1<<maxCurveSplits)+1][3];
  double y[(1<<maxCurveSplits)+1][3];
  int next[1<<maxCurveSplits];
  int p1, p2, p3;
  double xx1, yy1, xx2, yy2;
  double dx, dy, mx, my, d1, d2;
  double xl0, yl0, xl1, yl1, xl2, yl2;
  double xr0, yr0, xr1, yr1, xr2, yr2, xr3, yr3;
  double xh, yh;
  double flat;

  flat = (double)(flatness * flatness);
  if (flat < 1)
    flat = 1;

  // initial segment
  p1 = 0;
  p2 = 1<<maxCurveSplits;
  x[p1][0] = x0;  y[p1][0] = y0;
  x[p1][1] = x1;  y[p1][1] = y1;
  x[p1][2] = x2;  y[p1][2] = y2;
  x[p2][0] = x3;  y[p2][0] = y3;
  next[p1] = p2;

  while (p1 < (1<<maxCurveSplits)) {

    // get next segment
    xl0 = x[p1][0];  yl0 = y[p1][0];
    xx1 = x[p1][1];  yy1 = y[p1][1];
    xx2 = x[p1][2];  yy2 = y[p1][2];
    p2 = next[p1];
    xr3 = x[p2][0];  yr3 = y[p2][0];

    // compute distances from control points to midpoint of the
    // straight line (this is a bit of a hack, but it's much faster
    // than computing the actual distances to the line)
    mx = (xl0 + xr3) * 0.5;
    my = (yl0 + yr3) * 0.5;
    dx = xx1 - mx;
    dy = yy1 - my;
    d1 = dx*dx + dy*dy;
    dx = xx2 - mx;
    dy = yy2 - my;
    d2 = dx*dx + dy*dy;

    // if curve is flat enough, or no more divisions allowed then
    // add the straight line segment
    if (p2 - p1 <= 1 || (d1 <= flat && d2 <= flat)) {
      addPoint(points, size, n, xoutRound(xr3), xoutRound(yr3));
      p1 = p2;

    // otherwise, subdivide the curve
    } else {
      xl1 = (xl0 + xx1) * 0.5;
      yl1 = (yl0 + yy1) * 0.5;
      xh = (xx1 + xx2) * 0.5;
      yh = (yy1 + yy2) * 0.5;
      xl2 = (xl1 + xh) * 0.5;
      yl2 = (yl1 + yh) * 0.5;
      xr2 = (xx2 + xr3) * 0.5;
      yr2 = (yy2 + yr3) * 0.5;
      xr1 = (xh + xr2) * 0.5;
      yr1 = (yh + yr2) * 0.5;
      xr0 = (xl2 + xr1) * 0.5;
      yr0 = (yl2 + yr1) * 0.5;

      // add the new subdivision points
      p3 = (p1 + p2) / 2;
      x[p1][1] = xl1;  y[p1][1] = yl1;
      x[p1][2] = xl2;  y[p1][2] = yl2;
      next[p1] = p3;
      x[p3][0] = xr0;  y[p3][0] = yr0;
      x[p3][1] = xr1;  y[p3][1] = yr1;
      x[p3][2] = xr2;  y[p3][2] = yr2;
      next[p3] = p2;
    }
  }
}

//
// Add a point to the points array.  (This would use a generic resizable
// array type if C++ supported parameterized types in some reasonable
// way -- templates are a disgusting kludge.)
//
void XOutputDev::addPoint(XPoint **points, int *size, int *k, int x, int y) {
  if (*k >= *size) {
    *size += 32;
    if (*points == tmpPoints) {
      *points = (XPoint *)gmalloc(*size * sizeof(XPoint));
      memcpy(*points, tmpPoints, *k * sizeof(XPoint));
    } else {
      *points = (XPoint *)grealloc(*points, *size * sizeof(XPoint));
    }
  }
  (*points)[*k].x = x;
  (*points)[*k].y = y;
  ++(*k);
}

void XOutputDev::beginString(GfxState *state, GString *s) {
  text->beginString(state, s, font ? font->isHex() : gFalse);
}

void XOutputDev::endString(GfxState *state) {
  text->endString();
}

void XOutputDev::drawChar(GfxState *state, double x, double y,
			  double dx, double dy, Guchar c) {
  Gushort c1;
  char buf;
  char *p;
  int n, i;
  double x1, y1;
  double tx;

  text->addChar(state, x, y, dx, dy, c);

  if (!font)
    return;

  // check for invisible text -- this is used by Acrobat Capture
  if ((state->getRender() & 3) == 3)
    return;

  state->transform(x, y, &x1, &y1);
  c1 = font->mapChar(c);
  if (c1 <= lastRegularChar) {
    buf = (char)c1;
    XDrawString(display, pixmap,
		(state->getRender() & 1) ? strokeGC : fillGC,
		xoutRound(x1), xoutRound(y1), &buf, 1);
  } else if (c1 <= lastSubstChar) {
    buf = (char)substChars[c1 - firstSubstChar];
    XDrawString(display, pixmap,
		(state->getRender() & 1) ? strokeGC : fillGC,
		xoutRound(x1), xoutRound(y1), &buf, 1);
  } else if (c1 <= lastConstrChar) {
    //~ need to deal with rotated text here
    switch (c1 - firstConstrChar) {
    case 0: // bullet
      tx = 0.25 * state->getTransformedFontSize() * 
           gfxFont->getWidth(c);
      XFillRectangle(display, pixmap,
		     (state->getRender() & 1) ? strokeGC : fillGC,
		     xoutRound(x1 + tx),
		     xoutRound(y1 - 0.4 * font->getXFont()->ascent - tx),
		     xoutRound(2 * tx), xoutRound(2 * tx));
      break;
    case 1: // trademark
//~ this should use a smaller font
//      tx = state->getTransformedFontSize() *
//           (gfxFont->getWidth(c) -
//            gfxFont->getWidth(font->revCharMap('M')));
      tx = 0.9 * state->getTransformedFontSize() *
           gfxFont->getWidth(font->revMapChar('T'));
      y1 -= 0.33 * (double)font->getXFont()->ascent;
      buf = 'T';
      XDrawString(display, pixmap,
		  (state->getRender() & 1) ? strokeGC : fillGC,
		  xoutRound(x1), xoutRound(y1), &buf, 1);
      x1 += tx;
      buf = 'M';
      XDrawString(display, pixmap,
		  (state->getRender() & 1) ? strokeGC : fillGC,
		  xoutRound(x1), xoutRound(y1), &buf, 1);
      break;
    }
  } else if (c1 <= lastMultiChar) {
    p = multiChars[c1 - firstMultiChar];
    n = strlen(p);
    tx = gfxFont->getWidth(c);
    tx -= gfxFont->getWidth(font->revMapChar(p[n-1]));
    tx = tx * state->getTransformedFontSize() / (double)(n - 1);
    for (i = 0; i < n; ++i) {
      XDrawString(display, pixmap,
		  (state->getRender() & 1) ? strokeGC : fillGC,
		  xoutRound(x1), xoutRound(y1), p + i, 1);
      x1 += tx;
    }
  }
}

void XOutputDev::drawChar16(GfxState *state, double x, double y,
			    double dx, double dy, int c) {
  int c1;
  XChar2b c2[4];
  double x1, y1;
#if JAPANESE_SUPPORT
  int t1, t2;
  double x2;
  char *p;
  int n, i;
#endif

  if (!font)
    return;

  // check for invisible text -- this is used by Acrobat Capture
  if ((state->getRender() & 3) == 3)
    return;

  state->transform(x, y, &x1, &y1);

  c1 = 0;
  switch (gfxFont->getCharSet16()) {

  // convert Adobe-Japan1-2 to JIS X 0208-1983
  case font16AdobeJapan12:
#if JAPANESE_SUPPORT
    if (c <= 96) {
      c1 = japan12Map[c];
    } else if (c <= 632) {
      if (c <= 230)
	c1 = 0;
      else if (c <= 324)
	c1 = japan12Map[c - 230];
      else if (c <= 421)
	c1 = japan12KanaMap1[c - 325];
      else if (c <= 500)
	c1 = 0;
      else if (c <= 598)
	c1 = japan12KanaMap2[c - 501];
      else
	c1 = 0;
    } else if (c <= 1124) {
      if (c <= 779) {
	if (c <= 726)
	  c1 = 0x2121 + (c - 633);
	else if (c <= 740)
	  c1 = 0x2221 + (c - 727);
	else if (c <= 748)
	  c1 = 0x223a + (c - 741);
	else if (c <= 755)
	  c1 = 0x224a + (c - 749);
	else if (c <= 770)
	  c1 = 0x225c + (c - 756);
	else if (c <= 778)
	  c1 = 0x2272 + (c - 771);
	else
	  c1 = 0x227e;
      } else if (c <= 841) {
	if (c <= 789)
	  c1 = 0x2330 + (c - 780);
	else if (c <= 815)
	  c1 = 0x2341 + (c - 790);
	else
	  c1 = 0x2361 + (c - 816);
      } else if (c <= 1010) {
	if (c <= 924)
	  c1 = 0x2421 + (c - 842);
	else
	  c1 = 0x2521 + (c - 925);
      } else {
	if (c <= 1034)
	  c1 = 0x2621 + (c - 1011);
	else if (c <= 1058)
	  c1 = 0x2641 + (c - 1035);
	else if (c <= 1091)
	  c1 = 0x2721 + (c - 1059);
	else
	  c1 = 0x2751 + (c - 1092);
      }
    } else if (c <= 4089) {
      t1 = (c - 1125) / 94;
      t2 = (c - 1125) % 94;
      c1 = 0x3021 + (t1 << 8) + t2;
    } else if (c <= 7477) {
      t1 = (c - 4090) / 94;
      t2 = (c - 4090) % 94;
      c1 = 0x5021 + (t1 << 8) + t2;
    } else if (c <= 7554) {
      c1 = 0;
    } else if (c <= 7563) {	// circled Arabic numbers 1..9
      c1 = 0x2331 + (c - 7555);
      c2[0].byte1 = c1 >> 8;
      c2[0].byte2 = c1 & 0xff;
      XDrawString16(display, pixmap,
		    (state->getRender() & 1) ? strokeGC : fillGC,
		    xoutRound(x1), xoutRound(y1), c2, 1);
      c1 = 0x227e;
      c2[0].byte1 = c1 >> 8;
      c2[0].byte2 = c1 & 0xff;
      XDrawString16(display, pixmap,
		    (state->getRender() & 1) ? strokeGC : fillGC,
		    xoutRound(x1), xoutRound(y1), c2, 1);
      c1 = -1;
    } else if (c <= 7574) {	// circled Arabic numbers 10..20
      n = c - 7564 + 10;
      x2 = x1;
      for (i = 0; i < 2; ++i) {
	c1 = 0x2330 + (i == 0 ? (n / 10) : (n % 10));
	c2[0].byte1 = c1 >> 8;
	c2[0].byte2 = c1 & 0xff;
	XDrawString16(display, pixmap,
		      (state->getRender() & 1) ? strokeGC : fillGC,
		      xoutRound(x2), xoutRound(y1), c2, 1);
	x2 += 0.5 * state->getTransformedFontSize();
      }
      c1 = 0x227e;
      c2[0].byte1 = c1 >> 8;
      c2[0].byte2 = c1 & 0xff;
      XDrawString16(display, pixmap,
		    (state->getRender() & 1) ? strokeGC : fillGC,
		    xoutRound(x1), xoutRound(y1), c2, 1);
      c1 = -1;
    } else if (c <= 7584) {	// Roman numbers I..X
      p = japan12Roman[c - 7575];
      n = strlen(p);
      for (; *p; ++p) {
	if (*p == 'I')
	  c1 = 0x2349;
	else if (*p == 'V')
	  c1 = 0x2356;
	else // 'X'
	  c1 = 0x2358;
	c2[0].byte1 = c1 >> 8;
	c2[0].byte2 = c1 & 0xff;
	XDrawString16(display, pixmap,
		      (state->getRender() & 1) ? strokeGC : fillGC,
		      xoutRound(x1), xoutRound(y1), c2, 1);
	if (*p == 'I')
	  x1 += 0.2 * state->getTransformedFontSize();
	else
	  x1 += 0.5 * state->getTransformedFontSize();
      }
      c1 = -1;
    } else if (c <= 7632) {
      if (c <= 7600) {
	c1 = 0;
      } else if (c <= 7606) {
	p = japan12Abbrev1[c - 7601];
	n = strlen(p);
	for (; *p; ++p) {
	  c1 = 0x2300 + *p;
	  c2[0].byte1 = c1 >> 8;
	  c2[0].byte2 = c1 & 0xff;
	  XDrawString16(display, pixmap,
			(state->getRender() & 1) ? strokeGC : fillGC,
			xoutRound(x1), xoutRound(y1), c2, 1);
	  x1 += 0.5 * state->getTransformedFontSize();
	}
	c1 = -1;
      } else {
	c1 = 0;
      }
    } else {
      c1 = 0;
    }
#if 0 //~
    if (c1 == 0)
      error(-1, "Unsupported Adobe-Japan1-2 character: %d", c);
#endif
#endif // JAPANESE_SUPPORT
    break;
  }

  if (c1 > 0) {
    c2[0].byte1 = c1 >> 8;
    c2[0].byte2 = c1 & 0xff;
    XDrawString16(display, pixmap,
		  (state->getRender() & 1) ? strokeGC : fillGC,
		  xoutRound(x1), xoutRound(y1), c2, 1);
  }
}

void XOutputDev::drawImageMask(GfxState *state, Stream *str,
			       int width, int height, GBool invert,
			       GBool inlineImg) {
  XImage *image;
  int x0, y0;			// top left corner of image
  int w0, h0, w1, h1;		// size of image
  int x2, y2;
  int w2, h2;
  double xt, yt, wt, ht;
  GBool rotate, xFlip, yFlip;
  int x, y;
  int ix, iy;
  int px1, px2, qx, dx;
  int py1, py2, qy, dy;
  Guchar pixBuf;
  Gulong color;
  int i, j;

  // get image position and size
  state->transform(0, 0, &xt, &yt);
  state->transformDelta(1, 1, &wt, &ht);
  if (wt > 0) {
    x0 = xoutRound(xt);
    w0 = xoutRound(wt);
  } else {
    x0 = xoutRound(xt + wt);
    w0 = xoutRound(-wt);
  }
  if (ht > 0) {
    y0 = xoutRound(yt);
    h0 = xoutRound(ht);
  } else {
    y0 = xoutRound(yt + ht);
    h0 = xoutRound(-ht);
  }
  state->transformDelta(1, 0, &xt, &yt);
  rotate = fabs(xt) < fabs(yt);
  if (rotate) {
    w1 = h0;
    h1 = w0;
    xFlip = ht < 0;
    yFlip = wt > 0;
  } else {
    w1 = w0;
    h1 = h0;
    xFlip = wt < 0;
    yFlip = ht > 0;
  }

  // set up
  color = findColor(state->getFillColor());

  // check for tiny (zero width or height) images
  if (w0 == 0 || h0 == 0) {
    j = height * ((width + 7) / 8);
    str->reset();
    for (i = 0; i < j; ++i)
      str->getChar();
    return;
  }

  // Bresenham parameters
  px1 = w1 / width;
  px2 = w1 - px1 * width;
  py1 = h1 / height;
  py2 = h1 - py1 * height;

  // allocate XImage
  image = XCreateImage(display, DefaultVisual(display, screenNum),
		       depth, ZPixmap, 0, NULL, w0, h0, 8, 0);
  image->data = (char *)gmalloc(h0 * image->bytes_per_line);
  if (x0 + w0 > pixmapW)
    w2 = pixmapW - x0;
  else
    w2 = w0;
  if (x0 < 0) {
    x2 = -x0;
    w2 += x0;
    x0 = 0;
  } else {
    x2 = 0;
  }
  if (y0 + h0 > pixmapH)
    h2 = pixmapH - y0;
  else
    h2 = h0;
  if (y0 < 0) {
    y2 = -y0;
    h2 += y0;
    y0 = 0;
  } else {
    y2 = 0;
  }
  XGetSubImage(display, pixmap, x0, y0, w2, h2, (1 << depth) - 1, ZPixmap,
	       image, x2, y2);

  // initialize the image stream
  str->resetImage(width, 1, 1);

  // first line (column)
  y = yFlip ? h1 - 1 : 0;
  qy = 0;

  // read image
  for (i = 0; i < height; ++i) {

    // vertical (horizontal) Bresenham
    dy = py1;
    if ((qy += py2) >= height) {
      ++dy;
      qy -= height;
    }

    // drop a line (column)
    if (dy == 0) {
      str->skipImageLine();

    } else {

      // first column (line)
      x = xFlip ? w1 - 1 : 0;
      qx = 0;

      // for each column (line)...
      for (j = 0; j < width; ++j) {

	// horizontal (vertical) Bresenham
	dx = px1;
	if ((qx += px2) >= width) {
	  ++dx;
	  qx -= width;
	}

	// get image pixel
	str->getImagePixel(&pixBuf);
	if (invert)
	  pixBuf ^= 1;

	// draw image pixel
	if (dx > 0 && pixBuf == 0) {
	  if (dx == 1 && dy == 1) {
	    if (rotate)
	      XPutPixel(image, y, x, color);
	    else
	      XPutPixel(image, x, y, color);
	  } else {
	    for (ix = 0; ix < dx; ++ix) {
	      for (iy = 0; iy < dy; ++iy) {
		if (rotate)
		  XPutPixel(image, yFlip ? y - iy : y + iy,
			    xFlip ? x - ix : x + ix, color);
		else
		  XPutPixel(image, xFlip ? x - ix : x + ix,
			    yFlip ? y - iy : y + iy, color);
	      }
	    }
	  }
	}

	// next column (line)
	if (xFlip)
	  x -= dx;
	else
	  x += dx;
      }
    }

    // next line (column)
    if (yFlip)
      y -= dy;
    else
      y += dy;
  }

  // blit the image into the pixmap
  XPutImage(display, pixmap, fillGC, image, x2, y2, x0, y0, w2, h2);

  // free memory
  gfree(image->data);
  image->data = NULL;
  XDestroyImage(image);
}

inline Gulong XOutputDev::findColor(RGBColor *x, RGBColor *err) {
  double gray;
  int r, g, b;
  Gulong pixel;

  if (trueColor) {
    r = xoutRound(x->r * rMul);
    g = xoutRound(x->g * gMul);
    b = xoutRound(x->b * bMul);
    pixel = ((Gulong)r << rShift) +
            ((Gulong)g << gShift) +
            ((Gulong)b << bShift);
    err->r = x->r - (double)r / rMul;
    err->g = x->g - (double)g / gMul;
    err->b = x->b - (double)b / bMul;
  } else if (numColors == 1) {
    gray = 0.299 * x->r + 0.587 * x->g + 0.114 * x->b;
    if (gray < 0.5) {
      pixel = colors[0];
      err->r = x->r;
      err->g = x->g;
      err->b = x->b;
    } else {
      pixel = colors[1];
      err->r = x->r - 1;
      err->g = x->g - 1;
      err->b = x->b - 1;
    }
  } else {
    r = xoutRound(x->r * (numColors - 1));
    g = xoutRound(x->g * (numColors - 1));
    b = xoutRound(x->b * (numColors - 1));
    pixel = colors[(r * numColors + g) * numColors + b];
    err->r = x->r - (double)r / (numColors - 1);
    err->g = x->g - (double)g / (numColors - 1); 
    err->b = x->b - (double)b / (numColors - 1);
  }
  return pixel;
}

void XOutputDev::drawImage(GfxState *state, Stream *str, int width,
			   int height, GfxImageColorMap *colorMap,
			   GBool inlineImg) {
  XImage *image;
  int x0, y0;			// top left corner of image
  int w0, h0, w1, h1;		// size of image
  double xt, yt, wt, ht;
  GBool rotate, xFlip, yFlip;
  GBool dither;
  int x, y;
  int ix, iy;
  int px1, px2, qx, dx;
  int py1, py2, qy, dy;
  Guchar pixBuf[4];
  Gulong pixel;
  int nComps, nVals, nBits;
  double r1, g1, b1;
  GfxColor color;
  RGBColor color2, err;
  RGBColor *errRight, *errDown;
  int i, j;

  // get image position and size
  state->transform(0, 0, &xt, &yt);
  state->transformDelta(1, 1, &wt, &ht);
  if (wt > 0) {
    x0 = xoutRound(xt);
    w0 = xoutRound(wt);
  } else {
    x0 = xoutRound(xt + wt);
    w0 = xoutRound(-wt);
  }
  if (ht > 0) {
    y0 = xoutRound(yt);
    h0 = xoutRound(ht);
  } else {
    y0 = xoutRound(yt + ht);
    h0 = xoutRound(-ht);
  }
  state->transformDelta(1, 0, &xt, &yt);
  rotate = fabs(xt) < fabs(yt);
  if (rotate) {
    w1 = h0;
    h1 = w0;
    xFlip = ht < 0;
    yFlip = wt > 0;
  } else {
    w1 = w0;
    h1 = h0;
    xFlip = wt < 0;
    yFlip = ht > 0;
  }

  // set up
  nComps = colorMap->getNumPixelComps();
  nVals = width * nComps;
  nBits = colorMap->getBits();
  dither = nComps > 1 || nBits > 1;

  // check for tiny (zero width or height) images
  if (w0 == 0 || h0 == 0) {
    j = height * ((nVals * nBits + 7) / 8);
    str->reset();
    for (i = 0; i < j; ++i)
      str->getChar();
    return;
  }

  // Bresenham parameters
  px1 = w1 / width;
  px2 = w1 - px1 * width;
  py1 = h1 / height;
  py2 = h1 - py1 * height;

  // allocate XImage
  image = XCreateImage(display, DefaultVisual(display, screenNum),
		       depth, ZPixmap, 0, NULL, w0, h0, 8, 0);
  image->data = (char *)gmalloc(h0 * image->bytes_per_line);

  // allocate error diffusion accumulators
  if (dither) {
    errDown = (RGBColor *)gmalloc(w1 * sizeof(RGBColor));
    errRight = (RGBColor *)gmalloc((py1 + 1) * sizeof(RGBColor));
    for (j = 0; j < w1; ++j)
      errDown[j].r = errDown[j].g = errDown[j].b = 0;
  } else {
    errDown = NULL;
    errRight = NULL;
  }

  // initialize the image stream
  str->resetImage(width, nComps, nBits);

  // first line (column)
  y = yFlip ? h1 - 1 : 0;
  qy = 0;

  // read image
  for (i = 0; i < height; ++i) {

    // vertical (horizontal) Bresenham
    dy = py1;
    if ((qy += py2) >= height) {
      ++dy;
      qy -= height;
    }

    // drop a line (column)
    if (dy == 0) {
      str->skipImageLine();

    } else {

      // first column (line)
      x = xFlip ? w1 - 1 : 0;
      qx = 0;

      // clear error accumulator
      if (dither) {
	for (j = 0; j <= py1; ++j)
	  errRight[j].r = errRight[j].g = errRight[j].b = 0;
      }

      // for each column (line)...
      for (j = 0; j < width; ++j) {

	// horizontal (vertical) Bresenham
	dx = px1;
	if ((qx += px2) >= width) {
	  ++dx;
	  qx -= width;
	}

	// get image pixel
	str->getImagePixel(pixBuf);

	// draw image pixel
	if (dx > 0) {
	  colorMap->getColor(pixBuf, &color);
	  r1 = color.getR();
	  g1 = color.getG();
	  b1 = color.getB();
	  if (dither) {
	    pixel = 0;
	  } else {
	    color2.r = r1;
	    color2.g = g1;
	    color2.b = b1;
	    pixel = findColor(&color2, &err);
	  }
	  if (dx == 1 && dy == 1) {
	    if (dither) {
	      color2.r = r1 + errRight[0].r + errDown[x].r;
	      if (color2.r > 1)
		color2.r = 1;
	      else if (color2.r < 0)
		color2.r = 0;
	      color2.g = g1 + errRight[0].g + errDown[x].g;
	      if (color2.g > 1)
		color2.g = 1;
	      else if (color2.g < 0)
		color2.g = 0;
	      color2.b = b1 + errRight[0].b + errDown[x].b;
	      if (color2.b > 1)
		color2.b = 1;
	      else if (color2.b < 0)
		color2.b = 0;
	      pixel = findColor(&color2, &err);
	      errRight[0].r = errDown[x].r = err.r / 2;
	      errRight[0].g = errDown[x].g = err.g / 2;
	      errRight[0].b = errDown[x].b = err.b / 2;
	    }
	    if (rotate)
	      XPutPixel(image, y, x, pixel);
	    else
	      XPutPixel(image, x, y, pixel);
	  } else {
	    for (iy = 0; iy < dy; ++iy) {
	      for (ix = 0; ix < dx; ++ix) {
		if (dither) {
		  color2.r = r1 + errRight[iy].r +
		    errDown[xFlip ? x - ix : x + ix].r;
		  if (color2.r > 1)
		    color2.r = 1;
		  else if (color2.r < 0)
		    color2.r = 0;
		  color2.g = g1 + errRight[iy].g +
		    errDown[xFlip ? x - ix : x + ix].g;
		  if (color2.g > 1)
		    color2.g = 1;
		  else if (color2.g < 0)
		    color2.g = 0;
		  color2.b = b1 + errRight[iy].b +
		    errDown[xFlip ? x - ix : x + ix].b;
		  if (color2.b > 1)
		    color2.b = 1;
		  else if (color2.b < 0)
		    color2.b = 0;
		  pixel = findColor(&color2, &err);
		  errRight[iy].r = errDown[xFlip ? x - ix : x + ix].r =
		    err.r / 2;
		  errRight[iy].g = errDown[xFlip ? x - ix : x + ix].g =
		    err.g / 2;
		  errRight[iy].b = errDown[xFlip ? x - ix : x + ix].b =
		    err.b / 2;
		}
		if (rotate)
		  XPutPixel(image, yFlip ? y - iy : y + iy,
			    xFlip ? x - ix : x + ix, pixel);
		else
		  XPutPixel(image, xFlip ? x - ix : x + ix,
			    yFlip ? y - iy : y + iy, pixel);
	      }
	    }
	  }
	}

	// next column (line)
	if (xFlip)
	  x -= dx;
	else
	  x += dx;
      }
    }

    // next line (column)
    if (yFlip)
      y -= dy;
    else
      y += dy;
  }

  // blit the image into the pixmap
  XPutImage(display, pixmap, fillGC, image, 0, 0, x0, y0, w0, h0);

  // free memory
  gfree(image->data);
  image->data = NULL;
  XDestroyImage(image);
  gfree(errRight);
  gfree(errDown);
}

Gulong XOutputDev::findColor(GfxColor *color) {
  int r, g, b;
  double gray;
  Gulong pixel;

  if (trueColor) {
    r = xoutRound(color->getR() * rMul);
    g = xoutRound(color->getG() * gMul);
    b = xoutRound(color->getB() * bMul);
    pixel = ((Gulong)r << rShift) +
            ((Gulong)g << gShift) +
            ((Gulong)b << bShift);
  } else if (numColors == 1) {
    gray = color->getGray();
    if (gray < 0.5)
      pixel = colors[0];
    else
      pixel = colors[1];
  } else {
    r = xoutRound(color->getR() * (numColors - 1));
    g = xoutRound(color->getG() * (numColors - 1));
    b = xoutRound(color->getB() * (numColors - 1));
#if 0 //~ this makes things worse as often as better
    // even a very light color shouldn't map to white
    if (r == numColors - 1 && g == numColors - 1 && b == numColors - 1) {
      if (color->getR() < 0.95)
	--r;
      if (color->getG() < 0.95)
	--g;
      if (color->getB() < 0.95)
	--b;
    }
#endif
    pixel = colors[(r * numColors + g) * numColors + b];
  }
  return pixel;
}

GBool XOutputDev::findText(char *s, GBool top, GBool bottom,
			   int *xMin, int *yMin, int *xMax, int *yMax) {
  double xMin1, yMin1, xMax1, yMax1;
  
  xMin1 = (double)*xMin;
  yMin1 = (double)*yMin;
  xMax1 = (double)*xMax;
  yMax1 = (double)*yMax;
  if (text->findText(s, top, bottom, &xMin1, &yMin1, &xMax1, &yMax1)) {
    *xMin = xoutRound(xMin1);
    *xMax = xoutRound(xMax1);
    *yMin = xoutRound(yMin1);
    *yMax = xoutRound(yMax1);
    return gTrue;
  }
  return gFalse;
}

GString *XOutputDev::getText(int xMin, int yMin, int xMax, int yMax) {
  return text->getText((double)xMin, (double)yMin,
		       (double)xMax, (double)yMax);
}
