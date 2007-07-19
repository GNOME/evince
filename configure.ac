dnl Process this file with autoconf to produce a configure script.

AC_PREREQ(2.57)
AC_INIT([Evince],[0.9.2],[http://bugzilla.gnome.org/enter_bug.cgi?product=evince],[evince])
AM_INIT_AUTOMAKE([1.9 dist-bzip2 no-dist-gzip])

AM_CONFIG_HEADER(config.h)

AM_MAINTAINER_MODE

AM_PROG_LIBTOOL

AC_ISC_POSIX
AC_PROG_CC
AM_PROG_CC_STDC
AC_PROG_CXX
AC_STDC_HEADERS
IT_PROG_INTLTOOL([0.35.0])
AC_PATH_PROG([GLIB_GENMARSHAL], [glib-genmarshal])
AC_PATH_PROG([GLIB_MKENUMS],[glib-mkenums])

GNOME_COMMON_INIT
GNOME_DOC_INIT
GNOME_MAINTAINER_MODE_DEFINES
GNOME_COMPILE_WARNINGS
GNOME_CXX_WARNINGS
GNOME_DEBUG_CHECK

GTK_DOC_CHECK(1.0)

GETTEXT_PACKAGE=evince
AC_SUBST(GETTEXT_PACKAGE)
AC_DEFINE_UNQUOTED([GETTEXT_PACKAGE],["$GETTEXT_PACKAGE"],[Gettext package])
AM_GLIB_GNU_GETTEXT

dnl Check dependencies

# LIB_CFLAGS       for helpers and generic widgets. (lib, cut-and-paste)
# BACKEND_CFLAGS   for backend implementations.
# FRONTEND_CFLAGS  for frontend implementations. (properties, thumbnailer)
# FRONTEND_LIBS
# SHELL_CFLAGS     for shell implementation.
# SHELL_LIBS

DBUS_GLIB_REQUIRED=0.70
GTK_REQUIRED=2.8.15
KEYRING_REQUIRED=0.4.0

LIBGNOMEUI_REQUIRED=2.14.0
LIBGNOME_REQUIRED=2.14.0
LIBGNOMEPRINTUI_REQUIRED=2.6.0
GNOME_ICON_THEME_REQUIRED=2.17.1
LIBXML_REQUIRED=2.5.0

GTK_PRINT_REQUIRED=2.10.0

PKG_CHECK_MODULES(LIB, gtk+-2.0 >= $GTK_REQUIRED libxml-2.0 >= $LIBXML_REQUIRED)
PKG_CHECK_MODULES(BACKEND, gtk+-2.0 >= $GTK_REQUIRED gnome-vfs-2.0)
PKG_CHECK_MODULES(FRONTEND_CORE, gtk+-2.0 >= $GTK_REQUIRED libglade-2.0 gnome-vfs-2.0)
PKG_CHECK_MODULES(SHELL_CORE, libxml-2.0 >= $LIBXML_REQUIRED gtk+-2.0 >= $GTK_REQUIRED gnome-icon-theme >= $GNOME_ICON_THEME_REQUIRED gnome-vfs-2.0 libglade-2.0 gconf-2.0 gnome-keyring-1 >= $KEYRING_REQUIRED)

AC_ARG_WITH(libgnome,
        AC_HELP_STRING([--without-libgnome],[disable the use of libgnome]),
	[case "${withval}" in
	yes) WITH_GNOME=yes ;;
	no) WITH_GNOME=no ;;
	*) AC_MSG_ERROR(bad value ${withval} for --with-libgnome) ;;
	esac],
	[WITH_GNOME=yes]) dnl Default value
AM_CONDITIONAL(WITH_GNOME, test x$WITH_GNOME = "xyes")

if test x$WITH_GNOME = "xyes"; then
        PKG_CHECK_MODULES(GNOME, libgnome-2.0 >= $LIBGNOME_REQUIRED)
        PKG_CHECK_MODULES(GNOMEUI, libgnomeui-2.0 >= $LIBGNOMEUI_REQUIRED)
        AC_DEFINE([WITH_GNOME],[1],[Define if GNOME support is enabled])
fi

GLIB_GENMARSHAL=`$PKG_CONFIG --variable=glib_genmarshal glib-2.0`  
AC_SUBST(GLIB_GENMARSHAL)

