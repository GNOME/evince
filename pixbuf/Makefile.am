INCLUDES =							\
	-I$(top_srcdir)						\
	-I$(top_srcdir)/backend					\
	$(PIXBUF_CFLAGS)					\
	-DDATADIR=\""$(datadir)"\"

noinst_LTLIBRARIES = libpixbufdocument.la

libpixbufdocument_la_SOURCES =		\
	pixbuf-document.c		\
	pixbuf-document.h

libpixbufdocument_la_LIBADD = $(PIXBUF_LIBS)
