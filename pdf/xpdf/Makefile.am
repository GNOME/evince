INCLUDES =					\
	-I$(top_srcdir)/goo			\
	-I$(top_srcdir)/ltk			\
	-DGPDF_GLADE_DIR=\""$(gladedir)"\"	\
	$(EXTRA_GLIB_CFLAGS)			\
	$(EXTRA_GNOME_CFLAGS)

if WITH_GNOME
gui = gpdf
else
gui = xpdf
endif

bin_PROGRAMS = pdftops pdftotext pdfinfo pdftopbm pdfimages $(gui) bonobo-image-x-pdf

common_sources =		\
	Array.cc 		\
	BaseFile.h		\
	Catalog.cc 		\
	Dict.cc 		\
	Error.cc 		\
	FontEncoding.cc		\
	FontEncoding.h		\
	FontFile.cc		\
	FontFile.h		\
	FontInfo.h		\
	Gfx.cc 			\
	GfxFont.cc 		\
	GfxState.cc		\
	Lexer.cc 		\
	Link.cc 		\
	Object.cc 		\
	OutputDev.cc 		\
	Page.cc 		\
	Params.cc 		\
	Parser.cc 		\
	PDFDoc.cc 		\
	Stream.cc 		\
	TextOutputDev.cc	\
	XRef.cc

gpdf_CFLAGS = 			\
	-DBONOBO_IO

gpdf_SOURCES = 			\
	gpdf.cc

gpdf_LDADD =			\
	$(GTK_LIBS)		\
	$(GNOME_LIBDIR)		\
	-lbonobo		\
	$(GNOMEGNORBA_LIBS)	\
	$(EXTRA_GNOME_LIBS)	\
	$(INTLLIBS)

bonobo_image_x_pdf_SOURCES =	\
	$(common_sources)	\
	BonoboStream.h		\
	BonoboStream.cc		\
	GOutputDev.cc		\
	bonobo-image-x-pdf.cc

bonobo_image_x_pdf_LDADD =	\
	../goo/libgoo.a		\
	$(GTK_LIBS)		\
	$(GNOME_LIBDIR)		\
	-lbonobo		\
	$(GNOMEGNORBA_LIBS)	\
	$(EXTRA_GLIB_LIBS)	\
	$(INTLLIBS)

xpdf_SOURCES =			\
	$(common_sources)	\
	XOutputDev.cc		\
	LTKOutputDev.cc		\
	PSOutputDev.cc		\
	xpdf.cc

xpdf_CFLAGS = $(X_CFLAGS)
xpdf_LDADD = ../goo/libgoo.a ../ltk/libltk.a -lXpm $(EXTRA_GNOME_LIBS) 

pdfimages_SOURCES =		\
	$(common_sources)	\
	ImageOutputDev.cc	\
	pdfimages.cc

pdfimages_LDADD = ../goo/libgoo.a

pdftops_SOURCES =		\
	$(common_sources)	\
	PSOutputDev.cc		\
	pdftops.cc
pdftops_LDADD = ../goo/libgoo.a

pdftotext_SOURCES =		\
	$(common_sources)	\
	pdftotext.cc
pdftotext_LDADD = ../goo/libgoo.a

pdfinfo_SOURCES =		\
	$(common_sources)	\
	pdfinfo.cc
pdfinfo_LDADD = ../goo/libgoo.a

pdftopbm_SOURCES =		\
	$(common_sources)	\
	PBMOutputDev.cc		\
	XOutputDev.cc		\
	pdftopbm.cc

pdftopbm_LDADD = ../goo/libgoo.a $(EXTRA_GNOME_LIBS)

xpdf-ltk.h: xpdf.ltk
	rm -f $@
	$(top_srcdir)/ltk/ltkbuild <xpdf.ltk >$@.new
	mv $@.new $@

bitmaps =			\
	about.xbm		\
	dblLeftArrow.xbm	\
	dblRightArrow.xbm	\
	find.xbm		\
	leftArrow.xbm		\
	postscript.xbm		\
	rightArrow.xbm		\
	xpdfIcon.xpm		\
	zoomIn.xbm		\
	zoomOut.xbm		

ltk = 			\
	xpdf-flip.ltk	\
	xpdf-top.ltk	\
	xpdf.ltk

gladedir   = $(datadir)/xpdf/glade

glade_DATA = 		\
	about.glade	\
	gpdf.glade

gnorbadir = $(sysconfdir)/CORBA/servers
gnorba_DATA = image-x-pdf.gnorba

EXTRA_DIST = $(bitmaps) ChangeLog $(ltk) $(glade_DATA) $(gnorba_DATA)