dnl ========= Print support
AC_ARG_WITH(print,
	    [AC_HELP_STRING([--with-print=no/gtk/gnome/auto], [Compile with print support [default=auto]])],
	    PRINT=$withval,
            PRINT="auto")

PKG_CHECK_MODULES(GNOME_PRINT, 
		  libgnomeprintui-2.2 >= $LIBGNOMEPRINTUI_REQUIRED,
		  enable_gnome_print=yes, enable_gnome_print=no)
PKG_CHECK_MODULES(GTK_PRINT,
		  gtk+-unix-print-2.0 >= $GTK_PRINT_REQUIRED,
		  enable_gtk_print=yes, enable_gtk_print=no)
enable_print=no
if test x$PRINT = xgnome -a x$enable_gnome_print = xyes; then
   enable_print=gnome
else
   if test x$PRINT = xgtk -a x$enable_gtk_print = xyes; then
      enable_print=gtk
   else
      if test x$PRINT = xauto; then
	 if test x$enable_gtk_print = xyes; then
	    enable_print=gtk
	 else
	    if test x$enable_gnome_print = xyes; then
	       enable_print=gnome
	    fi
	 fi
      fi
   fi
fi

if test x$enable_print = xgnome; then
   AC_SUBST(GNOME_PRINT_CFLAGS)
   AC_SUBST(GNOME_PRINT_LIBS)
   AC_DEFINE([WITH_GNOME_PRINT],[1],[Enable GNOME Print Support.])
   AC_DEFINE([WITH_PRINT],[1],[Enable Print Support.])
fi

if test x$enable_print = xgtk; then   
   AC_SUBST(GTK_PRINT_CFLAGS)
   AC_SUBST(GTK_PRINT_LIBS)
   AC_DEFINE([WITH_GTK_PRINT],[1],[Enable GTK Print Support.])
   AC_DEFINE([WITH_PRINT],[1],[Enable Print Support.])
fi
   
AM_CONDITIONAL(WITH_GNOME_PRINT, test x$enable_print = xgnome)
AM_CONDITIONAL(WITH_GTK_PRINT, test x$enable_print = xgtk)


dnl ========= Check for DBUS
PKG_CHECK_MODULES([DBUS], [dbus-glib-1 >= $DBUS_GLIB_REQUIRED],
	[enable_dbus=yes],[enable_dbus=no])
AC_SUBST([DBUS_CFLAGS])
AC_SUBST([DBUS_LIBS])

if test "x$enable_dbus" = "xno"; then
	AC_MSG_WARN([DBUS support is disabled since dbus $DBUS_GLIB_REQUIRED or higher was not found])
fi

if test "x$enable_dbus" = "xyes" ; then
        AC_PATH_PROG([DBUS_BINDING_TOOL], [dbus-binding-tool], [no])

        if test x$DBUS_BINDING_TOOL = "xno" ; then
        	AC_MSG_ERROR([dbus-binding-tool executable not found in your path - should be installed with dbus glib bindings])
        fi                                            

        AC_DEFINE([ENABLE_DBUS],[1],[Define if DBUS support is enabled])
fi

LIB_CFLAGS="$LIB_CFLAGS $GNOME_CFLAGS"
LIB_LIBS="$LIB_LIBS $GNOME_LIBS"
AC_SUBST(LIB_CFLAGS)
AC_SUBST(LIB_LIBS)

BACKEND_CFLAGS="$BACKEND_CFLAGS $GNOME_CFLAGS -DGDK_MULTIHEAD_SAFE -DGTK_MULTIHEAD_SAFE"
BACKEND_LIBS="$BACKEND_LIBS $GNOME_LIBS"
AC_SUBST(BACKEND_CFLAGS)
AC_SUBST(BACKEND_LIBS)

SHELL_CFLAGS="$SHELL_CORE_CFLAGS $DBUS_CFLAGS $GNOMEUI_CFLAGS -DGDK_MULTIHEAD_SAFE -DGTK_MULTIHEAD_SAFE"
SHELL_LIBS="$SHELL_CORE_LIBS $DBUS_LIBS $GNOMEUI_LIBS -lz"
AC_SUBST(SHELL_CFLAGS)
AC_SUBST(SHELL_LIBS)

