SUBDIRS = mdvi-lib

backend_LTLIBRARIES = libdvidocument.la

libdvidocument_la_SOURCES = 	\
	dvi-document.c		\
	dvi-document.h  	\
	cairo-device.c		\
	cairo-device.h		\
	texmfcnf.c		\
	texmfcnf.h		\
	fonts.c			\
	fonts.h

libdvidocument_la_CPPFLAGS = \
	-I$(top_srcdir) \
	-I$(top_srcdir)/libdocument \
	-I$(srcdir)/mdvi-lib \
	-DGNOMELOCALEDIR=\"$(datadir)/locale\" \
	-DEVINCE_COMPILATION \
	$(AM_CPPFLAGS)

libdvidocument_la_CFLAGS = \
	$(BACKEND_CFLAGS) \
	$(SPECTRE_CFLAGS) \
	$(AM_CFLAGS)

libdvidocument_la_LDFLAGS = $(BACKEND_LIBTOOL_FLAGS)
libdvidocument_la_LIBADD = 				\
	mdvi-lib/libmdvi.la				\
	-lkpathsea					\
	$(top_builddir)/libdocument/libevdocument3.la	\
	$(BACKEND_LIBS)					\
	$(SPECTRE_LIBS)

if WITH_TYPE1_FONTS
libdvidocument_la_LIBADD += -lt1
endif

backend_in_files = dvidocument.evince-backend.in.in
backend_DATA = $(backend_in_files:.evince-backend.in.in=.evince-backend)
@EV_INTLTOOL_EVINCE_BACKEND_RULE@

appstreamdir = $(datadir)/appdata
appstream_in_files = evince-dvidocument.metainfo.xml.in.in
appstream_DATA = $(appstream_in_files:.xml.in.in=.xml)
@INTLTOOL_XML_RULE@

EXTRA_DIST = $(backend_in_files) $(appstream_in_files)

CLEANFILES = $(backend_DATA) $(appstream_DATA)

-include $(top_srcdir)/git.mk
