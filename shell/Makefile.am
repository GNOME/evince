NULL=

INCLUDES=					\
	-DDATADIR=\"$(pkgdatadir)\"		\
	-I$(top_srcdir)/lib			\
	-DGNOMELOCALEDIR=\"$(datadir)/locale\"	\
	$(SHELL_CFLAGS)				\
	$(EVINCE_DISABLE_DEPRECATED)		\
	$(NULL)

bin_PROGRAMS=evince

evince_SOURCES=				\
	ev-application.c		\
	ev-application.h		\
	ev-window.c			\
	ev-window.h			\
	main.c				\
	$(NULL)

evince_LDADD=					\
	$(SHELL_LIBS)				\
	$(top_builddir)/lib/libevprivate.a	\
	$(NULL)
