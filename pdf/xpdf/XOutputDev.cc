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
#include <unistd.h>
#include <string.h>
#include <ctype.h>
#include <math.h>
#include "gmem.h"
#include "gfile.h"
#include "GString.h"
#include "Object.h"
#include "Stream.h"
#include "Link.h"
#include "GfxState.h"
#include "GfxFont.h"
#include "FontFile.h"
#include "FontEncoding.h"
#include "Error.h"
#include "Params.h"
#include "TextOutputDev.h"
#include "XOutputDev.h"
#if HAVE_T1LIB_H
#include "T1Font.h"
#endif
#if HAVE_FREETYPE_FREETYPE_H | HAVE_FREETYPE_H
#include "TTFont.h"
#endif

#include "XOutputFontInfo.h"

#ifdef VMS
#if (__VMS_VER < 70000000)
extern "C" int unlink(char *filename);
#endif
#endif

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

GBool installCmap = gFalse;

int rgbCubeSize = defaultRGBCube;

#if HAVE_T1LIB_H
GString *t1libControl = NULL;
#endif

#if HAVE_FREETYPE_FREETYPE_H | HAVE_FREETYPE_H
GString *freeTypeControl = NULL;
#endif

GString *t1Courier = NULL;
GString *t1CourierBold = NULL;
GString *t1CourierBoldOblique = NULL;
GString *t1CourierOblique = NULL;
GString *t1Helvetica = NULL;
GString *t1HelveticaBold = NULL;
GString *t1HelveticaBoldOblique = NULL;
GString *t1HelveticaOblique = NULL;
GString *t1Symbol = NULL;
GString *t1TimesBold = NULL;
GString *t1TimesBoldItalic = NULL;
GString *t1TimesItalic = NULL;
GString *t1TimesRoman = NULL;
GString *t1ZapfDingbats = NULL;

GBool useEUCJP = gFalse;
#if JAPANESE_SUPPORT
GString *japan12Font = NULL;
#endif
#if CHINESE_GB_SUPPORT
GString *gb12Font = NULL;
#endif
#if CHINESE_CNS_SUPPORT
GString *cns13Font = NULL;
#endif

//------------------------------------------------------------------------
// Font map
//------------------------------------------------------------------------

struct FontMapEntry {
  char *pdfFont;
  char *xFont;
  GString **t1Font;
  FontEncoding *encoding;
};

static FontMapEntry fontMap[] = {
  {"Courier",               "-*-courier-medium-r-normal-*-%s-*-*-*-*-*-iso8859-1",          &t1Courier,              &isoLatin1Encoding},
  {"Courier-Bold",          "-*-courier-bold-r-normal-*-%s-*-*-*-*-*-iso8859-1",            &t1CourierBold,          &isoLatin1Encoding},
  {"Courier-BoldOblique",   "-*-courier-bold-o-normal-*-%s-*-*-*-*-*-iso8859-1",            &t1CourierBoldOblique,   &isoLatin1Encoding},
  {"Courier-Oblique",       "-*-courier-medium-o-normal-*-%s-*-*-*-*-*-iso8859-1",          &t1CourierOblique,       &isoLatin1Encoding},
  {"Helvetica",             "-*-helvetica-medium-r-normal-*-%s-*-*-*-*-*-iso8859-1",        &t1Helvetica,            &isoLatin1Encoding},
  {"Helvetica-Bold",        "-*-helvetica-bold-r-normal-*-%s-*-*-*-*-*-iso8859-1",          &t1HelveticaBold,        &isoLatin1Encoding},
  {"Helvetica-BoldOblique", "-*-helvetica-bold-o-normal-*-%s-*-*-*-*-*-iso8859-1",          &t1HelveticaBoldOblique, &isoLatin1Encoding},
  {"Helvetica-Oblique",     "-*-helvetica-medium-o-normal-*-%s-*-*-*-*-*-iso8859-1",        &t1HelveticaOblique,     &isoLatin1Encoding},
  {"Symbol",                "-*-symbol-medium-r-normal-*-%s-*-*-*-*-*-adobe-fontspecific",  &t1Symbol,               &symbolEncoding},
  {"Times-Bold",            "-*-times-bold-r-normal-*-%s-*-*-*-*-*-iso8859-1",              &t1TimesBold,            &isoLatin1Encoding},
  {"Times-BoldItalic",      "-*-times-bold-i-normal-*-%s-*-*-*-*-*-iso8859-1",              &t1TimesBoldItalic,      &isoLatin1Encoding},
  {"Times-Italic",          "-*-times-medium-i-normal-*-%s-*-*-*-*-*-iso8859-1",            &t1TimesItalic,          &isoLatin1Encoding},
  {"Times-Roman",           "-*-times-medium-r-normal-*-%s-*-*-*-*-*-iso8859-1",            &t1TimesRoman,           &isoLatin1Encoding},
  {"ZapfDingbats",          "-*-zapfdingbats-medium-r-normal-*-%s-*-*-*-*-*-*-*",           &t1ZapfDingbats,         &zapfDingbatsEncoding},
  {NULL}
};

static FontMapEntry *userFontMap;

//------------------------------------------------------------------------
// Font substitutions
//------------------------------------------------------------------------

struct FontSubst {
  char *xFont;
  GString **t1Font;
  double mWidth;
};

// index: {symbolic:12, fixed:8, serif:4, sans-serif:0} + bold*2 + italic
static FontSubst fontSubst[16] = {
  {"-*-helvetica-medium-r-normal-*-%s-*-*-*-*-*-iso8859-1",       &t1Helvetica,            0.833},
  {"-*-helvetica-medium-o-normal-*-%s-*-*-*-*-*-iso8859-1",       &t1HelveticaOblique,     0.833},
  {"-*-helvetica-bold-r-normal-*-%s-*-*-*-*-*-iso8859-1",         &t1HelveticaBold,        0.889},
  {"-*-helvetica-bold-o-normal-*-%s-*-*-*-*-*-iso8859-1",         &t1HelveticaBoldOblique, 0.889},
  {"-*-times-medium-r-normal-*-%s-*-*-*-*-*-iso8859-1",           &t1TimesRoman,           0.788},
  {"-*-times-medium-i-normal-*-%s-*-*-*-*-*-iso8859-1",           &t1TimesItalic,          0.722},
  {"-*-times-bold-r-normal-*-%s-*-*-*-*-*-iso8859-1",             &t1TimesBold,            0.833},
  {"-*-times-bold-i-normal-*-%s-*-*-*-*-*-iso8859-1",             &t1TimesBoldItalic,      0.778},
  {"-*-courier-medium-r-normal-*-%s-*-*-*-*-*-iso8859-1",         &t1Courier,              0.600},
  {"-*-courier-medium-o-normal-*-%s-*-*-*-*-*-iso8859-1",         &t1CourierOblique,       0.600},
  {"-*-courier-bold-r-normal-*-%s-*-*-*-*-*-iso8859-1",           &t1CourierBold,          0.600},
  {"-*-courier-bold-o-normal-*-%s-*-*-*-*-*-iso8859-1",           &t1CourierBoldOblique,   0.600},
  {"-*-symbol-medium-r-normal-*-%s-*-*-*-*-*-adobe-fontspecific", &t1Symbol,               0.576},
  {"-*-symbol-medium-r-normal-*-%s-*-*-*-*-*-adobe-fontspecific", &t1Symbol,               0.576},
  {"-*-symbol-medium-r-normal-*-%s-*-*-*-*-*-adobe-fontspecific", &t1Symbol,               0.576},
  {"-*-symbol-medium-r-normal-*-%s-*-*-*-*-*-adobe-fontspecific", &t1Symbol,               0.576}
};

//------------------------------------------------------------------------
// 16-bit fonts
//------------------------------------------------------------------------

#if JAPANESE_SUPPORT

static char *japan12DefFont =
    "-*-fixed-medium-r-normal-*-%s-*-*-*-*-*-jisx0208.1983-0";

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

#endif // JAPANESE_SUPPORT

#if CHINESE_GB_SUPPORT

static char *gb12DefFont =
    "-*-fangsong ti-medium-r-normal-*-%s-*-*-*-*-*-gb2312.1980-0";