FRONTEND_CFLAGS="$FRONTEND_CORE_CFLAGS $GNOME_CFLAGS"
FRONTEND_LIBS="$FRONTEND_CORE_LIBS $GNOME_LIBS -lz"
AC_SUBST(FRONTEND_CFLAGS)
AC_SUBST(FRONTEND_LIBS)

AM_CONDITIONAL([ENABLE_DBUS], [test "x$enable_dbus" = "xyes"])
AM_CONDITIONAL([DBUS_TOOL_NO_PREFIX], [test "x$DBUS_VERSION" = "x33"])

dnl Check for Nautilus property page build
AC_ARG_ENABLE(nautilus,
	AC_HELP_STRING([--enable-nautilus],[compile the nautilus plugin]),
	[case "${enableval}" in
	yes) ENABLE_NAUTILUS=yes ;;
	no) ENABLE_NAUTILUS=no ;;
	*) AC_MSG_ERROR(bad value ${enableval} for --enable-nautilus) ;;
	esac],
	[ENABLE_NAUTILUS=yes]) dnl Default value

if test x$ENABLE_NAUTILUS = "xyes" ; then
	PKG_CHECK_MODULES(NAUTILUS, gtk+-x11-2.0 $MM gthread-2.0 libnautilus-extension,
			[HAVE_NAUTILUS=yes], [HAVE_NAUTILUS=no])
fi

AC_SUBST(NAUTILUS_CFLAGS)
AC_SUBST(NAUTILUS_LIBS)
if test x$HAVE_NAUTILUS = "xyes"; then
	AC_DEFINE(HAVE_NAUTILUS, 1, [defined if you build the nautilus plugin])
fi
AM_CONDITIONAL(HAVE_NAUTILUS, test x$HAVE_NAUTILUS = "xyes")


dnl Check for thumbnailer build
AC_ARG_ENABLE(thumbnailer,
        AC_HELP_STRING([--disable-thumbnailer],[disable the GNOME thumbnailer]),
	[case "${enableval}" in
	yes) ENABLE_THUMBNAILER=yes ;;
	no) ENABLE_THUMBNAILER=no ;;
	*) AC_MSG_ERROR(bad value ${enableval} for --enable-thumbnailer) ;;
	esac],
	[ENABLE_THUMBNAILER=yes]) dnl Default value
AM_CONDITIONAL(ENABLE_THUMBNAILER, test x$ENABLE_THUMBNAILER = "xyes")


dnl Check for GtkRecent
PKG_CHECK_MODULES(GTK_RECENT, gtk+-2.0 >= 2.10.0, 
		  [HAVE_GTK_RECENT=yes], [HAVE_GTK_RECENT=no])
if test x$HAVE_GTK_RECENT = xyes; then
   AC_DEFINE([HAVE_GTK_RECENT], [1], [defined if GtkRecent is available])
fi
AM_CONDITIONAL(HAVE_GTK_RECENT, test x$HAVE_GTK_RECENT = "xyes")

dnl GConf configuration
AM_GCONF_SOURCE_2

AC_PATH_PROG([GCONFTOOL], [gconftool-2], [no])
if test "x$GCONFTOOL" = "xno"; then
	AC_MSG_ERROR([gconftool-2 executable not found in your path - should be installed with GConf])
fi

dnl ================== portability checks ===========================================

dnl for backtrace()
AC_CHECK_HEADERS([execinfo.h])

