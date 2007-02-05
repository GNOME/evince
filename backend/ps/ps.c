/*
 * ps.c -- Postscript scanning and copying routines.
 * Copyright (C) 1992, 1998  Timothy O. Theisen
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.
 *
 *   Author: Tim Theisen           Systems Programmer
 * Internet: tim@cs.wisc.edu       Department of Computer Sciences
 *     UUCP: uwvax!tim             University of Wisconsin-Madison
 *    Phone: (608)262-0438         1210 West Dayton Street
 *      FAX: (608)262-9777         Madison, WI   53706
 */

/* 18/3/98 Jake Hamby patch */

/*
 * 98/03/17: Jake Hamby (jehamby@lightside.com):
 * Added support for compressed/gzipped Postscript and PDF files.
 * Compressed files are gunzipped to a temporary file, and PDF files are
 * scanned by calling Ghostscript to generate a fake DSC file.
 * This is based on code from GV 3.5.8, which is available at:
 *    http://wwwthep.physik.uni-mainz.de/~plass/gv/
 */

/* GV by 	Johannes Plass
 *			Department of Physics
 *			Johannes Gutenberg University
 *			Mainz, Germany
 *		
 *			<plass@thep.physik.uni-mainz.de>
 */

/* end of patch */

#include <stdlib.h>
#include <stdio.h>
#ifndef SEEK_SET
#   define SEEK_SET 0
#endif
#ifndef BUFSIZ
#   define BUFSIZ 1024
#endif
#include <ctype.h>
#include <X11/Xos.h>            /* #includes the appropriate <string.h> */
#include "gstypes.h"
#include "gsdefaults.h"
#include "ps.h"
#include "gsio.h"

#include <glib.h>

/* length calculates string length at compile time */
/* can only be used with character constants */
#define length(a) (sizeof(a)-1)
#define iscomment(a, b)	(strncmp(a, b, length(b)) == 0)
#define DSCcomment(a) (a[0] == '%' && a[1] == '%')

    /* list of standard paper sizes from Adobe's PPD. */

/*--------------------------------------------------*/
/* Declarations for ps_io_*() routines. */

typedef struct FileDataStruct_ *FileData;

typedef struct FileDataStruct_ {
   FILE *file;           /* file */
   int   file_desc;      /* file descriptor corresponding to file */
   int   filepos;        /* file position corresponding to the start of the line */
   char *buf;            /* buffer */
   int   buf_size;       /* size of buffer */
   int   buf_end;        /* last char in buffer given as offset to buf */
   int   line_begin;     /* start of the line given as offset to buf */
   int   line_end;       /* end of the line given as offset to buf */
   int   line_len;       /* length of line, i.e. (line_end-line_begin) */
   char  line_termchar;  /* char exchanged for a '\0' at end of line */
   int   status;         /* 0 = okay, 1 = failed */
} FileDataStruct;

static FileData ps_io_init (FILE *file);
static void     ps_io_exit (FileData data);
static char    *ps_io_fgetchars (FileData data, int offset);

static char    *skipped_line = "% ps_io_fgetchars: skipped line";
static char    *empty_string = "";

static char *readline (FileData fd, char **lineP, long *positionP, unsigned int *line_lenP);
static char *gettextline(char *line);
static char *get_next_text(char *line, char **next_char);
static int blank(char *line);

static struct page *
pages_new(struct page *pages, int current, int maxpages)
{
  struct page *oldpages = pages;
  if(!oldpages)
    pages = g_new0(struct page, maxpages);
  else
    pages = g_renew(struct page, oldpages, maxpages);
  for(; current < maxpages; current++) {
    memset(&(pages[current]), 0x00, sizeof(struct page));
    pages[current].orientation = GTK_GS_ORIENTATION_NONE;
  }
  return pages;
}

/*
 *	psscan -- scan the PostScript file for document structuring comments.
 *
 *	This scanner is designed to retrieve the information necessary for
 *	the ghostview previewer.  It will scan files that conform to any
 *	version (1.0, 2.0, 2.1, or 3.0) of the document structuring conventions.
 *	It does not really care which version of comments the file contains.
 *	(The comments are largely upward compatible.)  It will scan a number
 *	of non-conforming documents.  (You could have part of the document
 *	conform to V2.0 and the rest conform to V3.0.  It would be similar
 *	to the DC-2 1/2+, it would look funny but it can still fly.)
 *
 *	This routine returns a pointer to the document structure.
 *	The structure contains the information relevant to previewing.
 *      These include EPSF flag (to tell if the file is a encapsulated figure),
 *      Page Size (for the Page Size), Bounding Box (to minimize backing
 *      pixmap size or determine window size for encapsulated PostScript), 
 *      Orientation of Paper (for default transformation matrix), and
 *      Page Order.  The Title, Creator, and CreationDate are also retrieved to
 *      help identify the document.
 *
 *      The following comments are examined:
 *
 *      Header section: 
 *      Must start with %!PS-Adobe-.  Version numbers ignored.
 *      Also allowed to be just %!PS, many files seem to have that.
 *
 *      %!PS-Adobe-* [EPSF-*]
 *      %%BoundingBox: <int> <int> <int> <int>|(atend)
 *      %%Creator: <textline>
 *      %%CreationDate: <textline>
 *      %%Orientation: Portrait|Landscape|(atend)
 *      %%Pages: <uint> [<int>]|(atend)
 *      %%PageOrder: Ascend|Descend|Special|(atend)
 *      %%Title: <textline>
 *      %%DocumentMedia: <text> <real> <real> <real> <text> <text>
 *      %%DocumentPaperSizes: <text>
 *      %%EndComments
 *
 *      Note: Either the 3.0 or 2.0 syntax for %%Pages is accepted.
 *            Also either the 2.0 %%DocumentPaperSizes or the 3.0
 *            %%DocumentMedia comments are accepted as well.
 *
 *      The header section ends either explicitly with %%EndComments or
 *      implicitly with any line that does not begin with %X where X is
 *      a not whitespace character.
 *
 *      If the file is encapsulated PostScript the optional Preview section
 *      is next:
 *
 *      %%BeginPreview
 *      %%EndPreview
 *
 *      This section explicitly begins and ends with the above comments.
 *
 *      Next the Defaults section for version 3 page defaults:
 *
 *      %%BeginDefaults
 *      %%PageBoundingBox: <int> <int> <int> <int>
 *      %%PageOrientation: Portrait|Landscape
 *      %%PageMedia: <text>
 *      %%EndDefaults
 *
 *      This section explicitly begins and ends with the above comments.
 *
 *      The prolog section either explicitly starts with %%BeginProlog or
 *      implicitly with any nonblank line.
 *
 *      %%BeginProlog
 *      %%EndProlog
 *
 *      The Prolog should end with %%EndProlog, however the proglog implicitly
 *      ends when %%BeginSetup, %%Page, %%Trailer or %%EOF are encountered.
 *
 *      The Setup section is where the version 2 page defaults are found.
 *      This section either explicitly begins with %%BeginSetup or implicitly
 *      with any nonblank line after the Prolog.
 *
 *      %%BeginSetup
 *      %%PageBoundingBox: <int> <int> <int> <int>
 *      %%PageOrientation: Portrait|Landscape
 *      %%PaperSize: <text>
 *      %%EndSetup
 *
 *      The Setup should end with %%EndSetup, however the setup implicitly
 *      ends when %%Page, %%Trailer or %%EOF are encountered.
 *
 *      Next each page starts explicitly with %%Page and ends implicitly with
 *      %%Page or %%Trailer or %%EOF.  The following comments are recognized:
 *
 *      %%Page: <text> <uint>
 *      %%PageBoundingBox: <int> <int> <int> <int>|(atend)
 *      %%PageOrientation: Portrait|Landscape
 *      %%PageMedia: <text>
 *      %%PaperSize: <text>
 *
 *      The tralier section start explicitly with %%Trailer and end with %%EOF.
 *      The following comment are examined with the proper (atend) notation
 *      was used in the header:
 *
 *      %%Trailer
 *      %%BoundingBox: <int> <int> <int> <int>|(atend)
 *      %%Orientation: Portrait|Landscape|(atend)
 *      %%Pages: <uint> [<int>]|(atend)
 *      %%PageOrder: Ascend|Descend|Special|(atend)
 *      %%EOF
 *
 *
 *  + A DC-3 received severe damage to one of its wings.  The wing was a total
 *    loss.  There was no replacement readily available, so the mechanic
 *    installed a wing from a DC-2.
 */

