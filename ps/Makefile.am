INCLUDES = \
	-I$(top_srcdir)						\
	-I$(top_srcdir)/backend					\
	-I$(top_srcdir)/lib					\
	-DGNOMEICONDIR=\""${prefix}/${DATADIRNAME}/pixmaps"\" 	\
	$(PS_CFLAGS)

noinst_LTLIBRARIES = libgtkgs.la

libgtkgs_la_SOURCES = 	\
	gsio.c		\
	gsio.h		\
	gstypes.h	\
	ps.c		\
	ps.h		\
	ps-document.c	\
	ps-document.h	\
	gsdefaults.c	\
	gsdefaults.h
