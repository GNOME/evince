INCLUDES=					\
	-DGNOMELOCALEDIR=\"$(datadir)/locale\"	\
	-I$(top_srcdir)/libdocument		\
	$(LIBVIEW_CFLAGS)			\
	$(WARN_CFLAGS)				\
	$(DISABLE_DEPRECATED)

lib_LTLIBRARIES = libevview.la

libevview_la_LDFLAGS = -export-dynamic
libevview_la_LIBADD = $(LIBVIEW_LIBS)

NOINST_H_FILES =			\
	ev-job-scheduler.h		\
	ev-pixbuf-cache.h		\
	ev-timeline.h			\
	ev-transition-animation.h	\
	ev-view-accessible.h		\
	ev-view-marshal.h		\
	ev-view-private.h

INST_H_FILES = 				\
	ev-jobs.h			\
	ev-page-cache.h			\
	ev-view.h			\
	ev-view-type-builtins.h

headerdir = $(prefix)/include/evince/@EV_API_VERSION@/ev-view
header_DATA = $(INST_H_FILES)

libevview_la_SOURCES = 			\
	ev-jobs.c			\
	ev-job-scheduler.c		\
	ev-page-cache.c			\
	ev-pixbuf-cache.c		\
	ev-timeline.c			\
	ev-transition-animation.c	\
	ev-view.c			\
	ev-view-accessible.c		\
	ev-view-marshal.c		\
	ev-view-type-builtins.c		\
	$(NOINST_H_FILES)		\
	$(INST_H_FILES)

BUILT_SOURCES = 			\
	ev-view-marshal.h		\
	ev-view-marshal.c		\
	ev-view-type-builtins.c		\
	ev-view-type-builtins.h

CLEANFILES = $(BUILT_SOURCES) stamp-ev-view-type-builtins.h

ev-view-marshal.h: $(srcdir)/ev-view-marshal.list
	$(GLIB_GENMARSHAL) --prefix=ev_view_marshal $(srcdir)/ev-view-marshal.list --header > $@

ev-view-marshal.c: $(srcdir)/ev-view-marshal.list
	echo '#include "ev-view-marshal.h"' > ev-view-marshal.c
	$(GLIB_GENMARSHAL) --prefix=ev_view_marshal $(srcdir)/ev-view-marshal.list --body >> $@

EXTRA_DIST = ev-view-marshal.list

ev-view-type-builtins.h: stamp-ev-view-type-builtins.h
	@true

stamp-ev-view-type-builtins.h: ev-view-type-builtins.h.template ev-job-scheduler.h
	$(GLIB_MKENUMS) --template $< $(filter-out $<,$^) > xgen-etbh \
	&& (cmp -s xgen-etbh ev-view-type-builtins.h || cp xgen-etbh ev-view-type-builtins.h ) \
	&& rm -f xgen-etbh \
	&& echo timestamp > $(@F)

ev-view-type-builtins.c: ev-view-type-builtins.c.template ev-job-scheduler.h
	$(GLIB_MKENUMS) --template $< $(filter-out $<,$^) > xgen-etbc \
	&& (cmp -s xgen-etbc ev-view-type-builtins.c || cp xgen-etbc ev-view-type-builtins.c ) \
	&& rm -f xgen-etbc

EXTRA_DIST = \
	ev-view-type-builtins.c.template  \
	ev-view-type-builtins.h.template

