noinst_LTLIBRARIES = libgd.la

libgd_la_SOURCES = 	\
	gd-icon-utils.c                 \
	gd-icon-utils.h                 \
	gd-main-icon-view.c		\
	gd-main-icon-view.h		\
	gd-main-view-generic.c		\
	gd-main-view-generic.h		\
	gd-toggle-pixbuf-renderer.c	\
	gd-toggle-pixbuf-renderer.h	\
	gd-two-lines-renderer.c		\
	gd-two-lines-renderer.h

libgd_la_CFLAGS =	\
	$(SHELL_CORE_CFLAGS)	\
	$(WARNING_CFLAGS)	\
	$(DISABLE_DEPRECATED)

libgd_la_LIBADD = $(BACKEND_LIBS)

-include $(top_srcdir)/git.mk

AM_CPPFLAGS = $(BACKEND_CFLAGS)
