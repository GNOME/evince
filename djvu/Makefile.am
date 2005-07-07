INCLUDES = \
	-I$(top_srcdir)						\
	-I$(top_srcdir)/backend					\
	-I$(top_srcdir)/lib					\
	-DGNOMEICONDIR=\""${prefix}/${DATADIRNAME}/pixmaps"\" 	\
	$(LIB_CFLAGS)

noinst_LTLIBRARIES = libgtkdjvu.la

libgtkdjvu_la_SOURCES = \
	djvu-document.c	\
	djvu-document.h

libgtkdjvu_la_LIBADD = -lpthread -ldjvulibre


