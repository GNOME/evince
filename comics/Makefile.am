INCLUDES =                      \
	-I$(top_srcdir)		\
	-I$(top_srcdir)/backend	\
	-I$(top_srcdir)/lib	\
	$(LIB_CFLAGS)

noinst_LTLIBRARIES = libcomicsdocument.la

libcomicsdocument_la_SOURCES = \
	comics-document.c      \
	comics-document.h

# need to respect $XDG_DATA_DIR probably
xdgmimedir = $(gnome_data)/mime/packages
xdgmime_DATA = ev-cbr-mime.xml

EXTRA_DIST = $(xdgmime_DATA)

install-data-hook:
	update-mime-database $(gnome_data)/mime

uninstall-hook:
	update-mime-database $(gnome_data)/mime