static Gushort gb12Map[940] = {
  // 0 - 95
  0x0000, 0x2121, 0x2321, 0x2322, 0x2323, 0x2167, 0x2325, 0x2326,
  0x2327, 0x2328, 0x2329, 0x232a, 0x232b, 0x232c, 0x232d, 0x232e,
  0x232f, 0x2330, 0x2331, 0x2332, 0x2333, 0x2334, 0x2335, 0x2336,
  0x2337, 0x2338, 0x2339, 0x233a, 0x233b, 0x233c, 0x233d, 0x233e,
  0x233f, 0x2340, 0x2341, 0x2342, 0x2343, 0x2344, 0x2345, 0x2346,
  0x2347, 0x2348, 0x2349, 0x234a, 0x234b, 0x234c, 0x234d, 0x234e,
  0x234f, 0x2350, 0x2351, 0x2352, 0x2353, 0x2354, 0x2355, 0x2356,
  0x2357, 0x2358, 0x2359, 0x235a, 0x235b, 0x235c, 0x235d, 0x235e,
  0x235f, 0x2360, 0x2361, 0x2362, 0x2363, 0x2364, 0x2365, 0x2366,
  0x2367, 0x2368, 0x2369, 0x236a, 0x236b, 0x236c, 0x236d, 0x236e,
  0x236f, 0x2370, 0x2371, 0x2372, 0x2373, 0x2374, 0x2375, 0x2376,
  0x2377, 0x2378, 0x2379, 0x237a, 0x237b, 0x237c, 0x237d, 0x212b,

  // 96-355
  0x2121, 0x2122, 0x2123, 0x2124, 0x2125, 0x2126, 0x2127, 0x2128,
  0x2129, 0x212a, 0x212b, 0x212c, 0x212d, 0x212e, 0x212f, 0x2130,
  0x2131, 0x2132, 0x2133, 0x2134, 0x2135, 0x2136, 0x2137, 0x2138,
  0x2139, 0x213a, 0x213b, 0x213c, 0x213d, 0x213e, 0x213f, 0x2140,
  0x2141, 0x2142, 0x2143, 0x2144, 0x2145, 0x2146, 0x2147, 0x2148,
  0x2149, 0x214a, 0x214b, 0x214c, 0x214d, 0x214e, 0x214f, 0x2150,
  0x2151, 0x2152, 0x2153, 0x2154, 0x2155, 0x2156, 0x2157, 0x2158,
  0x2159, 0x215a, 0x215b, 0x215c, 0x215d, 0x215e, 0x215f, 0x2160,
  0x2161, 0x2162, 0x2163, 0x2164, 0x2165, 0x2166, 0x2167, 0x2168,
  0x2169, 0x216a, 0x216b, 0x216c, 0x216d, 0x216e, 0x216f, 0x2170,
  0x2171, 0x2172, 0x2173, 0x2174, 0x2175, 0x2176, 0x2177, 0x2178,
  0x2179, 0x217a, 0x217b, 0x217c, 0x217d, 0x217e, 0x2231, 0x2232,
  0x2233, 0x2234, 0x2235, 0x2236, 0x2237, 0x2238, 0x2239, 0x223a,
  0x223b, 0x223c, 0x223d, 0x223e, 0x223f, 0x2240, 0x2241, 0x2242,
  0x2243, 0x2244, 0x2245, 0x2246, 0x2247, 0x2248, 0x2249, 0x224a,
  0x224b, 0x224c, 0x224d, 0x224e, 0x224f, 0x2250, 0x2251, 0x2252,
  0x2253, 0x2254, 0x2255, 0x2256, 0x2257, 0x2258, 0x2259, 0x225a,
  0x225b, 0x225c, 0x225d, 0x225e, 0x225f, 0x2260, 0x2261, 0x2262,
  0x2265, 0x2266, 0x2267, 0x2268, 0x2269, 0x226a, 0x226b, 0x226c,
  0x226d, 0x226e, 0x2271, 0x2272, 0x2273, 0x2274, 0x2275, 0x2276,
  0x2277, 0x2278, 0x2279, 0x227a, 0x227b, 0x227c, 0x2321, 0x2322,
  0x2323, 0x2324, 0x2325, 0x2326, 0x2327, 0x2328, 0x2329, 0x232a,
  0x232b, 0x232c, 0x232d, 0x232e, 0x232f, 0x2330, 0x2331, 0x2332,
  0x2333, 0x2334, 0x2335, 0x2336, 0x2337, 0x2338, 0x2339, 0x233a,
  0x233b, 0x233c, 0x233d, 0x233e, 0x233f, 0x2340, 0x2341, 0x2342,
  0x2343, 0x2344, 0x2345, 0x2346, 0x2347, 0x2348, 0x2349, 0x234a,
  0x234b, 0x234c, 0x234d, 0x234e, 0x234f, 0x2350, 0x2351, 0x2352,
  0x2353, 0x2354, 0x2355, 0x2356, 0x2357, 0x2358, 0x2359, 0x235a,
  0x235b, 0x235c, 0x235d, 0x235e, 0x235f, 0x2360, 0x2361, 0x2362,
  0x2363, 0x2364, 0x2365, 0x2366, 0x2367, 0x2368, 0x2369, 0x236a,
  0x236b, 0x236c, 0x236d, 0x236e, 0x236f, 0x2370, 0x2371, 0x2372,
  0x2373, 0x2374, 0x2375, 0x2376, 0x2377, 0x2378, 0x2379, 0x237a,
  0x237b, 0x237c, 0x237d, 0x237e,

  // 356-524
                                  0x2421, 0x2422, 0x2423, 0x2424,
  0x2425, 0x2426, 0x2427, 0x2428, 0x2429, 0x242a, 0x242b, 0x242c,
  0x242d, 0x242e, 0x242f, 0x2430, 0x2431, 0x2432, 0x2433, 0x2434,
  0x2435, 0x2436, 0x2437, 0x2438, 0x2439, 0x243a, 0x243b, 0x243c,
  0x243d, 0x243e, 0x243f, 0x2440, 0x2441, 0x2442, 0x2443, 0x2444,
  0x2445, 0x2446, 0x2447, 0x2448, 0x2449, 0x244a, 0x244b, 0x244c,
  0x244d, 0x244e, 0x244f, 0x2450, 0x2451, 0x2452, 0x2453, 0x2454,
  0x2455, 0x2456, 0x2457, 0x2458, 0x2459, 0x245a, 0x245b, 0x245c,
  0x245d, 0x245e, 0x245f, 0x2460, 0x2461, 0x2462, 0x2463, 0x2464,
  0x2465, 0x2466, 0x2467, 0x2468, 0x2469, 0x246a, 0x246b, 0x246c,
  0x246d, 0x246e, 0x246f, 0x2470, 0x2471, 0x2472, 0x2473, 0x2521,
  0x2522, 0x2523, 0x2524, 0x2525, 0x2526, 0x2527, 0x2528, 0x2529,
  0x252a, 0x252b, 0x252c, 0x252d, 0x252e, 0x252f, 0x2530, 0x2531,
  0x2532, 0x2533, 0x2534, 0x2535, 0x2536, 0x2537, 0x2538, 0x2539,
  0x253a, 0x253b, 0x253c, 0x253d, 0x253e, 0x253f, 0x2540, 0x2541,
  0x2542, 0x2543, 0x2544, 0x2545, 0x2546, 0x2547, 0x2548, 0x2549,
  0x254a, 0x254b, 0x254c, 0x254d, 0x254e, 0x254f, 0x2550, 0x2551,
  0x2552, 0x2553, 0x2554, 0x2555, 0x2556, 0x2557, 0x2558, 0x2559,
  0x255a, 0x255b, 0x255c, 0x255d, 0x255e, 0x255f, 0x2560, 0x2561,
  0x2562, 0x2563, 0x2564, 0x2565, 0x2566, 0x2567, 0x2568, 0x2569,
  0x256a, 0x256b, 0x256c, 0x256d, 0x256e, 0x256f, 0x2570, 0x2571,
  0x2572, 0x2573, 0x2574, 0x2575, 0x2576,

  // 525-572
                                          0x2621, 0x2622, 0x2623,
  0x2624, 0x2625, 0x2626, 0x2627, 0x2628, 0x2629, 0x262a, 0x262b,
  0x262c, 0x262d, 0x262e, 0x262f, 0x2630, 0x2631, 0x2632, 0x2633,
  0x2634, 0x2635, 0x2636, 0x2637, 0x2638, 0x2641, 0x2642, 0x2643,
  0x2644, 0x2645, 0x2646, 0x2647, 0x2648, 0x2649, 0x264a, 0x264b,
  0x264c, 0x264d, 0x264e, 0x264f, 0x2650, 0x2651, 0x2652, 0x2653,
  0x2654, 0x2655, 0x2656, 0x2657, 0x2658,

  // 573-601
                                          0,      0,      0,
  0,      0,      0,      0,      0,      0,      0,      0,
  0,      0,      0,      0,      0,      0,      0,      0,
  0,      0,      0,      0,      0,      0,      0,      0,
  0,      0,

  // 602-667
                  0x2721, 0x2722, 0x2723, 0x2724, 0x2725, 0x2726,
  0x2727, 0x2728, 0x2729, 0x272a, 0x272b, 0x272c, 0x272d, 0x272e,
  0x272f, 0x2730, 0x2731, 0x2732, 0x2733, 0x2734, 0x2735, 0x2736,
  0x2737, 0x2738, 0x2739, 0x273a, 0x273b, 0x273c, 0x273d, 0x273e,
  0x273f, 0x2740, 0x2741, 0x2751, 0x2752, 0x2753, 0x2754, 0x2755,
  0x2756, 0x2757, 0x2758, 0x2759, 0x275a, 0x275b, 0x275c, 0x275d,
  0x275e, 0x275f, 0x2760, 0x2761, 0x2762, 0x2763, 0x2764, 0x2765,
  0x2766, 0x2767, 0x2768, 0x2769, 0x276a, 0x276b, 0x276c, 0x276d,
  0x276e, 0x276f, 0x2770, 0x2771,

  // 668-699
                                  0x2821, 0x2822, 0x2823, 0x2824,
  0x2825, 0x2826, 0x2827, 0x2828, 0x2829, 0x282a, 0x282b, 0x282c,
  0x282d, 0x282e, 0x282f, 0x2830, 0x2831, 0x2832, 0x2833, 0x2834,
  0x2835, 0x2836, 0x2837, 0x2838, 0x2839, 0x283a, 0,      0,
  0,      0,      0,      0,

  // 700-737
                                  0x2845, 0x2846, 0x2847, 0x2848,
  0x2849, 0x284a, 0x284b, 0x284c, 0x284d, 0x284e, 0x284f, 0x2850,
  0x2851, 0x2852, 0x2853, 0x2854, 0x2855, 0x2856, 0x2857, 0x2858,
  0x2859, 0x285a, 0x285b, 0x285c, 0x285d, 0x285e, 0x285f, 0x2860,
  0x2861, 0x2862, 0x2863, 0x2864, 0x2865, 0x2866, 0x2867, 0x2868,
  0x2869, 0,

  // 738-813
                  0x2924, 0x2925, 0x2926, 0x2927, 0x2928, 0x2929,
  0x292a, 0x292b, 0x292c, 0x292d, 0x292e, 0x292f, 0x2930, 0x2931,
  0x2932, 0x2933, 0x2934, 0x2935, 0x2936, 0x2937, 0x2938, 0x2939,
  0x293a, 0x293b, 0x293c, 0x293d, 0x293e, 0x293f, 0x2940, 0x2941,
  0x2942, 0x2943, 0x2944, 0x2945, 0x2946, 0x2947, 0x2948, 0x2949,
  0x294a, 0x294b, 0x294c, 0x294d, 0x294e, 0x294f, 0x2950, 0x2951,
  0x2952, 0x2953, 0x2954, 0x2955, 0x2956, 0x2957, 0x2958, 0x2959,
  0x295a, 0x295b, 0x295c, 0x295d, 0x295e, 0x295f, 0x2960, 0x2961,
  0x2962, 0x2963, 0x2964, 0x2965, 0x2966, 0x2967, 0x2968, 0x2969,
  0x296a, 0x296b, 0x296c, 0x296d, 0x296e, 0x296f,

  // 814-939
                                                  0x2321, 0x2322,
  0x2323, 0x2324, 0x2325, 0x2326, 0x2327, 0x2328, 0x2329, 0x232a,
  0x232b, 0x232c, 0x232d, 0x232e, 0x232f, 0x2330, 0x2331, 0x2332,
  0x2333, 0x2334, 0x2335, 0x2336, 0x2337, 0x2338, 0x2339, 0x233a,
  0x233b, 0x233c, 0x233d, 0x233e, 0x233f, 0x2340, 0x2341, 0x2342,
  0x2343, 0x2344, 0x2345, 0x2346, 0x2347, 0x2348, 0x2349, 0x234a,
  0x234b, 0x234c, 0x234d, 0x234e, 0x234f, 0x2350, 0x2351, 0x2352,
  0x2353, 0x2354, 0x2355, 0x2356, 0x2357, 0x2358, 0x2359, 0x235a,
  0x235b, 0x235c, 0x235d, 0x235e, 0x235f, 0x2360, 0x2361, 0x2362,
  0x2363, 0x2364, 0x2365, 0x2366, 0x2367, 0x2368, 0x2369, 0x236a,
  0x236b, 0x236c, 0x236d, 0x236e, 0x236f, 0x2370, 0x2371, 0x2372,
  0x2373, 0x2374, 0x2375, 0x2376, 0x2377, 0x2378, 0x2379, 0x237a,
  0x237b, 0x237c, 0x237d, 0x237e, 0x2821, 0x2822, 0x2823, 0x2824,
  0x2825, 0x2826, 0x2827, 0x2828, 0x2829, 0x282a, 0x282b, 0x282c,
  0x282d, 0x282e, 0x282f, 0x2830, 0x2831, 0x2832, 0x2833, 0x2834,
  0x2835, 0x2836, 0x2837, 0x2838, 0x2839, 0x283a, 0,      0,
  0,      0,      0,      0
};

