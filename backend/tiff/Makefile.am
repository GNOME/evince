-include $(top_srcdir)/backend/Makefile.appdata

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

backend_in_files = tiffdocument.evince-backend.in.in
backend_DATA = $(backend_in_files:.evince-backend.in.in=.evince-backend)
@EV_INTLTOOL_EVINCE_BACKEND_RULE@

APPDATA_NAME=TIFF Documents
APPDATA_SUMMARY=Adds support for reading TIFF documents
APPDATA_MIME_TYPES=$(shell echo "$(TIFF_MIME_TYPES)" | sed -e 's/;/ /g')
appstreamdir = $(datadir)/appdata
appstream_in_files = evince-tiffdocument.metainfo.xml.in
appstream_DATA = $(appstream_in_files:.xml.in=.xml)
@INTLTOOL_XML_RULE@

EXTRA_DIST = $(backend_in_files) $(appstream_in_files)

CLEANFILES = $(backend_DATA) $(appstream_DATA)

-include $(top_srcdir)/git.mk
