noinst_LTLIBRARIES = libsmclient.la

if PLATFORM_WIN32
platform_sources = eggsmclient-win32.c
else
platform_libs = -lSM -lICE
platform_sources = 		\
	eggsmclient-xsmp.c	\
	eggdesktopfile.c	\
	eggdesktopfile.h
platform_defines = -DEGG_SM_CLIENT_BACKEND_XSMP
endif

libsmclient_la_SOURCES = 	\
	eggsmclient.c		\
	eggsmclient.h		\
	eggsmclient-private.h	\
	$(platform_sources)

libsmclient_la_CFLAGS =				\
	-DG_LOG_DOMAIN=\""EggSMClient"\"	\
	$(platform_defines)			\
	$(SMCLIENT_CFLAGS)			\
	$(WARNING_CFLAGS)			\
	$(DISABLE_DEPRECATED)

libsmclient_la_LIBADD = 	\
	$(SMCLIENT_LIBS)	\
	$(platform_libs)

EXTRA_DIST =			\
	eggsmclient-win32.c	\
	eggsmclient-xsmp.c	\
	eggdesktopfile.c	\
	eggdesktopfile.h

