INCLUDES =					\
	-I$(top_srcdir)				\
	-I$(top_srcdir)/libdocument		\
	-DGNOMELOCALEDIR=\"$(datadir)/locale\"  \
	-DEVINCE_COMPILATION			\
	$(BACKEND_CFLAGS)			\
	$(POPPLER_CFLAGS)                       \
	$(WARN_CXXFLAGS)			\
	$(DISABLE_DEPRECATED)

backend_LTLIBRARIES = libpdfdocument.la

libpdfdocument_la_SOURCES =			\
	ev-poppler.cc				\
	ev-poppler.h

libpdfdocument_la_LDFLAGS = $(BACKEND_LIBTOOL_FLAGS)
libpdfdocument_la_LIBADD =				\
	$(top_builddir)/libdocument/libevdocument.la	\
	$(BACKEND_LIBS)					\
	$(POPPLER_LIBS)					\
	$(CAIRO_PDF_LIBS)				\
	$(CAIRO_PS_LIBS)

backend_in_files = pdfdocument.evince-backend.in
backend_DATA = $(backend_in_files:.evince-backend.in=.evince-backend)

EXTRA_DIST = $(backend_in_files)

CLEANFILES = $(backend_DATA)

@EV_INTLTOOL_EVINCE_BACKEND_RULE@

-include $(top_srcdir)/git.mk