#include <glib.h>

struct document *
psscan(FILE * file, int respect_eof, const gchar * fname)
{
  struct document *doc;
  int bb_set = NONE;
  int pages_set = NONE;
  int page_order_set = NONE;
  int orientation_set = NONE;
  int page_bb_set = NONE;
  int page_size_set = NONE;
  int preread;                  /* flag which tells the readline isn't needed */
  int i;
  unsigned int maxpages = 0;
  unsigned int nextpage = 1;    /* Next expected page */
  unsigned int thispage;
  int ignore = 0;               /* whether to ignore page ordinals */
  char *label;
  char *line;
  char text[PSLINELENGTH];      /* Temporary storage for text */
  long position;                /* Position of the current line */
  long beginsection;            /* Position of the beginning of the section */
  unsigned int line_len;        /* Length of the current line */
  unsigned int section_len;     /* Place to accumulate the section length */
  char *next_char;              /* 1st char after text returned by get_next_text() */
  char *cp;
  GtkGSPaperSize *dmp;
  GtkGSPaperSize *papersizes = gtk_gs_defaults_get_paper_sizes();
  FileData fd;

  if(!file)
    return NULL;

  rewind(file);

  fd = ps_io_init(file);
  if (!readline(fd, &line, &position, &line_len)) {
    fprintf(stderr, "Warning: empty file.\n");
    ps_io_exit(fd);
    return(NULL);
  }

  /* HP printer job language data follows. Some printer drivers add pjl
   * commands to switch a pjl printer to postscript mode. If no PS header
   * follows, this seems to be a real pjl file. */
  if(iscomment(line, "\033%-12345X@PJL")) {
    /* read until first DSC comment */
    while(readline(fd, &line, &position, &line_len)
          && (line[0] != '%')) ;
    if(line[0] != '%') {
      g_print("psscan error: input files seems to be a PJL file.\n");
      ps_io_exit(fd);
      return (NULL);
    }
  }

  /* Header comments */

  /* Header should start with "%!PS-Adobe-", but some programms omit
   * parts of this or add a ^D at the beginning. */
  if(iscomment(line, "%!PS") || iscomment(line, "\004%!PS")) {
    doc = g_new0(struct document, 1);
    doc->default_page_orientation = GTK_GS_ORIENTATION_NONE;
    doc->orientation = GTK_GS_ORIENTATION_NONE;

    /* ignore possible leading ^D */
    if(*line == '\004') {
      position++;
      line_len--;
    }

/* Jake Hamby patch 18/3/98 */

    text[0] = '\0';
    sscanf(line, "%*s %256s", text);
    /*doc->epsf = iscomment(text, "EPSF-"); */
    doc->epsf = iscomment(text, "EPSF");    /* Hamby - This line changed */
    doc->beginheader = position;
    section_len = line_len;
  }
  else {
    /* There are postscript documents that do not have
       %PS at the beginning, usually unstructured. We should GS decide
       For instance, the tech reports at this university:

       http://svrc.it.uq.edu.au/Bibliography/svrc-tr.html?94-45

       add ugly PostScript before the actual document. 

       GS and gv is
       able to display them correctly as unstructured PS.

       In a way, this makes sense, a program PostScript does not need
       the !PS at the beginning.
     */
    doc = g_new0(struct document, 1);
    doc->default_page_orientation = GTK_GS_ORIENTATION_NONE;
    doc->orientation = GTK_GS_ORIENTATION_NONE;
    ps_io_exit(fd);
    return (doc);
  }

  preread = 0;
  while(preread || readline(fd, &line, &position, &line_len)) {
    if(!preread)
      section_len += line_len;
    preread = 0;
    if(line[0] != '%' ||
       iscomment(line + 1, "%EndComments") ||
       line[1] == ' ' || line[1] == '\t' || line[1] == '\n' ||
       !isprint(line[1])) {
      break;
    }
    else if(line[1] != '%') {
      /* Do nothing */
    }
    else if(doc->title == NULL && iscomment(line + 2, "Title:")) {
      doc->title = gettextline(line + length("%%Title:"));
    }
    else if(doc->date == NULL && iscomment(line + 2, "CreationDate:")) {
      doc->date = gettextline(line + length("%%CreationDate:"));
    }
    else if(doc->creator == NULL && iscomment(line + 2, "Creator:")) {
      doc->creator = gettextline(line + length("%%Creator:"));
    }
    else if(bb_set == NONE && iscomment(line + 2, "BoundingBox:")) {
      sscanf(line + length("%%BoundingBox:"), "%256s", text);
      if(strcmp(text, "(atend)") == 0) {
        bb_set = ATEND;
      }
      else {
        if(sscanf(line + length("%%BoundingBox:"), "%d %d %d %d",
                  &(doc->boundingbox[LLX]),
                  &(doc->boundingbox[LLY]),
                  &(doc->boundingbox[URX]), &(doc->boundingbox[URY])) == 4)
          bb_set = 1;
        else {
          float fllx, flly, furx, fury;
          if(sscanf(line + length("%%BoundingBox:"), "%f %f %f %f",
                    &fllx, &flly, &furx, &fury) == 4) {
            bb_set = 1;
            doc->boundingbox[LLX] = fllx;
            doc->boundingbox[LLY] = flly;
            doc->boundingbox[URX] = furx;
            doc->boundingbox[URY] = fury;
            if(fllx < doc->boundingbox[LLX])
              doc->boundingbox[LLX]--;
            if(flly < doc->boundingbox[LLY])
              doc->boundingbox[LLY]--;
            if(furx > doc->boundingbox[URX])
              doc->boundingbox[URX]++;
            if(fury > doc->boundingbox[URY])
              doc->boundingbox[URY]++;
          }
        }
      }
    }
    else if(orientation_set == NONE && iscomment(line + 2, "Orientation:")) {
      sscanf(line + length("%%Orientation:"), "%256s", text);
      if(strcmp(text, "(atend)") == 0) {
        orientation_set = ATEND;
      }
      else if(strcmp(text, "Portrait") == 0) {
        doc->orientation = GTK_GS_ORIENTATION_PORTRAIT;
        orientation_set = 1;
      }
      else if(strcmp(text, "Landscape") == 0) {
        doc->orientation = GTK_GS_ORIENTATION_LANDSCAPE;
        orientation_set = 1;
      }
      else if(strcmp(text, "Seascape") == 0) {
        doc->orientation = GTK_GS_ORIENTATION_SEASCAPE;
        orientation_set = 1;
      }
    }
    else if(page_order_set == NONE && iscomment(line + 2, "PageOrder:")) {
      sscanf(line + length("%%PageOrder:"), "%256s", text);
      if(strcmp(text, "(atend)") == 0) {
        page_order_set = ATEND;
      }
      else if(strcmp(text, "Ascend") == 0) {
        doc->pageorder = ASCEND;
        page_order_set = 1;
      }
      else if(strcmp(text, "Descend") == 0) {
        doc->pageorder = DESCEND;
        page_order_set = 1;
      }
      else if(strcmp(text, "Special") == 0) {
        doc->pageorder = SPECIAL;
        page_order_set = 1;
      }
    }
    else if(pages_set == NONE && iscomment(line + 2, "Pages:")) {
      sscanf(line + length("%%Pages:"), "%256s", text);
      if(strcmp(text, "(atend)") == 0) {
        pages_set = ATEND;
      }
      else {
        switch (sscanf(line + length("%%Pages:"), "%d %d", &maxpages, &i)) {
        case 2:
          if(page_order_set == NONE) {
            if(i == -1) {
              doc->pageorder = DESCEND;
              page_order_set = 1;
            }
            else if(i == 0) {
              doc->pageorder = SPECIAL;
              page_order_set = 1;
            }
            else if(i == 1) {
              doc->pageorder = ASCEND;
              page_order_set = 1;
            }
          }
        case 1:
          if(maxpages > 0)
            doc->pages = pages_new(NULL, 0, maxpages);
        }
      }
    }
    else if(doc->numsizes == NONE && iscomment(line + 2, "DocumentMedia:")) {
      float w, h;
      doc->size = g_new0(GtkGSPaperSize, 1);
      doc->size[0].name =
        get_next_text(line + length("%%DocumentMedia:"), &next_char);
      if(doc->size[0].name != NULL) {
        if(sscanf(next_char, "%f %f", &w, &h) == 2) {
          doc->size[0].width = w + 0.5;
          doc->size[0].height = h + 0.5;
        }
        if(doc->size[0].width != 0 && doc->size[0].height != 0)
          doc->numsizes = 1;
        else
          g_free(doc->size[0].name);
      }
      preread = 1;
      while(readline(fd, &line, &position, &line_len) &&
            DSCcomment(line) && iscomment(line + 2, "+")) {
        section_len += line_len;
        doc->size = g_renew(GtkGSPaperSize, doc->size, doc->numsizes + 1);
        doc->size[doc->numsizes].name =
          get_next_text(line + length("%%+"), &next_char);
        if(doc->size[doc->numsizes].name != NULL) {
          if(sscanf(next_char, "%f %f", &w, &h) == 2) {
            doc->size[doc->numsizes].width = w + 0.5;
            doc->size[doc->numsizes].height = h + 0.5;
          }
          if(doc->size[doc->numsizes].width != 0 &&
             doc->size[doc->numsizes].height != 0)
            doc->numsizes++;
          else
            g_free(doc->size[doc->numsizes].name);
        }
      }
      section_len += line_len;
      if(doc->numsizes != 0)
        doc->default_page_size = doc->size;
    }
    else if(doc->numsizes == NONE && iscomment(line + 2, "DocumentPaperSizes:")) {

      doc->size = g_new0(GtkGSPaperSize, 1);
      doc->size[0].name =
        get_next_text(line + length("%%DocumentPaperSizes:"), &next_char);
      if(doc->size[0].name != NULL) {
        doc->size[0].width = 0;
        doc->size[0].height = 0;
        for(dmp = papersizes; dmp->name != NULL; dmp++) {
          /* Note: Paper size comment uses down cased paper size
           * name.  Case insensitive compares are only used for
           * PaperSize comments.
           */
          if(strcasecmp(doc->size[0].name, dmp->name) == 0) {
            g_free(doc->size[0].name);
            doc->size[0].name = g_strdup(dmp->name);
            doc->size[0].width = dmp->width;
            doc->size[0].height = dmp->height;
            break;
          }
        }
        if(doc->size[0].width != 0 && doc->size[0].height != 0)
          doc->numsizes = 1;
        else
          g_free(doc->size[0].name);
      }
      while((cp = get_next_text(next_char, &next_char))) {
        doc->size = g_renew(GtkGSPaperSize, doc->size, doc->numsizes + 1);
        doc->size[doc->numsizes].name = cp;
        doc->size[doc->numsizes].width = 0;
        doc->size[doc->numsizes].height = 0;
        for(dmp = papersizes; dmp->name != NULL; dmp++) {
          /* Note: Paper size comment uses down cased paper size
           * name.  Case insensitive compares are only used for
           * PaperSize comments.
           */
          if(strcasecmp(doc->size[doc->numsizes].name, dmp->name) == 0) {
            g_free(doc->size[doc->numsizes].name);
            doc->size[doc->numsizes].name = g_strdup(dmp->name);
            doc->size[doc->numsizes].name = dmp->name;
            doc->size[doc->numsizes].width = dmp->width;
            doc->size[doc->numsizes].height = dmp->height;
            break;
          }
        }
        if(doc->size[doc->numsizes].width != 0 &&
           doc->size[doc->numsizes].height != 0)
          doc->numsizes++;
        else
          g_free(doc->size[doc->numsizes].name);
      }
      preread = 1;
      while(readline(fd, &line, &position, &line_len) &&
            DSCcomment(line) && iscomment(line + 2, "+")) {
        section_len += line_len;
        next_char = line + length("%%+");
        while((cp = get_next_text(next_char, &next_char))) {
          doc->size = g_renew(GtkGSPaperSize, doc->size, doc->numsizes + 1);
          doc->size[doc->numsizes].name = cp;
          doc->size[doc->numsizes].width = 0;
          doc->size[doc->numsizes].height = 0;
          for(dmp = papersizes; dmp->name != NULL; dmp++) {
            /* Note: Paper size comment uses down cased paper size
             * name.  Case insensitive compares are only used for
             * PaperSize comments.
             */
            if(strcasecmp(doc->size[doc->numsizes].name, dmp->name) == 0) {
              doc->size[doc->numsizes].width = dmp->width;
              doc->size[doc->numsizes].height = dmp->height;
              break;
            }
          }
          if(doc->size[doc->numsizes].width != 0 &&
             doc->size[doc->numsizes].height != 0)
            doc->numsizes++;
          else
            g_free(doc->size[doc->numsizes].name);
        }
      }
      section_len += line_len;
      if(doc->numsizes != 0)
        doc->default_page_size = doc->size;
    }
  }

  if(DSCcomment(line) && iscomment(line + 2, "EndComments")) {
    readline(fd, &line, &position, &line_len);
    section_len += line_len;
  }
  doc->endheader = position;
  doc->lenheader = section_len - line_len;

  /* Optional Preview comments for encapsulated PostScript files */

  beginsection = position;
  section_len = line_len;
  while(blank(line) && readline(fd, &line, &position, &line_len)) {
    section_len += line_len;
  }

  if(doc->epsf && DSCcomment(line) && iscomment(line + 2, "BeginPreview")) {
    doc->beginpreview = beginsection;
    beginsection = 0;
    while(readline(fd, &line, &position, &line_len) &&
          !(DSCcomment(line) && iscomment(line + 2, "EndPreview"))) {
      section_len += line_len;
    }
    section_len += line_len;
    readline(fd, &line, &position, &line_len);
    section_len += line_len;
    doc->endpreview = position;
    doc->lenpreview = section_len - line_len;
  }

  /* Page Defaults for Version 3.0 files */

  if(beginsection == 0) {
    beginsection = position;
    section_len = line_len;
  }
  while(blank(line) && readline(fd, &line, &position, &line_len)) {
    section_len += line_len;
  }

  if(DSCcomment(line) && iscomment(line + 2, "BeginDefaults")) {
    doc->begindefaults = beginsection;
    beginsection = 0;
    while(readline(fd, &line, &position, &line_len) &&
          !(DSCcomment(line) && iscomment(line + 2, "EndDefaults"))) {
      section_len += line_len;
      if(!DSCcomment(line)) {
        /* Do nothing */
      }
      else if(doc->default_page_orientation == GTK_GS_ORIENTATION_NONE &&
              iscomment(line + 2, "PageOrientation:")) {
        sscanf(line + length("%%PageOrientation:"), "%256s", text);
        if(strcmp(text, "Portrait") == 0) {
          doc->default_page_orientation = GTK_GS_ORIENTATION_PORTRAIT;
        }
        else if(strcmp(text, "Landscape") == 0) {
          doc->default_page_orientation = GTK_GS_ORIENTATION_LANDSCAPE;
        }
        else if(strcmp(text, "Seascape") == 0) {
          doc->default_page_orientation = GTK_GS_ORIENTATION_SEASCAPE;
        }
      }
      else if(page_size_set == NONE && iscomment(line + 2, "PageMedia:")) {
        cp = get_next_text(line + length("%%PageMedia:"), NULL);
        for(dmp = doc->size, i = 0; i < doc->numsizes; i++, dmp++) {
          if(strcmp(cp, dmp->name) == 0) {
            doc->default_page_size = dmp;
            page_size_set = 1;
            break;
          }
        }
        g_free(cp);
      }
      else if(page_bb_set == NONE && iscomment(line + 2, "PageBoundingBox:")) {
        if(sscanf(line + length("%%PageBoundingBox:"), "%d %d %d %d",
                  &(doc->default_page_boundingbox[LLX]),
                  &(doc->default_page_boundingbox[LLY]),
                  &(doc->default_page_boundingbox[URX]),
                  &(doc->default_page_boundingbox[URY])) == 4)
          page_bb_set = 1;
        else {
          float fllx, flly, furx, fury;
          if(sscanf
             (line + length("%%PageBoundingBox:"), "%f %f %f %f",
              &fllx, &flly, &furx, &fury) == 4) {
            page_bb_set = 1;
            doc->default_page_boundingbox[LLX] = fllx;
            doc->default_page_boundingbox[LLY] = flly;
            doc->default_page_boundingbox[URX] = furx;
            doc->default_page_boundingbox[URY] = fury;
            if(fllx < doc->default_page_boundingbox[LLX])
              doc->default_page_boundingbox[LLX]--;
            if(flly < doc->default_page_boundingbox[LLY])
              doc->default_page_boundingbox[LLY]--;
            if(furx > doc->default_page_boundingbox[URX])
              doc->default_page_boundingbox[URX]++;
            if(fury > doc->default_page_boundingbox[URY])
              doc->default_page_boundingbox[URY]++;
          }
        }
      }
    }
    section_len += line_len;
    readline(fd, &line, &position, &line_len);
    section_len += line_len;
    doc->enddefaults = position;
    doc->lendefaults = section_len - line_len;
  }

  /* Document Prolog */

  if(beginsection == 0) {
    beginsection = position;
    section_len = line_len;
  }
  while(blank(line) && readline(fd, &line, &position, &line_len)) {
    section_len += line_len;
  }

  if(!(DSCcomment(line) &&
       (iscomment(line + 2, "BeginSetup") ||
        iscomment(line + 2, "Page:") ||
        iscomment(line + 2, "Trailer") || iscomment(line + 2, "EOF")))) {
    doc->beginprolog = beginsection;
    beginsection = 0;
    preread = 1;

    while((preread ||
           readline(fd, &line, &position, &line_len)) &&
          !(DSCcomment(line) &&
            (iscomment(line + 2, "EndProlog") ||
             iscomment(line + 2, "BeginSetup") ||
             iscomment(line + 2, "Page:") ||
             iscomment(line + 2, "Trailer") || iscomment(line + 2, "EOF")))) {
      if(!preread)
        section_len += line_len;
      preread = 0;
    }
    section_len += line_len;
    if(DSCcomment(line) && iscomment(line + 2, "EndProlog")) {
      readline(fd, &line, &position, &line_len);
      section_len += line_len;
    }
    doc->endprolog = position;
    doc->lenprolog = section_len - line_len;
  }

  /* Document Setup,  Page Defaults found here for Version 2 files */

  if(beginsection == 0) {
    beginsection = position;
    section_len = line_len;
  }
  while(blank(line) && readline(fd, &line, &position, &line_len)) {
    section_len += line_len;
  }

  if(!(DSCcomment(line) &&
       (iscomment(line + 2, "Page:") ||
        iscomment(line + 2, "Trailer") ||
        (respect_eof && iscomment(line + 2, "EOF"))))) {
    doc->beginsetup = beginsection;
    beginsection = 0;
    preread = 1;
    while((preread ||
           readline(fd, &line, &position, &line_len)) &&
          !(DSCcomment(line) &&
            (iscomment(line + 2, "EndSetup") ||
             iscomment(line + 2, "Page:") ||
             iscomment(line + 2, "Trailer") ||
             (respect_eof && iscomment(line + 2, "EOF"))))) {
      if(!preread)
        section_len += line_len;
      preread = 0;
      if(!DSCcomment(line)) {
        /* Do nothing */
      }
      else if(doc->default_page_orientation == GTK_GS_ORIENTATION_NONE &&
              iscomment(line + 2, "PageOrientation:")) {
        sscanf(line + length("%%PageOrientation:"), "%256s", text);
        if(strcmp(text, "Portrait") == 0) {
          doc->default_page_orientation = GTK_GS_ORIENTATION_PORTRAIT;
        }
        else if(strcmp(text, "Landscape") == 0) {
          doc->default_page_orientation = GTK_GS_ORIENTATION_LANDSCAPE;
        }
        else if(strcmp(text, "Seascape") == 0) {
          doc->default_page_orientation = GTK_GS_ORIENTATION_SEASCAPE;
        }
      }
      else if(page_size_set == NONE && iscomment(line + 2, "PaperSize:")) {
        cp = get_next_text(line + length("%%PaperSize:"), NULL);
        for(dmp = doc->size, i = 0; i < doc->numsizes; i++, dmp++) {
          /* Note: Paper size comment uses down cased paper size
           * name.  Case insensitive compares are only used for
           * PaperSize comments.
           */
          if(strcasecmp(cp, dmp->name) == 0) {
            doc->default_page_size = dmp;
            page_size_set = 1;
            break;
          }
        }
        g_free(cp);
      }
      else if(page_bb_set == NONE && iscomment(line + 2, "PageBoundingBox:")) {
        if(sscanf(line + length("%%PageBoundingBox:"), "%d %d %d %d",
                  &(doc->default_page_boundingbox[LLX]),
                  &(doc->default_page_boundingbox[LLY]),
                  &(doc->default_page_boundingbox[URX]),
                  &(doc->default_page_boundingbox[URY])) == 4)
          page_bb_set = 1;
        else {
          float fllx, flly, furx, fury;
          if(sscanf
             (line + length("%%PageBoundingBox:"), "%f %f %f %f",
              &fllx, &flly, &furx, &fury) == 4) {
            page_bb_set = 1;
            doc->default_page_boundingbox[LLX] = fllx;
            doc->default_page_boundingbox[LLY] = flly;
            doc->default_page_boundingbox[URX] = furx;
            doc->default_page_boundingbox[URY] = fury;
            if(fllx < doc->default_page_boundingbox[LLX])
              doc->default_page_boundingbox[LLX]--;
            if(flly < doc->default_page_boundingbox[LLY])
              doc->default_page_boundingbox[LLY]--;
            if(furx > doc->default_page_boundingbox[URX])
              doc->default_page_boundingbox[URX]++;
            if(fury > doc->default_page_boundingbox[URY])
              doc->default_page_boundingbox[URY]++;
          }
        }
      }
    }
    section_len += line_len;
    if(DSCcomment(line) && iscomment(line + 2, "EndSetup")) {
      readline(fd, &line, &position, &line_len);
      section_len += line_len;
    }
    doc->endsetup = position;
    doc->lensetup = section_len - line_len;
  }

  /* HACK: Mozilla 1.8 Workaround.
  
     It seems that Mozilla 1.8 generates important postscript code 
     after the '%%EndProlog' and before the first page comment '%%Page: x y'.
     See comment below also.
   */
   
  if(doc->beginprolog && !doc->beginsetup) {
      doc->lenprolog += section_len - line_len;
      doc->endprolog = position;
  }
  
  /* HACK: Windows NT Workaround

     Mark Pfeifer (pfeiferm%ppddev@comet.cmis.abbott.com) noticed
     about problems when viewing Windows NT 3.51 generated postscript
     files with gv. He found that the relevant postscript files
     show important postscript code after the '%%EndSetup' and before
     the first page comment '%%Page: x y'.
   */
  if(doc->beginsetup) {
    while(!(DSCcomment(line) &&
            (iscomment(line + 2, "EndSetup") ||
             (iscomment(line + 2, "Page:") ||
              iscomment(line + 2, "Trailer") ||
              (respect_eof && iscomment(line + 2, "EOF"))))) &&
          (readline(fd, &line, &position, &line_len))) {
      section_len += line_len;
      doc->lensetup = section_len - line_len;
      doc->endsetup = position;
    }
  }

  /* Individual Pages */

  if(beginsection == 0) {
    beginsection = position;
    section_len = line_len;
  }
  while(blank(line) && readline(fd, &line, &position, &line_len)) {
    section_len += line_len;
  }


newpage:
  while(DSCcomment(line) && iscomment(line + 2, "Page:")) {
    if(maxpages == 0) {
      maxpages = 1;
      doc->pages = pages_new(NULL, 0, maxpages);
    }
    label = get_next_text(line + length("%%Page:"), &next_char);
    if(sscanf(next_char, "%d", &thispage) != 1)
      thispage = 0;
    if(nextpage == 1) {
      ignore = thispage != 1;
    }
    if(!ignore && thispage != nextpage) {
      g_free(label);
      doc->numpages--;
      goto continuepage;
    }
    nextpage++;
    if(doc->numpages == maxpages) {
      maxpages++;
      doc->pages = pages_new(doc->pages, maxpages - 1, maxpages);
    }
    page_bb_set = NONE;
    doc->pages[doc->numpages].label = label;
    if(beginsection) {
      doc->pages[doc->numpages].begin = beginsection;
      beginsection = 0;
    }
    else {
      doc->pages[doc->numpages].begin = position;
      section_len = line_len;
    }
  continuepage:
    while(readline(fd, &line, &position, &line_len) &&
          !(DSCcomment(line) &&
            (iscomment(line + 2, "Page:") ||
             iscomment(line + 2, "Trailer") ||
             (respect_eof && iscomment(line + 2, "EOF"))))) {
      section_len += line_len;
      if(!DSCcomment(line)) {
        /* Do nothing */
      }
      else if(doc->pages[doc->numpages].orientation == NONE &&
              iscomment(line + 2, "PageOrientation:")) {
        sscanf(line + length("%%PageOrientation:"), "%256s", text);
        if(strcmp(text, "Portrait") == 0) {
          doc->pages[doc->numpages].orientation = GTK_GS_ORIENTATION_PORTRAIT;
        }
        else if(strcmp(text, "Landscape") == 0) {
          doc->pages[doc->numpages].orientation = GTK_GS_ORIENTATION_LANDSCAPE;
        }
        else if(strcmp(text, "Seascape") == 0) {
          doc->pages[doc->numpages].orientation = GTK_GS_ORIENTATION_SEASCAPE;
        }
      }
      else if(doc->pages[doc->numpages].size == NULL &&
              iscomment(line + 2, "PageMedia:")) {
        cp = get_next_text(line + length("%%PageMedia:"), NULL);
        for(dmp = doc->size, i = 0; i < doc->numsizes; i++, dmp++) {
          if(strcmp(cp, dmp->name) == 0) {
            doc->pages[doc->numpages].size = dmp;
            break;
          }
        }
        g_free(cp);
      }
      else if(doc->pages[doc->numpages].size == NULL &&
              iscomment(line + 2, "PaperSize:")) {
        cp = get_next_text(line + length("%%PaperSize:"), NULL);
        for(dmp = doc->size, i = 0; i < doc->numsizes; i++, dmp++) {
          /* Note: Paper size comment uses down cased paper size
           * name.  Case insensitive compares are only used for
           * PaperSize comments.
           */
          if(strcasecmp(cp, dmp->name) == 0) {
            doc->pages[doc->numpages].size = dmp;
            break;
          }
        }
        g_free(cp);
      }
      else if((page_bb_set == NONE || page_bb_set == ATEND) &&
              iscomment(line + 2, "PageBoundingBox:")) {
        sscanf(line + length("%%PageBoundingBox:"), "%256s", text);
        if(strcmp(text, "(atend)") == 0) {
          page_bb_set = ATEND;
        }
        else {
          if(sscanf
             (line + length("%%PageBoundingBox:"), "%d %d %d %d",
              &(doc->pages[doc->numpages].boundingbox[LLX]),
              &(doc->pages[doc->numpages].boundingbox[LLY]),
              &(doc->pages[doc->numpages].boundingbox[URX]),
              &(doc->pages[doc->numpages].boundingbox[URY])) == 4) {
            if(page_bb_set == NONE)
              page_bb_set = 1;
          }
          else {
            float fllx, flly, furx, fury;
            if(sscanf(line + length("%%PageBoundingBox:"),
                      "%f %f %f %f", &fllx, &flly, &furx, &fury) == 4) {
              if(page_bb_set == NONE)
                page_bb_set = 1;
              doc->pages[doc->numpages].boundingbox[LLX] = fllx;
              doc->pages[doc->numpages].boundingbox[LLY] = flly;
              doc->pages[doc->numpages].boundingbox[URX] = furx;
              doc->pages[doc->numpages].boundingbox[URY] = fury;
              if(fllx < doc->pages[doc->numpages].boundingbox[LLX])
                doc->pages[doc->numpages].boundingbox[LLX]--;
              if(flly < doc->pages[doc->numpages].boundingbox[LLY])
                doc->pages[doc->numpages].boundingbox[LLY]--;
              if(furx > doc->pages[doc->numpages].boundingbox[URX])
                doc->pages[doc->numpages].boundingbox[URX]++;
              if(fury > doc->pages[doc->numpages].boundingbox[URY])
                doc->pages[doc->numpages].boundingbox[URY]++;
            }
          }
        }
      }
    }
    section_len += line_len;
    doc->pages[doc->numpages].end = position;
    doc->pages[doc->numpages].len = section_len - line_len;
    doc->numpages++;
  }

  /* Document Trailer */

  if(beginsection) {
    doc->begintrailer = beginsection;
    beginsection = 0;
  }
  else {
    doc->begintrailer = position;
    section_len = line_len;
  }

  preread = 1;
  while((preread ||
         readline(fd, &line, &position, &line_len)) &&
        !(respect_eof && DSCcomment(line) && iscomment(line + 2, "EOF"))) {
    if(!preread)
      section_len += line_len;
    preread = 0;
    if(!DSCcomment(line)) {
      /* Do nothing */
    }
    else if(iscomment(line + 2, "Page:")) {
      g_free(get_next_text(line + length("%%Page:"), &next_char));
      if(sscanf(next_char, "%d", &thispage) != 1)
        thispage = 0;
      if(!ignore && thispage == nextpage) {
        if(doc->numpages > 0) {
          doc->pages[doc->numpages - 1].end = position;
          doc->pages[doc->numpages - 1].len += section_len - line_len;
        }
        else {
          if(doc->endsetup) {
            doc->endsetup = position;
            doc->endsetup += section_len - line_len;
          }
          else if(doc->endprolog) {
            doc->endprolog = position;
            doc->endprolog += section_len - line_len;
          }
        }
        goto newpage;
      }
    }
    else if(!respect_eof && iscomment(line + 2, "Trailer")) {
      /* What we thought was the start of the trailer was really */
      /* the trailer of an EPS on the page. */
      /* Set the end of the page to this trailer and keep scanning. */
      if(doc->numpages > 0) {
        doc->pages[doc->numpages - 1].end = position;
        doc->pages[doc->numpages - 1].len += section_len - line_len;
      }
      doc->begintrailer = position;
      section_len = line_len;
    }
    else if(bb_set == ATEND && iscomment(line + 2, "BoundingBox:")) {
      if(sscanf(line + length("%%BoundingBox:"), "%d %d %d %d",
                &(doc->boundingbox[LLX]),
                &(doc->boundingbox[LLY]),
                &(doc->boundingbox[URX]), &(doc->boundingbox[URY])) != 4) {
        float fllx, flly, furx, fury;
        if(sscanf(line + length("%%BoundingBox:"), "%f %f %f %f",
                  &fllx, &flly, &furx, &fury) == 4) {
          doc->boundingbox[LLX] = fllx;
          doc->boundingbox[LLY] = flly;
          doc->boundingbox[URX] = furx;
          doc->boundingbox[URY] = fury;
          if(fllx < doc->boundingbox[LLX])
            doc->boundingbox[LLX]--;
          if(flly < doc->boundingbox[LLY])
            doc->boundingbox[LLY]--;
          if(furx > doc->boundingbox[URX])
            doc->boundingbox[URX]++;
          if(fury > doc->boundingbox[URY])
            doc->boundingbox[URY]++;
        }
      }
    }
    else if(orientation_set == ATEND && iscomment(line + 2, "Orientation:")) {
      sscanf(line + length("%%Orientation:"), "%256s", text);
      if(strcmp(text, "Portrait") == 0) {
        doc->orientation = GTK_GS_ORIENTATION_PORTRAIT;
      }
      else if(strcmp(text, "Landscape") == 0) {
        doc->orientation = GTK_GS_ORIENTATION_LANDSCAPE;
      }
      else if(strcmp(text, "Seascape") == 0) {
        doc->orientation = GTK_GS_ORIENTATION_SEASCAPE;
      }
    }
    else if(page_order_set == ATEND && iscomment(line + 2, "PageOrder:")) {
      sscanf(line + length("%%PageOrder:"), "%256s", text);
      if(strcmp(text, "Ascend") == 0) {
        doc->pageorder = ASCEND;
      }
      else if(strcmp(text, "Descend") == 0) {
        doc->pageorder = DESCEND;
      }
      else if(strcmp(text, "Special") == 0) {
        doc->pageorder = SPECIAL;
      }
    }
    else if(pages_set == ATEND && iscomment(line + 2, "Pages:")) {
      if(sscanf(line + length("%%Pages:"), "%*u %d", &i) == 1) {
        if(page_order_set == NONE) {
          if(i == -1)
            doc->pageorder = DESCEND;
          else if(i == 0)
            doc->pageorder = SPECIAL;
          else if(i == 1)
            doc->pageorder = ASCEND;
        }
      }
    }
  }
  section_len += line_len;
  if(DSCcomment(line) && iscomment(line + 2, "EOF")) {
    readline(fd, &line, &position, &line_len);
    section_len += line_len;
  }
  doc->endtrailer = position;
  doc->lentrailer = section_len - line_len;

#if 0
  section_len = line_len;
  preread = 1;
  while(preread || readline(line, sizeof line, file, &position, &line_len)) {
    if(!preread)
      section_len += line_len;
    preread = 0;
    if(DSCcomment(line) && iscomment(line + 2, "Page:")) {
      g_free(get_next_text(line + length("%%Page:"), &next_char));
      if(sscanf(next_char, "%d", &thispage) != 1)
        thispage = 0;
      if(!ignore && thispage == nextpage) {
        if(doc->numpages > 0) {
          doc->pages[doc->numpages - 1].end = position;
          doc->pages[doc->numpages - 1].len += doc->lentrailer +
            section_len - line_len;
        }
        else {
          if(doc->endsetup) {
            doc->endsetup = position;
            doc->endsetup += doc->lentrailer + section_len - line_len;
          }
          else if(doc->endprolog) {
            doc->endprolog = position;
            doc->endprolog += doc->lentrailer + section_len - line_len;
          }
        }
        goto newpage;
      }
    }
  }
#endif
  ps_io_exit(fd);
  return doc;
}

