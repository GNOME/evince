noinst_LTLIBRARIES = libevproperties.la

libevproperties_la_SOURCES=			\
	ev-properties-view.c			\
	ev-properties-view.h

libevproperties_la_CPPFLAGS = \
	-I$(top_srcdir) \
	-I$(top_builddir) \
	$(AM_CPPFLAGS)

libevproperties_la_CFLAGS = \
	$(FRONTEND_CFLAGS) \
	$(AM_CFLAGS)

if ENABLE_NAUTILUS

nautilusextensiondir=$(libdir)/$(NAUTILUS_EXTENSION_DIR)
nautilusextension_LTLIBRARIES = libevince-properties-page.la

libevince_properties_page_la_SOURCES = 	\
	ev-properties-main.c

libevince_properties_page_la_CPPFLAGS = \
	-I$(top_srcdir) \
	-I$(top_builddir) \
	$(AM_CPPFLAGS)

libevince_properties_page_la_CFLAGS = \
	$(FRONTEND_CFLAGS) \
	$(NAUTILUS_CFLAGS) \
	$(AM_CFLAGS)

libevince_properties_page_la_LIBADD = 		\
	libevproperties.la			\
	$(top_builddir)/libdocument/libevdocument3.la	\
	$(FRONTEND_LIBS)			\
	$(NAUTILUS_LIBS)

libevince_properties_page_la_LDFLAGS = -module -avoid-version -no-undefined

endif # ENABLE_NAUTILUS

-include $(top_srcdir)/git.mk
