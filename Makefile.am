SUBDIRS = lib cut-n-paste backend po help data

# Backends

SUBDIRS += pdf ps pixbuf tiff

if ENABLE_DJVU
     SUBDIRS += djvu
endif

if ENABLE_DVI
     SUBDIRS += dvi
endif

# Applications 

SUBDIRS += shell thumbnailer

intltool_extra = intltool-extract.in intltool-merge.in intltool-update.in

EXTRA_DIST = $(intltool_extra)

DISTCLEANFILES = 		\
	intltool-extract 	\
	intltool-merge 		\
	intltool-update

DISTCHECK_CONFIGURE_FLAGS = --disable-schemas-install

# Ignore scrollkeeper issues for now.  @#*$& scrollkeeper
distuninstallcheck_listfiles = find . -type f -print | grep -v scrollkeeper | grep -v /share/gnome/help/ | grep -v \.omf
