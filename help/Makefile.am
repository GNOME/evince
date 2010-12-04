SUBDIRS = reference

if ENABLE_HELP
include $(top_srcdir)/gnome-doc-utils.make

DOC_ID = evince
DOC_INCLUDES = legal.xml

dist-hook: doc-dist-hook

DOC_FIGURES = \
	add-text-annotation.png		\
	annotations-navigate.png	\
	annotations-nav-to-page.png	\
	evincelogo.png			\
	gconf-editor.png		\
	list-add-tabs.png		\
	print-2sided.png		\
	print-select.png		\
	reverse-collate.png		\
	zoom.png

DOC_PAGES = \
	annotation-properties.page	\
	annotations-delete.page		\
	annotations-navigate.page	\
	annotations.page		\
	bookmarks.page			\
	commandline.page		\
	convertpdf.page			\
	convertPostScript.page		\
	convertSVG.page			\
	editing.page			\
	finding.page			\
	formats.page			\
	forms.page			\
	index.page			\
	introduction.page		\
	invert-colors.page		\
	movingaround.page		\
	noprint.page			\
	openerror.page			\
	opening.page			\
	password.page			\
	presentations.page		\
	print-2sided.page		\
	print-differentsize.page	\
	printing.page			\
	print-order.page		\
	print-select.page		\
	reload.page			\
	shortcuts.page			\
	textselection.page		\
	tips.page			\
	zooming.page

DOC_LINGUAS = bg ca cs de el en_GB es eu fi fr gl it ja nl oc pt_BR ru sl sr sv uk vi zh_CN

endif

-include $(top_srcdir)/git.mk
