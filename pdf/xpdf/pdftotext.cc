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
#if JAPANESE_SUPPORT
static GBool useEUCJP = gFalse;
#endif
static GBool rawOrder = gFalse;
GBool printCommands = gFalse;
static GBool printHelp = gFalse;

static ArgDesc argDesc[] = {
  {"-f",      argInt,      &firstPage,     0,
   "first page to convert"},
  {"-l",      argInt,      &lastPage,      0,
   "last page to convert"},
  {"-ascii7", argFlag,     &useASCII7,     0,
   "convert to 7-bit ASCII (default is 8-bit ISO Latin-1)"},
#if JAPANESE_SUPPORT
  {"-eucjp",  argFlag,     &useEUCJP,      0,
   "convert Japanese text to EUC-JP"},
#endif
  {"-raw",    argFlag,     &rawOrder,      0,
   "keep strings in content stream order"},
  {"-q",      argFlag,     &errQuiet,      0,
   "don't print any messages or errors"},
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
  TextOutputDev *textOut;
  GBool ok;
  char *p;

  // parse args
  ok = parseArgs(argDesc, &argc, argv);
  if (!ok || argc < 2 || argc > 3 || printHelp) {
    fprintf(stderr, "pdftotext version %s\n", xpdfVersion);
    fprintf(stderr, "%s\n", xpdfCopyright);
    printUsage("pdftotext", "<PDF-file> [<text-file>]", argDesc);
    exit(1);
  }
  fileName = new GString(argv[1]);

  // init error file
  errorInit();

  // read config file
  initParams(xpdfConfigFile);

  // open PDF file
  xref = NULL;
  doc = new PDFDoc(fileName);
  if (!doc->isOk()) {
    goto err1;
  }

  // check for copy permission
  if (!doc->okToCopy()) {
    error(-1, "Copying of text from this document is not allowed.");
    goto err2;
  }

  // construct text file name
  if (argc == 3) {
    textFileName = new GString(argv[2]);
  } else {
    p = fileName->getCString() + fileName->getLength() - 4;
    if (!strcmp(p, ".pdf") || !strcmp(p, ".PDF"))
      textFileName = new GString(fileName->getCString(),
				 fileName->getLength() - 4);
    else
      textFileName = fileName->copy();
    textFileName->append(".txt");
  }

  // get page range
  if (firstPage < 1)
    firstPage = 1;
  if (lastPage < 1 || lastPage > doc->getNumPages())
    lastPage = doc->getNumPages();

  // write text file
#if JAPANESE_SUPPORT
  useASCII7 |= useEUCJP;
#endif
  textOut = new TextOutputDev(textFileName->getCString(), useASCII7, rawOrder);
  if (textOut->isOk())
    doc->displayPages(textOut, firstPage, lastPage, 72, 0);
  delete textOut;

  // clean up
  delete textFileName;
 err2:
  delete doc;
 err1:
  freeParams();

  // check for memory leaks
  Object::memCheck(stderr);
  gMemReport(stderr);

  return 0;
}
