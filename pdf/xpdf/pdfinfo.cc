//========================================================================
//
// pdfinfo.cc
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
#include "Params.h"
#include "Error.h"
#include "config.h"

GBool printCommands = gFalse;
static GBool printHelp = gFalse;

static ArgDesc argDesc[] = {
  {"-h",      argFlag,     &printHelp,     0,
   "print usage information"},
  {"-help",   argFlag,     &printHelp,     0,
   "print usage information"},
  {NULL}
};

int main(int argc, char *argv[]) {
  PDFDoc *doc;
  GString *fileName;
  Object info, obj;
  char *s;
  GBool ok;

  // parse args
  ok = parseArgs(argDesc, &argc, argv);
  if (!ok || argc != 2 || printHelp) {
    fprintf(stderr, "pdfinfo version %s\n", xpdfVersion);
    fprintf(stderr, "%s\n", xpdfCopyright);
    printUsage("pdfinfo", "<PDF-file>", argDesc);
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
  if (!doc->isOk())
    exit(1);

  // print doc info
  doc->getDocInfo(&info);
  if (info.isDict()) {
    if (info.dictLookup("Title", &obj)->isString())
      printf("Title:        %s\n", obj.getString()->getCString());
    obj.free();
    if (info.dictLookup("Subject", &obj)->isString())
      printf("Subject:      %s\n", obj.getString()->getCString());
    obj.free();
    if (info.dictLookup("Keywords", &obj)->isString())
      printf("Keywords:     %s\n", obj.getString()->getCString());
    obj.free();
    if (info.dictLookup("Author", &obj)->isString())
      printf("Author:       %s\n", obj.getString()->getCString());
    obj.free();
    if (info.dictLookup("Creator", &obj)->isString())
      printf("Creator:      %s\n", obj.getString()->getCString());
    obj.free();
    if (info.dictLookup("Producer", &obj)->isString())
      printf("Producer:     %s\n", obj.getString()->getCString());
    obj.free();
    if (info.dictLookup("CreationDate", &obj)->isString()) {
      s = obj.getString()->getCString();
      if (s[0] == 'D' && s[1] == ':')
	s += 2;
      printf("CreationDate: %s\n", s);
    }
    obj.free();
    if (info.dictLookup("ModDate", &obj)->isString()) {
      s = obj.getString()->getCString();
      if (s[0] == 'D' && s[1] == ':')
	s += 2;
      printf("ModDate:      %s\n", s);
    }
    obj.free();
  }
  info.free();

  // print page count
  printf("Pages:        %d\n", doc->getNumPages());

  // print encrypted flag
  printf("Encrypted:    %s\n", doc->isEncrypted() ? "yes" : "no");

  // clean up
  delete doc;
  freeParams();

  // check for memory leaks
  Object::memCheck(errFile);
  gMemReport(errFile);

  return 0;
}
