ACLOCAL_AMFLAGS = -I m4 ${ACLOCAL_FLAGS}

SUBDIRS = \
	cut-n-paste \
	data \
	libdocument \
	backend \
	libview \
	libmisc \
	properties

if ENABLE_VIEWER
SUBDIRS += shell
endif

SUBDIRS += \
	po \
	help

if ENABLE_THUMBNAILER
SUBDIRS += thumbnailer
endif

if ENABLE_PREVIEWER
SUBDIRS += previewer
endif

if ENABLE_BROWSER_PLUGIN
SUBDIRS += browser-plugin
endif

NULL =

pkgconfigdir = $(libdir)/pkgconfig
pkgconfig_DATA = \
	evince-document-$(EV_API_VERSION).pc \
	evince-view-$(EV_API_VERSION).pc \
	$(NULL)

headerdir = $(includedir)/evince/$(EV_API_VERSION)
header_DATA = \
	evince-document.h \
	evince-view.h \
	$(NULL)

# Appdata files

appdatadir = $(datadir)/metainfo
appdata_in_files = org.gnome.Evince.appdata.xml.in
nodist_appdata_DATA = $(appdata_in_files:.xml.in=.xml)

org.gnome.Evince.appdata.xml: org.gnome.Evince.appdata.xml.in
	$(AM_V_GEN)$(MSGFMT) --xml --template $< -d $(top_srcdir)/po -o $@

# Applications 

EXTRA_DIST = \
	$(header_DATA) \
	$(appdata_in_files) \
	autogen.sh \
	MAINTAINERS \
	README.md \
	generate-changelog.sh \
	lib-symbol.map \
	meson.build \
	meson_post_install.py \
	meson_options.txt \
	previewer/meson.build \
	libmisc/meson.build \
	cut-n-paste/libgd/meson.build \
	cut-n-paste/unarr/meson.build \
	cut-n-paste/gimpcellrenderertoggle/meson.build \
	cut-n-paste/meson.build \
	cut-n-paste/synctex/meson.build \
	shell/meson.build \
	properties/meson.build \
	thumbnailer/meson.build \
	data/icons/meson.build \
	data/meson.build \
	help/reference/shell/meson.build \
	help/reference/libview/meson.build \
	help/reference/libdocument/meson.build \
	help/meson.build \
	help/LINGUAS \
	libview/meson.build \
	backend/backend-symbol.map \
	backend/xps/meson.build \
	backend/tiff/meson.build \
	backend/pdf/meson.build \
	backend/comics/meson.build \
	backend/dvi/mdvi-lib/meson.build \
	backend/dvi/meson.build \
	backend/ps/meson.build \
	backend/meson.build \
	backend/djvu/meson.build \
	po/meson.build \
	libdocument/meson.build \
	browser-plugin/meson.build \
	browser-plugin/plugin-symbol.map


CLEANFILES = \
	$(nodist_appdata_DATA) \
	$(NULL)

DISTCLEANFILES = 

MAINTAINERCLEANFILES = \
	ChangeLog \
	$(srcdir)/INSTALL \
	$(srcdir)/aclocal.m4 \
	$(srcdir)/autoscan.log \
	$(srcdir)/compile \
	$(srcdir)/config.guess \
	$(srcdir)/config.h.in \
	$(srcdir)/config.sub \
	$(srcdir)/configure \
	$(srcdir)/configure.scan \
	$(srcdir)/depcomp \
	$(srcdir)/install-sh \
	$(srcdir)/ltmain.sh \
	$(srcdir)/m4 \
	$(srcdir)/missing \
	$(srcdir)/mkinstalldirs \
	$(srcdir)/gtk-doc.make \
	$(srcdir)/po/Makefile.in.in \
	`find "$(srcdir)" -type f -name Makefile.in -print`

DISTCHECK_CONFIGURE_FLAGS = \
	--disable-schemas-install \
	--disable-scrollkeeper \
	--enable-gtk-doc \
	--disable-nautilus \
	--disable-tests \
	--disable-silent-rules \
	--enable-introspection

# Ignore scrollkeeper issues for now.  @#*$& scrollkeeper
distuninstallcheck_listfiles = find . -type f -print | grep -v scrollkeeper | grep -v /share/gnome/help/ | grep -v \.omf

distclean-local:
	if test "$(srcdir)" = "."; then :; else \
		rm -f ChangeLog; \
	fi

ChangeLog:
	$(AM_V_GEN) if test -d "$(srcdir)/.git"; then \
	  (GIT_DIR=$(top_srcdir)/.git ./missing --run git log -M -C --name-status --date=short --no-color) | fmt --split-only > $@.tmp \
	  && mv -f $@.tmp $@ \
	  || ($(RM) $@.tmp; \
	      echo Failed to generate ChangeLog, your ChangeLog may be outdated >&2; \
	      (test -f $@ || echo git log is required to generate this file >> $@)); \
	else \
	  test -f $@ || \
	  (echo A git checkout and git log is required to generate ChangeLog >&2 && \
	  echo A git checkout and git log is required to generate this file >> $@); \
	fi

.PHONY: ChangeLog

-include $(top_srcdir)/git.mk
