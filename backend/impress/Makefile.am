INCLUDES =					\
	-I$(top_srcdir)				\
	-I$(top_srcdir)/libdocument		\
	-DGNOMELOCALEDIR=\"$(datadir)/locale\"	\
	-DDATADIR=\""$(datadir)"\"		\
	-DEVINCE_COMPILATION			\
	$(BACKEND_CFLAGS)			\
	$(WARN_CFLAGS)				\
	$(DISABLE_DEPRECATED)

backend_LTLIBRARIES = libimpressdocument.la

libimpressdocument_la_SOURCES =	\
	$(IMPOSTER_SOURCE_FILES)	\
	$(IMPOSTER_INCLUDE_FILES)	\
	impress-document.c		\
	impress-document.h

IMPOSTER_SOURCE_FILES =			\
	document.c			\
	f_oasis.c			\
	f_oo13.c			\
	iksemel.c			\
	r_back.c			\
	r_draw.c			\
	render.c			\
	r_geometry.c			\
	r_gradient.c			\
	r_style.c			\
	r_text.c			\
	zip.c
IMPOSTER_INCLUDE_FILES =		\
	common.h			\
	iksemel.h			\
	imposter.h			\
	internal.h			\
	zip.h
IMPOSTER_RENDER_SOURCE_FILES =		\
	render.c
IMPOSTER_RENDER_INCLUDE_FILES =		\
	render.h

libimpressdocument_la_LDFLAGS = $(BACKEND_LIBTOOL_FLAGS)
libimpressdocument_la_LIBADD =				\
	$(top_builddir)/libdocument/libevdocument.la     \
	$(BACKEND_LIBS) -lz

backend_in_files = impressdocument.evince-backend.in
backend_DATA = $(backend_in_files:.evince-backend.in=.evince-backend)

EXTRA_DIST = $(backend_in_files)

CLEANFILES = $(backend_DATA)

@EV_INTLTOOL_EVINCE_BACKEND_RULE@
