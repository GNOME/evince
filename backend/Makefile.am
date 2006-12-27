INCLUDES=					\
	-DEVINCE_UIDIR=\"$(pkgdatadir)\"	\
	-DGNOMELOCALEDIR=\"$(datadir)/locale\"	\
	-I$(top_srcdir)/lib			\
	-I$(top_srcdir)/pdf			\
	-I$(top_srcdir)/pixbuf			\
	-I$(top_srcdir)/tiff			\
	-I$(top_srcdir)/ps	                \
	-I$(top_srcdir)/djvu			\
	-I$(top_srcdir)/dvi			\
	-I$(top_srcdir)/impress			\
	-I$(top_srcdir)/comics			\
	$(BACKEND_CFLAGS)			\
	$(WARN_CFLAGS)				\
	$(DISABLE_DEPRECATED)

noinst_LTLIBRARIES = libevbackend.la

libevbackend_la_SOURCES=			\
	ev-async-renderer.c			\
	ev-async-renderer.h			\
	ev-attachment.c				\
	ev-attachment.h				\
	ev-backend-marshal.c			\
	ev-link.c				\
	ev-link.h				\
	ev-link-action.c			\
	ev-link-action.h			\
	ev-link-dest.c				\
	ev-link-dest.h				\
	ev-document.c				\
	ev-document.h				\
	ev-document-factory.c			\
	ev-document-factory.h			\
	ev-document-thumbnails.c		\
	ev-document-thumbnails.h		\
	ev-document-fonts.c                     \
	ev-document-fonts.h			\
	ev-document-links.c			\
	ev-document-links.h			\
	ev-document-security.c			\
	ev-document-security.h			\
	ev-document-find.c			\
	ev-document-find.h			\
	ev-document-info.h			\
	ev-document-transition.h		\
	ev-document-transition.c		\
	ev-file-exporter.c			\
	ev-file-exporter.h			\
	ev-render-context.h			\
	ev-render-context.c			\
	ev-selection.h				\
	ev-selection.c				\
	ev-document-misc.h			\
	ev-document-misc.c

libevbackend_la_LIBADD =

if ENABLE_PDF
libevbackend_la_LIBADD += 			\
	$(top_builddir)/pdf/libpdfdocument.la
endif

if ENABLE_PS
libevbackend_la_LIBADD += 			\
	$(top_builddir)/ps/libpsdocument.la
endif

if ENABLE_PIXBUF
libevbackend_la_LIBADD += 			\
	$(top_builddir)/pixbuf/libpixbufdocument.la
endif

if ENABLE_DJVU
libevbackend_la_LIBADD += 			\
	$(top_builddir)/djvu/libgtkdjvu.la
endif

if ENABLE_TIFF
libevbackend_la_LIBADD += 			\
	$(top_builddir)/tiff/libtiffdocument.la
endif

if ENABLE_DVI
libevbackend_la_LIBADD += 			\
	$(top_builddir)/dvi/libgtkdvi.la
endif

if ENABLE_COMICS
libevbackend_la_LIBADD += 			\
	$(top_builddir)/comics/libcomicsdocument.la
endif

if ENABLE_IMPRESS
libevbackend_la_LIBADD += 			\
	$(top_builddir)/impress/libimpressdocument.la
endif

BUILT_SOURCES=				\
	ev-backend-marshalers.h		\
	ev-backend-marshalers.c

CLEANFILES = $(BUILT_SOURCES)

ev-backend-marshalers.h: ev-backend-marshalers.list
	$(GLIB_GENMARSHAL) --prefix=_ev_backend_marshal $(srcdir)/ev-backend-marshalers.list --header > $@

ev-backend-marshalers.c: ev-backend-marshalers.list
	$(GLIB_GENMARSHAL) --prefix=_ev_backend_marshal $(srcdir)/ev-backend-marshalers.list --body > $@

ev-backend-marshal.c: ev-backend-marshalers.h ev-backend-marshalers.c

noinst_HEADERS = \
	ev-backend-marshalers.h

EXTRA_DIST=			\
	ev-backend-marshalers.list
