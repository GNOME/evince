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
	eggfindbar.c			\
	eggfindbar.h			\
	ev-application.c		\
	ev-application.h		\
	ev-window.c			\
	ev-window.h			\
	ev-sidebar.c			\
	ev-sidebar.h			\
	main.c				\
	$(NULL)

evince_LDADD=					\
	$(SHELL_LIBS)				\
	$(NULL)
