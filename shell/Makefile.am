INCLUDES=							\
	-DDATADIR=\"$(pkgdatadir)\"				\
	-DGNOMEDATADIR=\"$(datadir)\"				\
	-I$(top_srcdir)						\
	-I$(top_builddir)					\
	-I$(top_srcdir)/cut-n-paste/zoom-control/		\
	-I$(top_srcdir)/cut-n-paste/toolbar-editor/		\
	-I$(top_srcdir)/cut-n-paste/totem-screensaver/		\
	-I$(top_srcdir)/cut-n-paste/gedit-message-area/		\
	-I$(top_srcdir)/cut-n-paste/gimpcellrenderertoggle/	\
	-I$(top_srcdir)/cut-n-paste/smclient/			\
	-I$(top_srcdir)/libdocument				\
	-I$(top_builddir)/libdocument				\
	-I$(top_srcdir)/libview					\
	-I$(top_builddir)/libview				\
	-I$(top_srcdir)/libmisc					\
	-I$(top_srcdir)/properties				\
	-DGNOMELOCALEDIR=\"$(datadir)/locale\"			\
	-DGNOMEICONDIR=\""$(datadir)/pixmaps"\" 		\
	-DEVINCE_COMPILATION					\
	$(SHELL_CFLAGS)						\
	$(WARN_CFLAGS)						\
	$(DISABLE_DEPRECATED)

bin_PROGRAMS=evince

EV_MEDIA_PLAYER_KEYS_SOURCES = ev-media-player-keys.c ev-media-player-keys.h
if ENABLE_DBUS
DBUS_SOURCES = $(EV_MEDIA_PLAYER_KEYS_SOURCES)
else
DBUS_SOURCES = $(null)
endif

evince_SOURCES=				\
	eggfindbar.c			\
	eggfindbar.h			\
	ev-application.c		\
	ev-application.h		\
	ev-file-monitor.h		\
	ev-file-monitor.c		\
	ev-history.c			\
	ev-history.h			\
	ev-keyring.h			\
	ev-keyring.c			\
	ev-marshal.c			\
	ev-marshal.h			\
	$(DBUS_SOURCES)			\
	ev-message-area.c		\
	ev-message-area.h		\
	ev-metadata-manager.c		\
	ev-metadata-manager.h		\
	ev-navigation-action.c		\
	ev-navigation-action.h		\
	ev-navigation-action-widget.c	\
	ev-navigation-action-widget.h	\
	ev-password-view.h		\
	ev-password-view.c		\
	ev-print-operation.h		\
	ev-print-operation.c		\
	ev-progress-message-area.h	\
	ev-progress-message-area.c	\
	ev-properties-dialog.c		\
	ev-properties-dialog.h		\
	ev-properties-fonts.c		\
	ev-properties-fonts.h		\
	ev-open-recent-action.c		\
	ev-open-recent-action.h		\
	ev-utils.c			\
	ev-utils.h			\
	ev-window.c			\
	ev-window.h			\
	ev-window-title.c		\
	ev-window-title.h		\
	ev-sidebar.c			\
	ev-sidebar.h			\
	ev-sidebar-attachments.c	\
	ev-sidebar-attachments.h	\
	ev-sidebar-layers.c		\
	ev-sidebar-layers.h		\
	ev-sidebar-links.c		\
	ev-sidebar-links.h		\
	ev-sidebar-page.c		\
	ev-sidebar-page.h		\
	ev-sidebar-thumbnails.c		\
	ev-sidebar-thumbnails.h		\
	main.c

evince_LDFLAGS = $(AM_LDFLAGS)

if PLATFORM_WIN32
evince_LDFLAGS += -mwindows
endif

evince_LDADD=										\
	$(top_builddir)/cut-n-paste/zoom-control/libephyzoom.la				\
	$(top_builddir)/cut-n-paste/toolbar-editor/libtoolbareditor.la			\
	$(top_builddir)/cut-n-paste/totem-screensaver/libtotemscrsaver.la		\
	$(top_builddir)/cut-n-paste/gedit-message-area/libgeditmsgarea.la		\
	$(top_builddir)/cut-n-paste/gimpcellrenderertoggle/libgimpcellrenderertoggle.la	\
	$(top_builddir)/cut-n-paste/smclient/libsmclient.la				\
	$(top_builddir)/properties/libevproperties.la					\
	$(top_builddir)/libdocument/libevdocument.la					\
	$(top_builddir)/libview/libevview.la						\
	$(top_builddir)/libmisc/libevmisc.la						\
	$(SHELL_LIBS)

BUILT_SOURCES = ev-marshal.h ev-marshal.c

if ENABLE_DBUS
BUILT_SOURCES += ev-application-service.h
endif

EXTRA_DIST = ev-marshal.list		\
	ev-application-service.xml	\
	$(EV_MEDIA_PLAYER_KEYS_SOURCES)


ev-marshal.h: $(srcdir)/ev-marshal.list
	$(AM_V_GEN)$(GLIB_GENMARSHAL) --prefix=ev_marshal $(srcdir)/ev-marshal.list --header > ev-marshal.h

ev-marshal.c: $(srcdir)/ev-marshal.list
	echo '#include "ev-marshal.h"' > ev-marshal.c
	$(AM_V_GEN)$(GLIB_GENMARSHAL) --prefix=ev_marshal $(srcdir)/ev-marshal.list --body >> ev-marshal.c

DISTCLEANFILES= \
	ev-application-service.h

ev-application-service.h: $(srcdir)/ev-application-service.xml
	$(AM_V_GEN)dbus-binding-tool --prefix=ev_application --mode=glib-server --output=ev-application-service.h $(srcdir)/ev-application-service.xml
