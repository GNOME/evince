SUBDIRS = mdvi-lib

INCLUDES = \
	-I$(top_srcdir)						\
	-I$(top_srcdir)/backend					\
	-I$(top_srcdir)/lib					\
	-I./mdvi-lib						\
	$(LIB_CFLAGS)

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

if WITH_TYPE1_FONTS
libgtkdvi_la_LIBADD += -lt1lib
endif





