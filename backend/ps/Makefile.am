backend_LTLIBRARIES = libpsdocument.la

libpsdocument_la_SOURCES = 	\
	ev-spectre.c		\
	ev-spectre.h

libpsdocument_la_CPPFLAGS = \
	-I$(top_srcdir) \
	-I$(top_srcdir)/libdocument \
	-DGNOMELOCALEDIR=\"$(datadir)/locale\" \
	-DEVINCE_COMPILATION \
	$(AM_CPPFLAGS)

libpsdocument_la_CFLAGS = \
	$(BACKEND_CFLAGS) \
	$(SPECTRE_CFLAGS) \
	$(AM_CFLAGS)

libpsdocument_la_LDFLAGS = $(BACKEND_LIBTOOL_FLAGS)
libpsdocument_la_LIBADD = 				\
	$(top_builddir)/libdocument/libevdocument3.la	\
	$(BACKEND_LIBS)					\
	$(SPECTRE_LIBS)

backend_in_files = psdocument.evince-backend.in.in
backend_DATA = $(backend_in_files:.evince-backend.in.in=.evince-backend)
@EV_INTLTOOL_EVINCE_BACKEND_RULE@

appstreamdir = $(datadir)/appdata
appstream_in_files = evince-psdocument.metainfo.xml.in.in
appstream_DATA = $(appstream_in_files:.xml.in.in=.xml)
@INTLTOOL_XML_RULE@

EXTRA_DIST = $(backend_in_files) $(appstream_in_files)

CLEANFILES = $(backend_DATA) $(appstream_DATA)

-include $(top_srcdir)/git.mk
