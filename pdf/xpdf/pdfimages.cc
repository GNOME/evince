//========================================================================
//
// pdfimages.cc
//
// Copyright 1998 Derek B. Noonburg
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
#include "ImageOutputDev.h"
#include "Params.h"
#include "Error.h"
#include "config.h"

static int firstPage = 1;
static int lastPage = 0;
static GBool dumpJPEG = gFalse;
static char userPassword[33] = "";
static GBool printVersion = gFalse;
static GBool printHelp = gFalse;

static ArgDesc argDesc[] = {
  {"-f",      argInt,      &firstPage,     0,
   "first page to convert"},
  {"-l",      argInt,      &lastPage,      0,
   "last page to convert"},
  {"-j",      argFlag,     &dumpJPEG,      0,
   "write JPEG images as JPEG files"},
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
  char *imgRoot;
  GString *userPW;
  ImageOutputDev *imgOut;
  GBool ok;

  // parse args
  ok = parseArgs(argDesc, &argc, argv);
  if (!ok || argc != 3 || printVersion || printHelp) {
    fprintf(stderr, "pdfimages version %s\n", xpdfVersion);
    fprintf(stderr, "%s\n", xpdfCopyright);
    if (!printVersion) {
      printUsage("pdfimages", "<PDF-file> <image-root>", argDesc);
    }
    exit(1);
  }
  fileName = new GString(argv[1]);
  imgRoot = argv[2];

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
    error(-1, "Copying of images from this document is not allowed.");
    goto err;
  }

  // get page range
  if (firstPage < 1)
    firstPage = 1;
  if (lastPage < 1 || lastPage > doc->getNumPages())
    lastPage = doc->getNumPages();

  // write image files
  imgOut = new ImageOutputDev(imgRoot, dumpJPEG);
  if (imgOut->isOk())
    doc->displayPages(imgOut, firstPage, lastPage, 72, 0, gFalse);
  delete imgOut;

  // clean up
 err:
  delete doc;
  freeParams();

  // check for memory leaks
  Object::memCheck(stderr);
  gMemReport(stderr);

  return 0;
}
