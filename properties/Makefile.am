INCLUDES=					\
	-DDATADIR=\"$(pkgdatadir)\"		\
	-DGNOMELOCALEDIR=\"$(datadir)/locale\"	\
	-I$(top_srcdir)/libdocument		\
	$(FRONTEND_CFLAGS)			\
	$(NAUTILUS_CFLAGS)			\
	$(DISABLE_DEPRECATED)			\
	$(WARN_CFLAGS)

noinst_LTLIBRARIES = libevproperties.la

libevproperties_la_SOURCES=			\
	ev-properties-view.c			\
	ev-properties-view.h

if HAVE_NAUTILUS
EVINCE_PROPERTIES_PAGE = libevince-properties-page.la
else
EVINCE_PROPERTIES_PAGE =
endif

nautilus_LTLIBRARIES = $(EVINCE_PROPERTIES_PAGE)

nautilusdir=$(NAUTILUS_EXTENSION_DIR)
libevince_properties_page_la_CFLAGS = -I$(top_srcdir)
libevince_properties_page_la_SOURCES = 	\
	ev-properties-main.c

libevince_properties_page_la_LIBADD = 		\
	libevproperties.la			\
	$(top_builddir)/libdocument/libevbackend.la	\
	$(FRONTEND_LIBS)			\
	$(NAUTILUS_LIBS)

if ENABLE_DJVU
libevince_properties_page_la_LIBADD += 	$(DJVU_LIBS)
endif

if ENABLE_DVI
libevince_properties_page_la_LIBADD += 	-lkpathsea
endif

if WITH_TYPE1_FONTS
libevince_properties_page_la_LIBADD += -lt1
endif

libevince_properties_page_la_LDFLAGS = -module -avoid-version -no-undefined
