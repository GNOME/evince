INCLUDES =							\
	-I$(top_srcdir)						\
	-I$(top_srcdir)/pdf					\
	-I$(top_srcdir)/pdf/goo					\
	-I$(top_srcdir)/pdf/fofi				\
	-I$(top_srcdir)/pdf/splash				\
	$(GTK_CFLAGS)						\
	-DDATADIR=\""$(datadir)"\"

noinst_PROGRAMS = test-gdk-output-dev

noinst_LIBRARIES = libxpdf.a

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

test_gdk_output_dev_SOURCES =          \
       GDKSplashOutputDev.cc           \
       GDKSplashOutputDev.h            \
       SplashOutputDev.cc              \
       SplashOutputDev.h               \
       $(common_sources)               \
       test-gdk-output-dev.cc

test_gdk_output_dev_LDADD = 					\
        libxpdf.a                                               \
        $(top_builddir)/pdf/goo/libGoo.a                        \
        $(top_builddir)/pdf/fofi/libfofi.a                      \
	$(top_builddir)/pdf/splash/libsplash.a			\
	$(GTK_LIBS)

EXTRA_DIST = config.h
