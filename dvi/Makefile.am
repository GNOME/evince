SUBDIRS = dvilib

INCLUDES =					\
	-I$(srcdir)/dvilib			\
	$(DVI_CFLAGS)				\
	-I$(top_srcdir)

bin_PROGRAMS=dviviewer

dviviewer_SOURCES =				\
	main.cc					\
						\
	painter.hh				\
	painter.cc				\
						\
	font.hh					\
	font.cc					\
						\
	view.hh					\
	view.cc					\
						\
	model.hh				\
	model.cc				\
						\
	observer.hh

dviviewer_LDADD = 				\
        $(top_builddir)/dvi/dvilib/libdvilib.a	\
	$(GTK_LIBS)
