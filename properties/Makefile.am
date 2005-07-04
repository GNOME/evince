NULL=

INCLUDES=					\
	-DDATADIR=\"$(pkgdatadir)\"		\
	-I$(top_srcdir)/backend			\
	$(SHELL_CFLAGS)				\
	$(EVINCE_DISABLE_DEPRECATED)		\
	$(NULL)

noinst_LTLIBRARIES = libevproperties.la

libevproperties_la_SOURCES=			\
	ev-properties-view.c			\
	ev-properties-view.h			\
	$(NULL)
