INCLUDES =					\
	-I$(top_srcdir)				\
	-I$(top_srcdir)/backend			\
	$(POPPLER_GLIB_CFLAGS)			\
	$(GTK_CFLAGS)				\
	-DDATADIR=\""$(datadir)"\"

noinst_LTLIBRARIES = libpdfdocument.la

libpdfdocument_la_SOURCES =			\
	ev-poppler.cc				\
	ev-poppler.h


test_gdk_output_dev_LDADD =			\
	libpdfdocument.la			\
	$(top_builddir)/backend/libevbackend.la	\
	$(POPPLER_LIBS)				\
	$(GTK_LIBS)
