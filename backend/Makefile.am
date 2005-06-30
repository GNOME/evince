NULL=

INCLUDES=					\
	-DEVINCE_UIDIR=\"$(pkgdatadir)\"	\
	-DGNOMELOCALEDIR=\"$(datadir)/locale\"	\
	$(SHELL_CFLAGS)				\
	$(EVINCE_DISABLE_DEPRECATED)		\
	$(NULL)

noinst_LTLIBRARIES = libevbackend.la

libevbackend_la_SOURCES=			\
	ev-async-renderer.c			\
	ev-async-renderer.h			\
	ev-backend-marshal.c			\
	ev-link.c				\
	ev-link.h				\
	ev-document.c				\
	ev-document.h				\
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
	ev-ps-exporter.c			\
	ev-ps-exporter.h			\
	ev-render-context.h			\
	ev-render-context.c			\
	ev-selection.h				\
	ev-selection.c				\
	ev-document-misc.h			\
	ev-document-misc.c			\
	$(NULL)

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
