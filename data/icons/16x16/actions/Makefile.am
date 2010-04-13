themedir = $(pkgdatadir)/icons/hicolor
size = 16x16
context = actions

iconsdir = $(themedir)/$(size)/$(context)

icons_DATA = \
	view-page-continuous.png	\
	view-page-facing.png		\
	zoom.png                \
	zoom-fit-height.png		\
	zoom-fit-width.png		\
	object-rotate-left.png		\
	object-rotate-right.png

noinst_DATA = \
	view-page-continuous.xcf	\
	view-page-facing.xcf		\
	zoom.svg                \
	zoom-fit-page.svg		\
	zoom-fit-width.svg

EXTRA_DIST = \
	$(icons_DATA)	\
	$(noinst_DATA)

-include $(top_srcdir)/git.mk
