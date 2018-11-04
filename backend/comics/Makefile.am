backend_LTLIBRARIES = libcomicsdocument.la

libcomicsdocument_la_SOURCES = \
	comics-document.c      \
	comics-document.h      \
	ev-archive.c           \
	ev-archive.h

libcomicsdocument_la_CPPFLAGS = \
	-I$(top_srcdir) \
	-I$(top_srcdir)/libdocument \
	-I$(top_srcdir)/cut-n-paste \
	-DGNOMELOCALEDIR=\"$(datadir)/locale\" \
	-DEVINCE_COMPILATION \
	$(AM_CPPFLAGS)

libcomicsdocument_la_CFLAGS = \
	$(BACKEND_CFLAGS) \
	$(LIBARCHIVE_CFLAGS) \
	$(LIB_CFLAGS) \
	$(AM_CFLAGS)

libcomicsdocument_la_LDFLAGS = $(BACKEND_LIBTOOL_FLAGS)
libcomicsdocument_la_LIBADD =				\
	$(top_builddir)/libdocument/libevdocument3.la	\
	$(top_builddir)/cut-n-paste/unarr/libunarr.la	\
	$(LIBARCHIVE_LIBS)				\
	$(BACKEND_LIBS)					\
	$(LIB_LIBS)

backend_in_files = comicsdocument.evince-backend.desktop.in.in
backend_DATA = $(backend_in_files:.evince-backend.desktop.in.in=.evince-backend)

comicsdocument.evince-backend: comicsdocument.evince-backend.desktop.in
	$(AM_V_GEN)$(MSGFMT) --desktop --keyword=TypeDescription --template $< -d $(top_srcdir)/po -o $@

appstreamdir = $(datadir)/metainfo
appstream_in_files = evince-comicsdocument.metainfo.xml.in.in
appstream_DATA = $(appstream_in_files:.xml.in.in=.xml)

evince-comicsdocument.metainfo.xml: evince-comicsdocument.metainfo.xml.in
	$(AM_V_GEN)$(MSGFMT) --xml --template $< -d $(top_srcdir)/po -o $@

noinst_PROGRAMS = test-ev-archive

test_ev_archive_SOURCES = ev-archive.c ev-archive.h test-ev-archive.c
test_ev_archive_CPPFLAGS = $(libcomicsdocument_la_CPPFLAGS)
test_ev_archive_CFLAGS = $(libcomicsdocument_la_CFLAGS)
test_ev_archive_LDADD =					\
	$(top_builddir)/cut-n-paste/unarr/libunarr.la	\
	$(LIBARCHIVE_LIBS)				\
	$(BACKEND_LIBS)					\
	$(LIB_LIBS)

EXTRA_DIST = $(backend_in_files) $(appstream_in_files)

CLEANFILES = $(backend_DATA) $(appstream_DATA)

-include $(top_srcdir)/git.mk
