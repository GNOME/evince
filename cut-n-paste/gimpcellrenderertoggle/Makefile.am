noinst_LTLIBRARIES = libgimpcellrenderertoggle.la

libgimpcellrenderertoggle_la_sources =	\
	gimpcellrenderertoggle.h	\
	gimpcellrenderertoggle.c

libgimpcellrenderertoggle_la_CFLAGS =	\
	$(SHELL_CORE_CFLAGS)		\
	$(AM_CFLAGS)

libgimpcellrenderertoggle_la_built_sources =	\
	gimpwidgetsmarshal.h			\
	gimpwidgetsmarshal.c

libgimpcellrenderertoggle_la_SOURCES = 			\
	$(libgimpcellrenderertoggle_la_built_sources)	\
	$(libgimpcellrenderertoggle_la_sources)

libgimpcellrenderertoggle_la_extra_sources = gimpwidgetsmarshal.list

gimpwidgetsmarshal.h: $(srcdir)/gimpwidgetsmarshal.list
	$(AM_V_GEN)$(GLIB_GENMARSHAL) --prefix=_gimp_widgets_marshal $(srcdir)/gimpwidgetsmarshal.list --header >> xgen-wmh \
        && (cmp -s xgen-wmh $(@F) || cp xgen-wmh $(@F)) \
        && rm -f xgen-wmh xgen-wmh~

gimpwidgetsmarshal.c: gimpwidgetsmarshal.h
	$(AM_V_GEN)echo "#include \"gimpwidgetsmarshal.h\"" >> xgen-wmc \
        && $(GLIB_GENMARSHAL) --prefix=_gimp_widgets_marshal $(srcdir)/gimpwidgetsmarshal.list --body >> xgen-wmc \
        && cp xgen-wmc $(@F) \
        && rm -f xgen-wmc xgen-wmc~

gen_sources = xgen-wmh xgen-wmc $(libgimpcellrenderertoggle_la_built_sources)
CLEANFILES = $(gen_sources)

EXTRA_DIST = $(libgimpcellrenderertoggle_la_extra_sources)

-include $(top_srcdir)/git.mk
