NULL=

INCLUDES=					\
	-DDATADIR=\"$(pkgdatadir)\"		\
	-I$(top_srcdir)/lib			\
	-I$(top_srcdir)/backend			\
	-I$(top_srcdir)/pdf/xpdf		\
	-DGNOMELOCALEDIR=\"$(datadir)/locale\"	\
	-DGNOMEICONDIR=\""$(datadir)/pixmaps"\" \
	$(SHELL_CFLAGS)				\
	$(EVINCE_DISABLE_DEPRECATED)		\
	$(NULL)

bin_PROGRAMS=evince

evince_SOURCES=				\
	dummy.cc			\
	eggfindbar.c			\
	eggfindbar.h			\
	ev-application.c		\
	ev-application.h		\
	ev-marshal.c			\
	ev-marshal.h			\
	ev-view.c			\
	ev-view.h			\
	ev-window.c			\
	ev-window.h			\
	ev-sidebar.c			\
	ev-sidebar.h			\
	ev-sidebar-bookmarks.c		\
	ev-sidebar-bookmarks.h		\
	ev-sidebar-thumbnails.c		\
	ev-sidebar-thumbnails.h		\
	ev-stock-icons.c		\
	ev-stock-icons.h		\
	main.c				\
	$(NULL)

evince_LDADD=						\
	$(SHELL_LIBS)					\
	$(top_builddir)/backend/libevbackend.la		\
	$(top_builddir)/pdf/xpdf/libpdfdocument.la 	\
	$(NULL)

BUILT_SOURCES = ev-marshal.h ev-marshal.c

EXTRA_DIST = ev-marshal.list

ev-marshal.h: ev-marshal.list
	glib-genmarshal --prefix=ev_marshal ev-marshal.list --header > ev-marshal.h

ev-marshal.c: ev-marshal.list
	echo '#include "ev-marshal.h"' > ev-marshal.c
	glib-genmarshal --prefix=ev_marshal ev-marshal.list --body >> ev-marshal.c
