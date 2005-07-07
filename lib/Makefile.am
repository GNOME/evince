INCLUDES =					\
	$(LIB_CFLAGS)				\
	$(WARN_CFLAGS)				\
	$(DISABLE_DEPRECATED)

noinst_LTLIBRARIES = libev.la

libev_la_SOURCES =				\
	ev-debug.c				\
	ev-debug.h				\
	ev-file-helpers.c			\
	ev-file-helpers.h			\
	ev-gui.c				\
	ev-gui.h

libev_la_LIBADD = 					\
	$(top_builddir)/backend/libevbackend.la
