noinst_LTLIBRARIES = libevmisc.la

libevmisc_la_SOURCES = \
	ev-page-action.c	\
	ev-page-action.h	\
	ev-page-action-widget.c	\
	ev-page-action-widget.h

libevmisc_la_CFLAGS =	\
	-DEVINCEDATADIR=\"$(pkgdatadir)\"	\
	-I$(top_srcdir)			\
	-I$(top_srcdir)/libdocument	\
	-I$(top_srcdir)/libview		\
	$(SHELL_CORE_CFLAGS)    	\
	$(WARNING_CFLAGS)       	\
	$(DISABLE_DEPRECATED)

libevmisc_la_LIBADD = \
	$(top_builddir)/libdocument/libevdocument.la \
	$(top_builddir)/libview/libevview.la

-include $(top_srcdir)/git.mk
