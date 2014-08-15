plugindir = $(BROWSER_PLUGIN_DIR)
plugin_LTLIBRARIES = libevbrowserplugin.la

npapi_sources = \
	npapi/npapi.h \
	npapi/npfunctions.h \
	npapi/npruntime.h \
	npapi/nptypes.h

libevbrowserplugin_la_SOURCES = \
	$(npapi_sources) \
	EvBrowserPluginMain.cpp \
	EvBrowserPlugin.h \
	EvBrowserPlugin.cpp \
	EvBrowserPluginToolbar.h \
	EvBrowserPluginToolbar.cpp \
	EvMemoryUtils.h

nodist_libevbrowserplugin_la_SOURCES = \
	EvBrowserPluginResources.c

libevbrowserplugin_la_CPPFLAGS = \
	-I$(top_srcdir) \
	-I$(top_srcdir)/browser-plugin/npapi \
	-I$(top_srcdir)/libdocument \
	-I$(top_srcdir)/libview \
	-I$(top_srcdir)/libmisc \
	-DGNOMELOCALEDIR=\"$(datadir)/locale\" \
	-DEVINCE_COMPILATION \
	-DXP_UNIX \
	-DMOZ_X11 \
	$(BROWSER_PLUGIN_CFLAGS) \
	$(AM_CPPFLAGS)

libevbrowserplugin_la_CXXFLAGS = \
	-std=c++11 \
	-fno-exceptions \
	-fno-strict-aliasing \
	-fno-rtti \
	$(AM_CXXFLAGS)

libevbrowserplugin_la_LDFLAGS = \
	-avoid-version \
	-export-symbols $(top_srcdir)/browser-plugin/plugin.symbols \
	-module \
	-no-undefined \
	-Wl,-z,nodelete \
	$(AM_LDFLAGS)

libevbrowserplugin_la_LIBADD = \
	$(top_builddir)/libdocument/libevdocument3.la \
	$(top_builddir)/libview/libevview3.la \
	$(top_builddir)/libmisc/libevmisc.la \
	$(BROWSER_PLUGIN_LIBS)

EvBrowserPluginResources.c: EvBrowserPlugin.gresource.xml Makefile $(shell $(GLIB_COMPILE_RESOURCES) --generate-dependencies --sourcedir=$(srcdir) --sourcedir=$(top_srcdir)/data $(srcdir)/EvBrowserPlugin.gresource.xml)
	$(AM_V_GEN) XMLLINT=$(XMLLINT) $(GLIB_COMPILE_RESOURCES) --target=$@ --sourcedir=$(srcdir) --sourcedir=$(top_srcdir)/data --generate-source $<

EXTRA_DIST = \
	EvBrowserPlugin.gresource.xml \
	evince-browser.css \
	plugin.symbols

CLEANFILES = \
	EvBrowserPluginResources.c

-include $(top_srcdir)/git.mk
