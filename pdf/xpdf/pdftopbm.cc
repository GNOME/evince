//========================================================================
//
// pdftopbm.cc
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
#include "PBMOutputDev.h"
#include "Params.h"
#include "Error.h"
#include "config.h"

static int firstPage = 1;
static int lastPage = 0;
static int resolution = 150;
static char userPassword[33] = "";
static GBool printVersion = gFalse;
static GBool printHelp = gFalse;

static ArgDesc argDesc[] = {
  {"-f",      argInt,      &firstPage,     0,
   "first page to print"},
  {"-l",      argInt,      &lastPage,      0,
   "last page to print"},
  {"-r",      argInt,      &resolution,    0,
   "resolution, in DPI (default is 150)"},
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
  char *pbmRoot;
  GString *userPW;
  PBMOutputDev *pbmOut;
  GBool ok;

  // parse args
  ok = parseArgs(argDesc, &argc, argv);
  if (!ok || argc != 3 || printVersion || printHelp) {
    fprintf(stderr, "pdftopbm version %s\n", xpdfVersion);
    fprintf(stderr, "%s\n", xpdfCopyright);
    if (!printVersion) {
      printUsage("pdftopbm", "<PDF-file> <PBM-root>", argDesc);
    }
    exit(1);
  }
  fileName = new GString(argv[1]);
  pbmRoot = argv[2];

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

  // get page range
  if (firstPage < 1)
    firstPage = 1;
  if (lastPage < 1 || lastPage > doc->getNumPages())
    lastPage = doc->getNumPages();

  // write PBM files
  rgbCubeSize = 1;
  pbmOut = PBMOutputDev::makePBMOutputDev(NULL, pbmRoot);
  doc->displayPages(pbmOut, firstPage, lastPage, resolution, 0, gFalse);
  PBMOutputDev::killPBMOutputDev(pbmOut);

  // clean up
 err:
  delete doc;
  freeParams();

  // check for memory leaks
  Object::memCheck(stderr);
  gMemReport(stderr);

  return 0;
}
