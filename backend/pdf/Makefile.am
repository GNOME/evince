INCLUDES =					\
	-I$(top_srcdir)				\
	-I$(top_srcdir)/libdocument		\
	$(BACKEND_CFLAGS)			\
	$(POPPLER_CFLAGS)                       \
	$(WARN_CXXFLAGS)			\
	$(DISABLE_DEPRECATED)

noinst_LTLIBRARIES = libpdfdocument.la

libpdfdocument_la_SOURCES =			\
	ev-poppler.cc				\
	ev-poppler.h
