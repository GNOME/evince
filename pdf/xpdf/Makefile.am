SUBDIRS = . tests

INCLUDES =							\
	-I$(top_srcdir)						\
	-I$(top_srcdir)/goo					\
	-I$(top_srcdir)/lib/ggv-sidebar				\
	-I$(top_srcdir)/fofi					\
	-I$(top_srcdir)/splash					\
	-DDATADIR=\""$(datadir)"\"            			\
        -DGNOMELOCALEDIR=\""$(prefix)/$(DATADIRNAME)/locale"\"	\
	-DGNOMEICONDIR=\""$(datadir)/pixmaps"\"			\
	$(EXTRA_GNOME_CFLAGS)					\
	$(GNOME_PRINT_PRIVATE_CFLAGS)				\
	$(GPDF_DEPRECATED_CFLAGS)				\
	$(libpaper_CFLAGS)

if WITH_GNOME
gui =
else
gui = xpdf
endif

marshal_sources =			\
	gpdf-marshal.c			\
	gpdf-marshal.h	

bin_PROGRAMS = 	$(gui)
#	pdftops pdftotext	
#	pdfinfo	pdftopbm	
#	pdfimages  		

libexec_PROGRAMS = gnome-pdf-viewer

noinst_LIBRARIES = libxpdf.a libgpdf.a

libxpdf_a_SOURCES =		\
	Annot.cc		\
	Annot.h			\
	Array.cc 		\
	Array.h			\
	BaseFile.h		\
	BuiltinFont.cc		\
	BuiltinFont.h		\
	BuiltinFontTables.cc	\
	BuiltinFontTables.h	\
	Catalog.cc 		\
	Catalog.h		\
	CharCodeToUnicode.cc	\
	CharCodeToUnicode.h	\
	CMap.cc			\
	CMap.h			\
	Decrypt.cc		\
	Decrypt.h		\
	Dict.cc 		\
	Dict.h			\
	Error.cc 		\
	Error.h			\
	FontEncodingTables.cc	\
	FontEncodingTables.h	\
	Function.cc		\
	Function.h		\
	Gfx.cc 			\
	Gfx.h			\
	GfxFont.cc 		\
	GfxFont.h		\
	GfxState.cc		\
	GfxState.h		\
	GlobalParams.cc		\
	GlobalParams.h		\
	JArithmeticDecoder.cc	\
	JArithmeticDecoder.h	\
	JBIG2Stream.cc		\
	JBIG2Stream.h		\
	JPXStream.cc		\
	JPXStream.h		\
	Lexer.cc 		\
	Lexer.h			\
	Link.cc 		\
	Link.h			\
	NameToCharCode.cc	\
	NameToCharCode.h	\
	Object.cc 		\
	Object.h		\
	Outline.cc		\
	Outline.h		\
	OutputDev.cc 		\
	OutputDev.h		\
	Page.cc 		\
	Page.h			\
	Parser.cc 		\
	Parser.h		\
	PDFDoc.cc 		\
	PDFDoc.h		\
	PDFDocEncoding.cc	\
	PDFDocEncoding.h	\
	PSTokenizer.cc		\
	PSTokenizer.h		\
	Stream-CCITT.h		\
	Stream.cc 		\
	Stream.h		\
	UnicodeMap.cc		\
	UnicodeMap.h		\
	UnicodeMapTables.h	\
	UnicodeTypeTable.cc	\
	UnicodeTypeTable.h	\
	XRef.cc			\
	XRef.h

common_sources =		\
	CharTypes.h		\
	CompactFontTables.h	\
	ErrorCodes.h		\
	NameToUnicodeTable.h	\
	TextOutputDev.cc	\
	TextOutputDev.h		\
	UTF8.h

ANNOTS_VIEW_SRCS =

if USE_ANNOTS_VIEW
  ANNOTS_VIEW_SRCS += 			\
	gpdf-annots-view.cc		\
	gpdf-annots-view.h
endif

