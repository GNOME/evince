## Process this file with automake to produce Makefile.in

# We require automake 1.6 at least.
AUTOMAKE_OPTIONS = 1.6

# This is a blank Makefile.am for using gtk-doc.
# Copy this to your project's API docs directory and modify the variables to
# suit your project. See the GTK+ Makefiles in gtk+/docs/reference for examples
# of using the various options.

# The name of the module, e.g. 'glib'.
DOC_MODULE=evince

# The top-level SGML file. You can change this if you want to.
DOC_MAIN_SGML_FILE=$(DOC_MODULE)-docs.sgml

# The directory containing the source code. Relative to $(srcdir).
# gtk-doc will search all .c & .h files beneath here for inline comments
# documenting the functions and macros.
# e.g. DOC_SOURCE_DIR=../../../gtk
DOC_SOURCE_DIR=../../../shell --source-dir=../../../libmisc

# Extra options to pass to gtkdoc-scangobj. Not normally needed.
SCANGOBJ_OPTIONS=

# Extra options to supply to gtkdoc-scan.
# e.g. SCAN_OPTIONS=--deprecated-guards="GTK_DISABLE_DEPRECATED" 
SCAN_OPTIONS=

# Extra options to supply to gtkdoc-mkdb.
# e.g. MKDB_OPTIONS=--sgml-mode --output-format=xml
MKDB_OPTIONS=--sgml-mode --output-format=xml

# Extra options to supply to gtkdoc-mktmpl
# e.g. MKTMPL_OPTIONS=--only-section-tmpl
MKTMPL_OPTIONS=--source-dir=../../../libmisc

# Add the builddir to the HTML path so that version.xml is found
MKHTML_OPTIONS=--path="$(abs_builddir)"

# Extra options to supply to gtkdoc-fixref. Not normally needed.
# e.g. FIXXREF_OPTIONS=--extra-dir=../gdk-pixbuf/html --extra-dir=../gdk/html
FIXXREF_OPTIONS=

# Used for dependencies. The docs will be rebuilt if any of these change.
# e.g. HFILE_GLOB=$(top_srcdir)/gtk/*.h
# e.g. CFILE_GLOB=$(top_srcdir)/gtk/*.c
HFILE_GLOB=$(top_srcdir)/shell/*.h
CFILE_GLOB=$(top_srcdir)/shell/*.c

# Header files to ignore when scanning.
# e.g. IGNORE_HFILES=gtkdebug.h gtkintl.h
IGNORE_HFILES=		\
	ev-marshal.h 	\
	ev-marshal.c 

# Images to copy into HTML directory.
# e.g. HTML_IMAGES=$(top_srcdir)/gtk/stock-icons/stock_about_24.png
HTML_IMAGES=

# Extra SGML files that are included by $(DOC_MAIN_SGML_FILE).
# e.g. content_files=running.sgml building.sgml changes-2.0.sgml
content_files=

# SGML files where gtk-doc abbrevations (#GtkWidget) are expanded
# These files must be listed here *and* in content_files
# e.g. expand_content_files=running.sgml
expand_content_files=

# CFLAGS and LDFLAGS for compiling gtkdoc-scangobj with your library.
# Only needed if you are using gtkdoc-scangobj to dynamically query widget
# signals and properties.
# e.g. INCLUDES=-I$(top_srcdir) -I$(top_builddir) $(GTK_DEBUG_FLAGS)
# e.g. GTKDOC_LIBS=$(top_builddir)/gtk/$(gtktargetlib)
INCLUDES= \
	-I$(top_srcdir)					\
	-I$(top_builddir)				\
	-I$(top_srcdir)/shell \
	-DDATADIR=\"$(pkgdatadir)\"			\
	-DGNOMEDATADIR=\"$(datadir)\"			\
	-I$(top_srcdir)/cut-n-paste/zoom-control/	\
	-I$(top_srcdir)/cut-n-paste/toolbar-editor/	\
	-I$(top_srcdir)/cut-n-paste/totem-screensaver/	\
	-I$(top_srcdir)/libdocument			\
	-I$(top_builddir)/libdocument			\
	-I$(top_srcdir)/libview				\
	-I$(top_srcdir)/libmisc				\
	-I$(top_builddir)/libview			\
	-I$(top_srcdir)/properties			\
	-DGNOMELOCALEDIR=\"$(datadir)/locale\"		\
	-DGNOMEICONDIR=\""$(datadir)/pixmaps"\" 	\
	-DEVINCE_COMPILATION				\
	$(SHELL_CFLAGS)					\
	$(WARN_CFLAGS)					\
	$(DISABLE_DEPRECATED)				\
	$(GTK_PRINT_CFLAGS)

GTKDOC_LIBS= \
	$(top_builddir)/cut-n-paste/zoom-control/libephyzoom.la		\
	$(top_builddir)/cut-n-paste/toolbar-editor/libtoolbareditor.la	\
	$(top_builddir)/cut-n-paste/totem-screensaver/libtotemscrsaver.la	\
	$(top_builddir)/cut-n-paste/gedit-message-area/libgeditmsgarea.la	\
	$(top_builddir)/cut-n-paste/gimpcellrenderertoggle/libgimpcellrenderertoggle.la	\
	$(top_builddir)/cut-n-paste/smclient/libsmclient.la		\
	$(top_builddir)/properties/libevproperties.la			\
	$(top_builddir)/libdocument/libevdocument.la			\
	$(top_builddir)/libview/libevview.la				\
	$(top_builddir)/libmisc/libevmisc.la				\
	$(SHELL_LIBS)							\
	$(GTK_PRINT_LIBS)						\
	$(filter-out ../../../shell/main.o,$(wildcard ../../../shell/*.o))

if ENABLE_DJVU
GTKDOC_LIBS +=	$(DJVU_LIBS)
endif

if ENABLE_DVI
GTKDOC_LIBS +=	-lkpathsea
endif

if WITH_TYPE1_FONTS
GTKDOC_LIBS += -lt1
endif

# This includes the standard gtk-doc make rules, copied by gtkdocize.
include $(top_srcdir)/gtk-doc.make

# Other files to distribute
# e.g. EXTRA_DIST += version.xml.in
EXTRA_DIST += version.xml.in