/*
 *	psfree -- free dynamic storage associated with document structure.
 */

void
psfree(doc)
     struct document *doc;
{
  int i;

  if(doc) {
    /*
       printf("This document exists\n");
     */
    for(i = 0; i < doc->numpages; i++) {
      if(doc->pages[i].label)
        g_free(doc->pages[i].label);
    }
    for(i = 0; i < doc->numsizes; i++) {
      if(doc->size[i].name)
        g_free(doc->size[i].name);
    }
    if(doc->title)
      g_free(doc->title);
    if(doc->date)
      g_free(doc->date);
    if(doc->creator)
      g_free(doc->creator);
    if(doc->pages)
      g_free(doc->pages);
    if(doc->size)
      g_free(doc->size);
    g_free(doc);
  }
}

/*
 * gettextine -- skip over white space and return the rest of the line.
 *               If the text begins with '(' return the text string
 *		 using get_next_text().
 */

static char *
gettextline(char *line)
{
  char *cp;

  while(*line && (*line == ' ' || *line == '\t'))
    line++;
  if(*line == '(') {
    return get_next_text(line, NULL);
  }
  else {
    if(strlen(line) == 0)
      return NULL;

    cp = g_strdup(line);

    /* Remove end of line */
    if(cp[strlen(line) - 2] == '\r' && cp[strlen(line) - 1] == '\n')
      /* Handle DOS \r\n */
      cp[strlen(line) - 2] = '\0';
    else if(cp[strlen(line) - 1] == '\n' || cp[strlen(line) - 1] == '\r')
      /* Handle mac and unix */
      cp[strlen(line) - 1] = '\0';

    return cp;
  }
}

