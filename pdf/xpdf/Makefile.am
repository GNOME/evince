
INCLUDES =					\
	-I$(top_srcdir)/goo			\
	-I$(top_srcdir)/ltk			\
	-DDATADIR=\""$(datadir)"\"            	\
	-DGNOMELOCALEDIR=\""$(datadir)/locale"\" \
	$(EXTRA_GLIB_CFLAGS)			\
	$(EXTRA_GNOME_CFLAGS)			\
	$(libpaper_CFLAGS)

if WITH_GNOME
gui = gpdf
else
gui = xpdf
endif

bin_PROGRAMS = 	$(gui)	bonobo-application-x-pdf
#	pdftops pdftotext	
#	pdfinfo	pdftopbm	
#	pdfimages  		


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

gpdf_CFLAGS = 			\
	-DBONOBO_IO

gpdf_SOURCES = 			\
	gpdf.cc

gpdf_LDADD =			\
	$(GTK_LIBS)		\
	$(GNOME_LIBDIR)		\
	$(EXTRA_GNOME_LIBS)	\
	$(INTLLIBS)

bonobo_application_x_pdf_SOURCES =	\
	$(common_sources)		\
	BonoboStream.h			\
	BonoboStream.cc			\
	GOutputDev.cc			\
	GOutputDev.h			\
	bonobo-application-x-pdf.cc	\
	bonobo-application-x-pdf.h	\
	gtkgesture.c			\
	gtkgesture.h

bonobo_application_x_pdf_LDADD =	\
	../goo/libgoo.a			\
	$(GTK_LIBS)			\
	$(GNOME_LIBDIR)			\
	$(EXTRA_GNOME_LIBS)		\
	$(EXTRA_GLIB_LIBS)		\
	$(INTLLIBS)			\
	$(libpaper_CFLAGS)

xpdf_SOURCES =			\
	$(common_sources)	\
	XOutputDev.cc		\
	LTKOutputDev.cc		\
	PSOutputDev.cc		\
	PSOutputDev.h		\
	xpdf.cc

xpdf_CFLAGS = $(X_CFLAGS)
xpdf_LDADD = ../goo/libgoo.a ../ltk/libltk.a -lXpm $(EXTRA_GNOME_LIBS) $(libpaper_CFLAGS)

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


stockdir = $(datadir)/pixmaps/gpdf
stock_DATA = stock-zoom-in.xpm	stock-zoom-out.xpm \
	     stock-zoom-fit.xpm stock-zoom-fit-wide.xpm	\
	     stock-zoom-fit-tall.xpm	

ltk = 			\
	xpdf-flip.ltk	\
	xpdf-top.ltk	\
	xpdf.ltk

oafdir = $(datadir)/oaf
oaf_DATA = GNOME_XPDF.oafinfo

XML_FILES =				\
	gpdf-ui.xml			\
	bonobo-application-x-pdf-ui.xml

uidir = $(datadir)/gnome/ui
ui_DATA = $(XML_FILES)

EXTRA_DIST = $(stock_DATA) ChangeLog $(ltk)	\
             $(oaf_DATA) config.h $(XML_FILES)


