//========================================================================
//
// PDFDoc.cc
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
#include "GString.h"
#include "config.h"
#include "Page.h"
#include "Catalog.h"
#include "Stream.h"
#include "XRef.h"
#include "Link.h"
#include "OutputDev.h"
#include "Params.h"
#include "Error.h"
#include "PDFDoc.h"

//------------------------------------------------------------------------

#define headerSearchSize 1024	// read this many bytes at beginning of
				//   file to look for '%PDF'

//------------------------------------------------------------------------
// PDFDoc
//------------------------------------------------------------------------

PDFDoc::PDFDoc(GString *fileName1) {
  Object obj;
  GString *fileName2;

  ok = gFalse;

  file = NULL;
  str = NULL;

  // try to open file
  fileName = fileName1;
  fileName2 = NULL;
#ifdef VMS
  if (!(file = fopen(fileName->getCString(), "rb", "ctx=stm"))) {
    error(-1, "Couldn't open file '%s'", fileName->getCString());
    return;
  }
#else
  if (!(file = fopen(fileName->getCString(), "rb"))) {
    fileName2 = fileName->copy();
    fileName2->lowerCase();
    if (!(file = fopen(fileName2->getCString(), "rb"))) {
      fileName2->upperCase();
      if (!(file = fopen(fileName2->getCString(), "rb"))) {
	error(-1, "Couldn't open file '%s'", fileName->getCString());
	delete fileName2;
	return;
      }
    }
    delete fileName2;
  }
#endif

  // create stream
  obj.initNull();
  str = new FileStream(file, 0, -1, &obj);

  ok = setup();
}

PDFDoc::PDFDoc(BaseStream *str) {
  ok = gFalse;
  fileName = NULL;
  file = NULL;
  this->str = str;
  ok = setup();
}

GBool PDFDoc::setup() {
  Object catObj;

  xref = NULL;
  catalog = NULL;
  links = NULL;

  // check header
  checkHeader();

  // read xref table
  xref = new XRef(str);
  if (!xref->isOk()) {
    error(-1, "Couldn't read xref table");
    return gFalse;
  }

  // read catalog
  catalog = new Catalog(xref->getCatalog(&catObj));
  catObj.free();
  if (!catalog->isOk()) {
    error(-1, "Couldn't read page catalog");
    return gFalse;
  }

  // done
  return gTrue;
}

PDFDoc::~PDFDoc() {
  if (catalog) {
    delete catalog;
  }
  if (xref) {
    delete xref;
  }
  if (str) {
    delete str;
  }
  if (file) {
    fclose(file);
  }
  if (fileName) {
    delete fileName;
  }
  if (links) {
    delete links;
  }
}

// Check for a PDF header on this stream.  Skip past some garbage
// if necessary.
void PDFDoc::checkHeader() {
  char hdrBuf[headerSearchSize+1];
  char *p;
  double version;
  int i;

  for (i = 0; i < headerSearchSize; ++i) {
    hdrBuf[i] = str->getChar();
  }
  hdrBuf[headerSearchSize] = '\0';
  for (i = 0; i < headerSearchSize - 5; ++i) {
    if (!strncmp(&hdrBuf[i], "%PDF-", 5)) {
      break;
    }
  }
  if (i >= headerSearchSize - 5) {
    error(-1, "May not be a PDF file (continuing anyway)");
    return;
  }
  str->moveStart(i);
  p = strtok(&hdrBuf[i+5], " \t\n\r");
  version = atof(p);
  if (!(hdrBuf[i+5] >= '0' && hdrBuf[i+5] <= '9') ||
      version > pdfVersionNum + 0.0001) {
    error(-1, "PDF version %s -- xpdf supports version %s"
	  " (continuing anyway)", p, pdfVersion);
  }
}

void PDFDoc::displayPage(OutputDev *out, int page, int zoom, int rotate,
			 GBool doLinks) {
  Link *link;
  double x1, y1, x2, y2;
  double w;
  int i;

  if (printCommands)
    printf("***** page %d *****\n", page);
  catalog->getPage(page)->display(out, zoom, rotate);
  if (doLinks) {
    if (links)
      delete links;
    getLinks(page);
    for (i = 0; i < links->getNumLinks(); ++i) {
      link = links->getLink(i);
      link->getBorder(&x1, &y1, &x2, &y2, &w);
      if (w > 0)
	out->drawLinkBorder(x1, y1, x2, y2, w);
    }
    out->dump();
  }
}

void PDFDoc::displayPages(OutputDev *out, int firstPage, int lastPage,
			  int zoom, int rotate) {
  Page *p;
  int page;

  for (page = firstPage; page <= lastPage; ++page) {
    if (printCommands)
      printf("***** page %d *****\n", page);
    p = catalog->getPage(page);
    p->display(out, zoom, rotate);
  }
}

GBool PDFDoc::saveAs(GString *name) {
  FILE *f;
  int c;

  if (!(f = fopen(name->getCString(), "wb"))) {
    error(-1, "Couldn't open file '%s'", name->getCString());
    return gFalse;
  }
  str->reset();
  while ((c = str->getChar()) != EOF) {
    fputc(c, f);
  }
  fclose(f);
  return gTrue;
}

void PDFDoc::getLinks(int page) {
  Object obj;

  links = new Links(catalog->getPage(page)->getAnnots(&obj),
		    catalog->getBaseURI());
  obj.free();
}
