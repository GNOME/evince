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
xdgmimedir = $(datadir)/mime/packages
xdgmime_DATA = ev-cbr-mime.xml

EXTRA_DIST = $(xdgmime_DATA)

if ENABLE_UPDATE_MIMEDB

install-data-hook:
	$(UPDATE_MIME_DB) $(DESTDIR)$(datadir)/mime

uninstall-hook:
	$(UPDATE_MIME_DB) $(DESTDIR)$(datadir)/mime

endif # ENABLE_UPDATE_MIMEDB