libgpdf_a_SOURCES =			\
	BonoboStream.cc			\
	BonoboStream.h			\
	GnomeVFSStream.cc		\
	GnomeVFSStream.h		\
	GPOutputDev.cc			\
	GPOutputDev.h			\
	PSOutputDev.cc			\
	PSOutputDev.h			\
	gpdf-hig-dialog.c		\
	gpdf-hig-dialog.h		\
	Thumb.cc			\
	Thumb.h				\
	gpdf-control.cc			\
	gpdf-control.h			\
	gpdf-control-private.h		\
	gpdf-g-switch.h			\
	gpdf-link-canvas-item.cc	\
	gpdf-link-canvas-item.h		\
	gpdf-links-canvas-layer.cc	\
	gpdf-links-canvas-layer.h	\
	gpdf-persist-file.cc		\
	gpdf-persist-file.h		\
	gpdf-persist-stream.cc		\
	gpdf-persist-stream.h		\
	gpdf-stock-icons.c		\
	gpdf-stock-icons.h		\
	gpdf-util.h			\
	gpdf-view.cc			\
	gpdf-view.h			\
	gpdf-bookmarks-view.cc		\
	gpdf-bookmarks-view.h		\
	gpdf-thumbnails-view.cc		\
	gpdf-thumbnails-view.h		\
	$(ANNOTS_VIEW_SRCS)		\
	eel-gconf-extensions.c		\
	eel-gconf-extensions.h		\
	prefs-strings.h			\
	gpdf-sidebar.c			\
	gpdf-sidebar.h			\
	gtkgesture.c			\
	gtkgesture.h			\
	page-control.c			\
	page-control.h			\
	pdf-info-dict-util.cc		\
	pdf-info-dict-util.h		\
	pdf-properties-display.c	\
	pdf-properties-display.h	\
	gpdf-font-face.c		\
	gpdf-font-face.h		\
	$(marshal_sources)

gnome_pdf_viewer_SOURCES =		\
	$(common_sources)		\
	bonobo-application-x-pdf.cc	\
	nautilus-pdf-property-page.cc	\
	nautilus-pdf-property-page.h


# Old files
#	GOutputDev.cc			\
#	GOutputDev.h			\
#	pdf-view.cc			\
#	pdf-view.h			\
# 	gpdf-embeddable.cc		\
# 	gpdf-embeddable.h		\
# 	gpdf-embeddable-view.cc		\
# 	gpdf-embeddable-view.h

gnome_pdf_viewer_LDADD =					\
	libxpdf.a						\
	libgpdf.a						\
	$(top_builddir)/goo/libGoo.a				\
	$(top_builddir)/fofi/libfofi.a				\
	$(top_builddir)/lib/ggv-sidebar/libggv-sidebar.a	\
	$(EXTRA_GNOME_LIBS)					\
	$(libpaper_LIBS)

xpdf_SOURCES =			\
	$(common_sources)	\
	SplashOutputDev.cc	\
	SplashOutputDev.h	\
	XPDFApp.cc		\
	XPDFApp.h		\
	XPDFCore.cc		\
	XPDFCore.h		\
	XPDFTree.cc		\
	XPDFTree.h		\
	XPDFViewer.cc		\
	XPDFViewer.h		\
	XSplashOutputDev.cc	\
	XSplashOutputDev.h	\
	PSOutputDev.cc		\
	PSOutputDev.h		\
	xpdf.cc

xpdf_CFLAGS = $(X_CFLAGS)
xpdf_LDADD = 				\
	libxpdf.a			\
	$(top_builddir)/goo/libGoo.a 	\
	-lXpm                           \
	$(EXTRA_GNOME_LIBS) 		\
	$(libpaper_LIBS)

#pdfimages_SOURCES =		
#	$(common_sources)	
#	ImageOutputDev.cc	
#	ImageOutputDev.h	
#	pdfimages.cc

#pdfimages_LDADD = ../goo/libGoo.a
#
#pdftops_SOURCES =		
#	$(common_sources)	
#	PSOutputDev.cc		
#       pdftops.cc
#pdftops_LDADD = ../goo/libGoo.a

#pdftotext_SOURCES =		
#	$(common_sources)	
#	pdftotext.cc
#pdftotext_LDADD = ../goo/libGoo.a

#pdfinfo_SOURCES =		
#	$(common_sources)	
#	pdfinfo.cc
#pdfinfo_LDADD = ../goo/libGoo.a

#pdftopüm_SOURCES =		
#	$(common_sources)	
#	XOutputDev.cc		
#	XOutputDev.h		
#	XOutputFontInfo.h	
#	pdftopbm.cc

