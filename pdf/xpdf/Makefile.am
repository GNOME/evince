
INCLUDES =							\
	-I$(top_srcdir)/goo					\
	-I$(top_srcdir)/ltk					\
	-DDATADIR=\""$(datadir)"\"            			\
        -DGNOMELOCALEDIR=\""$(prefix)/$(DATADIRNAME)/locale"\"	\
	-DGNOMEICONDIR=\""$(datadir)/pixmaps"\"			\
	$(EXTRA_GNOME_CFLAGS)					\
	$(libpaper_CFLAGS)

if WITH_GNOME
gui =
else
gui = xpdf
endif

BUILT_SOURCES = 			\
	gpdf-marshal.c			\
	gpdf-marshal.h	

bin_PROGRAMS = 	$(gui)
#	pdftops pdftotext	
#	pdfinfo	pdftopbm	
#	pdfimages  		

libexec_PROGRAMS = gnome-pdf-viewer

common_sources =		\
	Annot.cc		\
	Annot.h			\
	Array.cc 		\
	Array.h			\
	BaseFile.h		\
	BuiltinFont.cc		\
	BuiltinFont.h		\
	BuiltinFontTables.cc	\
	BuiltinFontTables.h	\
	CMap.cc			\
	CMap.h			\
	Catalog.cc 		\
	Catalog.h		\
	CharCodeToUnicode.cc	\
	CharCodeToUnicode.h	\
	CharTypes.h		\
	CompactFontTables.h	\
	Decrypt.cc		\
	Decrypt.h		\
	Dict.cc 		\
	Dict.h			\
	DisplayFontTable.h	\
	Error.cc 		\
	Error.h			\
	ErrorCodes.h		\
	FTFont.cc		\
	FTFont.h		\
	FontEncodingTables.cc	\
	FontEncodingTables.h	\
	FontFile.cc		\
	FontFile.h		\
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
	Lexer.cc 		\
	Lexer.h			\
	Link.cc 		\
	Link.h			\
	NameToCharCode.cc	\
	NameToCharCode.h	\
	NameToUnicodeTable.h	\
	Object.cc 		\
	Object.h		\
	OutputDev.cc 		\
	OutputDev.h		\
	PDFDoc.cc 		\
	PDFDoc.h		\
	PSTokenizer.cc		\
	PSTokenizer.h		\
	Page.cc 		\
	Page.h			\
	Parser.cc 		\
	Parser.h		\
	SFont.cc		\
	SFont.h			\
	Stream-CCITT.h		\
	Stream.cc 		\
	Stream.h		\
	T1Font.cc		\
	T1Font.h		\
	TTFont.cc		\
	TTFont.h		\
	TextOutputDev.cc	\
	TextOutputDev.h		\
	UTF8.h			\
	UnicodeMap.cc		\
	UnicodeMap.h		\
	UnicodeMapTables.h	\
	XRef.cc			\
	XRef.h

gnome_pdf_viewer_SOURCES =		\
	$(common_sources)		\
	BonoboStream.h			\
	BonoboStream.cc			\
	GPOutputDev.cc			\
	GPOutputDev.h			\
	bonobo-application-x-pdf.cc	\
	gpdf-control.cc			\
	gpdf-control.h			\
	gpdf-g-switch.h			\
	gpdf-persist-stream.cc		\
	gpdf-persist-stream.h		\
	gpdf-util.h			\
	gpdf-view.cc			\
	gpdf-view.h			\
	gtkgesture.c			\
	gtkgesture.h			\
	page-control.c			\
	page-control.h			\
	$(BUILT_SOURCES)

# Font embedding hack for Gnome Print < 2.1.?2?
#	gpdf-gnome-font.c		\
#	gpdf-gnome-font.h		\

# Old files
#	GOutputDev.cc			\
#	GOutputDev.h			\
#	pdf-view.cc			\
#	pdf-view.h			\
# 	gpdf-embeddable.cc		\
# 	gpdf-embeddable.h		\
# 	gpdf-embeddable-view.cc		\
# 	gpdf-embeddable-view.h

gnome_pdf_viewer_LDADD =		\
	$(top_builddir)/goo/libgoo.a	\
	$(EXTRA_GNOME_LIBS)		\
	$(libpaper_LIBS)

xpdf_SOURCES =			\
	$(common_sources)	\
	XOutputDev.cc		\
	LTKOutputDev.cc		\
	PSOutputDev.cc		\
	PSOutputDev.h		\
	xpdf.cc

xpdf_CFLAGS = $(X_CFLAGS)
xpdf_LDADD = 				\
	$(top_builddir)/goo/libgoo.a 	\
	$(top_builddir)/ltk/libltk.a    \
	-lXpm                           \
	$(EXTRA_GNOME_LIBS) 		\
	$(libpaper_LIBS)

#pdfimages_SOURCES =		
#	$(common_sources)	
#	ImageOutputDev.cc	
#	ImageOutputDev.h	
#	pdfimages.cc

#pdfimages_LDADD = ../goo/libgoo.a
#
#pdftops_SOURCES =		
#	$(common_sources)	
#	PSOutputDev.cc		
#       pdftops.cc
#pdftops_LDADD = ../goo/libgoo.a

#pdftotext_SOURCES =		
#	$(common_sources)	
#	pdftotext.cc
#pdftotext_LDADD = ../goo/libgoo.a

#pdfinfo_SOURCES =		
#	$(common_sources)	
#	pdfinfo.cc
#pdfinfo_LDADD = ../goo/libgoo.a

#pdftopbm_SOURCES =		
#	$(common_sources)	
#	PBMOutputDev.cc		
#	PBMOutputDev.h		
#	XOutputDev.cc		
#	XOutputDev.h		
#	XOutputFontInfo.h	
#	pdftopbm.cc

#pdftopbm_LDADD = ../goo/libgoo.a $(EXTRA_GNOME_LIBS)

xpdf-ltk.h: xpdf.ltk
	rm -f $@
	$(top_srcdir)/ltk/ltkbuild <xpdf.ltk >$@.new
	mv $@.new $@

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

stockdir = $(datadir)/pixmaps/gpdf
stock_DATA = fitwidth.png

ltk = 			\
	xpdf-flip.ltk	\
	xpdf-top.ltk	\
	xpdf.ltk

server_in_files = GNOME_PDF.server.in.in

serverdir = $(libdir)/bonobo/servers
server_DATA = $(server_in_files:.server.in.in=.server)
$(server_in_files:.server.in.in=.server.in): $(server_in_files)
	sed -e "s|\@LIBEXECDIR\@|$(libexecdir)|" $< > $@
@INTLTOOL_SERVER_RULE@

uidir = $(datadir)/gnome-2.0/ui
ui_DATA =				\
	bonobo-application-x-pdf-ui.xml \
	gpdf-control-ui.xml

CLEANFILES = $(server_DATA) $(BUILT_SOURCES)
DISTCLEANFILES = $(server_in_files:.server.in.in=.server.in)

EXTRA_DIST = $(stock_DATA)		\
             gpdf-marshal.list		\
             $(server_in_files) config.h $(ui_DATA)
