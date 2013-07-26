
bin_PROGRAMS = evince-thumbnailer

evince_thumbnailer_SOURCES = \
	evince-thumbnailer.c

evince_thumbnailer_CPPFLAGS = \
	-I$(top_srcdir)				\
	-I$(top_builddir)			\
	$(AM_CPPFLAGS)

evince_thumbnailer_CFLAGS = \
	$(FRONTEND_CFLAGS)	\
	$(AM_CFLAGS)

evince_thumbnailer_LDFLAGS = $(AM_LDFLAGS)

if PLATFORM_WIN32
evince_thumbnailer_LDFLAGS += -mwindows
endif

evince_thumbnailer_LDADD = \
	$(top_builddir)/libdocument/libevdocument3.la	\
	$(FRONTEND_LIBS)

thumbnailerdir = $(datadir)/thumbnailers
thumbnailer_in_files = evince.thumbnailer.in
thumbnailer_DATA = $(thumbnailer_in_files:.thumbnailer.in=.thumbnailer)

evince.thumbnailer: $(thumbnailer_in_files)
	$(AM_V_GEN)sed \
		-e "s|\@EVINCE_MIME_TYPES\@|$(EVINCE_MIME_TYPES)|" \
		$< > $@


EXTRA_DIST =         	\
	$(thumbnailer_in_files)

DISTCLEANFILES = \
	$(thumbnailer_DATA)

-include $(top_srcdir)/git.mk
