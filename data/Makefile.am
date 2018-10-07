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

DESKTOP_IN_FILES= org.gnome.Evince.desktop.in.in org.gnome.Evince-previewer.desktop.in.in
DESKTOP_FILES= $(DESKTOP_IN_FILES:.desktop.in.in=.desktop)

desktopdir = $(datadir)/applications
desktop_DATA = $(DESKTOP_FILES)

org.gnome.Evince-previewer.desktop: org.gnome.Evince-previewer.desktop.in
	$(AM_V_GEN)$(MSGFMT) --desktop --template $< -d $(top_srcdir)/po -o $@

org.gnome.Evince.desktop: org.gnome.Evince.desktop.in
	$(AM_V_GEN)$(MSGFMT) --desktop --template $< -d $(top_srcdir)/po -o $@

#
# DBus servide file
#
if ENABLE_DBUS
servicedir = $(datadir)/dbus-1/services
service_in_files = org.gnome.evince.Daemon.service.in
service_DATA = $(service_in_files:.service.in=.service)

systemd_user_in_files = org.gnome.Evince.service.in

if WITH_SYSTEMD_USER_UNITS
systemd_user_DATA = $(systemd_user_in_files:.service.in=.service)

$(systemd_user_DATA): $(systemd_user_in_files) Makefile
	$(AM_V_GEN) $(SED) -e "s|\@libexecdir\@|$(libexecdir)|" $< > $@
endif

$(service_DATA): $(service_in_files) Makefile
	$(AM_V_GEN) $(SED) \
		-e "s|\@libexecdir\@|$(libexecdir)|" \
		$< > $@
endif

#
# GSettings schema
#
# gsettings_SCHEMAS is a list of all the schemas you want to install
gsettings_SCHEMAS = org.gnome.Evince.gschema.xml

.PRECIOUS: $(gsettings_SCHEMAS)

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
	$(gsettings_SCHEMAS)			\
	$(man_MANS)				\
	$(service_in_files)			\
	$(systemd_user_in_files)		\
	evince.ico				\
	evince.convert				\
	thumbnail-frame.png			\
	$(NULL)

#
# Clean up properly
#

DISTCLEANFILES = \
	$(DESKTOP_FILES)	\
	$(service_DATA)		\
	$(systemd_user_DATA)	\
	$(NULL)

-include $(top_srcdir)/git.mk
