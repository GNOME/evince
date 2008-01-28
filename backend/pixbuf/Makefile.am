backenddir = $(libdir)/evince/backends

INCLUDES =					\
	-I$(top_srcdir)				\
	-I$(top_srcdir)/libdocument		\
	-DGNOMELOCALEDIR=\"$(datadir)/locale\"	\
	$(BACKEND_CFLAGS)			\
	$(WARN_CFLAGS)				\
	$(DISABLE_DEPRECATED)

backend_LTLIBRARIES = libpixbufdocument.la

libpixbufdocument_la_SOURCES =		\
	pixbuf-document.c		\
	pixbuf-document.h

libpixbufdocument_la_LDFLAGS = $(BACKEND_LIBTOOL_FLAGS)
libpixbufdocument_la_LIBADD =				\
	$(top_builddir)/libdocument/libevbackend.la	\
	$(BACKEND_LIBS)

backend_in_files = pixbufdocument.evince-backend.in

%.evince-backend: %.evince-backend.in $(INTLTOOL_MERGE) $(wildcard $(top_srcdir)/po/*po) ; $(INTLTOOL_MERGE) $(top_srcdir)/po $< $@ -d -u -c $(top_builddir)/po/.intltool-merge-cache

backend_DATA = $(backend_in_files:.evince-backend.in=.evince-backend)

EXTRA_DIST = $(backend_in_files)

CLEANFILES = $(backend_DATA)
