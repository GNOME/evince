NULL=

INCLUDES=					\
	-DEVINCE_UIDIR=\"$(pkgdatadir)\"	\
	-DGNOMELOCALEDIR=\"$(datadir)/locale\"	\
	$(SHELL_CFLAGS)				\
	$(EVINCE_DISABLE_DEPRECATED)		\
	$(NULL)

noinst_LTLIBRARIES = libevbackend.la

libevbackend_la_SOURCES=			\
	ev-document.c				\
	ev-document.h				\
	$(NULL)
