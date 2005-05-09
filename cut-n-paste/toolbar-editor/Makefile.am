
INCLUDES = $(TOOLBAR_EDITOR_CFLAGS) \
	-DCURSOR_DIR=\"$(pkgdatadir)\" 

BUILT_SOURCES = eggmarshalers.c eggmarshalers.h

noinst_LTLIBRARIES = libtoolbareditor.la

libtoolbareditor_la_SOURCES = 	\
	egg-toolbars-model.c		\
	egg-editable-toolbar.c		\
	egg-toolbar-editor.c		\
	egg-marshal.c	

noinst_HEADERS = 			\
	egg-toolbars-model.h		\
	egg-editable-toolbar.h		\
	egg-toolbar-editor.h		

eggmarshalers.h: eggmarshalers.list
	cd $(srcdir) \
	&& $(GLIB_GENMARSHAL) --prefix=_egg_marshal eggmarshalers.list --header > xgen-emh \
	&& cp xgen-emh eggmarshalers.h \
	&& rm -f xgen-emh xgen-emh~

eggmarshalers.c: eggmarshalers.list
	cd $(srcdir) \
	&& $(GLIB_GENMARSHAL) --prefix=_egg_marshal eggmarshalers.list --body > xgen-emc \
	&& cp xgen-emc eggmarshalers.c \
	&& rm -f xgen-emc xgen-emc~

egg-marshal.c: eggmarshalers.c eggmarshalers.h

EXTRA_DIST= \
	eggmarshalers.list

EGGDIR=$(srcdir)/../../../libegg/libegg
regenerate-built-sources:
	EGGFILES="$(EGGFILES) eggmarshalers.list" EGGDIR="$(EGGDIR)" $(top_srcdir)/cut-n-paste/update-from-egg.sh
