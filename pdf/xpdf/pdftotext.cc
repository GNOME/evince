//========================================================================
//
// pdftotext.cc
//
// Copyright 1997 Derek B. Noonburg
//
//========================================================================

#include <stdio.h>
#include <stdlib.h>
#include <stddef.h>
#include <string.h>
#include "parseargs.h"
#include "GString.h"
#include "gmem.h"
#include "Object.h"
#include "Stream.h"
#include "Array.h"
#include "Dict.h"
#include "XRef.h"
#include "Catalog.h"
#include "Page.h"
#include "PDFDoc.h"
#include "TextOutputDev.h"
#include "Params.h"
#include "Error.h"
#include "config.h"

static int firstPage = 1;
static int lastPage = 0;
static GBool useASCII7 = gFalse;
static GBool useLatin2 = gFalse;
static GBool useLatin5 = gFalse;
#if JAPANESE_SUPPORT
static GBool useEUCJP = gFalse;
#endif
static GBool rawOrder = gFalse;
static char userPassword[33] = "";
static GBool printVersion = gFalse;
static GBool printHelp = gFalse;

static ArgDesc argDesc[] = {
  {"-f",      argInt,      &firstPage,     0,
   "first page to convert"},
  {"-l",      argInt,      &lastPage,      0,
   "last page to convert"},
  {"-ascii7", argFlag,     &useASCII7,     0,
   "convert to 7-bit ASCII (default is 8-bit ISO Latin-1)"},
  {"-latin2", argFlag,     &useLatin2,     0,
   "convert to ISO Latin-2 character set"},
  {"-latin5", argFlag,     &useLatin5,     0,
   "convert to ISO Latin-5 character set"},
#if JAPANESE_SUPPORT
  {"-eucjp",  argFlag,     &useEUCJP,      0,
   "convert Japanese text to EUC-JP"},
#endif
  {"-raw",    argFlag,     &rawOrder,      0,
   "keep strings in content stream order"},
  {"-upw",    argString,   userPassword,   sizeof(userPassword),
   "user password (for encrypted files)"},
  {"-q",      argFlag,     &errQuiet,      0,
   "don't print any messages or errors"},
  {"-v",      argFlag,     &printVersion,  0,
   "print copyright and version info"},
  {"-h",      argFlag,     &printHelp,     0,
   "print usage information"},
  {"-help",   argFlag,     &printHelp,     0,
   "print usage information"},
  {NULL}
};

int main(int argc, char *argv[]) {
  PDFDoc *doc;
  GString *fileName;
  GString *textFileName;
  GString *userPW;
  TextOutputDev *textOut;
  TextOutputCharSet charSet;
  GBool ok;
  char *p;

  // parse args
  ok = parseArgs(argDesc, &argc, argv);
  if (!ok || argc < 2 || argc > 3 || printVersion || printHelp) {
    fprintf(stderr, "pdftotext version %s\n", xpdfVersion);
    fprintf(stderr, "%s\n", xpdfCopyright);
    if (!printVersion) {
      printUsage("pdftotext", "<PDF-file> [<text-file>]", argDesc);
    }
    exit(1);
  }
  fileName = new GString(argv[1]);

  // init error file
  errorInit();

  // read config file
  initParams(xpdfConfigFile);

  // open PDF file
  xref = NULL;
  if (userPassword[0]) {
    userPW = new GString(userPassword);
  } else {
    userPW = NULL;
  }
  doc = new PDFDoc(fileName, userPW);
  if (userPW) {
    delete userPW;
  }
  if (!doc->isOk()) {
    goto err;
  }

  // check for copy permission
  if (!doc->okToCopy()) {
    error(-1, "Copying of text from this document is not allowed.");
    goto err;
  }

  // construct text file name
  if (argc == 3) {
    textFileName = new GString(argv[2]);
  } else {
    p = fileName->getCString() + fileName->getLength() - 4;
    if (!strcmp(p, ".pdf") || !strcmp(p, ".PDF")) {
      textFileName = new GString(fileName->getCString(),
				 fileName->getLength() - 4);
    } else {
      textFileName = fileName->copy();
    }
    textFileName->append(".txt");
  }

  // get page range
  if (firstPage < 1) {
    firstPage = 1;
  }
  if (lastPage < 1 || lastPage > doc->getNumPages()) {
    lastPage = doc->getNumPages();
  }

  // write text file
#if JAPANESE_SUPPORT
  useASCII7 |= useEUCJP;
#endif
  charSet = textOutLatin1;
  if (useASCII7) {
    charSet = textOutASCII7;
  } else if (useLatin2) {
    charSet = textOutLatin2;
  } else if (useLatin5) {
    charSet = textOutLatin5;
  }
  textOut = new TextOutputDev(textFileName->getCString(), charSet, rawOrder);
  if (textOut->isOk()) {
    doc->displayPages(textOut, firstPage, lastPage, 72, 0, gFalse);
  }
  delete textOut;

  // clean up
  delete textFileName;
 err:
  delete doc;
  freeParams();

  // check for memory leaks
  Object::memCheck(stderr);
  gMemReport(stderr);

  return 0;
}
