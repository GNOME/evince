SUBDIRS = icons

@INTLTOOL_DESKTOP_RULE@
@INTLTOOL_SCHEMAS_RULE@

NULL=

uidir = $(pkgdatadir)
ui_DATA =				\
	evince-ui.xml			\
	evince-toolbar.xml		\
	hand-open.png			\
	$(NULL)

gladedir = $(pkgdatadir)
glade_DATA = 				\
	evince-password.glade		\
	evince-properties.glade		\
	$(NULL)

DESKTOP_IN_FILES= evince.desktop.in.in
DESKTOP_FILES= $(DESKTOP_IN_FILES:.desktop.in.in=.desktop)

desktopdir = $(datadir)/applications
desktop_DATA = $(DESKTOP_FILES)

schema_in_files = evince.schemas.in
schemadir = $(GCONF_SCHEMA_FILE_DIR)
schema_DATA = $(schema_in_files:.schemas.in=.schemas)

gtk_update_icon_cache = gtk-update-icon-cache -f -t $(datadir)/icons/hicolor

install-data-hook:
if GCONF_SCHEMAS_INSTALL
	if test -z "$(DESTDIR)" ; then \
	for p in $(schema_DATA) ; do \
	GCONF_CONFIG_SOURCE=$(GCONF_SCHEMA_CONFIG_SOURCE) $(GCONFTOOL) --makefile-install-rule $(top_builddir)/data/$$p >&1 > /dev/null; \
	done \
	fi
endif
	if test -z "$(DESTDIR)"; then \
	        echo "Updating Gtk icon cache."; \
	        $(gtk_update_icon_cache); \
	else \
	        echo "*** Icon cache not updated.  After install, run this:"; \
	        echo "***   $(gtk_update_icon_cache)"; \
	        fi

EXTRA_DIST =				\
	$(glade_DATA)			\
	$(ui_DATA)			\
	$(DESKTOP_IN_FILES)		\
	$(schema_in_files)		\
	$(NULL)				

DISTCLEANFILES = \
	$(DESKTOP_FILES)	\
	$(schema_DATA)


