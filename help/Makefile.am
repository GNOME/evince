SUBDIRS = reference

if ENABLE_HELP

include $(top_srcdir)/gnome-doc-utils.make
dist-hook: doc-dist-hook

DOC_MODULE = evince
DOC_ENTITIES = legal.xml
DOC_INCLUDES = 
DOC_FIGURES = figures/evince_start_window.png

DOC_LINGUAS = bg ca cs de el en_GB es eu fi fr it ja nl oc pt_BR ru sl sr sv uk vi zh_CN zh_HK zh_TW

-include $(top_srcdir)/git.mk

endif
