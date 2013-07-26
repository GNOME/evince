noinst_LTLIBRARIES = libsynctex.la

libsynctex_la_SOURCES = 	\
	synctex_parser.c		\
	synctex_parser.h 	\
	synctex_parser_utils.h   \
	synctex_parser_utils.c

libsynctex_la_CPPFLAGS = \
	-DSYNCTEX_VERBOSE=0 \
	$(AM_CPPFLAGS)

libsynctex_la_CFLAGS =	\
	$(SHELL_CORE_CFLAGS)	\
	$(AM_CFLAGS)

-include $(top_srcdir)/git.mk
