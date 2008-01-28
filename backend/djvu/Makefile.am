backenddir = $(libdir)/evince/backends

INCLUDES = \
	-I$(top_srcdir)						\
	-I$(top_srcdir)/libdocument				\
	-DGNOMEICONDIR=\""${prefix}/${DATADIRNAME}/pixmaps"\" 	\
	-DGNOMELOCALEDIR=\"$(datadir)/locale\"			\
	$(BACKEND_CFLAGS)					\
	$(DJVU_CFLAGS)						\
	$(WARN_CFLAGS)						\
	$(DISABLE_DEPRECATED)

backend_LTLIBRARIES = libdjvudocument.la

libdjvudocument_la_SOURCES = 	\
	djvu-document.c		\
	djvu-document.h 	\
	djvu-document-private.h \
	djvu-links.c		\
	djvu-links.h		\
	djvu-text.c		\
	djvu-text.h		\
	djvu-text-page.c 	\
	djvu-text-page.h

libdjvudocument_la_LDFLAGS = $(BACKEND_LIBTOOL_FLAGS)
libdjvudocument_la_LIBADD =				\
	$(top_builddir)/libdocument/libevbackend.la	\
	$(BACKEND_LIBS)					\
	$(DJVU_LIBS)

backend_in_files = djvudocument.evince-backend.in

%.evince-backend: %.evince-backend.in $(INTLTOOL_MERGE) $(wildcard $(top_srcdir)/po/*po) ; $(INTLTOOL_MERGE) $(top_srcdir)/po $< $@ -d -u -c $(top_builddir)/po/.intltool-merge-cache

backend_DATA = $(backend_in_files:.evince-backend.in=.evince-backend)

EXTRA_DIST = $(backend_in_files)

CLEANFILES = $(backend_DATA)




