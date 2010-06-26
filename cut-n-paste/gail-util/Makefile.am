noinst_LTLIBRARIES = libgail.la
libgail_la_SOURCES = 	\
	gailtextutil.h 	\
	gailtextutil.c 	\
	gailmisc.h 	\
	gailmisc.c

libgail_la_CPPFLAGS = \
	$(AM_CPPFLAGS)

libgail_la_CFLAGS = 		\
	$(SHELL_CORE_CFLAGS)	\
	$(WARNING_CFLAGS)	\
	$(DISABLE_DEPRECATED)	\
	$(AM_CFLAGS)

-include $(top_srcdir)/git.mk
