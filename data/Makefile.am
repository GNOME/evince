SUBDIRS = icons
NULL=

install-data-local: install-schemas update-icon-cache
uninstall-local: uninstall-schemas

#
# man file
#

man_MANS=evince.1

#
# UI descriptions
#

uidir = $(pkgdatadir)
ui_DATA =				\
	evince-ui.xml			\
	evince-toolbar.xml		\
	hand-open.png

if ENABLE_PREVIEWER
ui_DATA += evince-previewer-ui.xml
endif

#
# Desktop file
#

@INTLTOOL_DESKTOP_RULE@

DESKTOP_IN_FILES= evince.desktop.in.in
DESKTOP_FILES= $(DESKTOP_IN_FILES:.desktop.in.in=.desktop)

desktopdir = $(datadir)/applications
desktop_DATA = $(DESKTOP_FILES)


#
# GConf schema
#

if WITH_GCONF
@INTLTOOL_SCHEMAS_RULE@

schema_in_files = evince.schemas.in
schemadir = $(GCONF_SCHEMA_FILE_DIR)
schema_DATA = $(schema_in_files:.schemas.in=.schemas)

install-schemas: $(schema_DATA)
if GCONF_SCHEMAS_INSTALL
	if test -z "$(DESTDIR)" ; then \
	for p in $^ ; do \
		GCONF_CONFIG_SOURCE=$(GCONF_SCHEMA_CONFIG_SOURCE) \
			$(GCONFTOOL) \
			--makefile-install-rule \
			$(top_builddir)/data/$$p; \
	done \
	fi
endif

uninstall-schemas: $(schema_DATA)
if GCONF_SCHEMAS_INSTALL
	if test -z "$(DESTDIR)" ; then \
	for p in $^ ; do \
		GCONF_CONFIG_SOURCE=$(GCONF_SCHEMA_CONFIG_SOURCE) \
			$(GCONFTOOL) \
			--makefile-uninstall-rule \
			$(top_builddir)/data/$$p; \
	done \
	fi
endif

else

install-schemas:
uninstall-schemas:

endif

#
# GTK icon cache
#

gtk_update_icon_cache = gtk-update-icon-cache -f -t $(datadir)/icons/hicolor

install-data-hook: update-icon-cache
uninstall-hook: update-icon-cache

update-icon-cache:
	if test -z "$(DESTDIR)"; then \
	        echo "Updating Gtk icon cache."; \
	        $(gtk_update_icon_cache); \
	else \
	        echo "*** Icon cache not updated.  After (un)install, run this:"; \
	        echo "***   $(gtk_update_icon_cache)"; \
	        fi


#
# Extra files to be included in the tarball
#

EXTRA_DIST =				\
	$(ui_DATA)			\
	$(DESKTOP_IN_FILES)		\
	$(schema_in_files)		\
	$(man_MANS)			\
	$(NULL)				

#
# Clean up properly
#

DISTCLEANFILES = \
	$(DESKTOP_FILES)	\
	$(schema_DATA)
