backend_LTLIBRARIES = libtiffdocument.la

libtiffdocument_la_SOURCES =	\
	tiff-document.c		\
	tiff-document.h		\
	tiff2ps.c		\
	tiff2ps.h

libtiffdocument_la_CPPFLAGS = \
	-I$(top_srcdir) \
	-I$(top_srcdir)/libdocument \
	-DGNOMELOCALEDIR=\"$(datadir)/locale\" \
	-DEVINCE_COMPILATION \
	$(AM_CPPFLAGS)

libtiffdocument_la_CFLAGS = \
	$(BACKEND_CFLAGS) \
	$(AM_CFLAGS)

libtiffdocument_la_LDFLAGS = $(BACKEND_LIBTOOL_FLAGS)
libtiffdocument_la_LIBADD = 				\
	$(top_builddir)/libdocument/libevdocument3.la	\
	$(BACKEND_LIBS)			\
	-ltiff

backend_in_files = tiffdocument.evince-backend.desktop.in.in
backend_DATA = $(backend_in_files:.evince-backend.desktop.in.in=.evince-backend)

tiffdocument.evince-backend: tiffdocument.evince-backend.desktop.in
	$(AM_V_GEN)$(MSGFMT) --desktop --keyword=TypeDescription --template $< -d $(top_srcdir)/po -o $@

appstreamdir = $(datadir)/metainfo
appstream_in_files = evince-tiffdocument.metainfo.xml.in.in
appstream_DATA = $(appstream_in_files:.xml.in.in=.xml)

evince-tiffdocument.metainfo.xml: evince-tiffdocument.metainfo.xml.in
	$(AM_V_GEN)$(MSGFMT) --xml --template $< -d $(top_srcdir)/po -o $@

EXTRA_DIST = $(backend_in_files) $(appstream_in_files)

CLEANFILES = $(backend_DATA) $(appstream_DATA)

-include $(top_srcdir)/git.mk
