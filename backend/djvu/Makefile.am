backend_LTLIBRARIES = libdjvudocument.la

libdjvudocument_la_SOURCES = 	\
	djvu-document.c		\
	djvu-document.h 	\
	djvu-document-private.h \
	djvu-links.c		\
	djvu-links.h		\
	djvu-text-page.c 	\
	djvu-text-page.h

libdjvudocument_la_CPPFLAGS = \
	-I$(top_srcdir) \
	-I$(top_srcdir)/libdocument \
	-DGNOMEICONDIR=\""${prefix}/${DATADIRNAME}/pixmaps"\" \
	-DGNOMELOCALEDIR=\"$(datadir)/locale\" \
	-DEVINCE_COMPILATION \
	$(AM_CPPFLAGS)

libdjvudocument_la_CFLAGS = \
	$(BACKEND_CFLAGS) \
	$(DJVU_CFLAGS) \
	$(AM_CFLAGS)

libdjvudocument_la_LDFLAGS = $(BACKEND_LIBTOOL_FLAGS)
libdjvudocument_la_LIBADD =				\
	$(top_builddir)/libdocument/libevdocument3.la	\
	$(BACKEND_LIBS)					\
	$(DJVU_LIBS)

backend_in_files = djvudocument.evince-backend.desktop.in.in
backend_DATA = $(backend_in_files:.evince-backend.desktop.in.in=.evince-backend)

djvudocument.evince-backend: djvudocument.evince-backend.desktop.in
	$(AM_V_GEN)$(MSGFMT) --desktop --keyword=TypeDescription --template $< -d $(top_srcdir)/po -o $@

appstreamdir = $(datadir)/metainfo
appstream_in_files = evince-djvudocument.metainfo.xml.in.in
appstream_DATA = $(appstream_in_files:.xml.in.in=.xml)

evince-djvudocument.metainfo.xml: evince-djvudocument.metainfo.xml.in
	$(AM_V_GEN)$(MSGFMT) --xml --template $< -d $(top_srcdir)/po -o $@

EXTRA_DIST = $(backend_in_files) $(appstream_in_files)

CLEANFILES = $(backend_DATA) $(appstream_DATA)

-include $(top_srcdir)/git.mk
