INCLUDES =							\
	-I$(top_srcdir)						\
	-I$(top_srcdir)/backend					\
	-I$(top_srcdir)/pdf					\
	-I$(top_srcdir)/pdf/goo					\
	-I$(top_srcdir)/pdf/fofi				\
	-I$(top_srcdir)/pdf/splash				\
	$(GTK_CFLAGS)						\
	-DDATADIR=\""$(datadir)"\"

noinst_PROGRAMS = test-gdk-output-dev

noinst_LTLIBRARIES = libxpdf.la libpdfdocument.la

libxpdf_la_SOURCES =		\
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
	SplashOutputDev.cc      \
	SplashOutputDev.h       \
	Stream-CCITT.h		\
	Stream.cc 		\
	Stream.h		\
	UnicodeMap.cc		\
	UnicodeMap.h		\
	UnicodeMapTables.h	\
	UnicodeTypeTable.cc	\
	UnicodeTypeTable.h	\
	XRef.cc			\
	XRef.h			\
				\
	CharTypes.h		\
	CompactFontTables.h	\
	ErrorCodes.h		\
	NameToUnicodeTable.h	\
	GDKSplashOutputDev.cc   \
	GDKSplashOutputDev.h    \
	PSOutputDev.cc		\
	PSOutputDev.h		\
	TextOutputDev.cc	\
	TextOutputDev.h		\
	UTF8.h

# The sources at the end are put in libxpdf.a for convenience here, but are 
# conceptually part of the apps not part of the libs.

test_gdk_output_dev_SOURCES =          \
       test-gdk-output-dev.cc

test_gdk_output_dev_LDADD =                                   	\
	libxpdf.la                                              \
	$(top_builddir)/pdf/goo/libGoo.la                       \
	$(top_builddir)/pdf/fofi/libfofi.la                     \
	$(top_builddir)/pdf/splash/libsplash.la                 \
	$(top_builddir)/backend/libevbackend.la			\
	$(GTK_LIBS)

libpdfdocument_la_SOURCES =	\
	pdf-document.cc		\
	pdf-document.h

libpdfdocument_la_LIBADD =                                    	\
	libxpdf.la                                             	\
	$(top_builddir)/backend/libevbackend.la			\
	$(top_builddir)/pdf/goo/libGoo.la                       \
	$(top_builddir)/pdf/fofi/libfofi.la                     \
	$(top_builddir)/pdf/splash/libsplash.la

EXTRA_DIST = config.h
