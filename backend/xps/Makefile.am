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
	$(AM_CFLAGS)

libxpsdocument_la_LDFLAGS = $(BACKEND_LIBTOOL_FLAGS)
libxpsdocument_la_LIBADD = 				\
	$(top_builddir)/libdocument/libevdocument3.la	\
	$(BACKEND_LIBS)					\
	$(GXPS_LIBS)

backend_in_files = xpsdocument.evince-backend.desktop.in.in
backend_DATA = $(backend_in_files:.evince-backend.desktop.in.in=.evince-backend)

xpsdocument.evince-backend: xpsdocument.evince-backend.desktop.in
	$(AM_V_GEN)$(MSGFMT) --desktop --keyword=TypeDescription --template $< -d $(top_srcdir)/po -o $@

appstreamdir = $(datadir)/metainfo
appstream_in_files = evince-xpsdocument.metainfo.xml.in.in
appstream_DATA = $(appstream_in_files:.xml.in.in=.xml)

evince-xpsdocument.metainfo.xml: evince-xpsdocument.metainfo.xml.in
	$(AM_V_GEN)$(MSGFMT) --xml --template $< -d $(top_srcdir)/po -o $@

EXTRA_DIST = $(backend_in_files) $(appstream_in_files)

CLEANFILES = $(backend_DATA) $(appstream_DATA)

-include $(top_srcdir)/git.mk
