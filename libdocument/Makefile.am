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
	ev-mapping-list.h			\
	ev-page.h				\
	ev-render-context.h			\
	ev-selection.h				\
	ev-transition-effect.h			\
	ev-version.h

INST_H_FILES = 					\
	$(INST_H_SRC_FILES)			\
	ev-document-type-builtins.h

headerdir = $(includedir)/evince/$(EV_API_VERSION)/libdocument
header_DATA = $(INST_H_FILES)

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
	ev-document-type-builtins.c		\
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
	$(INST_H_FILES)

libevdocument3_la_CPPFLAGS = \
	-DG_LOG_DOMAIN=\"EvinceDocument\"		\
	-DEVINCE_UIDIR=\"$(pkgdatadir)\"		\
	-DGNOMELOCALEDIR=\"$(datadir)/locale\"		\
	-DEV_BACKENDSDIR=\"$(backenddir)\"		\
	-DEV_BACKENDSBINARYVERSION=\"$(backend_binary_version)\"	\
	-DEVINCE_COMPILATION				\
	$(AM_CPPFLAGS)

libevdocument3_la_CFLAGS = \
	$(LIBDOCUMENT_CFLAGS)			\
	-I$(top_srcdir)/cut-n-paste/synctex	\
	$(WARN_CFLAGS)				\
	$(DISABLE_DEPRECATED)			\
	$(AM_CFLAGS)

libevdocument3_la_LDFLAGS = \
	-version-info $(EV_DOCUMENT_LT_VERSION_INFO)	\
	-no-undefined	\
	-export-symbols-regex "^ev_*"	\
	$(AM_LDFLAGS)

libevdocument3_la_LIBADD = \
	$(LIBDOCUMENT_LIBS)	\
	$(ZLIB_LIBS)		\
	$(LIBM)			\
	$(top_builddir)/cut-n-paste/synctex/libsynctex.la


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

if ENABLE_INTROSPECTION

EvinceDocument-$(EV_API_VERSION).gir: libevdocument3.la Makefile $(INST_H_FILES) $(filter %.c,$(libevdocument3_la_SOURCES))
	$(AM_V_GEN) PKG_CONFIG_PATH=$(top_builddir):$$PKG_CONFIG_PATH \
	$(G_IR_SCANNER) -v --namespace EvinceDocument \
	--strip-prefix=Ev \
	--nsversion=$(EV_API_VERSION) \
	--include=GLib-2.0 \
	--include=Gio-2.0 \
	--include=Gdk-3.0 \
	--include=GdkPixbuf-2.0 \
	--include=Gtk-3.0 \
	--library=evdocument3 \
	--libtool="$(LIBTOOL)" \
	--output $@ \
	--pkg evince-document-$(EV_API_VERSION) \
	-I$(top_srcdir) \
	-I$(top_builddir) \
	-I$(srcdir) \
	-I$(builddir) \
	-DEVINCE_COMPILATION \
	$(filter %.h,$^) \
	$(filter %.c,$^)

girdir = $(GIRDIR)
gir_DATA = EvinceDocument-$(EV_API_VERSION).gir

typelibsdir = $(GIRTYPELIBDIR)
typelibs_DATA = EvinceDocument-$(EV_API_VERSION).typelib

EXTRA_DIST += $(gir_DATA)
CLEANFILES += $(gir_DATA) $(typelibs_DATA)

%.typelib: %.gir
	$(AM_V_GEN) LD_LIBRARY_PATH=$${LD_LIBRARY_PATH:+$$LD_LIBRARY_PATH:}. $(G_IR_COMPILER) \
	--includedir=$(srcdir) \
	--includedir=. \
	$(G_IR_COMPILER_OPTS) \
	$< -o $@

endif # ENABLE_INTROSPECTION

-include $(top_srcdir)/git.mk
