lib_LTLIBRARIES = libevview.la

NOINST_H_FILES =			\
	ev-annotation-window.h		\
	ev-pixbuf-cache.h		\
	ev-timeline.h			\
	ev-transition-animation.h	\
	ev-view-accessible.h		\
	ev-view-marshal.h		\
	ev-view-private.h

INST_H_FILES = 				\
	ev-jobs.h			\
	ev-job-scheduler.h		\
	ev-page-cache.h			\
	ev-stock-icons.h		\
	ev-view.h			\
	ev-view-type-builtins.h

headerdir = $(includedir)/evince/$(EV_API_VERSION)/libview
header_DATA = $(INST_H_FILES)

libevview_la_SOURCES = 			\
	ev-annotation-window.c		\
	ev-jobs.c			\
	ev-job-scheduler.c		\
	ev-page-cache.c			\
	ev-pixbuf-cache.c		\
	ev-stock-icons.c		\
	ev-timeline.c			\
	ev-transition-animation.c	\
	ev-view.c			\
	ev-view-accessible.c		\
	ev-view-marshal.c		\
	ev-view-type-builtins.c		\
	$(NOINST_H_FILES)		\
	$(INST_H_FILES)

libevview_la_CPPFLAGS = \
	-DDATADIR=\"$(pkgdatadir)\"		\
	-DG_LOG_DOMAIN=\"EvinceView\"		\
	-DGNOMELOCALEDIR=\"$(datadir)/locale\"	\
	-DEVINCE_COMPILATION			\
	-I$(top_srcdir)				\
	-I$(top_srcdir)/libdocument		\
	-I$(top_builddir)			\
	$(AM_CPPFLAGS)

libevview_la_CFLAGS = \
	$(LIBVIEW_CFLAGS)	\
	$(WARN_CFLAGS)		\
	$(DISABLE_DEPRECATED)	\
	$(AM_CFLAGS)

libevview_la_LDFLAGS = \
	-version-info $(EV_VIEW_LT_VERSION_INFO)	\
	-no-undefined	\
	-export-symbols-regex "^ev_*"	\
	$(AM_LDFLAGS)

libevview_la_LIBADD = \
	$(top_builddir)/libdocument/libevdocument.la \
	$(LIBVIEW_LIBS)

BUILT_SOURCES = 			\
	ev-view-marshal.h		\
	ev-view-marshal.c		\
	ev-view-type-builtins.c		\
	ev-view-type-builtins.h

CLEANFILES = $(BUILT_SOURCES) stamp-ev-view-type-builtins.h

ev-view-marshal.h: $(srcdir)/ev-view-marshal.list
	$(AM_V_GEN)$(GLIB_GENMARSHAL) --prefix=ev_view_marshal $(srcdir)/ev-view-marshal.list --header --internal > $@

ev-view-marshal.c: $(srcdir)/ev-view-marshal.list
	echo '#include <config.h>' > ev-view-marshal.c
	$(AM_V_GEN)$(GLIB_GENMARSHAL) --prefix=ev_view_marshal $(srcdir)/ev-view-marshal.list --header --internal --body >> $@

ev-view-type-builtins.h: stamp-ev-view-type-builtins.h
	@true

stamp-ev-view-type-builtins.h: ev-view-type-builtins.h.template $(INST_H_FILES)
	$(AM_V_GEN)$(GLIB_MKENUMS) --template $< $(filter-out $<,$^) > xgen-etbh \
	&& (cmp -s xgen-etbh ev-view-type-builtins.h || cp xgen-etbh ev-view-type-builtins.h ) \
	&& rm -f xgen-etbh \
	&& echo timestamp > $(@F)

ev-view-type-builtins.c: ev-view-type-builtins.c.template $(INST_H_FILES)
	$(AM_V_GEN)$(GLIB_MKENUMS) --template $< $(filter-out $<,$^) > xgen-etbc \
	&& (cmp -s xgen-etbc ev-view-type-builtins.c || cp xgen-etbc ev-view-type-builtins.c ) \
	&& rm -f xgen-etbc

EXTRA_DIST = \
	ev-view-type-builtins.c.template  \
	ev-view-type-builtins.h.template  \
	ev-view-marshal.list

# GObject Introspection

if ENABLE_INTROSPECTION

EvinceView-$(EV_API_VERSION).gir: libevview.la Makefile $(INST_H_FILES)
	$(AM_V_GEN) PKG_CONFIG_PATH=$(top_builddir):$$PKG_CONFIG_PATH \
	$(G_IR_SCANNER) -v --namespace EvinceView \
	--add-include-path=$(top_builddir)/libdocument \
	--strip-prefix=Ev \
	--nsversion=$(EV_API_VERSION) \
	--include=GLib-2.0 \
	--include=GObject-2.0 \
	--include=Gio-2.0 \
	--include=Gdk-2.0 \
	--include=GdkPixbuf-2.0 \
	--include=Gtk-2.0 \
	--include=EvinceDocument-$(EV_API_VERSION) \
	--library=evview \
	--libtool="$(SHAVE_SAVED_LIBTOOL)" \
	--output $@ \
	--pkg evince-document-$(EV_API_VERSION) \
	--pkg evince-view-$(EV_API_VERSION) \
	-I$(top_srcdir) \
	-I$(top_builddir) \
	-DEVINCE_COMPILATION \
	$(filter %.h,$^)

girdir = $(GIRDIR)
gir_DATA = EvinceView-$(EV_API_VERSION).gir

typelibsdir = $(GIRTYPELIBDIR)
typelibs_DATA = EvinceView-$(EV_API_VERSION).typelib

EXTRA_DIST += $(gir_DATA)
CLEANFILES += $(gir_DATA) $(typelibs_DATA)

%.typelib: %.gir
	$(AM_V_GEN) LD_LIBRARY_PATH=$${LD_LIBRARY_PATH:+$$LD_LIBRARY_PATH:}. $(G_IR_COMPILER) \
	--includedir=$(srcdir) \
	--includedir=$(top_builddir)/libdocument \
	$(G_IR_COMPILER_OPTS) \
	$< -o $@

endif # ENABLE_INTROSPECTION
