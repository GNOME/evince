lib_LTLIBRARIES = libevdocument3.la

NOINST_H_FILES =				\
	ev-debug.h				\
	ev-backend-info.h			\
	ev-module.h

INST_H_SRC_FILES = 				\
	ev-annotation.h				\
	ev-async-renderer.h			\
	ev-attachment.h				\
	ev-backends-manager.h			\
	ev-document-factory.h			\
	ev-document-annotations.h		\
	ev-document-attachments.h		\
	ev-document-find.h			\
	ev-document-fonts.h			\
	ev-document-forms.h			\
	ev-document.h				\
	ev-document-images.h			\
	ev-document-info.h			\
	ev-document-layers.h			\
	ev-document-links.h			\
	ev-document-misc.h			\
	ev-document-print.h			\
	ev-document-security.h			\
	ev-document-transition.h		\
	ev-document-text.h			\
	ev-file-exporter.h			\
	ev-file-helpers.h			\
	ev-form-field.h				\
	ev-image.h				\
	ev-init.h				\
	ev-layer.h				\
	ev-link-action.h			\
	ev-link-dest.h				\
	ev-link.h				\
	ev-macros.h				\
	ev-mapping-list.h			\
	ev-page.h				\
	ev-render-context.h			\
	ev-selection.h				\
	ev-transition-effect.h

INST_H_BUILT_FILES = \
	ev-document-type-builtins.h \
	ev-version.h

headerdir = $(includedir)/evince/$(EV_API_VERSION)/libdocument
header_DATA = $(INST_H_SRC_FILES)
nodist_header_DATA = $(INST_H_BUILT_FILES)

libevdocument3_la_SOURCES=			\
	ev-annotation.c				\
	ev-async-renderer.c			\
	ev-attachment.c				\
	ev-backend-info.c			\
	ev-layer.c				\
	ev-link.c				\
	ev-link-action.c			\
	ev-link-dest.c				\
	ev-image.c				\
	ev-init.c				\
	ev-document.c				\
	ev-document-annotations.c		\
	ev-document-attachments.c		\
	ev-document-factory.c			\
	ev-document-fonts.c                     \
	ev-document-layers.c			\
	ev-document-links.c			\
	ev-document-images.c			\
	ev-document-print.c			\
	ev-document-security.c			\
	ev-document-find.c			\
	ev-document-transition.c		\
	ev-document-forms.c			\
	ev-document-text.c			\
	ev-form-field.c 			\
	ev-debug.c				\
	ev-file-exporter.c			\
	ev-file-helpers.c			\
	ev-mapping-list.c			\
	ev-module.c				\
	ev-page.c				\
	ev-render-context.c			\
	ev-selection.c				\
	ev-transition-effect.c			\
	ev-document-misc.c			\
	$(NOINST_H_FILES)			\
	$(INST_H_SRC_FILES)

nodist_libevdocument3_la_SOURCES = \
	ev-document-type-builtins.c \
	$(INST_H_BUILT_FILES)

libevdocument3_la_CPPFLAGS = \
	-DG_LOG_DOMAIN=\"EvinceDocument\"		\
	-DGNOMELOCALEDIR=\"$(datadir)/locale\"		\
	-DEV_BACKENDSDIR=\"$(backenddir)\"		\
	-DEV_BACKENDSBINARYVERSION=\"$(backend_binary_version)\"	\
	-DEVINCE_COMPILATION				\
	$(AM_CPPFLAGS)

libevdocument3_la_CFLAGS = \
	$(LIBDOCUMENT_CFLAGS)			\
	-I$(top_srcdir)/cut-n-paste/synctex	\
	$(AM_CFLAGS)

libevdocument3_la_LDFLAGS = \
	-version-info $(EV_DOCUMENT_LT_VERSION_INFO)	\
	-no-undefined	\
	-export-symbols-regex "^ev_*"	\
	$(AM_LDFLAGS)

libevdocument3_la_LIBADD = \
	$(top_builddir)/cut-n-paste/synctex/libsynctex.la \
	$(LIBDOCUMENT_LIBS)	\
	$(ZLIB_LIBS)		\
	$(LIBM)

BUILT_SOURCES = 			\
	ev-document-type-builtins.c	\
	ev-document-type-builtins.h

CLEANFILES = 					\
	$(BUILT_SOURCES)			\
	stamp-ev-document-type-builtins.h	\
	stamp-ev-document-type-builtins.c

ev-document-type-builtins.h: stamp-ev-document-type-builtins.h
	@true
stamp-ev-document-type-builtins.h: ev-document-type-builtins.h.template $(INST_H_SRC_FILES)
	$(AM_V_GEN)$(GLIB_MKENUMS) --template $< $(filter-out $<,$^) > xgen-etbh \
	&& (cmp -s xgen-etbh ev-document-type-builtins.h || cp xgen-etbh ev-document-type-builtins.h ) \
	&& rm -f xgen-etbh \
	&& echo timestamp > $(@F)

ev-document-type-builtins.c: stamp-ev-document-type-builtins.c
	@true
stamp-ev-document-type-builtins.c: ev-document-type-builtins.c.template $(INST_H_SRC_FILES)
	$(AM_V_GEN)$(GLIB_MKENUMS) --template $< $(filter-out $<,$^) > xgen-etbc \
	&& (cmp -s xgen-etbc ev-document-type-builtins.c || cp xgen-etbc ev-document-type-builtins.c ) \
	&& rm -f xgen-etbc \
	&& echo timestamp > $(@F)

EXTRA_DIST = \
	ev-document-type-builtins.c.template \
	ev-document-type-builtins.h.template

# GObject Introspection

if HAVE_INTROSPECTION

include $(INTROSPECTION_MAKEFILE)
EvinceDocument-$(EV_API_VERSION).gir: libevdocument3.la
EvinceDocument_@EV_API_VERSION_U@_gir_FILES = $(INST_H_SRC_FILES) $(INST_H_BUILT_FILES) $(filter %.c,$(libevdocument3_la_SOURCES))
EvinceDocument_@EV_API_VERSION_U@_gir_LIBS = libevdocument3.la
EvinceDocument_@EV_API_VERSION_U@_gir_SCANNERFLAGS = \
	--identifier-prefix=Ev	\
	--symbol-prefix=ev
EvinceDocument_@EV_API_VERSION_U@_gir_INCLUDES = GLib-2.0 Gio-2.0 Gdk-3.0 GdkPixbuf-2.0 Gtk-3.0
EvinceDocument_@EV_API_VERSION_U@_gir_CFLAGS = \
	-DEVINCE_COMPILATION	\
	-I$(top_srcdir)		\
	-I$(top_builddir)	\
	-I$(srcdir)		\
	-I$(builddir)
EvinceDocument_@EV_API_VERSION_U@_gir_EXPORT_PACKAGES = evince-document-$(EV_API_VERSION)
INTROSPECTION_GIRS = EvinceDocument-$(EV_API_VERSION).gir

girdir = $(datadir)/gir-1.0
gir_DATA = EvinceDocument-$(EV_API_VERSION).gir

typelibsdir = $(libdir)/girepository-1.0
typelibs_DATA = EvinceDocument-$(EV_API_VERSION).typelib

CLEANFILES += $(gir_DATA) $(typelibs_DATA)

endif # ENABLE_INTROSPECTION

-include $(top_srcdir)/git.mk
