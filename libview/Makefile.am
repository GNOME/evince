lib_LTLIBRARIES = libevview3.la

NOINST_H_SRC_FILES =			\
	ev-annotation-window.h		\
	ev-form-field-accessible.h	\
	ev-image-accessible.h		\
	ev-link-accessible.h		\
	ev-page-accessible.h		\
	ev-page-cache.h			\
	ev-pixbuf-cache.h		\
	ev-timeline.h			\
	ev-transition-animation.h	\
	ev-view-accessible.h		\
	ev-view-cursor.h		\
	ev-view-private.h

NOINST_H_BUILT_FILES = \
	ev-view-marshal.h

INST_H_SRC_FILES = 			\
	ev-document-model.h		\
	ev-jobs.h			\
	ev-job-scheduler.h		\
	ev-print-operation.h	        \
	ev-stock-icons.h		\
	ev-view.h			\
	ev-view-presentation.h

INST_H_BUILT_FILES = \
	ev-view-type-builtins.h

headerdir = $(includedir)/evince/$(EV_API_VERSION)/libview
header_DATA = $(INST_H_SRC_FILES)
nodist_header_DATA = $(INST_H_BUILT_FILES)

libevview3_la_SOURCES =			\
	ev-annotation-window.c		\
	ev-document-model.c		\
	ev-form-field-accessible.c	\
	ev-image-accessible.c		\
	ev-jobs.c			\
	ev-job-scheduler.c		\
	ev-link-accessible.c		\
	ev-page-accessible.c		\
	ev-page-cache.c			\
	ev-pixbuf-cache.c		\
	ev-print-operation.c	        \
	ev-stock-icons.c		\
	ev-timeline.c			\
	ev-transition-animation.c	\
	ev-view.c			\
	ev-view-accessible.c		\
	ev-view-cursor.c		\
	ev-view-presentation.c		\
	$(NOINST_H_SRC_FILES)		\
	$(INST_H_SRC_FILES)

nodist_libevview3_la_SOURCES = \
	ev-view-marshal.c \
	ev-view-type-builtins.c \
	$(NOINST_H_BUILT_FILES) \
	$(INST_H_BUILT_FILES)

libevview3_la_CPPFLAGS = \
	-DEVINCEDATADIR=\"$(pkgdatadir)\"	\
	-DG_LOG_DOMAIN=\"EvinceView\"		\
	-DEVINCE_COMPILATION			\
	-I$(top_srcdir)				\
	-I$(top_srcdir)/libdocument		\
	-I$(top_builddir)			\
	$(AM_CPPFLAGS)

libevview3_la_CFLAGS = \
	$(LIBVIEW_CFLAGS)	\
	$(AM_CFLAGS)

libevview3_la_LDFLAGS = \
	-version-info $(EV_VIEW_LT_VERSION_INFO)	\
	-no-undefined	\
	-export-symbols-regex "^ev_*"	\
	$(AM_LDFLAGS)

libevview3_la_LIBADD = \
	$(top_builddir)/libdocument/libevdocument3.la \
	$(LIBVIEW_LIBS)

BUILT_SOURCES = 			\
	ev-view-marshal.h		\
	ev-view-marshal.c		\
	ev-view-type-builtins.c		\
	ev-view-type-builtins.h

CLEANFILES = 				\
	$(BUILT_SOURCES)		\
	stamp-ev-view-type-builtins.h	\
	stamp-ev-view-type-builtins.c

ev-view-marshal.h: $(srcdir)/ev-view-marshal.list
	$(AM_V_GEN)$(GLIB_GENMARSHAL) --prefix=ev_view_marshal $(srcdir)/ev-view-marshal.list --header --internal > $@

ev-view-marshal.c: $(srcdir)/ev-view-marshal.list
	$(AM_V_at)echo '#include <config.h>' > ev-view-marshal.c
	$(AM_V_GEN)$(GLIB_GENMARSHAL) --prefix=ev_view_marshal $(srcdir)/ev-view-marshal.list --header --internal --body >> $@

ev-view-type-builtins.h: stamp-ev-view-type-builtins.h
	@true
stamp-ev-view-type-builtins.h: ev-view-type-builtins.h.template $(INST_H_SRC_FILES)
	$(AM_V_GEN)$(GLIB_MKENUMS) --template $< $(filter-out $<,$^) > xgen-etbh \
	&& (cmp -s xgen-etbh ev-view-type-builtins.h || cp xgen-etbh ev-view-type-builtins.h ) \
	&& rm -f xgen-etbh \
	&& echo timestamp > $(@F)

ev-view-type-builtins.c: stamp-ev-view-type-builtins.c
	@true
stamp-ev-view-type-builtins.c: ev-view-type-builtins.c.template $(INST_H_SRC_FILES)
	$(AM_V_GEN)$(GLIB_MKENUMS) --template $< $(filter-out $<,$^) > xgen-etbc \
	&& (cmp -s xgen-etbc ev-view-type-builtins.c || cp xgen-etbc ev-view-type-builtins.c ) \
	&& rm -f xgen-etbc \
	&& echo timestamp > $(@F)

EXTRA_DIST = \
	ev-view-type-builtins.c.template  \
	ev-view-type-builtins.h.template  \
	ev-view-marshal.list

# GObject Introspection

if HAVE_INTROSPECTION

include $(INTROSPECTION_MAKEFILE)
INTROSPECTION_COMPILER_ARGS = --includedir=$(top_builddir)/libdocument

EvinceView-$(EV_API_VERSION).gir: libevview3.la
EvinceView_@EV_API_VERSION_U@_gir_FILES = \
	$(INST_H_SRC_FILES)	\
	$(INST_H_BUILT_FILES)	\
	 $(filter %.c,$(libevview3_la_SOURCES))
EvinceView_@EV_API_VERSION_U@_gir_INCLUDES = GLib-2.0 GObject-2.0 Gio-2.0 Gdk-3.0 GdkPixbuf-2.0 Gtk-3.0
EvinceView_@EV_API_VERSION_U@_gir_LIBS = libevview3.la $(top_builddir)/libdocument/libevdocument3.la
EvinceView_@EV_API_VERSION_U@_gir_CFLAGS = \
	-I$(top_srcdir)			\
	-I$(top_builddir)		\
	-I$(top_srcdir)/libdocument	\
	-I$(top_builddir)/libdocument	\
	-DEVINCE_COMPILATION
EvinceView_@EV_API_VERSION_U@_gir_EXPORT_PACKAGES = evince-view-$(EV_API_VERSION)
EvinceView_@EV_API_VERSION_U@_gir_SCANNERFLAGS = \
	--add-include-path=$(top_builddir)/libdocument	\
	--include-uninstalled=$(top_builddir)/libdocument/EvinceDocument-$(EV_API_VERSION).gir	\
	--identifier-prefix=Ev				\
	--symbol-prefix=ev
INTROSPECTION_GIRS = EvinceView-$(EV_API_VERSION).gir

girdir = $(datadir)/gir-1.0
gir_DATA = EvinceView-$(EV_API_VERSION).gir

typelibsdir = $(libdir)/girepository-1.0
typelibs_DATA = EvinceView-$(EV_API_VERSION).typelib

CLEANFILES += $(gir_DATA) $(typelibs_DATA)

endif # ENABLE_INTROSPECTION

-include $(top_srcdir)/git.mk
