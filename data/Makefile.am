@INTLTOOL_DESKTOP_RULE@
@INTLTOOL_SCHEMAS_RULE@

NULL =

uidir = $(pkgdatadir)
ui_DATA =				\
	evince-ui.xml			\
	$(NULL)

gladedir = $(pkgdatadir)
glade_DATA = 				\
	evince-password.glade		\
	$(NULL)

stockdir = $(datadir)/pixmaps/evince
stock_DATA =				\
        ev-stock-zoom-fit-width.png	\
	$(NULL)

DESKTOP_IN_FILES= evince.desktop.in
DESKTOP_FILES= $(DESKTOP_IN_FILES:.desktop.in=.desktop)

desktopdir = $(datadir)/applications
desktop_DATA = $(DESKTOP_FILES)

schema_in_files = evince.schemas.in
schemadir = $(GCONF_SCHEMA_FILE_DIR)
schema_DATA = $(schema_in_files:.schemas.in=.schemas)

install-data-local:
if GCONF_SCHEMAS_INSTALL
	if test -z "$(DESTDIR)" ; then \
	for p in $(schema_DATA) ; do \
	GCONF_CONFIG_SOURCE=$(GCONF_SCHEMA_CONFIG_SOURCE) $(GCONFTOOL) --makefile-install-rule $(top_builddir)/data/$$p >&1 > /dev/null; \
	done \
	fi
endif

EXTRA_DIST =				\
	$(stock_DATA)			\
	$(glade_DATA)			\
	$(ui_DATA)			\
	$(DESKTOP_IN_FILES)		\
	$(schema_in_files)		\
	$(NULL)

DISTCLEANFILES = \
	$(DESKTOP_FILES)	\
	$(schema_DATA)
