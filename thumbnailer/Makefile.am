@INTLTOOL_SCHEMAS_RULE@

INCLUDES=					\
	-DDATADIR=\"$(pkgdatadir)\"		\
	-I$(top_srcdir)/lib			\
	-I$(top_srcdir)/backend			\
	-DGNOMELOCALEDIR=\"$(datadir)/locale\"	\
	-DGNOMEICONDIR=\""$(datadir)/pixmaps"\" \
	$(FRONTEND_CFLAGS)			\
	$(DISABLE_DEPRECATED)			\
	$(WARN_CFLAGS)

bin_PROGRAMS=evince-thumbnailer

evince_thumbnailer_SOURCES=	\
	evince-thumbnailer.c

evince_thumbnailer_LDADD=		\
	$(top_builddir)/lib/libev.la	\
	$(FRONTEND_LIBS)		\
	$(DJVU_LIBS)

if ENABLE_DJVU
evince_thumbnailer_LDADD += $(DJVU_LIBS)
endif

if ENABLE_DJVU
evince_thumbnailer_LDADD += -lkpathsea
endif

if WITH_TYPE1_FONTS
evince_thumbnailer_LDADD += -lt1
endif

schema_files = \
	evince-thumbnailer-dvi.schemas.in  \
	evince-thumbnailer-djvu.schemas.in \
	evince-thumbnailer-comics.schemas.in \
	evince-thumbnailer.schemas.in

schema_DATA = evince-thumbnailer.schemas

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
	GCONF_CONFIG_SOURCE=$(GCONF_SCHEMA_CONFIG_SOURCE) $(GCONFTOOL) --makefile-install-rule $(top_srcdir)/thumbnailer/$$p >&1 > /dev/null; \
	done \
	fi
endif

EXTRA_DIST =         	\
	$(schema_files)  

DISTCLEANFILES = \
	$(schema_DATA)