/*
 *	get_next_text -- return the next text string on the line.
 *		   return NULL if nothing is present.
 */

static char *
get_next_text(line, next_char)
     char *line;
     char **next_char;
{
  char text[PSLINELENGTH];      /* Temporary storage for text */
  char *cp;
  int quoted = 0;

  while(*line && (*line == ' ' || *line == '\t'))
    line++;
  cp = text;
  if(*line == '(') {
    int level = 0;
    quoted = 1;
    line++;
    while(*line && !(*line == ')' && level == 0)
	  && (cp - text) < PSLINELENGTH - 1) {
      if(*line == '\\') {
        if(*(line + 1) == 'n') {
          *cp++ = '\n';
          line += 2;
        }
        else if(*(line + 1) == 'r') {
          *cp++ = '\r';
          line += 2;
        }
        else if(*(line + 1) == 't') {
          *cp++ = '\t';
          line += 2;
        }
        else if(*(line + 1) == 'b') {
          *cp++ = '\b';
          line += 2;
        }
        else if(*(line + 1) == 'f') {
          *cp++ = '\f';
          line += 2;
        }
        else if(*(line + 1) == '\\') {
          *cp++ = '\\';
          line += 2;
        }
        else if(*(line + 1) == '(') {
          *cp++ = '(';
          line += 2;
        }
        else if(*(line + 1) == ')') {
          *cp++ = ')';
          line += 2;
        }
        else if(*(line + 1) >= '0' && *(line + 1) <= '9') {
          if(*(line + 2) >= '0' && *(line + 2) <= '9') {
            if(*(line + 3) >= '0' && *(line + 3) <= '9') {
              *cp++ =
                ((*(line + 1) - '0') * 8 + *(line + 2) -
                 '0') * 8 + *(line + 3) - '0';
              line += 4;
            }
            else {
              *cp++ = (*(line + 1) - '0') * 8 + *(line + 2) - '0';
              line += 3;
            }
          }
          else {
            *cp++ = *(line + 1) - '0';
            line += 2;
          }
        }
        else {
          line++;
          *cp++ = *line++;
        }
      }
      else if(*line == '(') {
        level++;
        *cp++ = *line++;
      }
      else if(*line == ')') {
        level--;
        *cp++ = *line++;
      }
      else {
        *cp++ = *line++;
      }
    }
  }
  else {
    while(*line && !(*line == ' ' || *line == '\t' || *line == '\n')
	  && (cp - text) < PSLINELENGTH - 1)
      *cp++ = *line++;
  }
  *cp = '\0';
  if(next_char)
    *next_char = line;
  if(!quoted && strlen(text) == 0)
    return NULL;
  return g_strdup(text);
}