AC_CHECK_DECL([_NL_MEASUREMENT_MEASUREMENT],[
  AC_DEFINE([HAVE__NL_MEASUREMENT_MEASUREMENT],[1],[Define if _NL_MEASUREMENT_MEASUREMENT is available])
  ],[],[#include <langinfo.h>])

dnl ================== pdf checks ===================================================
AC_ARG_ENABLE(pdf,
              [AC_HELP_STRING([--enable-pdf], [Compile with pdf support])],enable_pdf="$enableval",enable_pdf=yes)

if test "x$enable_pdf" = "xyes"; then
    POPPLER_REQUIRED=0.5.9
    PKG_CHECK_MODULES(POPPLER, poppler-glib >= $POPPLER_REQUIRED,enable_pdf=yes,enable_pdf=no)

    if test "x$enable_pdf" = "xyes"; then
	    AC_DEFINE([ENABLE_PDF], [1], [Enable pdf support.])
	    FRONTEND_LIBS="$FRONTEND_LIBS $POPPLER_LIBS"
	    SHELL_LIBS="$SHELL_LIBS $POPPLER_LIBS"
	    SHELL_CFLAGS="$SHELL_CFLAGS $POPPLER_CFLAGS"

	    evince_save_LIBS=$LIBS
	    LIBS="$LIBS $FRONTEND_LIBS"
	    AC_CHECK_FUNCS(poppler_page_render)
	    dnl we need latest poppler cvs head, 
	    dnl this function was the last to be added
	    AC_CHECK_FUNCS(poppler_form_field_button_get_button_type)
	    AC_CHECK_FUNCS(poppler_form_field_text_get_max_len)
	    LIBS=$evince_save_LIBS

	    PKG_CHECK_MODULES(CAIRO_PDF, cairo-pdf, enable_cairo_pdf=yes, enable_cairo_pdf=no)
	    if test x$enable_cairo_pdf == xyes; then
	            AC_DEFINE([HAVE_CAIRO_PDF], [1], [defined if cairo-pdf is available])
            fi
    else
	    AC_MSG_WARN("PDF support is disabled since poppler-glib library version $POPPLER_REQUIRED or newer not found")
    fi 
fi

AM_CONDITIONAL(ENABLE_PDF, test x$enable_pdf = xyes)
dnl ================== end of pdf checks ============================================

dnl ================== ggv checks ===================================================
AC_ARG_ENABLE(ps,
  	[AC_HELP_STRING([--disable-ps], [Compile without PostScript backend])],enable_ps=$enableval,enable_ps="yes")

if test x$enable_ps = xyes; then
 	AC_DEFINE([ENABLE_PS], [1], [Enable support for PostScript files.])
fi 

AM_CONDITIONAL(ENABLE_PS, test x$enable_ps = xyes)

if test x$enable_ps = xyes; then

dnl check for GS version
AC_MSG_CHECKING(for Ghostscript version...)
GS_VERSION=`gs --version | head -n 1 | sed 's/\([[0-9]]*\)\.\([[0-9]]*\).*/\1/'`
AC_MSG_RESULT(found $GS_VERSION)
if test "$GS_VERSION" -lt "7"; then
	AC_MSG_ERROR([You need Ghostscript version >= 7 in order to run evince])
fi

AC_ARG_WITH(gs-aa-params,
            [AC_HELP_STRING([--with-gs-aa-params], [Define antialiasing params for ghostscript])],AA_PARAMS=$withval,
	    AA_PARMS="-sDEVICE=x11alpha -dNOPLATFONTS -dGraphicsAlphaBits=4 -dTextAlphaBits=4 -dDOINTERPOLATE"
	    )
AC_DEFINE_UNQUOTED(ALPHA_PARAMS, "$AA_PARMS", [Anti-aliasing parameters for Ghostscript.])
AC_MSG_RESULT(Antialiasing parameters for Ghostscript: $AA_PARMS)

fi
dnl ======================== End of ggv checks =================================

dnl ================== tiff checks ===================================================
AC_ARG_ENABLE(tiff,
            [AC_HELP_STRING([--enable-tiff], [Compile with support of multipage tiff])],enable_tiff="$enableval",enable_tiff=yes)

if test "x$enable_tiff" = "xyes"; then
    AC_CHECK_HEADERS([tiff.h],enable_tiff=yes,enable_tiff=no,)
    if test "x$enable_tiff" = "xyes"; then
	AC_CHECK_LIB([tiff],TIFFOpen,enable_tiff=yes,enable_tiff=no,"-lz")
        AC_CHECK_LIB([tiff],TIFFReadRGBAImageOriented,enable_tiff=yes,enable_tiff=no,"-lz")
    fi
    if test "x$enable_tiff" = "xyes"; then
	    AC_DEFINE([ENABLE_TIFF], [1], [Enable multipage tiff support.])
	    FRONTEND_LIBS="$FRONTEND_LIBS -ltiff"
	    SHELL_LIBS="$SHELL_LIBS -ltiff"
    else
	    AC_MSG_WARN("Tiff support is disabled since tiff library version 3.6 or newer not found")
    fi 
fi

AM_CONDITIONAL(ENABLE_TIFF, test x$enable_tiff = xyes)
dnl ================== end of tiff checks ============================================

dnl ================== djvu checks ===================================================

AC_ARG_ENABLE(djvu,
            [AC_HELP_STRING([--enable-djvu], [Compile with support of djvu viewer])],enable_djvu="$enableval",enable_djvu=no)

if test "x$enable_djvu" = "xyes"; then

DJVULIBRE_REQUIRED=3.5.17

PKG_CHECK_MODULES(DJVU, ddjvuapi >= $DJVULIBRE_REQUIRED, enable_djvu=yes, enable_djvu=no)

    if test "x$enable_djvu" = "xyes"; then
        AC_DEFINE([ENABLE_DJVU], [1], [Enable djvu viewer support.])
    else
	AC_MSG_WARN([	
** Djvu support is disabled since a recent version of the djvulibre 
** library was not found. You need at least djvulibre-3.5.17 which 
** can be found on http://djvulibre.djvuzone.org 
])
    fi 
fi

AM_CONDITIONAL(ENABLE_DJVU, test x$enable_djvu = xyes)

dnl ================== End of djvu checks ===================================================

dnl ================== dvi checks ===================================================

AC_ARG_ENABLE(dvi,
            [AC_HELP_STRING([--enable-dvi], [Compile with support of dvi viewer])],enable_dvi="$enableval",enable_dvi=no)

AC_ARG_ENABLE(t1lib,
            [AC_HELP_STRING([--enable-t1lib], [Compile with support of t1lib for type1 fonts in dvi])],enable_type1_fonts="$enableval",enable_type1_fonts=no)

if test "x$enable_dvi" = "xyes"; then
    AC_C_CONST
    AC_C_INLINE
    AC_TYPE_SIZE_T
    AC_CHECK_SIZEOF(long, 4)
    AC_CHECK_SIZEOF(int, 4)
    AC_CHECK_SIZEOF(short, 2)
    AC_CHECK_SIZEOF(void *, 4)
    AC_CHECK_LIB([kpathsea],[kpse_init_prog],[enable_dvi=yes],[enable_dvi=no])

    if test "x$enable_dvi" = "xyes"; then
	AC_DEFINE([ENABLE_DVI], [1], [Enable dvi viewer support.])
    else
        AC_MSG_WARN("Dvi support is disabled since kpathsea library is not found. Check your TeX installation.")
    fi
fi
AM_CONDITIONAL(ENABLE_DVI, test x$enable_dvi = xyes)

if test "x$enable_dvi" = "xyes"; then
    if test "x$enable_type1_fonts" = "xyes"; then
	AC_CHECK_LIB([t1],T1_InitLib,enable_type1_fonts=yes,enable_type1_fonts=no,[-lm])
    fi

    if test "x$enable_type1_fonts" = xyes; then
        AC_DEFINE([WITH_TYPE1_FONTS], [1], [Enable t1lib support in dvi.])
    fi
else 
    enable_type1_fonts=no
fi
AM_CONDITIONAL(WITH_TYPE1_FONTS, test x$enable_type1_fonts = xyes)

dnl ================== End of dvi checks ===================================================

dnl ================== pixbuf checks ===================================================

AC_ARG_ENABLE(pixbuf,
            [AC_HELP_STRING([--enable-pixbuf], [Compile with support of pixbuf])],enable_pixbuf="$enableval",enable_pixbuf=no)
if test "x$enable_pixbuf" = "xyes"; then
	AC_DEFINE([ENABLE_PIXBUF], [1], [Enable pixbuf support.])
fi
AM_CONDITIONAL(ENABLE_PIXBUF, test x$enable_pixbuf = xyes)

dnl ================== End of pixbuf checks ===================================================

dnl ================== comic book checks ===================================================
 
AC_ARG_ENABLE(comics,
 	[AC_HELP_STRING([--enable-comics], [Compile with support for comic book archives])],enable_comics="$enableval",enable_comics=no)
if test "x$enable_comics" = "xyes"; then
	AC_DEFINE([ENABLE_COMICS], [1], [Enable support for comics.])
fi 
AM_CONDITIONAL(ENABLE_COMICS, test x$enable_comics = xyes)

dnl ================== End of comic book checks ============================================

dnl ================== impress book checks ===================================================

AC_ARG_ENABLE(impress,
 	[AC_HELP_STRING([--enable-impress], [Compile with support for impress presentations])],enable_impress="$enableval",enable_impress=no)
if test "x$enable_impress" = "xyes"; then
	AC_DEFINE([ENABLE_IMPRESS], [1], [Enable support for impress.])
fi 
AM_CONDITIONAL(ENABLE_IMPRESS, test x$enable_impress = xyes)

dnl ================== End of impress book checks ============================================

dnl =================== Mime types list ====================================================

if test "x$enable_pdf" = "xyes" ; then
	EVINCE_MIME_TYPES="application/pdf;application/x-bzpdf;application/x-gzpdf"
fi
if test "x$enable_ps" = "xyes" ; then
	EVINCE_MIME_TYPES="$EVINCE_MIME_TYPES;application/postscript;application/x-bzpostscript;application/x-gzpostscript;image/x-eps;image/x-bzeps;image/x-gzeps"
fi
if test "x$enable_dvi" = "xyes"; then
	EVINCE_MIME_TYPES="$EVINCE_MIME_TYPES;application/x-dvi"
fi
if test "x$enable_djvu" = "xyes"; then
	EVINCE_MIME_TYPES="$EVINCE_MIME_TYPES;image/vnd.djvu"
fi
if test "x$enable_tiff" = "xyes"; then
	EVINCE_MIME_TYPES="$EVINCE_MIME_TYPES;image/tiff"
fi
if test "x$enable_comics" = "xyes"; then
	EVINCE_MIME_TYPES="$EVINCE_MIME_TYPES;application/x-cbr;application/x-cbz"
fi
if test "x$enable_pixbuf" = "xyes"; then
	EVINCE_MIME_TYPES="$EVINCE_MIME_TYPES;image/*"
fi
if test "x$enable_impress" = "xyes"; then
	EVINCE_MIME_TYPES="$EVINCE_MIME_TYPES;application/vnd.sun.xml.impress;application/vnd.oasis.opendocument.presentation"
fi
AC_SUBST(EVINCE_MIME_TYPES)

AC_CONFIG_FILES([
backend/Makefile
backend/comics/Makefile
backend/djvu/Makefile
backend/dvi/Makefile
backend/dvi/mdvi-lib/Makefile
backend/impress/Makefile
backend/pdf/Makefile
backend/pixbuf/Makefile
backend/ps/Makefile
backend/tiff/Makefile
cut-n-paste/Makefile
cut-n-paste/recent-files/Makefile
cut-n-paste/toolbar-editor/Makefile
cut-n-paste/zoom-control/Makefile
cut-n-paste/totem-screensaver/Makefile
data/evince.desktop.in
data/Makefile
data/icons/Makefile
data/icons/16x16/Makefile
data/icons/16x16/apps/Makefile
data/icons/16x16/actions/Makefile
data/icons/22x22/Makefile
data/icons/22x22/apps/Makefile
data/icons/22x22/actions/Makefile
data/icons/24x24/Makefile
data/icons/24x24/apps/Makefile
data/icons/24x24/actions/Makefile
data/icons/48x48/Makefile
data/icons/48x48/apps/Makefile
data/icons/scalable/Makefile
data/icons/scalable/apps/Makefile
help/Makefile
help/reference/Makefile
help/reference/version.xml
libdocument/Makefile
Makefile
po/Makefile.in
properties/Makefile
shell/Makefile
test/Makefile
thumbnailer/Makefile
])

AC_OUTPUT

echo "
Configure summary:
	GNOME Support......:  $WITH_GNOME
	Print Support......:  $enable_print
	DBUS Support.......:  $enable_dbus
	Nautilus Plugin....:  $HAVE_NAUTILUS
	Thumbnailer........:  $ENABLE_THUMBNAILER
	Gtk-Doc Support....:  $enable_gtk_doc

	PDF Backend........:  $enable_pdf
	PostScript Backend.:  $enable_ps
	TIFF Backend.......:  $enable_tiff
	DJVU Backend.......:  $enable_djvu
	DVI Backend........:  $enable_dvi
	Pixbuf Backend.....:  $enable_pixbuf
	Comics Backend.....:  $enable_comics
	Impress Backend....:  $enable_impress
"
