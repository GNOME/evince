@INTLTOOL_DESKTOP_RULE@

NULL =

uidir = $(pkgdatadir)
ui_DATA =				\
	evince-ui.xml			\
	$(NULL)

gladedir = $(pkgdatadir)
glade_DATA = 				\
	evince-password.glade		\
	$(NULL)

stockdir = $(datadir)/pixmaps/evince
stock_DATA =				\
        ev-stock-zoom-fit-width.png	\
	$(NULL)

DESKTOP_IN_FILES= evince.desktop.in
DESKTOP_FILES= $(DESKTOP_IN_FILES:.desktop.in=.desktop)

desktopdir = $(datadir)/applications
desktop_DATA = $(DESKTOP_FILES)

EXTRA_DIST =				\
	$(stock_DATA)			\
	$(glade_DATA)			\
	$(ui_DATA)			\
	$(DESKTOP_IN_FILES)		\
	$(NULL)

DISTCLEANFILES = \
	$(DESKTOP_FILES)