/*
 *	pscopy -- copy lines of Postscript from a section of one file
 *		  to another file.
 *                Automatically switch to binary copying whenever
 *                %%BeginBinary/%%EndBinary or %%BeginData/%%EndData
 *		  comments are encountered.
 */

void
pscopy(from, to, begin, end)
     FILE *from;
     GtkGSDocSink *to;
     long begin;                /* set negative to avoid initial seek */
     long end;
{
  char line[PSLINELENGTH];      /* 255 characters + 1 newline + 1 NULL */
  char text[PSLINELENGTH];      /* Temporary storage for text */
  unsigned int num;
  int i;
  char buf[BUFSIZ];

  if(begin >= 0)
    fseek(from, begin, SEEK_SET);
  while(ftell(from) < end) {
    fgets(line, sizeof line, from);
    gtk_gs_doc_sink_write(to, line, strlen(line));

    if(!(DSCcomment(line) && iscomment(line + 2, "Begin"))) {
      /* Do nothing */
    }
    else if(iscomment(line + 7, "Data:")) {
      text[0] = '\0';
      if(sscanf(line + length("%%BeginData:"), "%d %*s %256s", &num, text) >= 1) {
        if(strcmp(text, "Lines") == 0) {
          for(i = 0; i < num; i++) {
            fgets(line, sizeof(line), from);
            gtk_gs_doc_sink_write(to, line, strlen(line));
          }
        }
        else {
          while(num > BUFSIZ) {
            fread(buf, sizeof(char), BUFSIZ, from);
            gtk_gs_doc_sink_write(to, buf, BUFSIZ);
            num -= BUFSIZ;
          }
          fread(buf, sizeof(char), num, from);
          gtk_gs_doc_sink_write(to, buf, num);
        }
      }
    }
    else if(iscomment(line + 7, "Binary:")) {
      if(sscanf(line + length("%%BeginBinary:"), "%d", &num) == 1) {
        while(num > BUFSIZ) {
          fread(buf, sizeof(char), BUFSIZ, from);
          gtk_gs_doc_sink_write(to, buf, BUFSIZ);
          num -= BUFSIZ;
        }
        fread(buf, sizeof(char), num, from);
        gtk_gs_doc_sink_write(to, buf, num);
      }
    }
  }
}

