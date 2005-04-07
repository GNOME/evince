SUBDIRS = mdvi-lib

INCLUDES = \
	-I$(top_srcdir)						\
	-I$(top_srcdir)/backend					\
	-I$(top_srcdir)/lib					\
	-I./mdvi-lib						\
	$(DVI_CFLAGS)

noinst_LTLIBRARIES = libgtkdvi.la
    
libgtkdvi_la_SOURCES = \
	dvi-document.c	\
	dvi-document.h  \
	pixbuf-device.c \
	pixbuf-device.h \
	fonts.c		\
	fonts.h

libgtkdvi_la_LIBADD = mdvi-lib/libmdvi.la	\
	$(DVI_LIBS) -lkpathsea





