NULL=

INCLUDES=						\
	-DDATADIR=\"$(pkgdatadir)\"			\
	-DGNOMEDATADIR=\"$(datadir)\"			\
	-I$(top_srcdir)/lib				\
	-I$(top_srcdir)/cut-n-paste/recent-files/	\
	-I$(top_srcdir)/cut-n-paste/zoom-control/	\
	-I$(top_srcdir)/cut-n-paste/toolbar-editor/	\
	-I$(top_srcdir)/backend				\
	-I$(top_srcdir)/pdf				\
	-I$(top_srcdir)/pixbuf				\
	-I$(top_srcdir)/tiff				\
	-I$(top_srcdir)/ps	                	\
	-I$(top_srcdir)/djvu				\
	-I$(top_srcdir)/dvi				\
	-DGNOMELOCALEDIR=\"$(datadir)/locale\"		\
	-DGNOMEICONDIR=\""$(datadir)/pixmaps"\" 	\
	$(SHELL_CFLAGS)					\
	$(EVINCE_DISABLE_DEPRECATED)			\
	$(NULL)

bin_PROGRAMS=evince
noinst_LTLIBRARIES = libevbackendfactory.la

libevbackendfactory_la_SOURCES=         		\
	ev-document-types.c				\
	ev-document-types.h

libevbackendfactory_la_LIBADD = 			\
	$(top_builddir)/pdf/libpdfdocument.la	 	\
	$(top_builddir)/pixbuf/libpixbufdocument.la	\
	$(top_builddir)/ps/libgtkgs.la			\
	$(top_builddir)/backend/libevbackend.la		\
	$(NULL)

if ENABLE_DJVU
libevbackendfactory_la_LIBADD += 		\
	$(top_builddir)/djvu/libgtkdjvu.la	\
	$(NULL)
endif

if ENABLE_TIFF
libevbackendfactory_la_LIBADD += 		\
	$(top_builddir)/tiff/libtiffdocument.la	\
	$(NULL)
endif

if ENABLE_DVI
libevbackendfactory_la_LIBADD += 		\
	$(top_builddir)/dvi/libgtkdvi.la	\
	$(NULL)
endif

evince_SOURCES=				\
	dummy.cc			\
	eggfindbar.c			\
	eggfindbar.h			\
	ev-application.c		\
	ev-application.h		\
	ev-marshal.c			\
	ev-marshal.h			\
	ev-page-action.c		\
	ev-page-action.h		\
	ev-password.h			\
	ev-password.c			\
	ev-password-view.h		\
	ev-password-view.c		\
	ev-pixbuf-cache.c		\
	ev-pixbuf-cache.h		\
	ev-print-job.c			\
	ev-print-job.h			\
	ev-properties.c			\
	ev-properties.h			\
	ev-utils.c			\
	ev-utils.h			\
	ev-view.c			\
	ev-view.h			\
	ev-window.c			\
	ev-window.h			\
	ev-sidebar.c			\
	ev-sidebar.h			\
	ev-sidebar-links.c		\
	ev-sidebar-links.h		\
	ev-sidebar-page.c		\
	ev-sidebar-page.h		\
	ev-sidebar-thumbnails.c		\
	ev-sidebar-thumbnails.h		\
	ev-stock-icons.c		\
	ev-stock-icons.h		\
	main.c				\
	$(NULL)

evince_LDADD=								\
	$(SHELL_LIBS)							\
	$(top_builddir)/cut-n-paste/recent-files/librecent.la		\
	$(top_builddir)/cut-n-paste/zoom-control/libephymisc.la		\
	$(top_builddir)/cut-n-paste/zoom-control/libephywidgets.la	\
	$(top_builddir)/cut-n-paste/toolbar-editor/libtoolbareditor.la	\
	$(top_builddir)/lib/libev.la					\
	-ltiff -lz							\
	libevbackendfactory.la						\
	$(NULL)

BUILT_SOURCES = ev-marshal.h ev-marshal.c

EXTRA_DIST = ev-marshal.list

ev-marshal.h: ev-marshal.list
	glib-genmarshal --prefix=ev_marshal ev-marshal.list --header > ev-marshal.h

ev-marshal.c: ev-marshal.list
	echo '#include "ev-marshal.h"' > ev-marshal.c
	glib-genmarshal --prefix=ev_marshal ev-marshal.list --body >> ev-marshal.c