/*
 *	pscopyuntil -- copy lines of Postscript from a section of one file
 *		       to another file until a particular comment is reached.
 *                     Automatically switch to binary copying whenever
 *                     %%BeginBinary/%%EndBinary or %%BeginData/%%EndData
 *		       comments are encountered.
 */

char *
pscopyuntil(FILE * from, GtkGSDocSink * to, long begin, long end,
            const char *comment)
{
  char line[PSLINELENGTH];      /* 255 characters + 1 newline + 1 NULL */
  char text[PSLINELENGTH];      /* Temporary storage for text */
  unsigned int num;
  int comment_length;
  int i;
  char buf[BUFSIZ];

  if(comment != NULL)
    comment_length = strlen(comment);
  else
    comment_length = 0;
  if(begin >= 0)
    fseek(from, begin, SEEK_SET);

  while(ftell(from) < end && !feof(from)) {
    fgets(line, sizeof line, from);

    /* iscomment cannot be used here,
     * because comment_length is not known at compile time. */
    if(comment != NULL && strncmp(line, comment, comment_length) == 0) {
      return g_strdup(line);
    }
    gtk_gs_doc_sink_write(to, line, strlen(line));
    if(!(DSCcomment(line) && iscomment(line + 2, "Begin"))) {
      /* Do nothing */
    }
    else if(iscomment(line + 7, "Data:")) {
      text[0] = '\0';
      if(sscanf(line + length("%%BeginData:"), "%d %*s %256s", &num, text) >= 1) {
        if(strcmp(text, "Lines") == 0) {
          for(i = 0; i < num; i++) {
            fgets(line, sizeof line, from);
            gtk_gs_doc_sink_write(to, line, strlen(line));
          }
        }
        else {
          while(num > BUFSIZ) {
            fread(buf, sizeof(char), BUFSIZ, from);
            gtk_gs_doc_sink_write(to, buf, BUFSIZ);
            num -= BUFSIZ;
          }
          fread(buf, sizeof(char), num, from);
          gtk_gs_doc_sink_write(to, buf, num);
        }
      }
    }
    else if(iscomment(line + 7, "Binary:")) {
      if(sscanf(line + length("%%BeginBinary:"), "%d", &num) == 1) {
        while(num > BUFSIZ) {
          fread(buf, sizeof(char), BUFSIZ, from);
          gtk_gs_doc_sink_write(to, buf, BUFSIZ);
          num -= BUFSIZ;
        }
        fread(buf, sizeof(char), num, from);
        gtk_gs_doc_sink_write(to, buf, num);
      }
    }
  }
  return NULL;
}