#endif // CHINESE_GB_SUPPORT

#if CHINESE_CNS_SUPPORT

static char *cns13DefFont =
    "-*-fixed-medium-r-normal-*-%s-*-*-*-*-*-big5-0";

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
// Constructed characters
//------------------------------------------------------------------------

#define lastRegularChar 0x0ff
#define firstSubstChar  0x100
#define lastSubstChar   0x104
#define firstConstrChar 0x105
#define lastConstrChar  0x106
#define firstMultiChar  0x107
#define lastMultiChar   0x110

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
  "ff",				// 109: ff
  "ffi",			// 10a: ffi
  "ffl",			// 10b: ffl
  "OE",				// 10c: OE
  "oe",				// 10d: oe
  "...",			// 10e: ellipsis
  "``",				// 10f: quotedblleft
  "''"				// 110: quotedblright
};

// ignored chars
// 111: Lslash
//    : Scaron
//    : Zcaron
//    : Ydieresis
//    : breve
//    : caron
//    : circumflex
//    : dagger
//    : daggerdbl
//    : dotaccent
//    : dotlessi
//    : florin
//    : grave
//    : guilsinglleft
//    : guilsinglright
//    : hungarumlaut
//    : lslash
//    : ogonek
//    : perthousand
//    : quotedblbase
//    : quotesinglbase
//    : scaron
//    : tilde
//    : zcaron

//------------------------------------------------------------------------
// XOutputFont
//------------------------------------------------------------------------

XOutputFont::XOutputFont(GfxFont *gfxFont, double m11, double m12,
			 double m21, double m22, Display *display,
			 XOutputFontCache *cache) {
  int code;
  char *charName;

  id = gfxFont->getID();
  this->display = display;
  tm11 = m11;
  tm12 = m12;
  tm21 = m21;
  tm22 = m22;

  // check for hex char names
  hex = gFalse;
  if (!gfxFont->is16Bit()) {
    for (code = 0; code < 256; ++code) {
      if ((charName = gfxFont->getCharName(code))) {
	if ((charName[0] == 'B' || charName[0] == 'C' ||
	     charName[0] == 'G') &&
	    strlen(charName) == 3 &&
	    isxdigit(charName[1]) && isxdigit(charName[2]) &&
	    ((charName[1] >= 'a' && charName[1] <= 'f') ||
	     (charName[1] >= 'A' && charName[1] <= 'F') ||
	     (charName[2] >= 'a' && charName[2] <= 'f') ||
	     (charName[2] >= 'A' && charName[2] <= 'F'))) {
	  hex = gTrue;
	  break;
	} else if ((strlen(charName) == 2) &&
		   isxdigit(charName[0]) && isxdigit(charName[1]) &&
		   ((charName[0] >= 'a' && charName[0] <= 'f') ||
		    (charName[0] >= 'A' && charName[0] <= 'F') ||
		    (charName[1] >= 'a' && charName[1] <= 'f') ||
		    (charName[1] >= 'A' && charName[1] <= 'F'))) {
	  hex = gTrue;
	  break;
	}
      }
    }
  }
}

XOutputFont::~XOutputFont() {
}

#if HAVE_T1LIB_H
//------------------------------------------------------------------------
// XOutputT1Font
//------------------------------------------------------------------------

XOutputT1Font::XOutputT1Font(GfxFont *gfxFont, GString *pdfBaseFont,
			     double m11, double m12, double m21, double m22,
			     Display *display, XOutputFontCache *cache):
  XOutputFont(gfxFont, m11, m12, m21, m22, display, cache)
{
  Ref embRef;
  double matrix[4];

  fontFile = NULL;
  font = NULL;

  // we can only handle 8-bit, Type 1/1C, with embedded font file
  // or user-specified base fonts
  //~ also look for external font files
  if (!(pdfBaseFont ||
	(!gfxFont->is16Bit() &&
	 (gfxFont->getType() == fontType1 ||
	  gfxFont->getType() == fontType1C) &&
	 gfxFont->getEmbeddedFontID(&embRef)))) {
    return;
  }

  // load the font
  if (!(fontFile = cache->getT1Font(gfxFont, pdfBaseFont))) {
    return;
  }

  // create the transformed instance
  matrix[0] = m11;
  matrix[1] = -m12;
  matrix[2] = m21;
  matrix[3] = -m22;
  font = new T1Font(fontFile, matrix);
}

XOutputT1Font::~XOutputT1Font() {
  if (font) {
    delete font;
  }
}

GBool XOutputT1Font::isOk() {
  return font != NULL;
}

void XOutputT1Font::updateGC(GC gc) {
}

void XOutputT1Font::drawChar(GfxState *state, Pixmap pixmap, int w, int h,
			     GC gc, double x, double y, int c) {
  GfxRGB rgb;

  if (state->getRender() & 1) {
    state->getStrokeRGB(&rgb);
  } else {
    state->getFillRGB(&rgb);
  }
  font->drawChar(pixmap, w, h, gc, xoutRound(x), xoutRound(y),
		 (int)(rgb.r * 65535), (int)(rgb.g * 65535),
		 (int)(rgb.b * 65535), c);
}
#endif // HAVE_T1LIB_H

#if HAVE_FREETYPE_FREETYPE_H | HAVE_FREETYPE_H
//------------------------------------------------------------------------
// XOutputTTFont
//------------------------------------------------------------------------

XOutputTTFont::XOutputTTFont(GfxFont *gfxFont, double m11, double m12,
			     double m21, double m22, Display *display,
			     XOutputFontCache *cache):
  XOutputFont(gfxFont, m11, m12, m21, m22, display, cache)
{
  Ref embRef;
  double matrix[4];

  fontFile = NULL;
  font = NULL;

  // we can only handle 8-bit, TrueType, with embedded font file
  if (!(!gfxFont->is16Bit() &&
	gfxFont->getType() == fontTrueType &&
	gfxFont->getEmbeddedFontID(&embRef))) {
    return;
  }

  // load the font
  if (!(fontFile = cache->getTTFont(gfxFont))) {
    return;
  }

  // create the transformed instance
  matrix[0] = m11;
  matrix[1] = -m12;
  matrix[2] = m21;
  matrix[3] = -m22;
  font = new TTFont(fontFile, matrix);
}

XOutputTTFont::~XOutputTTFont() {
  if (font) {
    delete font;
  }
}

GBool XOutputTTFont::isOk() {
  return font != NULL;
}

void XOutputTTFont::updateGC(GC gc) {
}

void XOutputTTFont::drawChar(GfxState *state, Pixmap pixmap, int w, int h,
			     GC gc, double x, double y, int c) {
  GfxRGB rgb;

  if (state->getRender() & 1) {
    state->getStrokeRGB(&rgb);
  } else {
    state->getFillRGB(&rgb);
  }
  font->drawChar(pixmap, w, h, gc, xoutRound(x), xoutRound(y),
		 (int)(rgb.r * 65535), (int)(rgb.g * 65535),
		 (int)(rgb.b * 65535), c);
}
#endif // HAVE_FREETYPE_FREETYPE_H | HAVE_FREETYPE_H

//------------------------------------------------------------------------
// XOutputServerFont
//------------------------------------------------------------------------

