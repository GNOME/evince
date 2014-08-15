NULL =

bin_PROGRAMS = evince-previewer

evince_previewer_SOURCES = \
	ev-previewer.c \
	ev-previewer-window.h \
	ev-previewer-window.c \
	$(NULL)

nodist_evince_previewer_SOURCES = \
	ev-previewer-resources.c \
	$(NULL)

evince_previewer_CPPFLAGS = \
	-I$(top_srcdir)                         \
	-I$(top_builddir) 			\
        -I$(top_srcdir)/libdocument             \
        -I$(top_srcdir)/libview                 \
	-I$(top_srcdir)/libmisc			\
	$(AM_CPPFLAGS)

evince_previewer_CFLAGS = \
	$(PREVIEWER_CFLAGS)			\
	$(AM_CFLAGS)

evince_previewer_LDFLAGS = $(AM_LDFLAGS)

if PLATFORM_WIN32
evince_previewer_LDFLAGS += -mwindows
endif

evince_previewer_LDADD = \
	$(top_builddir)/libdocument/libevdocument3.la	\
	$(top_builddir)/libview/libevview3.la		\
	$(top_builddir)/libmisc/libevmisc.la		\
	$(PREVIEWER_LIBS)

ev-previewer-resources.c: previewer.gresource.xml Makefile $(shell $(GLIB_COMPILE_RESOURCES) --generate-dependencies --sourcedir=$(srcdir) --sourcedir=$(top_srcdir)/data $(srcdir)/previewer.gresource.xml)
	$(AM_V_GEN) XMLLINT=$(XMLLINT) $(GLIB_COMPILE_RESOURCES) --target=$@ --sourcedir=$(srcdir) --sourcedir=$(top_srcdir)/data --generate-source --c-name ev_previewer $<

EXTRA_DIST = \
	previewer.gresource.xml \
	evince-previewer.css \
	previewer-ui.xml \
	$(NULL)

CLEANFILES = \
	ev-previewer-resources.c \
	$(NULL)

-include $(top_srcdir)/git.mk
