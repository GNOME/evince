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

appdatadir = $(datadir)/appdata
appdata_in_files = evince.appdata.xml.in
nodist_appdata_DATA = $(appdata_in_files:.xml.in=.xml)

@INTLTOOL_XML_RULE@

# Applications 

intltool_extra = intltool-extract.in intltool-merge.in intltool-update.in

EXTRA_DIST = \
	$(intltool_extra) \
	$(header_DATA) \
	$(appdata_in_files) \
	autogen.sh \
	MAINTAINERS

CLEANFILES = \
	$(nodist_appdata_DATA) \
	$(NULL)

DISTCLEANFILES = 		\
	intltool-extract 	\
	intltool-merge 		\
	intltool-update

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
