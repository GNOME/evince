INCLUDES =							\
	-I$(top_srcdir)						\
	-I$(top_srcdir)/backend					\
	$(TIFF_CFLAGS)						\
	-DDATADIR=\""$(datadir)"\"

noinst_LTLIBRARIES = libtiffdocument.la

libtiffdocument_la_SOURCES =	\
	tiff-document.c		\
	tiff-document.h		\
	tiff2ps.c		\
	tiff2ps.h

libtiffdocument_la_LIBADD = 	\
	$(TIFF_LIBS)		\
	-ltiff
