
INCLUDES = $(TOOLBAR_EDITOR_CFLAGS) \
	-DCURSOR_DIR=\"$(pkgdatadir)\" 

noinst_LTLIBRARIES = libtoolbareditor.la

libtoolbareditor_la_SOURCES = 	\
	egg-toolbars-model.c		\
	egg-editable-toolbar.c		\
	egg-toolbar-editor.c		\
	egg-marshal.c			\
	eggmarshalers.c	

noinst_HEADERS = 			\
	egg-toolbars-model.h		\
	egg-editable-toolbar.h		\
	egg-toolbar-editor.h		\
	eggmarshalers.h

BUILT_SOURCES = eggmarshalers.h eggmarshalers.c

eggmarshalers.h: eggmarshalers.list
	$(GLIB_GENMARSHAL) --prefix=_egg_marshal $(srcdir)/eggmarshalers.list --header > eggmarshalers.h

eggmarshalers.c: eggmarshalers.list
	echo '#include "eggmarshalers.h"' > eggmarshalers.c
	$(GLIB_GENMARSHAL) --prefix=_egg_marshal $(srcdir)/eggmarshalers.list --body >> eggmarshalers.c

egg-marshal.c: eggmarshalers.c eggmarshalers.h

EXTRA_DIST= \
	eggmarshalers.list

EGGDIR=$(srcdir)/../../../libegg/libegg
regenerate-built-sources:
	EGGFILES="$(EGGFILES) eggmarshalers.list" EGGDIR="$(EGGDIR)" $(top_srcdir)/cut-n-paste/update-from-egg.sh
