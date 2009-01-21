INCLUDES=						\
	-DEVINCE_UIDIR=\"$(pkgdatadir)\"		\
	-DGNOMELOCALEDIR=\"$(datadir)/locale\"		\
	-DEV_BACKENDSDIR=\"$(libdir)/evince/backends\"	\
	$(LIBDOCUMENT_CFLAGS)				\
	$(WARN_CFLAGS)					\
	$(DISABLE_DEPRECATED)

lib_LTLIBRARIES = libevbackend.la

libevbackend_la_LDFLAGS = -export-dynamic
libevbackend_la_LIBADD = $(LIBDOCUMENT_LIBS)

NOINST_H_FILES =				\
	ev-debug.h				\
	ev-document-factory.h			\
	ev-module.h

INST_H_FILES = 					\
	ev-async-renderer.h			\
	ev-attachment.h				\
	ev-backends-manager.h			\
	ev-document-find.h			\
	ev-document-fonts.h			\
	ev-document-forms.h			\
	ev-document.h				\
	ev-document-images.h			\
	ev-document-info.h			\
	ev-document-layers.h			\
	ev-document-links.h			\
	ev-document-misc.h			\
	ev-document-security.h			\
	ev-document-thumbnails.h		\
	ev-document-transition.h		\
	ev-document-type-builtins.h		\
	ev-file-exporter.h			\
	ev-file-helpers.h			\
	ev-form-field.h				\
	ev-image.h				\
	ev-layer.h				\
	ev-link-action.h			\
	ev-link-dest.h				\
	ev-link.h				\
	ev-page.h				\
	ev-render-context.h			\
	ev-selection.h				\
	ev-transition-effect.h			\
	ev-version.h

headerdir = $(includedir)/evince/$(EV_API_VERSION)/ev-backend
header_DATA = $(INST_H_FILES)

libevbackend_la_SOURCES=			\
	ev-async-renderer.c			\
	ev-attachment.c				\
	ev-backends-manager.c			\
	ev-layer.c				\
	ev-link.c				\
	ev-link-action.c			\
	ev-link-dest.c				\
	ev-image.c				\
	ev-document.c				\
	ev-document-factory.c			\
	ev-document-thumbnails.c		\
	ev-document-fonts.c                     \
	ev-document-layers.c			\
	ev-document-links.c			\
	ev-document-images.c			\
	ev-document-security.c			\
	ev-document-find.c			\
	ev-document-transition.c		\
	ev-document-forms.c			\
	ev-document-type-builtins.c		\
	ev-form-field.c 			\
	ev-debug.c				\
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

BUILT_SOURCES = 			\
	ev-document-type-builtins.c	\
	ev-document-type-builtins.h

CLEANFILES = $(BUILT_SOURCES) stamp-ev-document-type-builtins.h

ev-document-type-builtins.h: stamp-ev-document-type-builtins.h
	@true

stamp-ev-document-type-builtins.h: ev-document-type-builtins.h.template ev-document-info.h
	$(GLIB_MKENUMS) --template $< $(filter-out $<,$^) > xgen-etbh \
	&& (cmp -s xgen-etbh ev-document-type-builtins.h || cp xgen-etbh ev-document-type-builtins.h ) \
	&& rm -f xgen-etbh \
	&& echo timestamp > $(@F)

ev-document-type-builtins.c: ev-document-type-builtins.c.template ev-document-info.h
	$(GLIB_MKENUMS) --template $< $(filter-out $<,$^) > xgen-etbc \
	&& (cmp -s xgen-etbc ev-document-type-builtins.c || cp xgen-etbc ev-document-type-builtins.c ) \
	&& rm -f xgen-etbc

EXTRA_DIST = \
	ev-document-type-builtins.c.template \
	ev-document-type-builtins.h.template

