INCLUDES=						\
	-DEVINCE_UIDIR=\"$(pkgdatadir)\"		\
	-DGNOMELOCALEDIR=\"$(datadir)/locale\"		\
	-DEV_BACKENDSDIR=\"$(libdir)/evince/backends\"	\
	$(LIB_CFLAGS)					\
	$(WARN_CFLAGS)					\
	$(DISABLE_DEPRECATED)

lib_LTLIBRARIES = libevbackend.la

libevbackend_la_LDFLAGS = -export-dynamic
libevbackend_la_LIBADD = $(LIB_LIBS)

NOINST_H_FILES =				\
	ev-backend-marshalers.h			\
	ev-backends-manager.h			\
	ev-document-factory.h			\
	ev-module.h

INST_H_FILES = 					\
	ev-async-renderer.h			\
	ev-attachment.h				\
	ev-document-find.h			\
	ev-document-fonts.h			\
	ev-document-forms.h			\
	ev-document.h				\
	ev-document-images.h			\
	ev-document-info.h			\
	ev-document-links.h			\
	ev-document-misc.h			\
	ev-document-security.h			\
	ev-document-thumbnails.h		\
	ev-document-transition.h		\
	ev-file-exporter.h			\
	ev-file-helpers.h			\
	ev-form-field.h				\
	ev-image.h				\
	ev-link-action.h			\
	ev-link-dest.h				\
	ev-link.h				\
	ev-page.h				\
	ev-render-context.h			\
	ev-selection.h				\
	ev-transition-effect.h

headerdir = $(prefix)/include/evince-@EV_API_VERSION@/evince
header_DATA = $(INST_H_FILES)

libevbackend_la_SOURCES=			\
	ev-async-renderer.c			\
	ev-attachment.c				\
	ev-backends-manager.c			\
	ev-backend-marshal.c			\
	ev-link.c				\
	ev-link-action.c			\
	ev-link-dest.c				\
	ev-image.c				\
	ev-document.c				\
	ev-document-factory.c			\
	ev-document-thumbnails.c		\
	ev-document-fonts.c                     \
	ev-document-links.c			\
	ev-document-images.c			\
	ev-document-security.c			\
	ev-document-find.c			\
	ev-document-transition.c		\
	ev-document-forms.c			\
	ev-form-field.c 			\
	ev-file-exporter.c			\
	ev-file-helpers.c			\
	ev-module.c				\
	ev-page.c				\
	ev-render-context.c			\
	ev-selection.c				\
	ev-transition-effect.c			\
	ev-document-misc.c			\
	$(NOINST_H_FILES)			\
	$(INST_H_FILES)

BUILT_SOURCES=				\
	ev-backend-marshalers.h		\
	ev-backend-marshalers.c

CLEANFILES = $(BUILT_SOURCES)

ev-backend-marshalers.h: ev-backend-marshalers.list
	$(GLIB_GENMARSHAL) --prefix=_ev_backend_marshal $(srcdir)/ev-backend-marshalers.list --header > $@

ev-backend-marshalers.c: ev-backend-marshalers.list
	$(GLIB_GENMARSHAL) --prefix=_ev_backend_marshal $(srcdir)/ev-backend-marshalers.list --body > $@

ev-backend-marshal.c: ev-backend-marshalers.h ev-backend-marshalers.c

EXTRA_DIST= \
	ev-backend-marshalers.list
