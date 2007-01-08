SUBDIRS = mdvi-lib

INCLUDES = \
	-I$(top_srcdir)					\
	-I$(top_srcdir)/libdocument			\
	-I$(srcdir)/mdvi-lib				\
	$(LIB_CFLAGS)

noinst_LTLIBRARIES = libgtkdvi.la
    
libgtkdvi_la_SOURCES = \
	dvi-document.c	\
	dvi-document.h  \
	pixbuf-device.c \
	pixbuf-device.h \
	fonts.c		\
	fonts.h

libgtkdvi_la_LIBADD = mdvi-lib/libmdvi.la




