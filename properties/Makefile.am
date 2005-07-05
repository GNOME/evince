NULL=

INCLUDES=					\
	-DDATADIR=\"$(pkgdatadir)\"		\
	-DGNOMELOCALEDIR=\"$(datadir)/locale\"	\
	-I$(top_srcdir)/backend			\
	$(PROPERTIES_CFLAGS)			\
	$(NAUTILUS_CFLAGS)			\
	$(EVINCE_DISABLE_DEPRECATED)		\
	$(NULL)

noinst_LTLIBRARIES = libevproperties.la

libevproperties_la_SOURCES=			\
	ev-properties-view.c			\
	ev-properties-view.h			\
	$(NULL)

if HAVE_NAUTILUS
EVINCE_PROPERTIES_PAGE = libevince-properties-page.la
else
EVINCE_PROPERTIES_PAGE =
endif

nautilus_LTLIBRARIES = $(EVINCE_PROPERTIES_PAGE)

nautilusdir = $(libdir)/nautilus/extensions-1.0/
libevince_properties_page_la_CFLAGS = -I$(top_srcdir)
libevince_properties_page_la_SOURCES = 	\
	ev-properties-main.c

libevince_properties_page_la_LIBADD = 			\
	$(PROPERTIES_LIBS)				\
	libevproperties.la				\
	$(top_builddir)/shell/libevbackendfactory.la	\
	$(top_builddir)/lib/libev.la			\
	-ltiff -lz
