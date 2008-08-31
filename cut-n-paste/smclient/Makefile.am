noinst_LTLIBRARIES = libsmclient.la

libsmclient_la_SOURCES = 	\
	eggdesktopfile.c	\
	eggdesktopfile.h	\
	eggsmclient.c		\
	eggsmclient.h		\
	eggsmclient-private.h	\
	eggsmclient-xsmp.c

libsmclient_la_CFLAGS =				\
	-I$(top_srcdir)/lib			\
	-DG_LOG_DOMAIN=\""EggSMClient"\"	\	
	-DEGG_SM_CLIENT_BACKEND_XSMP		\
	$(SMCLIENT_CFLAGS)			\
	$(WARNING_CFLAGS)			\
	$(DISABLE_DEPRECATED)

libsmclient_la_LIBADD = 	\
	$(SMCLIENT_LIBS)	\
	-lSM -lICE
