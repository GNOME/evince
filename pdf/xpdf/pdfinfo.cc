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

static void printInfoString(Dict *infoDict, char *key, char *fmt);
static void printInfoDate(Dict *infoDict, char *key, char *fmt);

static char userPassword[33] = "";
static GBool printVersion = gFalse;
static GBool printHelp = gFalse;

static ArgDesc argDesc[] = {
  {"-upw",    argString,   userPassword,   sizeof(userPassword),
   "user password (for encrypted files)"},
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
  GString *userPW;
  Object info;
  GBool ok;

  // parse args
  ok = parseArgs(argDesc, &argc, argv);
  if (!ok || argc != 2 || printVersion || printHelp) {
    fprintf(stderr, "pdfinfo version %s\n", xpdfVersion);
    fprintf(stderr, "%s\n", xpdfCopyright);
    if (!printVersion) {
      printUsage("pdfinfo", "<PDF-file>", argDesc);
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
    exit(1);
  }

  // print doc info
  doc->getDocInfo(&info);
  if (info.isDict()) {
    printInfoString(info.getDict(), "Title",        "Title:        %s\n");
    printInfoString(info.getDict(), "Subject",      "Subject:      %s\n");
    printInfoString(info.getDict(), "Keywords",     "Keywords:     %s\n");
    printInfoString(info.getDict(), "Author",       "Author:       %s\n");
    printInfoString(info.getDict(), "Creator",      "Creator:      %s\n");
    printInfoString(info.getDict(), "Producer",     "Producer:     %s\n");
    printInfoDate(info.getDict(),   "CreationDate", "CreationDate: %s\n");
    printInfoDate(info.getDict(),   "ModDate",      "ModDate:      %s\n");
  }
  info.free();

  // print page count
  printf("Pages:        %d\n", doc->getNumPages());

  // print encryption info
  printf("Encrypted:    ");
  if (doc->isEncrypted()) {
    printf("yes (print:%s copy:%s change:%s addNotes:%s)\n",
	   doc->okToPrint() ? "yes" : "no",
	   doc->okToCopy() ? "yes" : "no",
	   doc->okToChange() ? "yes" : "no",
	   doc->okToAddNotes() ? "yes" : "no");
  } else {
    printf("no\n");
  }

  // print linearization info
  printf("Linearized:   %s\n", doc->isLinearized() ? "yes" : "no");

  // clean up
  delete doc;
  freeParams();

  // check for memory leaks
  Object::memCheck(stderr);
  gMemReport(stderr);

  return 0;
}

static void printInfoString(Dict *infoDict, char *key, char *fmt) {
  Object obj;
  GString *s1, *s2;
  int i;

  if (infoDict->lookup(key, &obj)->isString()) {
    s1 = obj.getString();
    if ((s1->getChar(0) & 0xff) == 0xfe &&
	(s1->getChar(1) & 0xff) == 0xff) {
      s2 = new GString();
      for (i = 2; i < obj.getString()->getLength(); i += 2) {
	if (s1->getChar(i) == '\0') {
	  s2->append(s1->getChar(i+1));
	} else {
	  delete s2;
	  s2 = new GString("<unicode>");
	  break;
	}
      }
      printf(fmt, s2->getCString());
      delete s2;
    } else {
      printf(fmt, s1->getCString());
    }
  }
  obj.free();
}

static void printInfoDate(Dict *infoDict, char *key, char *fmt) {
  Object obj;
  char *s;

  if (infoDict->lookup(key, &obj)->isString()) {
    s = obj.getString()->getCString();
    if (s[0] == 'D' && s[1] == ':') {
      s += 2;
    }
    printf(fmt, s);
  }
  obj.free();
}
