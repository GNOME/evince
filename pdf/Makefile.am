INCLUDES =					\
	-I$(top_srcdir)				\
	-I$(top_srcdir)/backend			\
	$(POPPLER_CFLAGS)			\
	$(GTK_CFLAGS)				\
	-DDATADIR=\""$(datadir)"\"

noinst_PROGRAMS = test-gdk-output-dev

noinst_LTLIBRARIES = libpdfdocument.la

libpdfdocument_la_SOURCES =			\
	GDKSplashOutputDev.cc			\
	GDKSplashOutputDev.h			\
	Thumb.cc				\
	Thumb.h					\
	pdf-document.cc				\
	pdf-document.h

test_gdk_output_dev_SOURCES =			\
       test-gdk-output-dev.cc

test_gdk_output_dev_LDADD =			\
	libpdfdocument.la			\
	$(top_builddir)/backend/libevbackend.la	\
	$(POPPLER_LIBS)				\
	$(GTK_LIBS)
