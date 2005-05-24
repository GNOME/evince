INCLUDES =							\
	-I$(top_srcdir)						\
	-I$(top_srcdir)/backend					\
	$(GTK_CFLAGS)						\
	-DDATADIR=\""$(datadir)"\"

noinst_LTLIBRARIES = libtiffdocument.la

libtiffdocument_la_SOURCES =	\
	tiff-document.c		\
	tiff-document.h

libtiffdocument_la_LIBADD = \
	-ltiff -lz

