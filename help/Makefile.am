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
	annotations-save.page		\
	annotations-view.page		\
	bookmarks.page			\
	commandline.page		\
	convertpdf.page			\
	convertPostScript.page		\
	convertSVG.page			\
	duplex-3pages.page		\
	duplex-4pages.page		\
	duplex-5pages.page		\
	duplex-6pages.page		\
	duplex-7pages.page		\
	duplex-8pages.page		\
	duplex-9pages.page		\
	duplex-10pages.page		\
	duplex-11pages.page		\
	duplex-12pages.page		\
	duplex-13pages.page		\
	duplex-14pages.page		\
	duplex-15pages.page		\
	duplex-16pages.page		\
	duplex-npages.page		\
	editing.page			\
	finding.page			\
	formats.page			\
	forms.page			\
	forms-saving.page		\
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
	print-booklet.page		\
	printing.page			\
	print-order.page		\
	print-select.page		\
	reload.page			\
	shortcuts.page			\
	singlesided-3-4pages.page	\
	singlesided-5-8pages.page	\
	singlesided-9-12pages.page	\
	singlesided-13-16pages.page	\
	singlesided-17-20pages.page	\
	singlesided-Npages.page		\
	synctex.page			\
	synctex-beamer.page		\
	synctex-compile.page		\
	synctex-editors.page		\
	synctex-search.page		\
	synctex-support.page		\
	textselection.page		\
	zooming.page

DOC_LINGUAS = bg ca cs de el en_GB es eu fi fr gl it ja nl oc pt_BR ru sl sr sv uk vi zh_CN

-include $(top_srcdir)/git.mk

endif
