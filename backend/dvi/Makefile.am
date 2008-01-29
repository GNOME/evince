SUBDIRS = mdvi-lib

backenddir = $(libdir)/evince/backends

INCLUDES = 					\
	-I$(top_srcdir)				\
	-I$(top_srcdir)/libdocument		\
	-I$(srcdir)/mdvi-lib			\
	-DGNOMELOCALEDIR=\"$(datadir)/locale\"	\
	$(BACKEND_CFLAGS)			\
	$(SPECTRE_CFLAGS)

backend_LTLIBRARIES = libdvidocument.la

libdvidocument_la_SOURCES = 	\
	dvi-document.c		\
	dvi-document.h  	\
	cairo-device.c		\
	cairo-device.h		\
	fonts.c			\
	fonts.h

libdvidocument_la_LDFLAGS = $(BACKEND_LIBTOOL_FLAGS)
libdvidocument_la_LIBADD = 				\
	mdvi-lib/libmdvi.la				\
	-lkpathsea					\
	$(top_builddir)/libdocument/libevbackend.la	\
	$(BACKEND_LIBS)					\
	$(SPECTRE_LIBS)

if WITH_TYPE1_FONTS
libdvidocument_la_LIBADD += -lt1
endif

backend_in_files = dvidocument.evince-backend.in

%.evince-backend: %.evince-backend.in $(INTLTOOL_MERGE) $(wildcard $(top_srcdir)/po/*po) ; $(INTLTOOL_MERGE) $(top_srcdir)/po $< $@ -d -u -c $(top_builddir)/po/.intltool-merge-cache

backend_DATA = $(backend_in_files:.evince-backend.in=.evince-backend)

EXTRA_DIST = $(backend_in_files)

CLEANFILES = $(backend_DATA)




