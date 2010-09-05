backend_LTLIBRARIES = libxpsdocument.la

libxpsdocument_la_SOURCES = 	\
	xps-document.c		\
	xps-document.h

libxpsdocument_la_CPPFLAGS =			\
	-I$(top_srcdir)				\
	-I$(top_srcdir)/libdocument		\
	-DGNOMELOCALEDIR=\"$(datadir)/locale\"	\
	-DEVINCE_COMPILATION

libxpsdocument_la_CFLAGS = 	\
	$(BACKEND_CFLAGS)	\
	$(GXPS_CFLAGS)		\
	$(WARN_CFLAGS)		\
	$(DISABLE_DEPRECATED)

libxpsdocument_la_LDFLAGS = $(BACKEND_LIBTOOL_FLAGS)
libxpsdocument_la_LIBADD = 				\
	$(top_builddir)/libdocument/libevdocument3.la	\
	$(BACKEND_LIBS)					\
	$(GXPS_LIBS)

backend_in_files = xpsdocument.evince-backend.in
backend_DATA = $(backend_in_files:.evince-backend.in=.evince-backend)

EXTRA_DIST = $(backend_in_files)

CLEANFILES = $(backend_DATA)

@EV_INTLTOOL_EVINCE_BACKEND_RULE@

-include $(top_srcdir)/git.mk