/*
 *	blank -- determine whether the line contains nothing but whitespace.
 */

static int
blank(char *line)
{
  char *cp = line;

  while(*cp == ' ' || *cp == '\t')
    cp++;
  return *cp == '\n' || (*cp == '%' && (line[0] != '%' || line[1] != '%'));
}

/*##########################################################*/
/* pscopydoc */
/* Copy the headers, marked pages, and trailer to fp */
/*##########################################################*/

void
pscopydoc(GtkGSDocSink * dest,
          char *src_filename, struct document *d, gint * pagelist)
{
  FILE *src_file;
  char text[PSLINELENGTH];
  char *comment;
  gboolean pages_written = FALSE;
  gboolean pages_atend = FALSE;
  int pages;
  int page = 1;
  int i, j;
  int here;

  src_file = fopen(src_filename, "r");
  i = 0;
  pages = 0;
  for(i = 0; i < d->numpages; i++) {
    if(pagelist[i])
      pages++;
  }

  here = d->beginheader;

  while((comment = pscopyuntil(src_file, dest, here, d->endheader, "%%Pages:"))) {
    here = ftell(src_file);
    if(pages_written || pages_atend) {
      g_free(comment);
      continue;
    }
    sscanf(comment + length("%%Pages:"), "%256s", text);
    if(strcmp(text, "(atend)") == 0) {
      gtk_gs_doc_sink_write(dest, comment, strlen(comment));
      pages_atend = TRUE;
    }
    else {
      switch (sscanf(comment + length("%%Pages:"), "%*d %d", &i)) {
      case 1:
        gtk_gs_doc_sink_printf(dest, "%%%%Pages: %d %d\n", pages, i);
        break;
      default:
        gtk_gs_doc_sink_printf(dest, "%%%%Pages: %d\n", pages);
        break;
      }
      pages_written = TRUE;
    }
    g_free(comment);
  }
  pscopyuntil(src_file, dest, d->beginpreview, d->endpreview, NULL);
  pscopyuntil(src_file, dest, d->begindefaults, d->enddefaults, NULL);
  pscopyuntil(src_file, dest, d->beginprolog, d->endprolog, NULL);
  pscopyuntil(src_file, dest, d->beginsetup, d->endsetup, NULL);

  for(i = 0; i < d->numpages; i++) {
    if(d->pageorder == DESCEND)
      j = (d->numpages - 1) - i;
    else
      j = i;
    j = i;
    if(pagelist[j]) {
      comment = pscopyuntil(src_file, dest,
                            d->pages[i].begin, d->pages[i].end, "%%Page:");
      gtk_gs_doc_sink_printf(dest, "%%%%Page: %s %d\n",
                             d->pages[i].label, page++);
      g_free(comment);
      pscopyuntil(src_file, dest, -1, d->pages[i].end, NULL);
    }
  }

  here = d->begintrailer;
  while((comment = pscopyuntil(src_file, dest, here, d->endtrailer,
                               "%%Pages:"))) {
    here = ftell(src_file);
    if(pages_written) {
      g_free(comment);
      continue;
    }
    switch (sscanf(comment + length("%%Pages:"), "%*d %d", &i)) {
    case 1:
      gtk_gs_doc_sink_printf(dest, "%%%%Pages: %d %d\n", pages, i);
      break;
    default:
      gtk_gs_doc_sink_printf(dest, "%%%%Pages: %d\n", pages);
      break;
    }
    pages_written = TRUE;
    g_free(comment);
  }

  fclose(src_file);
}

/*----------------------------------------------------------*/
/* ps_io_init */
/*----------------------------------------------------------*/

#define FD_FILE             (fd->file)
#define FD_FILE_DESC        (fd->file_desc)
#define FD_FILEPOS	    (fd->filepos)
#define FD_LINE_BEGIN       (fd->line_begin)
#define FD_LINE_END	    (fd->line_end)
#define FD_LINE_LEN	    (fd->line_len)
#define FD_LINE_TERMCHAR    (fd->line_termchar)
#define FD_BUF		    (fd->buf)
#define FD_BUF_END	    (fd->buf_end)
#define FD_BUF_SIZE	    (fd->buf_size)
#define FD_STATUS	    (fd->status)

#define FD_STATUS_OKAY        0
#define FD_STATUS_BUFTOOLARGE 1
#define FD_STATUS_NOMORECHARS 2

#define LINE_CHUNK_SIZE     4096
#define MAX_PS_IO_FGETCHARS_BUF_SIZE 57344
#define BREAK_PS_IO_FGETCHARS_BUF_SIZE 49152

static FileData ps_io_init(file)
   FILE *file;
{
   FileData fd;
   size_t size = sizeof(FileDataStruct);

   fd = (FileData) g_malloc(size);
   memset((void*) fd ,0,(size_t)size);

   rewind(file);
   FD_FILE      = file;
   FD_FILE_DESC = fileno(file);
   FD_FILEPOS   = ftell(file);
   FD_BUF_SIZE  = (2*LINE_CHUNK_SIZE)+1;
   FD_BUF       = g_malloc(FD_BUF_SIZE);
   FD_BUF[0]    = '\0';
   return(fd);
}

/*----------------------------------------------------------*/
/* ps_io_exit */
/*----------------------------------------------------------*/

static void
ps_io_exit(fd)
   FileData fd;
{
   g_free(FD_BUF);
   g_free(fd);
}

/*----------------------------------------------------------*/
/* ps_io_fseek */
/*----------------------------------------------------------*/

/*static int
ps_io_fseek(fd,offset)
   FileData fd;
   int offset;
{
   int status;
   status=fseek(FD_FILE,(long)offset,SEEK_SET);
   FD_BUF_END = FD_LINE_BEGIN = FD_LINE_END = FD_LINE_LEN = 0;
   FD_FILEPOS = offset;
   FD_STATUS  = FD_STATUS_OKAY;
   return(status);
}*/

/*----------------------------------------------------------*/
/* ps_io_ftell */
/*----------------------------------------------------------*/

/*static int
ps_io_ftell(fd)
   FileData fd;
{
   return(FD_FILEPOS);
}*/

/*----------------------------------------------------------*/
/* ps_io_fgetchars */
/*----------------------------------------------------------*/

#ifdef USE_MEMMOVE_CODE
static void ps_memmove (d, s, l)
  char *d;
  const char *s;
  unsigned l;
{
  if (s < d) for (s += l, d += l; l; --l) *--d = *--s;
  else if (s != d) for (; l; --l)         *d++ = *s++;
}
#else
#   define ps_memmove memmove
#endif

