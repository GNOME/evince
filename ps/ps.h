/*
 * ps.h -- Include file for PostScript routines.
 * Copyright (C) 1992  Timothy O. Theisen
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
#ifndef __GGV_PS_H__
#define __GGV_PS_H__

#include <stdio.h>

#include <gtkgs.h>
#include <gsio.h>

G_BEGIN_DECLS

/* Constants used to index into the bounding box array. */
#define LLX 0
#define LLY 1
#define URX 2
#define URY 3

/* Constants used to store keywords that are scanned. */
/* NONE is not a keyword, it tells when a field was not set */

enum { ATEND = -1, NONE = 0, ASCEND, DESCEND, SPECIAL };

#define PSLINELENGTH 257     /* 255 characters + 1 newline + 1 NULL */

struct document {
  int epsf;                     /* Encapsulated PostScript flag. */
  char *title;                  /* Title of document. */
  char *date;                   /* Creation date. */
  int pageorder;                /* ASCEND, DESCEND, SPECIAL */
  long beginheader, endheader;  /* offsets into file */
  unsigned int lenheader;
  long beginpreview, endpreview;
  unsigned int lenpreview;
  long begindefaults, enddefaults;
  unsigned int lendefaults;
  long beginprolog, endprolog;
  unsigned int lenprolog;
  long beginsetup, endsetup;
  unsigned int lensetup;
  long begintrailer, endtrailer;
  unsigned int lentrailer;
  int boundingbox[4];
  int default_page_boundingbox[4];
  int orientation;              /* GTK_GS_ORIENTATION_PORTRAIT, GTK_GS_ORIENTATION_LANDSCAPE */
  int default_page_orientation; /* GTK_GS_ORIENTATION_PORTRAIT, GTK_GS_ORIENTATION_LANDSCAPE */
  unsigned int numsizes;
  GtkGSPaperSize *size;
  GtkGSPaperSize *default_page_size;
  unsigned int numpages;
  struct page *pages;
};

struct page {
  char *label;
  int boundingbox[4];
  GtkGSPaperSize *size;
  int orientation;              /* GTK_GS_ORIENTATION_PORTRAIT, GTK_GS_ORIENTATION_LANDSCAPE */
  long begin, end;              /* offsets into file */
  unsigned int len;
};

/* scans a PostScript file and return a pointer to the document
   structure.  Returns NULL if file does not Conform to commenting
   conventions . */
struct document *psscan(FILE * fileP, int respect_eof, const gchar * fname);

/* free data structure malloc'ed by psscan */
void psfree(struct document *);

/* Copy a portion of the PostScript file */
void pscopy(FILE * from, GtkGSDocSink * to, long begin, long end);

/* Copy a portion of the PostScript file upto a comment */
char *pscopyuntil(FILE * from, GtkGSDocSink * to, long begin, long end,
                  const char *comment);

/* Copy the headers, marked pages, and trailer to fp */
void pscopydoc(GtkGSDocSink * dest_file, char *src_filename,
               struct document *d, int *pagelist);

G_END_DECLS

#endif /* __GGV_PS_H__ */
