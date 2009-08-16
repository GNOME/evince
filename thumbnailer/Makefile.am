
bin_PROGRAMS = evince-thumbnailer

evince_thumbnailer_SOURCES = \
	evince-thumbnailer.c

evince_thumbnailer_CPPFLAGS = \
	-DDATADIR=\"$(pkgdatadir)\"		\
	-I$(top_srcdir)				\
	-I$(top_builddir)			\
	-DGNOMELOCALEDIR=\"$(datadir)/locale\"	\
	-DGNOMEICONDIR=\""$(datadir)/pixmaps"\" \
	$(AM_CPPFLAGS)

evince_thumbnailer_CFLAGS = \
	$(FRONTEND_CFLAGS)	\
	$(DISABLE_DEPRECATED)	\
	$(WARN_CFLAGS)		\
	$(AM_CFLAGS)

evince_thumbnailer_LDFLAGS = $(AM_LDFLAGS)

if PLATFORM_WIN32
evince_thumbnailer_LDFLAGS += -mwindows
endif

evince_thumbnailer_LDADD = \
	$(top_builddir)/libdocument/libevdocument.la	\
	$(FRONTEND_LIBS)

schema_files = \
	evince-thumbnailer-dvi.schemas.in  \
	evince-thumbnailer-djvu.schemas.in \
	evince-thumbnailer-comics.schemas.in \
	evince-thumbnailer-ps.schemas.in \
	evince-thumbnailer.schemas.in

if WITH_GCONF

@INTLTOOL_SCHEMAS_RULE@

schema_DATA = 

if ENABLE_PDF
schema_DATA += evince-thumbnailer.schemas
endif

if ENABLE_PS
schema_DATA += evince-thumbnailer-ps.schemas
endif

if ENABLE_DJVU
schema_DATA += evince-thumbnailer-djvu.schemas
endif

if ENABLE_DVI
schema_DATA += evince-thumbnailer-dvi.schemas
endif

if ENABLE_COMICS
schema_DATA += evince-thumbnailer-comics.schemas
endif

schemadir = $(GCONF_SCHEMA_FILE_DIR)

install-data-local:
if GCONF_SCHEMAS_INSTALL
	if test -z "$(DESTDIR)" ; then \
	for p in $(schema_DATA) ; do \
	GCONF_CONFIG_SOURCE=$(GCONF_SCHEMA_CONFIG_SOURCE) $(GCONFTOOL) --makefile-install-rule $(top_builddir)/thumbnailer/$$p >&1 > /dev/null; \
	done \
	fi
endif

endif # WITH_GCONF

EXTRA_DIST =         	\
	$(schema_files)  

DISTCLEANFILES = \
	$(schema_DATA)