static char * ps_io_fgetchars(fd,num)
   FileData fd;
   int num;
{
   char *eol=NULL,*tmp;
   size_t size_of_char = sizeof(char);

   if (FD_STATUS != FD_STATUS_OKAY) {
      return(NULL);
   }

   FD_BUF[FD_LINE_END] = FD_LINE_TERMCHAR; /* restoring char previously exchanged against '\0' */
   FD_LINE_BEGIN       = FD_LINE_END;

   do {
      if (num<0) { /* reading whole line */
         if (FD_BUF_END-FD_LINE_END) {
 	    /* strpbrk is faster but fails on lines with embedded NULLs 
              eol = strpbrk(FD_BUF+FD_LINE_END,"\n\r");
            */
	    tmp = FD_BUF + FD_BUF_END;
	    eol = FD_BUF + FD_LINE_END;
	    while (eol < tmp && *eol != '\n' && *eol != '\r') eol++;
	    if (eol >= tmp) eol = NULL;
            if (eol) {
               if (*eol=='\r' && *(eol+1)=='\n') eol += 2;
               else eol++;
               break;
            }
         }
      } else { /* reading specified num of chars */
	 if (FD_BUF_END >= FD_LINE_BEGIN+num) {
            eol = FD_BUF+FD_LINE_BEGIN+num;
            break;
         }
      }

      if (FD_BUF_END - FD_LINE_BEGIN > BREAK_PS_IO_FGETCHARS_BUF_SIZE) {
	eol = FD_BUF + FD_BUF_END - 1;
	break;
      }

      while (FD_BUF_SIZE < FD_BUF_END+LINE_CHUNK_SIZE+1) {
         if (FD_BUF_SIZE > MAX_PS_IO_FGETCHARS_BUF_SIZE) {
	   /* we should never get here, since the line is broken
             artificially after BREAK_PS_IO_FGETCHARS_BUF_SIZE bytes. */
	    fprintf(stderr, "gv: ps_io_fgetchars: Fatal Error: buffer became too large.\n");
	    exit(-1);
         }
         if (FD_LINE_BEGIN) {
            ps_memmove((void*)FD_BUF,(void*)(FD_BUF+FD_LINE_BEGIN),
                    ((size_t)(FD_BUF_END-FD_LINE_BEGIN+1))*size_of_char);
            FD_BUF_END    -= FD_LINE_BEGIN; 
            FD_LINE_BEGIN  = 0;
         } else {
            FD_BUF_SIZE    = FD_BUF_SIZE+LINE_CHUNK_SIZE+1;
            FD_BUF         = g_realloc(FD_BUF,FD_BUF_SIZE);
         }
      }

      FD_LINE_END = FD_BUF_END;
#ifdef VMS
      /* different existing VMS file formats require that we use read here ###jp###,10/12/96 */ 
      if (num<0) FD_BUF_END += read(FD_FILE_DESC,FD_BUF+FD_BUF_END,LINE_CHUNK_SIZE);
      else       FD_BUF_END += fread(FD_BUF+FD_BUF_END,size_of_char,LINE_CHUNK_SIZE,FD_FILE);
#else
      /* read() seems to fail sometimes (? ? ?) so we always use fread ###jp###,07/31/96*/
      FD_BUF_END += fread(FD_BUF+FD_BUF_END,size_of_char,LINE_CHUNK_SIZE,FD_FILE);
#endif

      FD_BUF[FD_BUF_END] = '\0';
      if (FD_BUF_END-FD_LINE_END == 0) {
         FD_STATUS = FD_STATUS_NOMORECHARS;
         return(NULL);
      }
   }
   while (1);

   FD_LINE_END          = eol - FD_BUF;
   FD_LINE_LEN          = FD_LINE_END - FD_LINE_BEGIN;
   FD_LINE_TERMCHAR     = FD_BUF[FD_LINE_END];
   FD_BUF[FD_LINE_END]  = '\0';
#ifdef USE_FTELL_FOR_FILEPOS
   if (FD_LINE_END==FD_BUF_END) {
      /*
      For VMS we cannot assume that the record is FD_LINE_LEN bytes long
      on the disk. For stream_lf and stream_cr that is true, but not for
      other formats, since VAXC/DECC converts the formatting into a single \n.
      eg. variable format files have a 2-byte length and padding to an even
      number of characters. So, we use ftell for each record.
      This still will not work if we need to fseek to a \n or \r inside a
      variable record (ftell always returns the start of the record in this
      case).
      (Tim Adye, adye@v2.rl.ac.uk)
      */
      FD_FILEPOS         = ftell(FD_FILE);
   } else
#endif /* USE_FTELL_FOR_FILEPOS */
      FD_FILEPOS        += FD_LINE_LEN;

   return(FD_BUF+FD_LINE_BEGIN);
}

/*----------------------------------------------------------*/
/*
   readline()
   Read the next line in the postscript file.
   Automatically skip over data (as indicated by
   %%BeginBinary/%%EndBinary or %%BeginData/%%EndData
   comments.)
   Also, skip over included documents (as indicated by
   %%BeginDocument/%%EndDocument comments.)
*/
/*----------------------------------------------------------*/

static char *readline (fd, lineP, positionP, line_lenP)
   FileData fd;
   char **lineP;
   long *positionP;
   unsigned int *line_lenP;
{
   unsigned int nbytes=0;
   int skipped=0;
   char *line;

   if (positionP) *positionP = FD_FILEPOS;
   line = ps_io_fgetchars(fd,-1);
   if (!line) {
      *line_lenP = 0;
      *lineP     = empty_string;
      return(NULL); 
   }

   *line_lenP = FD_LINE_LEN;

#define IS_COMMENT(comment)				\
           (DSCcomment(line) && iscomment(line+2,(comment)))
#define IS_BEGIN(comment)				\
           (iscomment(line+7,(comment)))
#define SKIP_WHILE(cond)				\
	   while (readline(fd, &line, NULL, &nbytes) && (cond)) *line_lenP += nbytes;\
           skipped=1;
#define SKIP_UNTIL_1(comment) {				\
           SKIP_WHILE((!IS_COMMENT(comment)))		\
        }
#define SKIP_UNTIL_2(comment1,comment2) {		\
           SKIP_WHILE((!IS_COMMENT(comment1) && !IS_COMMENT(comment2)))\
        }

   if  (!IS_COMMENT("Begin"))     {} /* Do nothing */
   else if IS_BEGIN("Document:")  SKIP_UNTIL_1("EndDocument")
   else if IS_BEGIN("Feature:")   SKIP_UNTIL_1("EndFeature")
#ifdef USE_ACROREAD_WORKAROUND
   else if IS_BEGIN("File")       SKIP_UNTIL_2("EndFile","EOF")
#else
   else if IS_BEGIN("File")       SKIP_UNTIL_1("EndFile")
#endif
   else if IS_BEGIN("Font")       SKIP_UNTIL_1("EndFont")
   else if IS_BEGIN("ProcSet")    SKIP_UNTIL_1("EndProcSet")
   else if IS_BEGIN("Resource")   SKIP_UNTIL_1("EndResource")
   else if IS_BEGIN("Data:")      {
      int  num;
      char text[101];
      if (FD_LINE_LEN > 100) FD_BUF[100] = '\0';
      text[0] = '\0';
      if (sscanf(line+length("%%BeginData:"), "%d %*s %s", &num, text) >= 1) {
         if (strcmp(text, "Lines") == 0) {
            while (num) {
               line = ps_io_fgetchars(fd,-1);
               if (line) *line_lenP += FD_LINE_LEN;
               num--;
            }
         } else {
            int read_chunk_size = LINE_CHUNK_SIZE;
            while (num>0) {
               if (num <= LINE_CHUNK_SIZE) read_chunk_size=num;
               line = ps_io_fgetchars(fd,read_chunk_size);
               if (line) *line_lenP += FD_LINE_LEN;
               num -= read_chunk_size;
            }
         }
      }
      SKIP_UNTIL_1("EndData")
   }
   else if IS_BEGIN("Binary:") {
      int  num;
      if (sscanf(line+length("%%BeginBinary:"), "%d", &num) == 1) {
         int read_chunk_size = LINE_CHUNK_SIZE;
         while (num>0) {
            if (num <= LINE_CHUNK_SIZE) read_chunk_size=num;
            line = ps_io_fgetchars(fd,read_chunk_size);
            if (line) *line_lenP += FD_LINE_LEN;
            num -= read_chunk_size;
         }
         SKIP_UNTIL_1("EndBinary")
      }
   }

   if (skipped) {
      *line_lenP += nbytes;
      *lineP = skipped_line;      
   } else {
      *lineP = FD_BUF+FD_LINE_BEGIN;
   }

   return(FD_BUF+FD_LINE_BEGIN);
}
