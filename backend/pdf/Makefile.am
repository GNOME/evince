backend_LTLIBRARIES = libpdfdocument.la

libpdfdocument_la_SOURCES =			\
	ev-poppler.cc				\
	ev-poppler.h

libpdfdocument_la_CPPFLAGS = \
	-I$(top_srcdir) \
	-I$(top_srcdir)/libdocument \
	-DGNOMELOCALEDIR=\"$(datadir)/locale\" \
	-DEVINCE_COMPILATION \
	$(AM_CPPFLAGS)

libpdfdocument_la_CXXFLAGS = \
	$(BACKEND_CFLAGS) \
	$(POPPLER_CFLAGS) \
	$(AM_CXXFLAGS)

libpdfdocument_la_LDFLAGS = $(BACKEND_LIBTOOL_FLAGS)
libpdfdocument_la_LIBADD =				\
	$(top_builddir)/libdocument/libevdocument3.la	\
	$(BACKEND_LIBS)					\
	$(POPPLER_LIBS)					\
	$(CAIRO_PDF_LIBS)				\
	$(CAIRO_PS_LIBS)

backend_in_files = pdfdocument.evince-backend.desktop.in.in
backend_DATA = $(backend_in_files:.evince-backend.desktop.in.in=.evince-backend)

pdfdocument.evince-backend: pdfdocument.evince-backend.desktop.in
	$(AM_V_GEN)$(MSGFMT) --desktop --keyword=TypeDescription --template $< -d $(top_srcdir)/po -o $@

appstreamdir = $(datadir)/metainfo
appstream_in_files = evince-pdfdocument.metainfo.xml.in.in
appstream_DATA = $(appstream_in_files:.xml.in.in=.xml)

evince-pdfdocument.metainfo.xml: evince-pdfdocument.metainfo.xml.in
	$(AM_V_GEN)$(MSGFMT) --xml --template $< -d $(top_srcdir)/po -o $@

EXTRA_DIST = $(backend_in_files) $(appstream_in_files)

CLEANFILES = $(backend_DATA) $(appstream_DATA)

-include $(top_srcdir)/git.mk
