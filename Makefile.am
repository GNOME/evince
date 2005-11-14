SUBDIRS = cut-n-paste po help data

# Backends

SUBDIRS += pdf ps

if ENABLE_PIXBUF
SUBDIRS += pixbuf    
endif

if ENABLE_DJVU
SUBDIRS += djvu
endif

if ENABLE_TIFF
SUBDIRS += tiff
endif

if ENABLE_DVI
SUBDIRS += dvi
endif

if ENABLE_COMICS
     SUBDIRS += comics
endif

SUBDIRS += backend

# Library

SUBDIRS += lib

# Properties (library and application)

SUBDIRS += properties

# Applications 

SUBDIRS += shell thumbnailer

intltool_extra = intltool-extract.in intltool-merge.in intltool-update.in

EXTRA_DIST = $(intltool_extra) gnome-doc-utils.make

DISTCLEANFILES = 		\
	gnome-doc-utils.make	\
	intltool-extract 	\
	intltool-merge 		\
	intltool-update

DISTCHECK_CONFIGURE_FLAGS = --disable-schemas-install --disable-scrollkeeper

# Ignore scrollkeeper issues for now.  @#*$& scrollkeeper
distuninstallcheck_listfiles = find . -type f -print | grep -v scrollkeeper | grep -v /share/gnome/help/ | grep -v \.omf
