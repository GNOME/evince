backend_LTLIBRARIES = libcomicsdocument.la

libcomicsdocument_la_SOURCES = \
	comics-document.c      \
	comics-document.h

libcomicsdocument_la_CPPFLAGS = \
	-I$(top_srcdir) \
	-I$(top_srcdir)/libdocument \
	-DGNOMELOCALEDIR=\"$(datadir)/locale\" \
	-DEVINCE_COMPILATION \
	$(AM_CPPFLAGS)

libcomicsdocument_la_CFLAGS = \
	$(BACKEND_CFLAGS) \
	$(LIB_CFLAGS) \
	$(AM_CFLAGS)

libcomicsdocument_la_LDFLAGS = $(BACKEND_LIBTOOL_FLAGS)
libcomicsdocument_la_LIBADD =				\
	$(top_builddir)/libdocument/libevdocument3.la	\
	$(BACKEND_LIBS)					\
	$(LIB_LIBS)

backend_in_files = comicsdocument.evince-backend.in.in
backend_DATA = $(backend_in_files:.evince-backend.in.in=.evince-backend)
@EV_INTLTOOL_EVINCE_BACKEND_RULE@

appstreamdir = $(datadir)/appdata
appstream_in_files = evince-comicsdocument.metainfo.xml.in.in
appstream_DATA = $(appstream_in_files:.xml.in.in=.xml)
@INTLTOOL_XML_RULE@

EXTRA_DIST = $(backend_in_files) $(appstream_in_files)

CLEANFILES = $(backend_DATA) $(appstream_DATA)

-include $(top_srcdir)/git.mk
