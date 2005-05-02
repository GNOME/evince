noinst_LTLIBRARIES = libephymisc.la libephywidgets.la

libephywidgets_la_SOURCES = \
	ephy-zoom-action.h			\
	ephy-zoom-action.c			\
	ephy-zoom-control.c			\
	ephy-zoom-control.h

libephywidgets_la_CPPFLAGS = \
	-I$(top_builddir)/lib		\
	-I$(top_srcdir)/lib		\
	-DSHARE_DIR=\"$(pkgdatadir)\"	\
	$(AM_CPPFLAGS)

libephywidgets_la_CFLAGS = \
	$(ZOOM_CONTROL_CFLAGS)	\
	$(AM_CFLAGS)

libephywidgets_la_LIBADD = \
	libephymisc.la

NOINST_H_FILES = \
	ephy-zoom.h

libephymisc_la_SOURCES = \
	ephy-zoom.c				\
	$(NOINST_H_FILES)

libephymisc_la_CPPFLAGS = \
	-I$(top_builddir)/lib			\
	-DSHARE_DIR=\"$(pkgdatadir)\" 		\
	$(AM_CPPFLAGS)

libephymisc_la_CFLAGS = \
        $(ZOOM_CONTROL_CFLAGS)	\
	$(AM_CFLAGS)