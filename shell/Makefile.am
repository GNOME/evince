INCLUDES=						\
	-DDATADIR=\"$(pkgdatadir)\"			\
	-DGNOMEDATADIR=\"$(datadir)\"			\
	-I$(top_srcdir)/lib				\
	-I$(top_srcdir)/cut-n-paste/recent-files/	\
	-I$(top_srcdir)/cut-n-paste/zoom-control/	\
	-I$(top_srcdir)/cut-n-paste/toolbar-editor/	\
	-I$(top_srcdir)/backend				\
	-I$(top_srcdir)/properties			\
	-DGNOMELOCALEDIR=\"$(datadir)/locale\"		\
	-DGNOMEICONDIR=\""$(datadir)/pixmaps"\" 	\
	$(SHELL_CFLAGS)					\
	$(WARN_CFLAGS)					\
	$(GNOME_PRINT_CFLAGS)

bin_PROGRAMS=evince

evince_SOURCES=				\
	eggfindbar.c			\
	eggfindbar.h			\
	ev-application.c		\
	ev-application.h		\
	ev-job-queue.h			\
	ev-job-queue.c			\
	ev-jobs.h			\
	ev-jobs.c			\
	ev-marshal.c			\
	ev-marshal.h			\
	ev-metadata-manager.c		\
	ev-metadata-manager.h		\
	ev-page-action.c		\
	ev-page-action.h		\
	ev-page-cache.h			\
	ev-page-cache.c			\
	ev-password.h			\
	ev-password.c			\
	ev-password-view.h		\
	ev-password-view.c		\
	ev-pixbuf-cache.c		\
	ev-pixbuf-cache.h		\
	ev-properties-dialog.c		\
	ev-properties-dialog.h		\
	ev-properties-fonts.c		\
	ev-properties-fonts.h		\
	ev-utils.c			\
	ev-utils.h			\
	ev-view-accessible.c		\
	ev-view-accessible.h		\
	ev-view-private.h		\
	ev-view.c			\
	ev-view.h			\
	ev-window.c			\
	ev-window.h			\
	ev-window-title.c		\
	ev-window-title.h		\
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
	main.c

if WITH_GNOME_PRINT
evince_SOURCES+=			\
	ev-print-job.c			\
	ev-print-job.h
endif


evince_LDADD=								\
	$(top_builddir)/cut-n-paste/recent-files/librecent.la		\
	$(top_builddir)/cut-n-paste/zoom-control/libephyzoom.la		\
	$(top_builddir)/cut-n-paste/toolbar-editor/libtoolbareditor.la	\
	$(top_builddir)/properties/libevproperties.la			\
	$(top_builddir)/lib/libev.la					\
	$(SHELL_LIBS)							\
	$(GNOME_PRINT_LIBS)

BUILT_SOURCES = ev-marshal.h ev-marshal.c

if ENABLE_DBUS
BUILT_SOURCES += ev-application-service.h
endif

EXTRA_DIST = ev-marshal.list		\
	ev-application-service.xml


ev-marshal.h: $(srcdir)/ev-marshal.list
	$(GLIB_GENMARSHAL) --prefix=ev_marshal $(srcdir)/ev-marshal.list --header > ev-marshal.h

ev-marshal.c: $(srcdir)/ev-marshal.list
	echo '#include "ev-marshal.h"' > ev-marshal.c
	$(GLIB_GENMARSHAL) --prefix=ev_marshal $(srcdir)/ev-marshal.list --body >> ev-marshal.c

DISTCLEANFILES= \
	ev-application-service.h

if DBUS_TOOL_NO_PREFIX
ev-application-service.h: $(srcdir)/ev-application-service.xml
	dbus-binding-tool --mode=glib-server --output=ev-application-service.h $(srcdir)/ev-application-service.xml
else
ev-application-service.h: $(srcdir)/ev-application-service.xml
	dbus-binding-tool --prefix=ev_application --mode=glib-server --output=ev-application-service.h $(srcdir)/ev-application-service.xml
endif
