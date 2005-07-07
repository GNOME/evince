@INTLTOOL_SCHEMAS_RULE@

NULL=

INCLUDES=					\
	-DDATADIR=\"$(pkgdatadir)\"		\
	-I$(top_srcdir)/lib			\
	-I$(top_srcdir)/backend			\
	-DGNOMELOCALEDIR=\"$(datadir)/locale\"	\
	-DGNOMEICONDIR=\""$(datadir)/pixmaps"\" \
	$(THUMBNAILER_CFLAGS)			\
	$(DISABLE_DEPRECATED)			\
	$(WARN_CFLAGS)				\
	$(NULL)

bin_PROGRAMS=evince-thumbnailer

evince_thumbnailer_SOURCES=	\
	evince-thumbnailer.c
	$(NULL)

evince_thumbnailer_LDADD=				\
	$(THUMBNAILER_LIBS)				\
	$(LIBTIFF)					\
	$(top_builddir)/lib/libev.la			\
	$(top_builddir)/backend/libevbackend.la		\
	-ltiff -lz					\
	$(NULL)

pixmapdir = $(pkgdatadir)
pixmap_DATA = pdf-icon.png

schema_files = \
	evince-thumbnailer-dvi.schemas.in  \
	evince-thumbnailer-djvu.schemas.in \
	evince-thumbnailer.schemas.in

schema_DATA = evince-thumbnailer.schemas

if ENABLE_DJVU
schema_DATA += evince-thumbnailer-djvu.schemas
endif

if ENABLE_DVI
schema_DATA += evince-thumbnailer-dvi.schemas
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
	$(pixmap_DATA)	\
	$(schema_files)  

DISTCLEANFILES = \
	$(schema_DATA)
