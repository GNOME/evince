NULL=

INCLUDES=					\
	-DEVINCE_UIDIR=\"$(pkgdatadir)\"	\
	-DGNOMELOCALEDIR=\"$(datadir)/locale\"	\
	$(SHELL_CFLAGS)				\
	$(EVINCE_DISABLE_DEPRECATED)		\
	$(NULL)

noinst_LTLIBRARIES = libevbackend.la

libevbackend_la_SOURCES=			\
	ev-backend-marshal.c			\
	ev-document.c				\
	ev-document.h				\
	$(NULL)

ev-backend-marshalers.h: ev-backend-marshalers.list
	cd $(srcdir) \
	&& $(GLIB_GENMARSHAL) --prefix=_ev_backend_marshal ev-backend-marshalers.list --header > xgen-emh \
	&& cp xgen-emh ev-backend-marshalers.h \
	&& rm -f xgen-emh xgen-emh~

ev-backend-marshalers.c: ev-backend-marshalers.list
	cd $(srcdir) \
	&& $(GLIB_GENMARSHAL) --prefix=_ev_backend_marshal ev-backend-marshalers.list --body > xgen-emc \
	&& cp xgen-emc ev-backend-marshalers.c \
	&& rm -f xgen-emc xgen-emc~

ev-backend-marshal.c: ev-backend-marshalers.h ev-backend-marshalers.c

noinst_HEADERS = \
	ev-backend-marshalers.h

EXTRA_DIST=			\
	ev-backend-marshalers.list
