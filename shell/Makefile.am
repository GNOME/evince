NULL =

INCLUDES=							\
	-DGNOMEDATADIR=\"$(datadir)\"				\
	-I$(top_srcdir)						\
	-I$(top_builddir)					\
	-I$(top_srcdir)/cut-n-paste/zoom-control/		\
	-I$(top_srcdir)/cut-n-paste/toolbar-editor/		\
	-I$(top_srcdir)/cut-n-paste/gimpcellrenderertoggle/	\
	-I$(top_srcdir)/cut-n-paste/smclient/			\
	-I$(top_srcdir)/libdocument				\
	-I$(top_builddir)/libdocument				\
	-I$(top_srcdir)/libview					\
	-I$(top_builddir)/libview				\
	-I$(top_srcdir)/libmisc					\
	-I$(top_srcdir)/properties				\
	-DBINDIR=\""$(bindir)"\"				\
	-DEVINCE_COMPILATION					\
	$(SHELL_CFLAGS)						\
	$(WARN_CFLAGS)						\
	$(DISABLE_DEPRECATED)

bin_PROGRAMS=evince

if ENABLE_DBUS
libexec_PROGRAMS = evinced
endif

evince_SOURCES=				\
	eggfindbar.c			\
	eggfindbar.h			\
	ev-annotation-properties-dialog.h \
	ev-annotation-properties-dialog.c \
	ev-bookmarks.h			\
	ev-bookmarks.c			\
	ev-bookmark-action.h		\
	ev-bookmark-action.c		\
	ev-application.c		\
	ev-application.h		\
	ev-file-monitor.h		\
	ev-file-monitor.c		\
	ev-history.c			\
	ev-history.h			\
	ev-keyring.h			\
	ev-keyring.c			\
	ev-message-area.c		\
	ev-message-area.h		\
	ev-metadata.c			\
	ev-metadata.h			\
	ev-navigation-action.c		\
	ev-navigation-action.h		\
	ev-navigation-action-widget.c	\
	ev-navigation-action-widget.h	\
	ev-password-view.h		\
	ev-password-view.c		\
	ev-progress-message-area.h	\
	ev-progress-message-area.c	\
	ev-properties-dialog.c		\
	ev-properties-dialog.h		\
	ev-properties-fonts.c		\
	ev-properties-fonts.h		\
	ev-properties-license.c		\
	ev-properties-license.h		\
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
	ev-sidebar-annotations.c	\
	ev-sidebar-annotations.h	\
	ev-sidebar-attachments.c	\
	ev-sidebar-attachments.h	\
	ev-sidebar-bookmarks.h		\
	ev-sidebar-bookmarks.c		\
	ev-sidebar-layers.c		\
	ev-sidebar-layers.h		\
	ev-sidebar-links.c		\
	ev-sidebar-links.h		\
	ev-sidebar-page.c		\
	ev-sidebar-page.h		\
	ev-sidebar-thumbnails.c		\
	ev-sidebar-thumbnails.h		\
	main.c

nodist_evince_SOURCES = \
	ev-resources.c \
	$(NULL)

if ENABLE_DBUS
evince_SOURCES += \
	ev-media-player-keys.c \
	ev-media-player-keys.h \
	$(NULL)
nodist_evince_SOURCES += \
	ev-gdbus-generated.c \
	ev-gdbus-generated.h \
	$(NULL)
endif

evince_LDFLAGS = $(AM_LDFLAGS)

if PLATFORM_WIN32
evince_LDFLAGS += -mwindows
endif

evince_LDADD=										\
	$(top_builddir)/cut-n-paste/zoom-control/libephyzoom.la				\
	$(top_builddir)/cut-n-paste/toolbar-editor/libtoolbareditor.la			\
	$(top_builddir)/cut-n-paste/gimpcellrenderertoggle/libgimpcellrenderertoggle.la	\
	$(top_builddir)/cut-n-paste/smclient/libsmclient.la				\
	$(top_builddir)/properties/libevproperties.la					\
	$(top_builddir)/libdocument/libevdocument3.la					\
	$(top_builddir)/libview/libevview3.la						\
	$(top_builddir)/libmisc/libevmisc.la						\
	$(SHELL_LIBS)

if PLATFORM_WIN32
evince_LDADD += evince-icon.o

evince-icon.o: $(srcdir)/evince-icon.rc $(top_srcdir)/data/evince.ico
	$(AM_V_GEN)$(WINDRES) $(srcdir)/evince-icon.rc -I $(top_srcdir)/data evince-icon.o
endif

BUILT_SOURCES = \
	ev-resources.c

if ENABLE_DBUS
BUILT_SOURCES += \
	ev-gdbus-generated.c \
	ev-gdbus-generated.h \
	ev-daemon-gdbus-generated.c \
	ev-daemon-gdbus-generated.h
endif

if ENABLE_DBUS
evinced_SOURCES = \
	ev-daemon.c \
	$(NULL)
nodist_evinced_SOURCES = \
	ev-daemon-gdbus-generated.c \
	ev-daemon-gdbus-generated.h
	$(NULL)

evinced_CFLAGS=				\
	-I$(top_srcdir)						\
	-I$(top_builddir)					\
	-DEVINCE_COMPILATION					\
	$(EV_DAEMON_CFLAGS)					\
	$(WARN_CFLAGS)						\
	$(DISABLE_DEPRECATED)

evinced_LDADD=				\
	$(EV_DAEMON_LIBS)
endif

EXTRA_DIST = \
	evince.css \
	evince-icon.rc \
	evince-ui.xml \
	evince-toolbar.xml \
	evince.gresource.xml \
	ev-gdbus.xml \
	ev-daemon-gdbus.xml

ev-resources.c: evince.gresource.xml Makefile $(shell $(GLIB_COMPILE_RESOURCES) --generate-dependencies --sourcedir $(srcdir) $(srcdir)/evince.gresource.xml)
	$(AM_V_GEN) XMLLINT=$(XMLLINT) $(GLIB_COMPILE_RESOURCES) --target $@ --sourcedir $(srcdir) --generate-source --c-name ev $<

ev-gdbus-generated.c ev-gdbus-generated.h: ev-gdbus.xml Makefile
	$(AM_V_GEN) $(GDBUS_CODEGEN) \
			--interface-prefix=org.gnome.evince \
			--c-namespace=Ev \
			--c-generate-object-manager \
			--generate-c-code ev-gdbus-generated \
			$<

ev-daemon-gdbus-generated.c ev-daemon-gdbus-generated.h: ev-daemon-gdbus.xml Makefile
	$(AM_V_GEN) $(GDBUS_CODEGEN) \
			--interface-prefix=org.gnome.evince \
			--c-namespace=Ev \
			--c-generate-object-manager \
			--generate-c-code ev-daemon-gdbus-generated \
			$<

DISTCLEANFILES = $(BUILT_SOURCES)

-include $(top_srcdir)/git.mk
