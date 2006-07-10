INCLUDES =							\
	-I$(top_srcdir)						\
	-I$(top_srcdir)/backend					\
	$(SHELL_CFLAGS)						\
	-DDATADIR=\""$(datadir)"\"

noinst_LTLIBRARIES = libimpressdocument.la

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

