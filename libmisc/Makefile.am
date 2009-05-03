noinst_LTLIBRARIES = libevmisc.la

libevmisc_la_SOURCES = \
	ev-page-action.c	\
	ev-page-action.h	\
	ev-page-action-widget.c	\
	ev-page-action-widget.h	\
	ev-stock-icons.c	\
	ev-stock-icons.h

libevmisc_la_CFLAGS =	\
	-DDATADIR=\"$(pkgdatadir)\"	\
	-I$(top_srcdir)			\
	-I$(top_srcdir)/libdocument	\
	$(SHELL_CORE_CFLAGS)    	\
	$(WARNING_CFLAGS)       	\
	$(DISABLE_DEPRECATED)