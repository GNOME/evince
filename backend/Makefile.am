NULL=

INCLUDES=					\
	-DEVINCE_UIDIR=\"$(pkgdatadir)\"	\
	-DGNOMELOCALEDIR=\"$(datadir)/locale\"	\
	$(SHELL_CFLAGS)				\
	$(EVINCE_DISABLE_DEPRECATED)		\
	$(NULL)

noinst_LTLIBRARIES = liblibevbackend.la

liblibevbackend_la_SOURCES=			\
	ev-document.c				\
	ev-document.h				\
	$(NULL)
