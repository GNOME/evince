NULL=

INCLUDES=					\
	-DDATADIR=\"$(pkgdatadir)\"		\
	-I$(top_srcdir)/lib			\
	-I$(top_srcdir)/backend			\
	-I$(top_srcdir)/pdf/xpdf		\
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

evince_LDADD=						\
	$(SHELL_LIBS)					\
	$(top_builddir)/backend/libevbackend.la		\
	$(top_builddir)/pdf/xpdf/libpdfdocument.la 	\
	$(NULL)
