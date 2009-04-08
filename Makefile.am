SUBDIRS = \
	cut-n-paste \
	data \
	libdocument \
	backend \
	libview \
	libmisc \
	properties \
	shell \
	po \
	help \
	test

if ENABLE_THUMBNAILER
SUBDIRS += thumbnailer
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

# Applications 

intltool_extra = intltool-extract.in intltool-merge.in intltool-update.in

EXTRA_DIST = \
	$(intltool_extra) \
	$(header_DATA) \
	autogen.sh \
	gnome-doc-utils.make \
	MAINTAINERS

DISTCLEANFILES = 		\
	gnome-doc-utils.make	\
	intltool-extract 	\
	intltool-merge 		\
	intltool-update

DISTCHECK_CONFIGURE_FLAGS = --disable-schemas-install --disable-scrollkeeper --enable-gtk-doc

# Ignore scrollkeeper issues for now.  @#*$& scrollkeeper
distuninstallcheck_listfiles = find . -type f -print | grep -v scrollkeeper | grep -v /share/gnome/help/ | grep -v \.omf
