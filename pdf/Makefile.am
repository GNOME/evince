INCLUDES =					\
	-I$(top_srcdir)				\
	-I$(top_srcdir)/backend			\
	$(PDF_CFLAGS)				\
	$(WARN_CXXFLAGS)			\
	$(DISABLE_DEPRECATED)			\
	-DDATADIR=\""$(datadir)"\"

noinst_LTLIBRARIES = libpdfdocument.la

libpdfdocument_la_SOURCES =			\
	ev-poppler.cc				\
	ev-poppler.h

libpdfdocument_la_LIBADD = $(PDF_LIBS)
