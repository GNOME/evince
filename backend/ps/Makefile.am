backenddir = $(libdir)/evince/backends

INCLUDES = 					\
	-I$(top_srcdir)				\
	-I$(top_srcdir)/libdocument		\
	-DGNOMELOCALEDIR=\"$(datadir)/locale\"	\
	$(BACKEND_CFLAGS)			\
	$(WARN_CFLAGS)				\
	$(DISABLE_DEPRECATED)

if HAVE_SPECTRE
INCLUDES += $(SPECTRE_CFLAGS)
endif

backend_LTLIBRARIES = libpsdocument.la

if HAVE_SPECTRE
libpsdocument_la_SOURCES = 	\
	ev-spectre.c		\
	ev-spectre.h
else
libpsdocument_la_SOURCES = 	\
	gsio.c			\
	gsio.h			\
	gstypes.h		\
	ps.c			\
	ps.h			\
	ps-document.c		\
	ps-document.h		\
	ps-interpreter.c	\
	ps-interpreter.h	\
	gsdefaults.c		\
	gsdefaults.h
endif

libpsdocument_la_LDFLAGS = $(BACKEND_LIBTOOL_FLAGS)
libpsdocument_la_LIBADD = 				\
	$(top_builddir)/libdocument/libevbackend.la	\
	$(BACKEND_LIBS)

if HAVE_SPECTRE
libpsdocument_la_LIBADD += $(SPECTRE_LIBS)
endif

backend_in_files = psdocument.evince-backend.in

%.evince-backend: %.evince-backend.in $(INTLTOOL_MERGE) $(wildcard $(top_srcdir)/po/*po) ; $(INTLTOOL_MERGE) $(top_srcdir)/po $< $@ -d -u -c $(top_builddir)/po/.intltool-merge-cache

backend_DATA = $(backend_in_files:.evince-backend.in=.evince-backend)

EXTRA_DIST = $(backend_in_files)

CLEANFILES = $(backend_DATA)