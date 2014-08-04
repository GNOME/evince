SUBDIRS = icons
NULL=

install-data-local: update-icon-cache

#
# man file
#

man_MANS=evince.1

#
# UI descriptions
#

uidir = $(pkgdatadir)
ui_DATA =				\
	hand-open.png

#
# Desktop file
#

@INTLTOOL_DESKTOP_RULE@

DESKTOP_IN_FILES= evince.desktop.in.in evince-previewer.desktop.in.in
DESKTOP_FILES= $(DESKTOP_IN_FILES:.desktop.in.in=.desktop)

desktopdir = $(datadir)/applications
desktop_DATA = $(DESKTOP_FILES)

#
# DBus servide file
#
if ENABLE_DBUS
servicedir = $(datadir)/dbus-1/services
service_in_files = org.gnome.evince.Daemon.service.in
service_DATA = $(service_in_files:.service.in=.service)

$(service_DATA): $(service_in_files) Makefile
	$(AM_V_GEN) $(SED) -e "s|\@libexecdir\@|$(libexecdir)|" $< > $@
endif

#
# GSettings schema
#
gsettingsschema_in_files = org.gnome.Evince.gschema.xml.in
# gsettings_SCHEMAS is a list of all the schemas you want to install
gsettings_SCHEMAS = $(gsettingsschema_in_files:.xml.in=.xml)

.PRECIOUS: $(gsettings_SCHEMAS)

@INTLTOOL_XML_NOMERGE_RULE@

# include the appropriate makefile rules for schema handling
@GSETTINGS_RULES@

gsettingsconvertdir = $(datadir)/GConf/gsettings
gsettingsconvert_DATA = evince.convert

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

EXTRA_DIST =					\
	$(ui_DATA)				\
	$(DESKTOP_IN_FILES)			\
	$(gsettingsschema_in_files)		\
	org.gnome.evince.Daemon.service.in	\
	$(man_MANS)				\
	evince.ico				\
	evince.convert				\
	thumbnail-frame.png			\
	$(NULL)

#
# Clean up properly
#

DISTCLEANFILES = \
	$(DESKTOP_FILES)	\
	$(gsettings_SCHEMAS)	\
	$(service_DATA)

-include $(top_srcdir)/git.mk
