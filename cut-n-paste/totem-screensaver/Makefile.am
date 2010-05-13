noinst_LTLIBRARIES = libtotemscrsaver.la
libtotemscrsaver_la_SOURCES = \
	totem-scrsaver.h \
	totem-scrsaver.c

libtotemscrsaver_la_CPPFLAGS = \
	$(AM_CPPFLAGS)

libtotemscrsaver_la_CFLAGS = 	\
	$(SHELL_CORE_CFLAGS)	\
	$(DBUS_CFLAGS)		\
	$(WARNING_CFLAGS)	\
	$(DISABLE_DEPRECATED)	\
	$(AM_CFLAGS)

-include $(top_srcdir)/git.mk