XOutputServerFont::XOutputServerFont(GfxFont *gfxFont, char *fontNameFmt,
				     FontEncoding *encoding,
				     double m11, double m12,
				     double m21, double m22,
				     double size,
				     double ntm11, double ntm12,
				     double ntm21, double ntm22,
				     Display *display,
				     XOutputFontCache *cache):
  XOutputFont(gfxFont, m11, m12, m21, m22, display, cache)
{
  char fontName[200], fontSize[100];
  GBool rotated;
  int startSize, sz;
  int code, code2;
  char *charName;
  int n;

  xFont = NULL;

  // Construct forward and reverse map.
  // This tries to deal with font subset character names of the form
  // 'Bxx', 'Cxx', 'Gxx', or 'xx' with decimal or hex numbering.
  if (!gfxFont->is16Bit()) {
    for (code = 0; code < 256; ++code)
      revMap[code] = 0;
    if (encoding) {
      for (code = 0; code < 256; ++code) {
	if ((charName = gfxFont->getCharName(code))) {
	  if ((code2 = encoding->getCharCode(charName)) < 0) {
	    n = strlen(charName);
	    if (hex && n == 3 &&
		(charName[0] == 'B' || charName[0] == 'C' ||
		 charName[0] == 'G') &&
		isxdigit(charName[1]) && isxdigit(charName[2])) {
	      sscanf(charName+1, "%x", &code2);
	    } else if (hex && n == 2 &&
		       isxdigit(charName[0]) && isxdigit(charName[1])) {
	      sscanf(charName, "%x", &code2);
	    } else if (!hex && n >= 2 && n <= 3 &&
		       isdigit(charName[0]) && isdigit(charName[1])) {
	      code2 = atoi(charName);
	      if (code2 >= 256)
		code2 = -1;
	    } else if (n >= 3 && n <= 5 && isdigit(charName[1])) {
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

  // adjust transform for the X transform convention
  ntm12 = -ntm12;
  ntm22 = -ntm22;

  // try to get a rotated font?
  rotated = !(ntm11 > 0 && ntm22 > 0 &&
	      fabs(ntm11 / ntm22 - 1) < 0.2 &&
	      fabs(ntm12) < 0.01 &&
	      fabs(ntm21) < 0.01);

  // open X font -- if font is not found (which means the server can't
  // scale fonts), try progressively smaller and then larger sizes
  //~ This does a linear search -- it should get a list of fonts from
  //~ the server and pick the closest.
  startSize = (int)size;
  if (rotated)
    sprintf(fontSize, "[%s%0.2f %s%0.2f %s%0.2f %s%0.2f]",
	    ntm11<0 ? "~" : "", fabs(ntm11 * size),
	    ntm12<0 ? "~" : "", fabs(ntm12 * size),
	    ntm21<0 ? "~" : "", fabs(ntm21 * size),
	    ntm22<0 ? "~" : "", fabs(ntm22 * size));
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

XOutputServerFont::~XOutputServerFont() {
  if (xFont)
    XFreeFont(display, xFont);
}

GBool XOutputServerFont::isOk() {
  return xFont != NULL;
}

void XOutputServerFont::updateGC(GC gc) {
  XSetFont(display, gc, xFont->fid);
}

void XOutputServerFont::drawChar(GfxState *state, Pixmap pixmap, int w, int h,
				 GC gc, double x, double y, int c) {
  GfxFont *gfxFont;
  Gushort c1;
  char buf;
  char *p;
  int n, i;
  double tx;

  c1 = map[c];
  if (c1 <= lastRegularChar) {
    buf = (char)c1;
    XDrawString(display, pixmap, gc, xoutRound(x), xoutRound(y), &buf, 1);
  } else if (c1 <= lastSubstChar) {
    buf = (char)substChars[c1 - firstSubstChar];
    XDrawString(display, pixmap, gc, xoutRound(x), xoutRound(y), &buf, 1);
  } else if (c1 <= lastConstrChar) {
    gfxFont = state->getFont();
    //~ need to deal with rotated text here
    switch (c1 - firstConstrChar) {
    case 0: // bullet
      tx = 0.25 * state->getTransformedFontSize() * gfxFont->getWidth(c);
      XFillRectangle(display, pixmap, gc,
		     xoutRound(x + tx),
		     xoutRound(y - 0.4 * xFont->ascent - tx),
		     xoutRound(2 * tx), xoutRound(2 * tx));
      break;
    case 1: // trademark
//~ this should use a smaller font
//      tx = state->getTransformedFontSize() *
//           (gfxFont->getWidth(c) -
//            gfxFont->getWidth(font->revMap['M']));
      tx = 0.9 * state->getTransformedFontSize() *
           gfxFont->getWidth(revMap['T']);
      y -= 0.33 * (double)xFont->ascent;
      buf = 'T';
      XDrawString(display, pixmap, gc,
		  xoutRound(x), xoutRound(y), &buf, 1);
      x += tx;
      buf = 'M';
      XDrawString(display, pixmap, gc,
		  xoutRound(x), xoutRound(y), &buf, 1);
      break;
    }
  } else if (c1 <= lastMultiChar) {
    gfxFont = state->getFont();
    p = multiChars[c1 - firstMultiChar];
    n = strlen(p);
    tx = gfxFont->getWidth(c);
    tx -= gfxFont->getWidth(revMap[p[n-1]]);
    tx = tx * state->getTransformedFontSize() / (double)(n - 1);
    for (i = 0; i < n; ++i) {
      XDrawString(display, pixmap, gc,
		  xoutRound(x), xoutRound(y), p + i, 1);
      x += tx;
    }
  }
}

//------------------------------------------------------------------------
// XOutputFontCache
//------------------------------------------------------------------------

XOutputFontCache::XOutputFontCache(Display *display, Guint depth) {
  this->display = display;
  this->depth = depth;

#if HAVE_T1LIB_H
  t1Engine = NULL;
  if (t1libControl) {
    useT1lib = t1libControl->cmp("none") != 0;
    t1libAA = t1libControl->cmp("plain") != 0;
    t1libAAHigh = t1libControl->cmp("high") == 0;
  } else {
    useT1lib = gFalse;
    t1libAA = gFalse;
    t1libAAHigh = gFalse;
  }
#endif

#if HAVE_FREETYPE_FREETYPE_H | HAVE_FREETYPE_H
  ttEngine = NULL;
  if (freeTypeControl) {
    useFreeType = freeTypeControl->cmp("none") != 0;
    freeTypeAA = freeTypeControl->cmp("plain") != 0;
  } else {
    useFreeType = gFalse;
    freeTypeAA = gFalse;
  }
#endif

  clear();
}

XOutputFontCache::~XOutputFontCache() {
  delFonts();
}

void XOutputFontCache::startDoc(int screenNum, Colormap colormap,
				GBool trueColor,
				int rMul, int gMul, int bMul,
				int rShift, int gShift, int bShift,
				Gulong *colors, int numColors) {
  delFonts();
  clear();

#if HAVE_T1LIB_H
  if (useT1lib) {
    t1Engine = new T1FontEngine(display, DefaultVisual(display, screenNum),
				depth, colormap, t1libAA, t1libAAHigh);
    if (t1Engine->isOk()) {
      if (trueColor) {
	t1Engine->useTrueColor(rMul, rShift, gMul, gShift, bMul, bShift);
      } else {
	t1Engine->useColorCube(colors, numColors);
      }
    } else {
      delete t1Engine;
      t1Engine = NULL;
    }
  }
#endif // HAVE_T1LIB_H

#if HAVE_FREETYPE_FREETYPE_H | HAVE_FREETYPE_H
  if (useFreeType) {
    ttEngine = new TTFontEngine(display, DefaultVisual(display, screenNum),
				depth, colormap, freeTypeAA);
    if (ttEngine->isOk()) {
      if (trueColor) {
	ttEngine->useTrueColor(rMul, rShift, gMul, gShift, bMul, bShift);
      } else {
	ttEngine->useColorCube(colors, numColors);
      }
    } else {
      delete ttEngine;
      ttEngine = NULL;
    }
  }
#endif
}

void XOutputFontCache::delFonts() {
  int i;

#if HAVE_T1LIB_H
  // delete Type 1 fonts
  for (i = 0; i < nT1Fonts; ++i) {
    delete t1Fonts[i];
  }
  for (i = 0; i < t1FontFilesSize && t1FontFiles[i].num >= 0; ++i) {
    delete t1FontFiles[i].fontFile;
  }
  gfree(t1FontFiles);
  if (t1Engine) {
    delete t1Engine;
  }
#endif

#if HAVE_FREETYPE_FREETYPE_H | HAVE_FREETYPE_H
  // delete TrueType fonts
  for (i = 0; i < nTTFonts; ++i) {
    delete ttFonts[i];
  }
  for (i = 0; i < ttFontFilesSize && ttFontFiles[i].num >= 0; ++i) {
    delete ttFontFiles[i].fontFile;
  }
  gfree(ttFontFiles);
  if (ttEngine) {
    delete ttEngine;
  }
#endif

  // delete server fonts
  for (i = 0; i < nServerFonts; ++i) {
    delete serverFonts[i];
  }
}

void XOutputFontCache::clear() {
  int i;

#if HAVE_T1LIB_H
  // clear Type 1 font cache
  for (i = 0; i < t1FontCacheSize; ++i) {
    t1Fonts[i] = NULL;
  }
  nT1Fonts = 0;
  t1FontFiles = NULL;
  t1FontFilesSize = 0;
#endif

#if HAVE_FREETYPE_FREETYPE_H | HAVE_FREETYPE_H
  // clear TrueType font cache
  for (i = 0; i < ttFontCacheSize; ++i) {
    ttFonts[i] = NULL;
  }
  nTTFonts = 0;
  ttFontFiles = NULL;
  ttFontFilesSize = 0;
#endif

  // clear server font cache
  for (i = 0; i < serverFontCacheSize; ++i) {
    serverFonts[i] = NULL;
  }
  nServerFonts = 0;
}

XOutputFont *XOutputFontCache::getFont(GfxFont *gfxFont,
				       double m11, double m12,
				       double m21, double m22) {
#if HAVE_T1LIB_H
  XOutputT1Font *t1Font;
#endif
#if HAVE_FREETYPE_FREETYPE_H | HAVE_FREETYPE_H
  XOutputTTFont *ttFont;
#endif
  XOutputServerFont *serverFont;
  FontMapEntry *fme;
  GString *t1FontName;
  char *xFontName;
  FontEncoding *xEncoding;
  double size;
  double ntm11, ntm12, ntm21, ntm22;
  double w1, w2, v;
  double *fm;
  int index;
  int code;
  int i, j;

  // is it the most recently used Type 1, TrueType, or server font?
#if HAVE_T1LIB_H
  if (useT1lib && nT1Fonts > 0 &&
      t1Fonts[0]->matches(gfxFont->getID(), m11, m12, m21, m22)) {
    return t1Fonts[0];
  }
#endif
#if HAVE_FREETYPE_FREETYPE_H | HAVE_FREETYPE_H
  if (useFreeType && nTTFonts > 0 &&
      ttFonts[0]->matches(gfxFont->getID(), m11, m12, m21, m22)) {
    return ttFonts[0];
  }
#endif
  if (nServerFonts > 0 && serverFonts[0]->matches(gfxFont->getID(),
						  m11, m12, m21, m22)) {
    return serverFonts[0];
  }

#if HAVE_T1LIB_H
  // is it in the Type 1 cache?
  if (useT1lib) {
    for (i = 1; i < nT1Fonts; ++i) {
      if (t1Fonts[i]->matches(gfxFont->getID(), m11, m12, m21, m22)) {
	t1Font = t1Fonts[i];
	for (j = i; j > 0; --j) {
	  t1Fonts[j] = t1Fonts[j-1];
	}
	t1Fonts[0] = t1Font;
	return t1Font;
      }
    }
  }
#endif

#if HAVE_FREETYPE_FREETYPE_H | HAVE_FREETYPE_H
  // is it in the TrueType cache?
  if (useFreeType) {
    for (i = 1; i < nTTFonts; ++i) {
      if (ttFonts[i]->matches(gfxFont->getID(), m11, m12, m21, m22)) {
	ttFont = ttFonts[i];
	for (j = i; j > 0; --j) {
	  ttFonts[j] = ttFonts[j-1];
	}
	ttFonts[0] = ttFont;
	return ttFont;
      }
    }
  }
#endif

  // is it in the server cache?
  for (i = 1; i < nServerFonts; ++i) {
    if (serverFonts[i]->matches(gfxFont->getID(), m11, m12, m21, m22)) {
      serverFont = serverFonts[i];
      for (j = i; j > 0; --j)
	serverFonts[j] = serverFonts[j-1];
      serverFonts[0] = serverFont;
      return serverFont;
    }
  }

  // search for a font map entry
  t1FontName = NULL;
  xFontName = NULL;
  xEncoding = NULL;
  if (!gfxFont->is16Bit() && gfxFont->getName()) {
    for (fme = userFontMap; fme->pdfFont; ++fme) {
      if (!gfxFont->getName()->cmp(fme->pdfFont)) {
	break;
      }
    }
    if (!fme->pdfFont) {
      for (fme = fontMap; fme->pdfFont; ++fme) {
	if (!gfxFont->getName()->cmp(fme->pdfFont)) {
	  break;
	}
      }
    }
    if (fme && fme->t1Font) {
      t1FontName = *fme->t1Font;
    }
    if (fme && fme->xFont && fme->encoding) {
      xFontName = fme->xFont;
      xEncoding = fme->encoding;
    }
  }

  // no font map entry found, so substitute a font
  if (!t1FontName && !xFontName) {
    if (gfxFont->is16Bit()) {
      xFontName = fontSubst[0].xFont;
      t1FontName = NULL;
      switch (gfxFont->getCharSet16()) {
      case font16AdobeJapan12:
#if JAPANESE_SUPPORT
	xFontName = japan12Font ? japan12Font->getCString() : japan12DefFont;
#endif
	break;
      case font16AdobeGB12:
#if CHINESE_GB_SUPPORT
	xFontName = gb12Font ? gb12Font->getCString() : gb12DefFont;
#endif
	break;
      case font16AdobeCNS13:
#if CHINESE_CNS_SUPPORT
	xFontName = cns13Font ? cns13Font->getCString() : cns13DefFont;
#endif
	break;
      }
    } else {
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
      xFontName = fontSubst[index].xFont;
      t1FontName = *fontSubst[index].t1Font;
      xEncoding = &isoLatin1Encoding;
      // get width of 'm' in real font and substituted font
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
	  m11 *= w1;
	  m12 *= w1;
	  m21 *= w1;
	  m22 *= w1;
	}
	fm = gfxFont->getFontMatrix();
	v = (fm[0] == 0) ? 1 : (fm[3] / fm[0]);
	m21 *= v;
	m22 *= v;
      } else if (!gfxFont->isSymbolic()) {
	// if real font is substantially narrower than substituted
	// font, reduce the font size accordingly
	if (w1 > 0.01 && w1 < 0.9 * w2) {
	  w1 /= w2;
	  if (w1 < 0.8) {
	    w1 = 0.8;
	  }
	  m11 *= w1;
	  m12 *= w1;
	  m21 *= w1;
	  m22 *= w1;
	}
      }
    }
  }

#if HAVE_FREETYPE_FREETYPE_H | HAVE_FREETYPE_H
  // try to create a new TrueType font
  if (useFreeType) {
    ttFont = new XOutputTTFont(gfxFont, m11, m12, m21, m22, display, this);
    if (ttFont->isOk()) {

      // insert in cache
      if (nTTFonts == ttFontCacheSize) {
	--nTTFonts;
	delete ttFonts[nTTFonts];
      }
      for (j = nTTFonts; j > 0; --j) {
	ttFonts[j] = ttFonts[j-1];
      }
      ttFonts[0] = ttFont;
      ++nTTFonts;

      return ttFont;
    }
    delete ttFont;
  }
#endif

#if HAVE_T1LIB_H
  // try to create a new Type 1 font
  if (useT1lib) {
    t1Font = new XOutputT1Font(gfxFont, t1FontName, m11, m12, m21, m22,
			       display, this);
    if (t1Font->isOk()) {

      // insert in cache
      if (nT1Fonts == t1FontCacheSize) {
	--nT1Fonts;
	delete t1Fonts[nT1Fonts];
      }
      for (j = nT1Fonts; j > 0; --j) {
	t1Fonts[j] = t1Fonts[j-1];
      }
      t1Fonts[0] = t1Font;
      ++nT1Fonts;

      return t1Font;
    }
    delete t1Font;
  }
#endif

  // compute size and normalized transform matrix
  size = sqrt(m21*m21 + m22*m22);
  ntm11 = m11 / size;
  ntm12 = m12 / size;
  ntm21 = m21 / size;
  ntm22 = m22 / size;

  // create a new server font
  serverFont = new XOutputServerFont(gfxFont, xFontName, xEncoding,
				     m11, m12, m21, m22,
				     size, ntm11, ntm12, ntm21, ntm22,
				     display, this);
  if (serverFont->isOk()) {

    // insert in cache
    if (nServerFonts == serverFontCacheSize) {
      --nServerFonts;
      delete serverFonts[nServerFonts];
    }
    for (j = nServerFonts; j > 0; --j)
      serverFonts[j] = serverFonts[j-1];
    serverFonts[0] = serverFont;
    ++nServerFonts;

    return serverFont;
  }
  delete serverFont;

  return NULL;
}

#if HAVE_T1LIB_H
T1FontFile *XOutputFontCache::getT1Font(GfxFont *gfxFont,
					GString *pdfBaseFont) {
  Ref id;
  T1FontFile *fontFile;
  GString *fileName;
  GString *tmpFileName;
  FILE *f;
  char *fontBuf;
  int fontLen;
  Type1CFontConverter *cvt;
  Ref embRef;
  Object refObj, strObj;
  int c;
  int i, j;

  id = gfxFont->getID();

  // check available fonts
  fontFile = NULL;
  for (i = 0; i < t1FontFilesSize && t1FontFiles[i].num >= 0; ++i) {
    if (t1FontFiles[i].num == id.num && t1FontFiles[i].gen == id.gen) {
      fontFile = t1FontFiles[i].fontFile;
    }
  }

  // create a new font file
  if (!fontFile) {

    // resize t1FontFiles if necessary
    if (i == t1FontFilesSize) {
      t1FontFiles = (XOutputT1FontFile *)
	grealloc(t1FontFiles,
		 (t1FontFilesSize + 16) * sizeof(XOutputT1FontFile));
      for (j = 0; j < 16; ++j) {
	t1FontFiles[t1FontFilesSize + j].num = -1;
      }
      t1FontFilesSize += 16;
    }

    // create the font file
    tmpFileName = NULL;
    if (!gfxFont->is16Bit() &&
	(gfxFont->getType() == fontType1 ||
	 gfxFont->getType() == fontType1C) &&
	gfxFont->getEmbeddedFontID(&embRef)) {
      if (!openTempFile(&tmpFileName, &f, "wb", NULL)) {
	error(-1, "Couldn't create temporary Type 1 font file");
	return NULL;
      }
      if (gfxFont->getType() == fontType1C) {
	if (!(fontBuf = gfxFont->readEmbFontFile(&fontLen))) {
	  fclose(f);
	  return NULL;
	}
	cvt = new Type1CFontConverter(fontBuf, fontLen, f);
	cvt->convert();
	delete cvt;
	gfree(fontBuf);
      } else {
	gfxFont->getEmbeddedFontID(&embRef);
	refObj.initRef(embRef.num, embRef.gen);
	refObj.fetch(&strObj);
	refObj.free();
	strObj.streamReset();
	while ((c = strObj.streamGetChar()) != EOF) {
	  fputc(c, f);
	}
	strObj.streamClose();
	strObj.free();
      }
      fclose(f);
      fileName = tmpFileName;
    } else if (!gfxFont->is16Bit() &&
	       gfxFont->getType() == fontType1 &&
	       gfxFont->getExtFontFile()) {
      fileName = gfxFont->getExtFontFile();
    } else {
      fileName = pdfBaseFont;
    }

    // create the t1lib font
    fontFile = new T1FontFile(t1Engine, fileName->getCString(),
			      gfxFont->getEncoding());
    if (!fontFile->isOk()) {
      error(-1, "Couldn't create t1lib font from '%s'",
	    fileName->getCString());
      delete fontFile;
      return NULL;
    }
    t1FontFiles[i].num = id.num;
    t1FontFiles[i].gen = id.gen;
    t1FontFiles[i].fontFile = fontFile;

    // remove the font file
    if (tmpFileName) {
      unlink(tmpFileName->getCString());
      delete tmpFileName;
    }
  }

  return fontFile;
}
#endif

#if HAVE_FREETYPE_FREETYPE_H | HAVE_FREETYPE_H
TTFontFile *XOutputFontCache::getTTFont(GfxFont *gfxFont) {
  Ref id;
  TTFontFile *fontFile;
  GString *fileName;
  FILE *f;
  Ref embRef;
  Object refObj, strObj;
  int c;
  int i, j;

  id = gfxFont->getID();

  // check available fonts
  fontFile = NULL;
  for (i = 0; i < ttFontFilesSize && ttFontFiles[i].num >= 0; ++i) {
    if (ttFontFiles[i].num == id.num && ttFontFiles[i].gen == id.gen) {
      fontFile = ttFontFiles[i].fontFile;
    }
  }

  // create a new font file
  if (!fontFile) {

    // resize ttFontFiles if necessary
    if (i == ttFontFilesSize) {
      ttFontFiles = (XOutputTTFontFile *)
	grealloc(ttFontFiles,
		 (ttFontFilesSize + 16) * sizeof(XOutputTTFontFile));
      for (j = 0; j < 16; ++j) {
	ttFontFiles[ttFontFilesSize + j].num = -1;
      }
      ttFontFilesSize += 16;
    }

    // create the font file
    if (!openTempFile(&fileName, &f, "wb", NULL)) {
      error(-1, "Couldn't create temporary TrueType font file");
      return NULL;
    }
    gfxFont->getEmbeddedFontID(&embRef);
    refObj.initRef(embRef.num, embRef.gen);
    refObj.fetch(&strObj);
    refObj.free();
    strObj.streamReset();
    while ((c = strObj.streamGetChar()) != EOF) {
      fputc(c, f);
    }
    strObj.streamClose();
    strObj.free();
    fclose(f);

    // create the FreeType font file
    fontFile = new TTFontFile(ttEngine, fileName->getCString());
    if (!fontFile->isOk()) {
      error(-1, "Couldn't create FreeType font from '%s'",
	    fileName->getCString());
      delete fontFile;
      return NULL;
    }
    ttFontFiles[i].num = id.num;
    ttFontFiles[i].gen = id.gen;
    ttFontFiles[i].fontFile = fontFile;

    // remove the font file
    unlink(fileName->getCString());
    delete fileName;
  }

  return fontFile;
}
#endif

//------------------------------------------------------------------------
// XOutputDev
//------------------------------------------------------------------------

XOutputDev::XOutputDev(Display *display, Pixmap pixmap, Guint depth,
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
  this->display = display;
  screenNum = DefaultScreen(display);
  this->pixmap = pixmap;
  this->depth = depth;
  this->colormap = colormap;

  // check for TrueColor visual
  trueColor = gFalse;
  if (depth == 0) {
    this->depth = DefaultDepth(display, screenNum);
    visualList = XGetVisualInfo(display, 0, &visualTempl, &nVisuals);
    for (i = 0; i < nVisuals; ++i) {
      if (visualList[i].visual == DefaultVisual(display, screenNum)) {
	if (visualList[i].c_class == TrueColor) {
	  trueColor = gTrue;
	  for (mask = visualList[i].red_mask, rShift = 0;
	       mask && !(mask & 1);
	       mask >>= 1, ++rShift) ;
	  rMul = (int)mask;
	  for (mask = visualList[i].green_mask, gShift = 0;
	       mask && !(mask & 1);
	       mask >>= 1, ++gShift) ;
	  gMul = (int)mask;
	  for (mask = visualList[i].blue_mask, bShift = 0;
	       mask && !(mask & 1);
	       mask >>= 1, ++bShift) ;
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
    userFontMap[i].t1Font = NULL;
  }
  userFontMap[n].pdfFont = NULL;

  // set up the font cache and fonts
  gfxFont = NULL;
  font = NULL;
  fontCache = new XOutputFontCache(display, this->depth);

  // empty state stack
  save = NULL;

  // create text object
  text = new TextPage(useEUCJP ? textOutASCII7 : textOutLatin1, gFalse);

  type3Warning = gFalse;
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

void XOutputDev::startDoc() {
  fontCache->startDoc(screenNum, colormap, trueColor, rMul, gMul, bMul,
		      rShift, gShift, bShift, colors, numColors);
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

void XOutputDev::drawLink(Link *link, Catalog *catalog) {
  double x1, y1, x2, y2, w;
  GfxRGB rgb;
  XPoint points[5];
  int x, y;

  link->getBorder(&x1, &y1, &x2, &y2, &w);
  if (w > 0) {
    rgb.r = 0;
    rgb.g = 0;
    rgb.b = 1;
    XSetForeground(display, strokeGC, findColor(&rgb));
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
#if 1 //~ work around a bug in XFree86 (???)
  if (dashLength > 0 && cap == CapProjecting) {
    cap = CapButt;
  }
#endif
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
  GfxRGB rgb;

  state->getFillRGB(&rgb);
  XSetForeground(display, fillGC, findColor(&rgb));
}

void XOutputDev::updateStrokeColor(GfxState *state) {
  GfxRGB rgb;

  state->getStrokeRGB(&rgb);
  XSetForeground(display, strokeGC, findColor(&rgb));
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
    font->updateGC(fillGC);
    font->updateGC(strokeGC);
  }

  // look for Type 3 font
  if (!type3Warning && gfxFont->getType() == fontType3) {
    error(-1, "This document uses Type 3 fonts - some text may not be correctly displayed");
    type3Warning = gTrue;
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
  double x1, y1;

  text->addChar(state, x, y, dx, dy, c);

  if (!font)
    return;

  // check for invisible text -- this is used by Acrobat Capture
  if ((state->getRender() & 3) == 3)
    return;

  state->transform(x, y, &x1, &y1);

  font->drawChar(state, pixmap, pixmapW, pixmapH,
		 (state->getRender() & 1) ? strokeGC : fillGC,
		 xoutRound(x1), xoutRound(y1), c);
}

void XOutputDev::drawChar16(GfxState *state, double x, double y,
			    double dx, double dy, int c) {
  int c1;
  XChar2b c2[4];
  double x1, y1;
#if JAPANESE_SUPPORT | CHINESE_GB_SUPPORT | CHINESE_CNS_SUPPORT
  int t1, t2;
#endif
#if JAPANESE_SUPPORT
  double x2;
  char *p;
  int n, i;
#endif

  if (gfxFont) {
    text->addChar16(state, x, y, dx, dy, c, gfxFont->getCharSet16());
  }

  //~ assumes font is an XOutputServerFont

  if (!font)
    return;

  // check for invisible text -- this is used by Acrobat Capture
  if ((state->getRender() & 3) == 3)
    return;

  // handle origin offset for vertical fonts
  if (gfxFont->getWMode16() == 1) {
    x -= gfxFont->getOriginX16(c) * state->getFontSize();
    y -= gfxFont->getOriginY16(c) * state->getFontSize();
  }

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

  // convert Adobe-GB1-2 to GB 2312-80
  case font16AdobeGB12:
#if CHINESE_GB_SUPPORT
    if (c <= 939) {
      c1 = gb12Map[c];
    } else if (c <= 4605) {
      t1 = (c - 940) / 94;
      t2 = (c - 940) % 94;
      c1 = 0x3021 + (t1 << 8) + t2;
    } else if (c <= 4694) {
      c1 = c - 4606 + 0x5721;
    } else if (c <= 7702) {
      t1 = (c - 4695) / 94;
      t2 = (c - 4695) % 94;
      c1 = 0x5821 + (t1 << 8) + t2;
    } else if (c == 7716) {
      c1 = 0x2121;
    }
#if 1 //~
    if (c1 == 0) {
      error(-1, "Unsupported Adobe-GB1-2 character: %d", c);
    }
#endif
#endif // CHINESE_GB_SUPPORT
    break;

  // convert Adobe-CNS1-3 to Big5
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

  if (c1 > 0) {
    c2[0].byte1 = c1 >> 8;
    c2[0].byte2 = c1 & 0xff;
    XDrawString16(display, pixmap,
		  (state->getRender() & 1) ? strokeGC : fillGC,
		  xoutRound(x1), xoutRound(y1), c2, 1);
  }
}

inline Gulong XOutputDev::findColor(GfxRGB *x, GfxRGB *err) {
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

Gulong XOutputDev::findColor(GfxRGB *rgb) {
  int r, g, b;
  double gray;
  Gulong pixel;

  if (trueColor) {
    r = xoutRound(rgb->r * rMul);
    g = xoutRound(rgb->g * gMul);
    b = xoutRound(rgb->b * bMul);
    pixel = ((Gulong)r << rShift) +
            ((Gulong)g << gShift) +
            ((Gulong)b << bShift);
  } else if (numColors == 1) {
    gray = 0.299 * rgb->r + 0.587 * rgb->g + 0.114 * rgb->b;
    if (gray < 0.5)
      pixel = colors[0];
    else
      pixel = colors[1];
  } else {
    r = xoutRound(rgb->r * (numColors - 1));
    g = xoutRound(rgb->g * (numColors - 1));
    b = xoutRound(rgb->b * (numColors - 1));
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

void XOutputDev::drawImageMask(GfxState *state, Object *ref, Stream *str,
			       int width, int height, GBool invert,
			       GBool inlineImg) {
  ImageStream *imgStr;
  XImage *image;
  double xt, yt;
  int ulx, uly, llx, lly, urx, ury, lrx, lry;
  int hx, hy;
  int bx0, by0, bx1, by1, bw, bh;
  int cx0, cy0, cx1, cy1, cw, ch;
  int dx, dy;
  int dvx, dvdx, dvpx, dvqx, dvdx2, dvtx;
  int dvy, dvdy, dvpy, dvqy, dvdy2, dvty;
  int dhx, dhdx, dhpx, dhqx, dhdx2, dhtx, dhtx0;
  int dhy, dhdy, dhpy, dhqy, dhdy2, dhty, dhty0;
  int ivy, ivdy, ivpy, ivqy, ivty;
  int ihx, ihdx, ihpx, ihqx, ihtx;
  int vn, vi, hn, hi;
  int bufy;
  GfxRGB rgb;
  Guchar *pixBuf;
  int imgPix;
  double alpha;
  XColor xcolor;
  Gulong lastPixel;
  GfxRGB rgb2;
  double r0, g0, b0, r1, g1, b1;
  Gulong pix;
  Guchar *p;
  int n, m, i, j;

  // corners in device space
  state->transform(0, 0, &xt, &yt);
  llx = xoutRound(xt);
  lly = xoutRound(yt);
  state->transform(0, 1, &xt, &yt);
  ulx = xoutRound(xt);
  uly = xoutRound(yt);
  state->transform(1, 0, &xt, &yt);
  lrx = xoutRound(xt);
  lry = xoutRound(yt);
  state->transform(1, 1, &xt, &yt);
  urx = xoutRound(xt);
  ury = xoutRound(yt);

  // horizontal traversal
  hx = urx - ulx;
  if (abs(lrx - llx) < abs(hx)) {
    hx = lrx - llx;
  }
  hy = ury - uly;
  if (abs(lry - lly) < abs(hy)) {
    hy = lry - lly;
  }

  // bounding box:
  //   (bx0, by0) = upper-left corner
  //   (bx1, by1) = lower-right corner
  //   (bw, bh) = size
  bx0 = bx1 = ulx;
  if (llx < bx0) {
    bx0 = llx;
  } else if (llx > bx1) {
    bx1 = llx;
  }
  if (urx < bx0) {
    bx0 = urx;
  } else if (urx > bx1) {
    bx1 = urx;
  }
  if (lrx < bx0) {
    bx0 = lrx;
  } else if (lrx > bx1) {
    bx1 = lrx;
  }
  by0 = by1 = uly;
  if (lly < by0) {
    by0 = lly;
  } else if (lly > by1) {
    by1 = lly;
  }
  if (ury < by0) {
    by0 = ury;
  } else if (ury > by1) {
    by1 = ury;
  }
  if (lry < by0) {
    by0 = lry;
  } else if (lry > by1) {
    by1 = lry;
  }
  bw = bx1 - bx0 + 1;
  bh = by1 - by0 + 1;

  // bounding box clipped to pixmap, i.e., "valid" rectangle:
  //   (cx0, cy0) = upper-left corner of valid rectangle in page pixmap
  //   (cx1, cy1) = upper-left corner of valid rectangle in image pixmap
  //   (cw, ch) = size of valid rectangle
  if (bx1 >= pixmapW) {
    cw = pixmapW - bx0;
  } else {
    cw = bw;
  }
  if (bx0 < 0) {
    cx0 = 0;
    cx1 = -bx0;
    cw += bx0;
  } else {
    cx0 = bx0;
    cx1 = 0;
  }
  if (by1 >= pixmapH) {
    ch = pixmapH - by0;
  } else {
    ch = bh;
  }
  if (by0 < 0) {
    cy0 = 0;
    cy1 = -by0;
    ch += by0;
  } else {
    cy0 = by0;
    cy1 = 0;
  }

  // check for tiny (zero width or height) images
  // and off-page images
  if (cw <= 0 || ch <= 0) {
    if (inlineImg) {
      j = height * ((width + 7) / 8);
      str->reset();
      for (i = 0; i < j; ++i) {
	str->getChar();
      }
    }
    return;
  }

  // Bresenham parameters for vertical traversal
  // (device coordinates and image coordinates)
  dx = llx - ulx;
  dy = lly - uly;
  if (abs(dx) > abs(dy)) {
    vn = abs(dx);
    dvdx = dx > 0 ? 1 : -1;
    dvpx = 0;
    dvdx2 = 0;
    dvdy = 0;
    dvpy = abs(dy);
    dvdy2 = dy > 0 ? 1 : -1;
  } else {
    vn = abs(dy);
    dvdx = 0;
    dvpx = abs(dx);
    dvdx2 = dx > 0 ? 1 : -1;
    dvdy = dy > 0 ? 1 : -1;
    dvpy = 0;
    dvdy2 = 0;
  }
  dvqx = dvqy = vn;
  ivqy = vn + 1;
  ivdy = height / ivqy;
  ivpy = height % ivqy;

  // Bresenham parameters for horizontal traversal
  // (device coordinates and image coordinates)
  if (abs(hx) > abs(hy)) {
    hn = abs(hx);
    dhdx = hx > 0 ? 1 : -1;
    dhpx = 0;
    dhdx2 = 0;
    dhdy = 0;
    dhpy = abs(hy);
    dhdy2 = hy > 0 ? 1 : -1;
  } else {
    hn = abs(hy);
    dhdx = 0;
    dhpx = abs(hx);
    dhdx2 = hx > 0 ? 1 : -1;
    dhdy = hy > 0 ? 1 : -1;
    dhpy = 0;
    dhdy2 = 0;
  }
  dhqx = dhqy = hn;
  ihqx = hn + 1;
  ihdx = width / ihqx;
  ihpx = width % ihqx;

  // allocate pixel buffer
  n = ivdy + (ivpy > 0 ? 1 : 0);
  pixBuf = (Guchar *)gmalloc(n * width * sizeof(Guchar));

  // allocate XImage and read from page pixmap
  image = XCreateImage(display, DefaultVisual(display, screenNum),
		       depth, ZPixmap, 0, NULL, bw, bh, 8, 0);
  image->data = (char *)gmalloc(bh * image->bytes_per_line);
  XGetSubImage(display, pixmap, cx0, cy0, cw, ch, (1 << depth) - 1, ZPixmap,
	       image, cx1, cy1);

  // get mask color
  state->getFillRGB(&rgb);
  r0 = rgb.r;
  g0 = rgb.g;
  b0 = rgb.b;

  // initialize background color
  // (the specific pixel value doesn't matter here, as long as
  // r1,g1,b1 correspond correctly to lastPixel)
  xcolor.pixel = lastPixel = 0;
  XQueryColor(display, colormap, &xcolor);
  r1 = (double)xcolor.red / 65535.0;
  g1 = (double)xcolor.green / 65535.0;
  b1 = (double)xcolor.blue / 65535.0;

  // initialize the image stream
  imgStr = new ImageStream(str, width, 1, 1);
  imgStr->reset();

  // traverse left edge of image
  dvx = ulx;
  dvtx = 0;
  dvy = uly;
  dvty = 0;
  ivy = 0;
  ivty = 0;
  dhtx0 = 0;
  dhty0 = 0;
  n = 0;
  bufy = -1;
  for (vi = 0; vi <= vn; ++vi) {

    // read row(s) from image
    if (ivy > bufy) {
      if (ivdy == 0) {
	n = 1;
      } else {
	n = ivdy + (ivty + ivpy >= ivqy ? 1 : 0);
      }
      p = pixBuf;
      for (i = 0; i < n; ++i) {
	for (j = 0; j < width; ++j) {
	  imgStr->getPixel(p);
	  if (invert) {
	    *p ^= 1;
	  }
	  ++p;
	}
      }
      bufy = ivy;
    }

    // traverse a horizontal stripe
    dhx = 0;
    dhy = 0;
    dhtx = dhtx0;
    dhty = dhty0;
    ihx = 0;
    ihtx = 0;
    for (hi = 0; hi <= hn; ++hi) {

      // compute filtered pixel value
      imgPix = 0;
      if (ihdx == 0) {
	m = 1;
      } else {
	m = ihdx + (ihtx + ihpx >= ihqx ? 1 : 0);
      }
      p = pixBuf + ihx * sizeof(Guchar);
      for (i = 0; i < n; ++i) {
	for (j = 0; j < m; ++j) {
	  imgPix += *p++;
	}
	p += width - m;
      }

      // blend image pixel with background
      alpha = (double)imgPix / (double)(n * m);
      xcolor.pixel = XGetPixel(image, dvx + dhx - bx0, dvy + dhy - by0);
      if (xcolor.pixel != lastPixel) {
	XQueryColor(display, colormap, &xcolor);
	r1 = (double)xcolor.red / 65535.0;
	g1 = (double)xcolor.green / 65535.0;
	b1 = (double)xcolor.blue / 65535.0;
	lastPixel = xcolor.pixel;
      }
      rgb2.r = r0 * (1 - alpha) + r1 * alpha;
      rgb2.g = g0 * (1 - alpha) + g1 * alpha;
      rgb2.b = b0 * (1 - alpha) + b1 * alpha;
      pix = findColor(&rgb2);

      // set pixel
      XPutPixel(image, dvx + dhx - bx0, dvy + dhy - by0, pix);

      // Bresenham increment (horizontal stripe)
      dhx += dhdx;
      dhtx += dhpx;
      if (dhtx >= dhqx) {
	dhx += dhdx2;
	dhtx -= dhqx;
      }
      dhy += dhdy;
      dhty += dhpy;
      if (dhty >= dhqy) {
	dhy += dhdy2;
	dhty -= dhqy;
      }
      ihx += ihdx;
      ihtx += ihpx;
      if (ihtx >= ihqx) {
	++ihx;
	ihtx -= ihqx;
      }
    }

    // Bresenham increment (left edge)
    dvx += dvdx;
    dvtx += dvpx;
    dhty0 += dvdx * dhdx * dhpy;
    if (dvtx >= dvqx) {
      dvx += dvdx2;
      dvtx -= dvqx;
      dhty0 += dvdx2 * dhdx * dhpy;
    }
    dvy += dvdy;
    dvty += dvpy;
    dhtx0 += dvdy * dhdy * dhpx;
    if (dvty >= dvqy) {
      dvy += dvdy2;
      dvty -= dvqy;
      dhtx0 += dvdy2 * dhdy * dhpx;
    }
    ivy += ivdy;
    ivty += ivpy;
    if (ivty >= ivqy) {
      ++ivy;
      ivty -= ivqy;
    }
    if (dhtx0 >= dhqy) {
      dhtx0 -= dhqx;
    } else if (dhtx0 < 0) {
      dhtx0 += dhqx;
    }
    if (dhty0 >= dhqx) {
      dhty0 -= dhqy;
    } else if (dhty0 < 0) {
      dhty0 += dhqy;
    }
  }
  
  // blit the image into the pixmap
  XPutImage(display, pixmap, fillGC, image, cx1, cy1, cx0, cy0, cw, ch);

  // free memory
  delete imgStr;
  gfree(pixBuf);
  gfree(image->data);
  image->data = NULL;
  XDestroyImage(image);
}

void XOutputDev::drawImage(GfxState *state, Object *ref, Stream *str,
			   int width, int height, GfxImageColorMap *colorMap,
			   GBool inlineImg) {
  ImageStream *imgStr;
  XImage *image;
  int nComps, nVals, nBits;
  GBool dither;
  double xt, yt;
  int ulx, uly, llx, lly, urx, ury, lrx, lry;
  int hx, hy;
  int bx0, by0, bx1, by1, bw, bh;
  int cx0, cy0, cx1, cy1, cw, ch;
  int dx, dy;
  int dvx, dvdx, dvpx, dvqx, dvdx2, dvtx;
  int dvy, dvdy, dvpy, dvqy, dvdy2, dvty;
  int dhx, dhdx, dhpx, dhqx, dhdx2, dhtx, dhtx0;
  int dhy, dhdy, dhpy, dhqy, dhdy2, dhty, dhty0;
  int ivy, ivdy, ivpy, ivqy, ivty;
  int ihx, ihdx, ihpx, ihqx, ihtx;
  int vn, vi, hn, hi;
  int bufy;
  GfxRGB *pixBuf;
  Guchar pixBuf2[gfxColorMaxComps];
  GfxRGB color2, err, errRight;
  GfxRGB *errDown;
  double r0, g0, b0;
  Gulong pix;
  GfxRGB *p;
  int n, m, i, j;

  // image parameters
  nComps = colorMap->getNumPixelComps();
  nVals = width * nComps;
  nBits = colorMap->getBits();
  dither = nComps > 1 || nBits > 1;

  // corners in device space
  state->transform(0, 0, &xt, &yt);
  llx = xoutRound(xt);
  lly = xoutRound(yt);
  state->transform(0, 1, &xt, &yt);
  ulx = xoutRound(xt);
  uly = xoutRound(yt);
  state->transform(1, 0, &xt, &yt);
  lrx = xoutRound(xt);
  lry = xoutRound(yt);
  state->transform(1, 1, &xt, &yt);
  urx = xoutRound(xt);
  ury = xoutRound(yt);

  // horizontal traversal
  hx = urx - ulx;
  if (abs(lrx - llx) < abs(hx)) {
    hx = lrx - llx;
  }
  hy = ury - uly;
  if (abs(lry - lly) < abs(hy)) {
    hy = lry - lly;
  }

  // bounding box:
  //   (bx0, by0) = upper-left corner
  //   (bx1, by1) = lower-right corner
  //   (bw, bh) = size
  bx0 = bx1 = ulx;
  if (llx < bx0) {
    bx0 = llx;
  } else if (llx > bx1) {
    bx1 = llx;
  }
  if (urx < bx0) {
    bx0 = urx;
  } else if (urx > bx1) {
    bx1 = urx;
  }
  if (lrx < bx0) {
    bx0 = lrx;
  } else if (lrx > bx1) {
    bx1 = lrx;
  }
  by0 = by1 = uly;
  if (lly < by0) {
    by0 = lly;
  } else if (lly > by1) {
    by1 = lly;
  }
  if (ury < by0) {
    by0 = ury;
  } else if (ury > by1) {
    by1 = ury;
  }
  if (lry < by0) {
    by0 = lry;
  } else if (lry > by1) {
    by1 = lry;
  }
  bw = bx1 - bx0 + 1;
  bh = by1 - by0 + 1;

  // Bounding box clipped to pixmap, i.e., "valid" rectangle:
  //   (cx0, cy0) = upper-left corner of valid rectangle in Pixmap
  //   (cx1, cy1) = upper-left corner of valid rectangle in XImage
  //   (cw, ch) = size of valid rectangle
  // These values will be used to transfer the XImage from/to the
  // Pixmap.
  if (bx1 >= pixmapW) {
    cw = pixmapW - bx0;
  } else {
    cw = bw;
  }
  if (bx0 < 0) {
    cx0 = 0;
    cx1 = -bx0;
    cw += bx0;
  } else {
    cx0 = bx0;
    cx1 = 0;
  }
  if (by1 >= pixmapH) {
    ch = pixmapH - by0;
  } else {
    ch = bh;
  }
  if (by0 < 0) {
    cy0 = 0;
    cy1 = -by0;
    ch += by0;
  } else {
    cy0 = by0;
    cy1 = 0;
  }

  // check for tiny (zero width or height) images
  // and off-page images
  if (cw <= 0 || ch <= 0) {
    if (inlineImg) {
      str->reset();
      j = height * ((nVals * nBits + 7) / 8);
      for (i = 0; i < j; ++i)
	str->getChar();
    }
    return;
  }

  // Bresenham parameters for vertical traversal
  // (device coordinates and image coordinates)
  dx = llx - ulx;
  dy = lly - uly;
  if (abs(dx) > abs(dy)) {
    vn = abs(dx);
    dvdx = dx > 0 ? 1 : -1;
    dvpx = 0;
    dvdx2 = 0;
    dvdy = 0;
    dvpy = abs(dy);
    dvdy2 = dy > 0 ? 1 : -1;
  } else {
    vn = abs(dy);
    dvdx = 0;
    dvpx = abs(dx);
    dvdx2 = dx > 0 ? 1 : -1;
    dvdy = dy > 0 ? 1 : -1;
    dvpy = 0;
    dvdy2 = 0;
  }
  dvqx = dvqy = vn;
  ivqy = vn + 1;
  ivdy = height / ivqy;
  ivpy = height % ivqy;

  // Bresenham parameters for horizontal traversal
  // (device coordinates and image coordinates)
  if (abs(hx) > abs(hy)) {
    hn = abs(hx);
    dhdx = hx > 0 ? 1 : -1;
    dhpx = 0;
    dhdx2 = 0;
    dhdy = 0;
    dhpy = abs(hy);
    dhdy2 = hy > 0 ? 1 : -1;
  } else {
    hn = abs(hy);
    dhdx = 0;
    dhpx = abs(hx);
    dhdx2 = hx > 0 ? 1 : -1;
    dhdy = hy > 0 ? 1 : -1;
    dhpy = 0;
    dhdy2 = 0;
  }
  dhqx = dhqy = hn;
  ihqx = hn + 1;
  ihdx = width / ihqx;
  ihpx = width % ihqx;

  // allocate pixel buffer
  n = ivdy + (ivpy > 0 ? 1 : 0);
  pixBuf = (GfxRGB *)gmalloc(n * width * sizeof(GfxRGB));

  // allocate XImage
  image = XCreateImage(display, DefaultVisual(display, screenNum),
		       depth, ZPixmap, 0, NULL, bw, bh, 8, 0);
  image->data = (char *)gmalloc(bh * image->bytes_per_line);

  // if the transform is anything other than a 0/90/180/270 degree
  // rotation/flip, read the backgound pixmap to fill in the corners
  if (!((ulx == llx && uly == ury) ||
	(uly == lly && ulx == urx))) {
    XGetSubImage(display, pixmap, cx0, cy0, cw, ch, (1 << depth) - 1, ZPixmap,
		 image, cx1, cy1);
  }

  // allocate error diffusion accumulators
  if (dither) {
    errDown = (GfxRGB *)gmalloc(bw * sizeof(GfxRGB));
    for (j = 0; j < bw; ++j) {
      errDown[j].r = errDown[j].g = errDown[j].b = 0;
    }
  } else {
    errDown = NULL;
  }

  // initialize the image stream
  imgStr = new ImageStream(str, width, nComps, nBits);
  imgStr->reset();

  // traverse left edge of image
  dvx = ulx;
  dvtx = 0;
  dvy = uly;
  dvty = 0;
  ivy = 0;
  ivty = 0;
  dhtx0 = 0;
  dhty0 = 0;
  n = 0;
  bufy = -1;
  for (vi = 0; vi <= vn; ++vi) {

    // read row(s) from image
    if (ivy > bufy) {
      if (ivdy == 0) {
	n = 1;
      } else {
	n = ivdy + (ivty + ivpy >= ivqy ? 1 : 0);
      }
      p = pixBuf;
      for (i = 0; i < n; ++i) {
	for (j = 0; j < width; ++j) {
	  imgStr->getPixel(pixBuf2);
	  colorMap->getRGB(pixBuf2, p);
	  ++p;
	}
      }
      bufy = ivy;
    }

    // clear error accumulator
    errRight.r = errRight.g = errRight.b = 0;

    // traverse a horizontal stripe
    dhx = 0;
    dhy = 0;
    dhtx = dhtx0;
    dhty = dhty0;
    ihx = 0;
    ihtx = 0;
    for (hi = 0; hi <= hn; ++hi) {

      // compute filtered pixel value
      if (ihdx == 0) {
	m = 1;
      } else {
	m = ihdx + (ihtx + ihpx >= ihqx ? 1 : 0);
      }
      p = pixBuf + ihx * sizeof(Guchar);
      r0 = g0 = b0 = 0;
      for (i = 0; i < n; ++i) {
	for (j = 0; j < m; ++j) {
	  r0 += p->r;
	  g0 += p->g;
	  b0 += p->b;
	  ++p;
	}
	p += width - m;
      }
      r0 /= n * m;
      g0 /= n * m;
      b0 /= n * m;

      // compute pixel
      if (dither) {
	color2.r = r0 + errRight.r + errDown[dvx + dhx - bx0].r;
	if (color2.r > 1) {
	  color2.r = 1;
	} else if (color2.r < 0) {
	  color2.r = 0;
	}
	color2.g = g0 + errRight.g + errDown[dvx + dhx - bx0].g;
	if (color2.g > 1) {
	  color2.g = 1;
	} else if (color2.g < 0) {
	  color2.g = 0;
	}
	color2.b = b0 + errRight.b + errDown[dvx + dhx - bx0].b;
	if (color2.b > 1) {
	  color2.b = 1;
	} else if (color2.b < 0) {
	  color2.b = 0;
	}
	pix = findColor(&color2, &err);
	errRight.r = errDown[dvx + dhx - bx0].r = err.r / 2;
	errRight.g = errDown[dvx + dhx - bx0].g = err.g / 2;
	errRight.b = errDown[dvx + dhx - bx0].b = err.b / 2;
      } else {
	color2.r = r0;
	color2.g = g0;
	color2.b = b0;
	pix = findColor(&color2, &err);
      }

      // set pixel
      XPutPixel(image, dvx + dhx - bx0, dvy + dhy - by0, pix);

      // Bresenham increment (horizontal stripe)
      dhx += dhdx;
      dhtx += dhpx;
      if (dhtx >= dhqx) {
	dhx += dhdx2;
	dhtx -= dhqx;
      }
      dhy += dhdy;
      dhty += dhpy;
      if (dhty >= dhqy) {
	dhy += dhdy2;
	dhty -= dhqy;
      }
      ihx += ihdx;
      ihtx += ihpx;
      if (ihtx >= ihqx) {
	++ihx;
	ihtx -= ihqx;
      }
    }

    // Bresenham increment (left edge)
    dvx += dvdx;
    dvtx += dvpx;
    dhty0 += dvdx * dhdx * dhpy;
    if (dvtx >= dvqx) {
      dvx += dvdx2;
      dvtx -= dvqx;
      dhty0 += dvdx2 * dhdx * dhpy;
    }
    dvy += dvdy;
    dvty += dvpy;
    dhtx0 += dvdy * dhdy * dhpx;
    if (dvty >= dvqy) {
      dvy += dvdy2;
      dvty -= dvqy;
      dhtx0 += dvdy2 * dhdy * dhpx;
    }
    ivy += ivdy;
    ivty += ivpy;
    if (ivty >= ivqy) {
      ++ivy;
      ivty -= ivqy;
    }
    if (dhtx0 >= dhqy) {
      dhtx0 -= dhqx;
    } else if (dhtx0 < 0) {
      dhtx0 += dhqx;
    }
    if (dhty0 >= dhqx) {
      dhty0 -= dhqy;
    } else if (dhty0 < 0) {
      dhty0 += dhqy;
    }
  }
  
  // blit the image into the pixmap
  XPutImage(display, pixmap, fillGC, image, cx1, cy1, cx0, cy0, cw, ch);

  // free memory
  delete imgStr;
  gfree(pixBuf);
  gfree(image->data);
  image->data = NULL;
  XDestroyImage(image);
  gfree(errDown);
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
