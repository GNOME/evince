NULL=

INCLUDES=					\
	-DEVINCE_UIDIR=\"$(pkgdatadir)\"	\
	-DGNOMELOCALEDIR=\"$(datadir)/locale\"	\
	$(SHELL_CFLAGS)				\
	$(EVINCE_DISABLE_DEPRECATED)		\
	$(NULL)

noinst_LTLIBRARIES = libev.la

libev_la_SOURCES=				\
	ev-debug.c				\
	ev-debug.h				\
	ev-file-helpers.c			\
	ev-file-helpers.h			\
	$(NULL)
