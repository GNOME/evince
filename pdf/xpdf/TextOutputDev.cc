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
#include "FontEncoding.h"
#include "TextOutputDev.h"

#ifdef MACOS
// needed for setting type/creator of MacOS files
#include "ICSupport.h"
#endif

#include "TextOutputFontInfo.h"

//------------------------------------------------------------------------
// Character substitutions
//------------------------------------------------------------------------

static char *generalSubstNames[] = {
  "zerooldstyle",
  "oneoldstyle",
  "twooldstyle",
  "threeoldstyle",
  "fouroldstyle",
  "fiveoldstyle",
  "sixoldstyle",
  "sevenoldstyle",
  "eightoldstyle",
  "nineoldstyle",
  "oldstylezero",
  "oldstyleone",
  "oldstyletwo",
  "oldstylethree",
  "oldstylefour",
  "oldstylefive",
  "oldstylesix",
  "oldstyleseven",
  "oldstyleeight",
  "oldstylenine"
};

static FontEncoding generalSubstEncoding(generalSubstNames,
					 sizeof(generalSubstNames) /
					   sizeof(char *));

static char *generalSubst[] = {
  "zero",
  "one",
  "two",
  "three",
  "four",
  "five",
  "six",
  "seven",
  "eight",
  "nine",
  "zero",
  "one",
  "two",
  "three",
  "four",
  "five",
  "six",
  "seven",
  "eight",
  "nine"
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
  "fi", "fl",			// ligatures
  "ff", "ffi", "ffl",		// ligatures
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

static char *isoLatin1Subst[] = {
  "L",				// Lslash
  "OE",				// OE
  "S",				// Scaron
  "Y",				// Ydieresis
  "Z",				// Zcaron
  "fi", "fl",			// ligatures
  "ff", "ffi", "ffl",		// ligatures
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

static char *isoLatin2Subst[] = {
  "fi", "fl",			// ligatures
  "ff", "ffi", "ffl",		// ligatures
  "*",				// bullet
  "...",			// ellipsis
  "-", "-",			// emdash, hyphen
  "\"", "\"",			// quotedblleft, quotedblright
  "'",				// quotesingle
  "TM"				// trademark
};

static char **isoLatin5Subst = isoLatin1Subst;

//------------------------------------------------------------------------
// 16-bit fonts
//------------------------------------------------------------------------

#if JAPANESE_SUPPORT

// CID 0 .. 96
static Gushort japan12Map[96] = {
  0x2121, 0x2121, 0x212a, 0x2149, 0x2174, 0x2170, 0x2173, 0x2175, // 00 .. 07
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

#if CHINESE_CNS_SUPPORT

static Gushort cns13Map1[99] = {
  // 0-98
  0,      0xa140, 0xa149, 0xa1a8, 0xa1ad, 0xa243, 0xa248, 0xa1ae,
  0xa1a6, 0xa15d, 0xa15e, 0xa1af, 0xa1cf, 0xa141, 0xa1df, 0xa144,
  0xa241, 0xa2af, 0xa2b0, 0xa2b1, 0xa2b2, 0xa2b3, 0xa2b4, 0xa2b5,
  0xa2b6, 0xa2b7, 0xa2b8, 0xa147, 0xa146, 0xa1d5, 0xa1d7, 0xa1d6,
  0xa148, 0xa249, 0xa2cf, 0xa2d0, 0xa2d1, 0xa2d2, 0xa2d3, 0xa2d4,
  0xa2d5, 0xa2d6, 0xa2d7, 0xa2d8, 0xa2d9, 0xa2da, 0xa2db, 0xa2dc,
  0xa2dd, 0xa2de, 0xa2df, 0xa2e0, 0xa2e1, 0xa2e2, 0xa2e3, 0xa2e4,
  0xa2e5, 0xa2e6, 0xa2e7, 0xa2e8, 0xa165, 0xa242, 0xa166, 0xa173,
  0xa15a, 0xa1a5, 0xa2e9, 0xa2ea, 0xa2eb, 0xa2ec, 0xa2ed, 0xa2ee,
  0xa2ef, 0xa2f0, 0xa2f1, 0xa2f2, 0xa2f3, 0xa2f4, 0xa2f5, 0xa2f6,
  0xa2f7, 0xa2f8, 0xa2f9, 0xa2fa, 0xa2fb, 0xa2fc, 0xa2fd, 0xa2fe,
  0xa340, 0xa341, 0xa342, 0xa343, 0xa161, 0xa159, 0xa162, 0xa1e3,
  0,      0,      0xa14b
};

static Gushort cns13Map2[95] = {
  // 13648-13742
          0xa140, 0xa149, 0xa1a8, 0xa1ad, 0xa244, 0xa248, 0xa1ae,
  0xa1a6, 0xa15d, 0xa15e, 0xa1af, 0xa1cf, 0xa141, 0xa1df, 0xa144,
  0xa241, 0xa2af, 0xa2b0, 0xa2b1, 0xa2b2, 0xa2b3, 0xa2b4, 0xa2b5,
  0xa2b6, 0xa2b7, 0xa2b8, 0xa147, 0xa146, 0xa1d5, 0xa1d7, 0xa1d6,
  0xa148, 0xa249, 0xa2cf, 0xa2d0, 0xa2d1, 0xa2d2, 0xa2d3, 0xa2d4,
  0xa2d5, 0xa2d6, 0xa2d7, 0xa2d8, 0xa2d9, 0xa2da, 0xa2db, 0xa2dc,
  0xa2dd, 0xa2de, 0xa2df, 0xa2e0, 0xa2e1, 0xa2e2, 0xa2e3, 0xa2e4,
  0xa2e5, 0xa2e6, 0xa2e7, 0xa2e8, 0xa165, 0xa242, 0xa166, 0xa173,
  0xa15a, 0xa1a5, 0xa2e9, 0xa2ea, 0xa2eb, 0xa2ec, 0xa2ed, 0xa2ee,
  0xa2ef, 0xa2f0, 0xa2f1, 0xa2f2, 0xa2f3, 0xa2f4, 0xa2f5, 0xa2f6,
  0xa2f7, 0xa2f8, 0xa2f9, 0xa2fa, 0xa2fb, 0xa2fc, 0xa2fd, 0xa2fe,
  0xa340, 0xa341, 0xa342, 0xa343, 0xa161, 0xa159, 0xa162, 0xa1c3
};

#endif

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
			 Guchar c, TextOutputCharSet charSet) {
  char *charName, *sub;
  int c1;
  int i, j, n, m;

  // get current index
  i = text->getLength();

  // append translated character(s) to string
  sub = NULL;
  n = 1;
  if ((charName = state->getFont()->getCharName(c))) {
    if ((c1 = generalSubstEncoding.getCharCode(charName)) >= 0) {
      charName = generalSubst[c1];
    }
    switch (charSet) {
    case textOutASCII7:
      c1 = ascii7Encoding.getCharCode(charName);
      break;
    case textOutLatin1:
      c1 = isoLatin1Encoding.getCharCode(charName);
      break;
    case textOutLatin2:
      c1 = isoLatin2Encoding.getCharCode(charName);
      break;
    case textOutLatin5:
      c1 = isoLatin5Encoding.getCharCode(charName);
      break;
    }
    if (c1 < 0) {
      m = strlen(charName);
      if (hexCodes && m == 3 &&
	  (charName[0] == 'B' || charName[0] == 'C' ||
	   charName[0] == 'G') &&
	  isxdigit(charName[1]) && isxdigit(charName[2])) {
	sscanf(charName+1, "%x", &c1);
      } else if (hexCodes && m == 2 &&
		 isxdigit(charName[0]) && isxdigit(charName[1])) {
	sscanf(charName, "%x", &c1);
      } else if (!hexCodes && m >= 2 && m <= 3 &&
		 isdigit(charName[0]) && isdigit(charName[1])) {
	c1 = atoi(charName);
	if (c1 >= 256) {
	  c1 = -1;
	}
      } else if (m >= 3 && m <= 5 && isdigit(charName[1])) {
	c1 = atoi(charName+1);
	if (c1 >= 256) {
	  c1 = -1;
	}
      }
      //~ this is a kludge -- is there a standard internal encoding
      //~ used by all/most Type 1 fonts?
      if (c1 == 262)		// hyphen
	c1 = 45;
      else if (c1 == 266)	// emdash
	c1 = 208;
      if (c1 >= 0) {
	charName = isoLatin1Encoding.getCharName(c1);
	if (charName) {
	  switch (charSet) {
	  case textOutASCII7:
	    c1 = ascii7Encoding.getCharCode(charName);
	    break;
	  case textOutLatin1:
	    // no translation
	    break;
	  case textOutLatin2:
	    c1 = isoLatin2Encoding.getCharCode(charName);
	    break;
	  case textOutLatin5:
	    c1 = isoLatin5Encoding.getCharCode(charName);
	    break;
	  }
	} else {
	  c1 = -1;
	}
      }
    }
    switch (charSet) {
      case textOutASCII7:
	if (c1 >= 128) {
	  sub = ascii7Subst[c1 - 128];
	  n = strlen(sub);
	}
	break;
      case textOutLatin1:
	if (c1 >= 256) {
	  sub = isoLatin1Subst[c1 - 256];
	  n = strlen(sub);
	}
	break;
      case textOutLatin2:
	if (c1 >= 256) {
	  sub = isoLatin2Subst[c1 - 256];
	  n = strlen(sub);
	}
	break;
      case textOutLatin5:
	if (c1 >= 256) {
	  sub = isoLatin5Subst[c1 - 256];
	  n = strlen(sub);
	}
	break;
    }
  } else {
    c1 = -1;
  }
  if (sub)
    text->append(sub);
  else if (c1 >= ' ')
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

void TextString::addChar16(GfxState *state, double x, double y,
			   double dx, double dy,
			   int c, GfxFontCharSet16 charSet) {
  int c1, t1, t2;
  int sub[8];
  char *p;
  int *q;
  int i, j, n;

  // get current index
  i = text->getLength();

  // convert the 16-bit character
  c1 = 0;
  sub[0] = 0;
  switch (charSet) {

  // convert Adobe-Japan1-2 to JIS X 0208-1983
  case font16AdobeJapan12:
#if JAPANESE_SUPPORT
    if (c <= 96) {
      c1 = 0x8080 + japan12Map[c];
    } else if (c <= 632) {
      if (c <= 230)
	c1 = 0;
      else if (c <= 324)
	c1 = 0x8080 + japan12Map[c - 230];
      else if (c <= 421)
	c1 = 0x8080 + japan12KanaMap1[c - 325];
      else if (c <= 500)
	c1 = 0;
      else if (c <= 598)
	c1 = 0x8080 + japan12KanaMap2[c - 501];
      else
	c1 = 0;
    } else if (c <= 1124) {
      if (c <= 779) {
	if (c <= 726)
	  c1 = 0xa1a1 + (c - 633);
	else if (c <= 740)
	  c1 = 0xa2a1 + (c - 727);
	else if (c <= 748)
	  c1 = 0xa2ba + (c - 741);
	else if (c <= 755)
	  c1 = 0xa2ca + (c - 749);
	else if (c <= 770)
	  c1 = 0xa2dc + (c - 756);
	else if (c <= 778)
	  c1 = 0xa2f2 + (c - 771);
	else
	  c1 = 0xa2fe;
      } else if (c <= 841) {
	if (c <= 789)
	  c1 = 0xa3b0 + (c - 780);
	else if (c <= 815)
	  c1 = 0xa3c1 + (c - 790);
	else
	  c1 = 0xa3e1 + (c - 816);
      } else if (c <= 1010) {
	if (c <= 924)
	  c1 = 0xa4a1 + (c - 842);
	else
	  c1 = 0xa5a1 + (c - 925);
      } else {
	if (c <= 1034)
	  c1 = 0xa6a1 + (c - 1011);
	else if (c <= 1058)
	  c1 = 0xa6c1 + (c - 1035);
	else if (c <= 1091)
	  c1 = 0xa7a1 + (c - 1059);
	else
	  c1 = 0xa7d1 + (c - 1092);
      }
    } else if (c <= 4089) {
      t1 = (c - 1125) / 94;
      t2 = (c - 1125) % 94;
      c1 = 0xb0a1 + (t1 << 8) + t2;
    } else if (c <= 7477) {
      t1 = (c - 4090) / 94;
      t2 = (c - 4090) % 94;
      c1 = 0xd0a1 + (t1 << 8) + t2;
    } else if (c <= 7554) {
      c1 = 0;
    } else if (c <= 7563) {	// circled Arabic numbers 1..9
      c1 = 0xa3b1 + (c - 7555);
    } else if (c <= 7574) {	// circled Arabic numbers 10..20
      t1 = c - 7564 + 10;
      sub[0] = 0xa3b0 + (t1 / 10);
      sub[1] = 0xa3b0 + (t1 % 10);
      sub[2] = 0;
      c1 = -1;
    } else if (c <= 7584) {	// Roman numbers I..X
      for (p = japan12Roman[c - 7575], q = sub; *p; ++p, ++q) {
	*q = 0xa380 + *p;
      }
      *q = 0;
      c1 = -1;
    } else if (c <= 7632) {
      if (c <= 7600) {
	c1 = 0;
      } else if (c <= 7606) {
	for (p = japan12Abbrev1[c - 7601], q = sub; *p; ++p, ++q) {
	  *q = 0xa380 + *p;
	}
	*q = 0;
	c1 = -1;
      } else {
	c1 = 0;
      }
    } else {
      c1 = 0;
    }
#if 0 //~
    if (c1 == 0) {
      error(-1, "Unsupported Adobe-Japan1-2 character: %d", c);
    }
#endif
#endif // JAPANESE_SUPPORT
    break;

  case font16AdobeGB12:
#if CHINESE_GB_SUPPORT
#endif
    break;

  case font16AdobeCNS13:
#if CHINESE_CNS_SUPPORT
    if (c <= 98) {
      c1 = cns13Map1[c];
    } else if (c <= 502) {
      if (c == 247) {
	c1 = 0xa1f7;
      } else if (c == 248) {
	c1 = 0xa1f6;
      } else {
	t1 = (c - 99) / 157;
	t2 = (c - 99) % 157;
	if (t2 <= 62) {
	  c1 = 0xa140 + (t1 << 8) + t2;
	} else {
	  c1 = 0xa162 + (t1 << 8) + t2;
	}
      }
    } else if (c <= 505) {
      c1 = 0xa3bd + (c - 503);
    } else if (c <= 594) {
      c1 = 0;
    } else if (c <= 5995) {
      if (c == 2431) {
	c1 = 0xacfe;
      } else if (c == 4308) {
	c1 = 0xbe52;
      } else if (c == 5221) {
	c1 = 0xc2cb;
      } else if (c == 5495) {
	c1 = 0xc456;
      } else if (c == 5550) {
	c1 = 0xc3ba;
      } else if (c == 5551) {
	c1 = 0xc3b9;
      } else {
	if (c >= 2007 && c <= 2430) {
	  t1 = c - 594;
	} else if (c >= 4309 && c <= 4695) {
	  t1 = c - 596;
	} else if (c >= 5222 && c <= 5410) {
	  t1 = c - 596;
	} else if (c >= 5496 && c <= 5641) {
	  t1 = c - 596;
	} else {
	  t1 = c - 595;
	}
	t2 = t1 % 157;
	t1 /= 157;
	if (t2 <= 62) {
	  c1 = 0xa440 + (t1 << 8) + t2;
	} else {
	  c1 = 0xa462 + (t1 << 8) + t2;
	}
      }
    } else if (c <= 13645) {
      if (c == 6039) {
	c1 = 0xc9be;
      } else if (c == 6134) {
	c1 = 0xcaf7;
      } else if (c == 8142) {
	c1 = 0xdadf;
      } else if (c == 8788) {
	c1 = 0xd6cc;
      } else if (c == 8889) {
	c1 = 0xd77a;
      } else if (c == 10926) {
	c1 = 0xebf1;
      } else if (c == 11073) {
	c1 = 0xecde;
      } else if (c == 11361) {
	c1 = 0xf0cb;
      } else if (c == 11719) {
	c1 = 0xf056;
      } else if (c == 12308) {
	c1 = 0xeeeb;
      } else if (c == 12526) {
	c1 = 0xf4b5;
      } else if (c == 12640) {
	c1 = 0xf16b;
      } else if (c == 12783) {
	c1 = 0xf268;
      } else if (c == 12900) {
	c1 = 0xf663;
      } else if (c == 13585) {
	c1 = 0xf9c4;
      } else if (c == 13641) {
	c1 = 0xf9c6;
      } else {
	if (c >= 6006 && c <= 6038) {
	  t1 = c - 5995;
	} else if (c >= 6088 && c <= 6133) {
	  t1 = c - 5995;
	} else if (c >= 6302 && c <= 8250) {
	  t1 = c - 5995;
	} else if (c >= 8251 && c <= 8888) {
	  t1 = c - 5994;
	} else if (c >= 8890 && c <= 9288) {
	  t1 = c - 5995;
	} else if (c >= 9289 && c <= 10925) {
	  t1 = c - 5994;
	} else if (c >= 10927 && c <= 11072) {
	  t1 = c - 5995;
	} else if (c >= 11362 && c <= 11477) {
	  t1 = c - 5997;
	} else if (c >= 11615 && c <= 11718) {
	  t1 = c - 5995;
	} else if (c >= 11942 && c <= 12139) {
	  t1 = c - 5995;
	} else if (c >= 12140 && c <= 12221) {
	  t1 = c - 5994;
	} else if (c >= 12222 && c <= 12307) {
	  t1 = c - 5993;
	} else if (c >= 12309 && c <= 12316) {
	  t1 = c - 5994;
	} else if (c >= 12317 && c <= 12469) {
	  t1 = c - 5993;
	} else if (c >= 12470 && c <= 12525) {
	  t1 = c - 5992;
	} else if (c >= 12527 && c <= 12639) {
	  t1 = c - 5993;
	} else if (c >= 12641 && c <= 12782) {
	  t1 = c - 5994;
	} else if (c >= 12784 && c <= 12828) {
	  t1 = c - 5995;
	} else if (c >= 12829 && c <= 12899) {
	  t1 = c - 5994;
	} else if (c >= 12901 && c <= 13094) {
	  t1 = c - 5995;
	} else if (c >= 13095 && c <= 13584) {
	  t1 = c - 5994;
	} else if (c >= 13586 && c <= 13628) {
	  t1 = c - 5995;
	} else if (c == 13629) {
	  t1 = c - 5994;
	} else if (c >= 13630 && c <= 13640) {
	  t1 = c - 5993;
	} else if (c >= 13642 && c <= 13645) {
	  t1 = c - 5994;
	} else {
	  t1 = c - 5996;
	}
	t2 = t1 % 157;
	t1 /= 157;
	if (t2 <= 62) {
	  c1 = 0xc940 + (t1 << 8) + t2;
	} else {
	  c1 = 0xc962 + (t1 << 8) + t2;
	}
      }
    } else if (c == 13646) {
      c1 = 0xa14b;
    } else if (c == 13647) {
      c1 = 0xa1e3;
    } else if (c <= 13742) {
      c1 = cns13Map2[c - 13648];
    } else if (c <= 13746) {
      c1 = 0xa159 + (c - 13743);
    } else if (c <= 14055) {
      c1 = 0;
    } else if (c <= 14062) {
      c1 = 0xf9d6 + (c - 14056);
    }
#if 1 //~
    if (c1 == 0) {
      error(-1, "Unsupported Adobe-CNS1-3 character: %d", c);
    }
#endif
#endif
    break;
  }

  // append converted character to string
  if (c1 == 0) {
    text->append(' ');
    n = 1;
  } else if (c1 > 0) {
    text->append(c1 >> 8);
    text->append(c1 & 0xff);
    n = 2;
  } else {
    n = 0;
    for (q = sub; *q; ++q) {
      text->append(*q >> 8);
      text->append(*q & 0xff);
      n += 2;
    }
  }

  // update position information
  if (i+n > ((i+15) & ~15)) {
    xRight = (double *)grealloc(xRight, ((i+n+15) & ~15) * sizeof(double));
  }
  if (i == 0) {
    xMin = x;
  }
  for (j = 0; j < n; ++j) {
    xRight[i+j] = x + dx;
  }
  xMax = x + dx;
}

//------------------------------------------------------------------------
// TextPage
//------------------------------------------------------------------------

TextPage::TextPage(TextOutputCharSet charSet, GBool rawOrder) {
  this->charSet = charSet;
  this->rawOrder = rawOrder;
  curStr = NULL;
  yxStrings = NULL;
  xyStrings = NULL;
  yxCur1 = yxCur2 = NULL;
  nest = 0;
}

TextPage::~TextPage() {
  clear();
}

void TextPage::beginString(GfxState *state, GString *s, GBool hexCodes) {
  // This check is needed because Type 3 characters can contain
  // text-drawing operations.
  if (curStr) {
    ++nest;
    return;
  }

  curStr = new TextString(state, hexCodes);
}

void TextPage::addChar(GfxState *state, double x, double y,
		       double dx, double dy, Guchar c) {
  double x1, y1, w1, h1, dx2, dy2;
  int n;
  GBool hexCodes;

  state->transform(x, y, &x1, &y1);
  state->textTransformDelta(state->getCharSpace(), 0, &dx2, &dy2);
  dx -= dx2;
  dy -= dy2;
  state->transformDelta(dx, dy, &w1, &h1);
  n = curStr->text->getLength();
  if (n > 0 &&
      x1 - curStr->xRight[n-1] > 0.1 * (curStr->yMax - curStr->yMin)) {
    hexCodes = curStr->hexCodes;
    endString();
    beginString(state, NULL, hexCodes);
  }
  curStr->addChar(state, x1, y1, w1, h1, c, charSet);
}

void TextPage::addChar16(GfxState *state, double x, double y,
			 double dx, double dy, int c,
			 GfxFontCharSet16 charSet) {
  double x1, y1, w1, h1, dx2, dy2;
  int n;
  GBool hexCodes;

  state->transform(x, y, &x1, &y1);
  state->textTransformDelta(state->getCharSpace(), 0, &dx2, &dy2);
  dx -= dx2;
  dy -= dy2;
  state->transformDelta(dx, dy, &w1, &h1);
  n = curStr->text->getLength();
  if (n > 0 &&
      x1 - curStr->xRight[n-1] > 0.1 * (curStr->yMax - curStr->yMin)) {
    hexCodes = curStr->hexCodes;
    endString();
    beginString(state, NULL, hexCodes);
  }
  curStr->addChar16(state, x1, y1, w1, h1, c, charSet);
}

void TextPage::endString() {
  TextString *p1, *p2;
  double h, y1, y2;

  if (nest > 0) {
    --nest;
    return;
  }

  // throw away zero-length strings -- they don't have valid xMin/xMax
  // values, and they're useless anyway
  if (curStr->text->getLength() == 0) {
    delete curStr;
    curStr = NULL;
    return;
  }

  // insert string in y-major list
  h = curStr->yMax - curStr->yMin;
  y1 = curStr->yMin + 0.5 * h;
  y2 = curStr->yMin + 0.8 * h;
  if (rawOrder) {
    p1 = yxCur1;
    p2 = NULL;
  } else if ((!yxCur1 ||
	      (y1 >= yxCur1->yMin &&
	       (y2 >= yxCur1->yMax || curStr->xMax >= yxCur1->xMin))) &&
	     (!yxCur2 ||
	      (y1 < yxCur2->yMin ||
	       (y2 < yxCur2->yMax && curStr->xMax < yxCur2->xMin)))) {
    p1 = yxCur1;
    p2 = yxCur2;
  } else {
    for (p1 = NULL, p2 = yxStrings; p2; p1 = p2, p2 = p2->yxNext) {
      if (y1 < p2->yMin || (y2 < p2->yMax && curStr->xMax < p2->xMin))
	break;
    }
    yxCur2 = p2;
  }
  yxCur1 = curStr;
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
    if (((rawOrder &&
	  ((str2->yMin >= str1->yMin && str2->yMin <= str1->yMax) ||
	   (str2->yMax >= str1->yMin && str2->yMax <= str1->yMax))) ||
	 (!rawOrder && str2->yMin < str1->yMax)) &&
	d > -0.5 * space && d < space) {
      n = str1->text->getLength();
      if (d > 0.1 * space)
	str1->text->append(' ');
      str1->text->append(str2->text);
      str1->xRight = (double *)
	grealloc(str1->xRight,
		 ((str1->text->getLength() + 15) & ~15) * sizeof(double));
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
#ifdef MACOS
	  s->append('\r');
#else
	  s->append('\n');
#endif
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
  if (multiLine) {
#ifdef MACOS
    s->append('\r');
#else
    s->append('\n');
#endif
  }
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
    if (rawOrder && col1 == 0) {
      col1 = str1->col;
    } else {
      for (; col1 < str1->col; ++col1) {
	fputc(' ', f);
      }
    }

    // print the string
    fputs(str1->text->getCString(), f);

    // increment column
    col1 += str1->text->getLength();

    // update yMax for this line
    if (str1->yMax > yMax)
      yMax = str1->yMax;

    // if we've hit the end of the line...
    if (!(str1->yxNext &&
	  !(rawOrder && str1->yxNext->yMax < str1->yMin) &&
	  str1->yxNext->yMin < 0.2*str1->yMin + 0.8*str1->yMax &&
	  str1->yxNext->xMin >= str1->xMax)) {

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
	if (rawOrder && d > 2) {
	  d = 2;
	}
	for (; d > 0; --d) {
	  fputc('\n', f);
	}
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
  yxCur1 = yxCur2 = NULL;
}

//------------------------------------------------------------------------
// TextOutputDev
//------------------------------------------------------------------------

TextOutputDev::TextOutputDev(char *fileName, TextOutputCharSet charSet,
			     GBool rawOrder) {
  text = NULL;
  this->rawOrder = rawOrder;
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
  text = new TextPage(charSet, rawOrder);
}

TextOutputDev::~TextOutputDev() {
  if (needClose) {
#ifdef MACOS
    ICS_MapRefNumAndAssign((short)f->handle);
#endif
    fclose(f);
  }
  if (text) {
    delete text;
  }
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
  if ((font = state->getFont()) && !font->is16Bit()) {
    for (c = 0; c < 256; ++c) {
      if ((charName = font->getCharName(c))) {
	if ((charName[0] == 'B' || charName[0] == 'C' ||
	     charName[0] == 'G') &&
	    strlen(charName) == 3 &&
	    isxdigit(charName[1]) && isxdigit(charName[2]) &&
	    ((charName[1] >= 'a' && charName[1] <= 'f') ||
	     (charName[1] >= 'A' && charName[1] <= 'F') ||
	     (charName[2] >= 'a' && charName[2] <= 'f') ||
	     (charName[2] >= 'A' && charName[2] <= 'F'))) {
	  hexCodes = gTrue;
	  break;
	} else if ((strlen(charName) == 2) &&
		   isxdigit(charName[0]) && isxdigit(charName[1]) &&
		   ((charName[0] >= 'a' && charName[0] <= 'f') ||
		    (charName[0] >= 'A' && charName[0] <= 'F') ||
		    (charName[1] >= 'a' && charName[1] <= 'f') ||
		    (charName[1] >= 'A' && charName[1] <= 'F'))) {
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

void TextOutputDev::drawChar16(GfxState *state, double x, double y,
			       double dx, double dy, int c) {
  text->addChar16(state, x, y, dx, dy, c, state->getFont()->getCharSet16());
}

GBool TextOutputDev::findText(char *s, GBool top, GBool bottom,
			      double *xMin, double *yMin,
			      double *xMax, double *yMax) {
  return text->findText(s, top, bottom, xMin, yMin, xMax, yMax);
}
