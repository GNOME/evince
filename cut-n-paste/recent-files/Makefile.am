NULL =

INCLUDES = \
	$(RECENT_FILES_CFLAGS)		\
	$(NULL)

#	Uses the deprecated GnomeIconTheme
#	$(EVINCE_DISABLE_DEPRECATED)

EGG_FILES =				\
        egg-recent-model.c		\
        egg-recent-model.h		\
        egg-recent-item.c		\
        egg-recent-item.h		\
        egg-recent-view.c		\
        egg-recent-view.h		\
        egg-recent-view-bonobo.c	\
        egg-recent-view-bonobo.h	\
        egg-recent-view-gtk.c		\
        egg-recent-view-gtk.h		\
        egg-recent-util.c		\
	egg-recent-util.h

noinst_LIBRARIES = librecent.a

librecent_a_SOURCES = $(EGG_FILES)

EXTRA_DIST = update-from-egg.sh

EGGDIR=$(srcdir)/../../../libegg/libegg/recent-files
regenerate-built-sources:
	EGGFILES="$(EGG_FILES)" EGGDIR="$(EGGDIR)" $(srcdir)/update-from-egg.sh
