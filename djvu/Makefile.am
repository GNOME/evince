INCLUDES = \
	-I$(top_srcdir)						\
	-I$(top_srcdir)/backend					\
	-I$(top_srcdir)/lib					\
	-DGNOMEICONDIR=\""${prefix}/${DATADIRNAME}/pixmaps"\" 	\
	$(LIB_CFLAGS)						\
	$(DJVU_CFLAGS)

noinst_LTLIBRARIES = libgtkdjvu.la

libgtkdjvu_la_SOURCES = \
	djvu-document.c	\
	djvu-document.h \
	djvu-document-private.h \
	djvu-text.c	\
	djvu-text.h	\
	djvu-text-page.c \
	djvu-text-page.h



