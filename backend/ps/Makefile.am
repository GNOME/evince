INCLUDES = \
	-I$(top_srcdir)						\
	-I$(top_srcdir)/libdocument				\
	$(BACKEND_CFLAGS)					\
	$(WARN_CFLAGS)						\
	$(DISABLE_DEPRECATED)

if HAVE_SPECTRE
INCLUDES += $(SPECTRE_CFLAGS)
endif

noinst_LTLIBRARIES = libpsdocument.la

if HAVE_SPECTRE
libpsdocument_la_SOURCES = 	\
	ev-spectre.c		\
	ev-spectre.h
else
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
endif
