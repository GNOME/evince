SUBDIRS = lib cut-n-paste data backend po pdf ps shell thumbnailer

intltool_extra = intltool-extract.in intltool-merge.in intltool-update.in

EXTRA_DIST = $(intltool_extra)

DISTCLEANFILES = 		\
	intltool-extract 	\
	intltool-merge 		\
	intltool-update

DISTCHECK_CONFIGURE_FLAGS = --disable-schemas-install