#pdftoppm_LDADD = ../goo/libGoo.a $(EXTRA_GNOME_LIBS)

#bitmaps =		
#	about.xbm		
#	dblLeftArrow.xbm	
#	dblRightArrow.xbm	
#	find.xbm		
#	leftArrow.xbm		
#	postscript.xbm		
#	rightArrow.xbm		
#	xpdfIcon.xpm		
#	zoomIn.xbm		
#	zoomOut.xbm		

gpdf-marshal.h: gpdf-marshal.list $(GLIB_GENMARSHAL)
	$(GLIB_GENMARSHAL) $< --header --prefix=gpdf_marshal > $@

gpdf-marshal.c: gpdf-marshal.list $(GLIB_GENMARSHAL)
	echo "#include \"gpdf-marshal.h\"" > $@ && \
	$(GLIB_GENMARSHAL) $< --body --prefix=gpdf_marshal >> $@

BUILT_SOURCES = $(marshal_sources)
DONT_DIST_SOURCE = $(marshal_sources)

dist-hook:
	for file in $(DONT_DIST_SOURCE) ; do \
	    rm -f $(distdir)/$$file ; \
	done

STOCK_ANNOTS_ICONS = 

if USE_ANNOTS_VIEW
  STOCK_ANNOTS_ICONS += 			\
	stock-annot-circle.png		\
	stock-annot-fileattachment.png	\
	stock-annot-freetext.png	\
	stock-annot-highlight.png	\
	stock-annot-ink.png		\
	stock-annot-line.png		\
	stock-annot-link.png		\
	stock-annot-movie.png		\
	stock-annot-popup.png		\
	stock-annot-sound.png		\
	stock-annot-square.png		\
	stock-annot-stamp.png		\
	stock-annot-strikeout.png	\
	stock-annot-text.png		\
	stock-annot-trapnet.png		\
	stock-annot-underline.png	\
	stock-annot-widget.png		\
	stock-annot-unknown.png
endif

stockdir = $(datadir)/pixmaps/gpdf
stock_DATA = 				\
	fitwidth.png			\
	stock_book-closed-mark.png	\
	stock_book-closed.png		\
	stock_book-opened-mark.png	\
	stock_book-opened.png		\
	stock_bookmarks.png		\
	$(STOCK_ANNOTS_ICONS)

gladedir = $(datadir)/gpdf/glade
glade_DATA =					\
	gpdf-print-progress-dialog.glade	\
	gpdf-properties-dialog.glade

server_in_files = GNOME_PDF.server.in.in
serverdir = $(libdir)/bonobo/servers
server_DATA = $(server_in_files:%.server.in.in=%.server)

$(server_in_files:%.server.in.in=%.server.in): $(server_in_files)
	sed -e "s|\@LIBEXECDIR\@|$(libexecdir)|" $< > $@

@INTLTOOL_SERVER_RULE@

uidir = $(datadir)/gnome-2.0/ui
ui_DATA =				\
	gpdf-control-ui.xml

CLEANFILES =				\
	$(server_DATA)			\
	$(server_in_files:.server.in.in=.server.in) \
	$(marshal_sources)

if USE_ANNOTS_VIEW
  EXTRA_DIST = $(stock_DATA)		\
	gpdf-marshal.list		\
	$(server_in_files) 		\
	config.h 			\
	$(ui_DATA)			\
	$(glade_DATA)
else
  EXTRA_DIST = $(stock_DATA)		\
	gpdf-marshal.list		\
	$(server_in_files) 		\
	config.h 			\
	$(ui_DATA)			\
	$(glade_DATA)			\
	stock-annot-circle.png		\
	stock-annot-fileattachment.png	\
	stock-annot-freetext.png	\
	stock-annot-highlight.png	\
	stock-annot-ink.png		\
	stock-annot-line.png		\
	stock-annot-link.png		\
	stock-annot-movie.png		\
	stock-annot-popup.png		\
	stock-annot-sound.png		\
	stock-annot-square.png		\
	stock-annot-stamp.png		\
	stock-annot-strikeout.png	\
	stock-annot-text.png		\
	stock-annot-trapnet.png		\
	stock-annot-underline.png	\
	stock-annot-widget.png		\
	stock-annot-unknown.png
endif
