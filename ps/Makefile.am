INCLUDES = \
	-I$(top_srcdir)						\
	-I$(top_srcdir)/backend					\
	-DGNOMEICONDIR=\""${prefix}/${DATADIRNAME}/pixmaps"\" 	\
	$(PS_CFLAGS)

noinst_LTLIBRARIES = libgtkgs.la

libgtkgs_la_SOURCES = 	\
	gsio.c		\
	gsio.h		\
	gsdefaults.c	\
	gsdefaults.h	\
	ps.c		\
	ps.h		\
	gtkgs.c		\
	gtkgs.h
