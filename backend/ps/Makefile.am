INCLUDES = \
	-I$(top_srcdir)						\
	-I$(top_srcdir)/libdocument				\
	$(BACKEND_CFLAGS)					\
	$(WARN_CFLAGS)						\
	$(DISABLE_DEPRECATED)

noinst_LTLIBRARIES = libpsdocument.la

libpsdocument_la_SOURCES = 	\
	gsio.c			\
	gsio.h			\
	gstypes.h		\
	ps.c			\
	ps.h			\
	ps-document.c		\
	ps-document.h		\
	ps-interpreter.c	\
	ps-interpreter.h	\
	gsdefaults.c		\
	gsdefaults.h

