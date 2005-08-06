dnl Process this file with autoconf to produce a configure script.

AC_PREREQ(2.57)
AC_INIT(evince, 0.3.2)
AM_INIT_AUTOMAKE(AC_PACKAGE_NAME, AC_PACKAGE_VERSION)

AM_CONFIG_HEADER(config.h)

AM_MAINTAINER_MODE

AM_PROG_LIBTOOL

AC_ISC_POSIX
AC_PROG_CC
AM_PROG_CC_STDC
AC_PROG_CXX
AC_STDC_HEADERS
AC_PROG_INTLTOOL
AC_PATH_PROG([GLIB_GENMARSHAL], [glib-genmarshal])
AC_PATH_PROG([GLIB_MKENUMS],[glib-mkenums])

GNOME_COMMON_INIT
GNOME_DOC_INIT
GNOME_MAINTAINER_MODE_DEFINES
GNOME_COMPILE_WARNINGS
GNOME_CXX_WARNINGS
GNOME_DEBUG_CHECK

ALL_LINGUAS="bg ca cs da de el en_CA en_GB es et eu fi fr gl gu he hi hu ja ko lt nb ne nl no pa pl pt_BR ru rw sk sq sr sr@Latn sv th uk wa zh_CN zh_TW"

AM_GLIB_GNU_GETTEXT

GETTEXT_PACKAGE=AC_PACKAGE_NAME
AC_SUBST(GETTEXT_PACKAGE)
AC_DEFINE_UNQUOTED(GETTEXT_PACKAGE,"$GETTEXT_PACKAGE", [Gettext package.])

dnl Check dependencies

# LIB_CFLAGS       for helpers and generic widgets. (lib, cut-and-paste)
# BACKEND_CFLAGS   for backend implementations.
# FRONTEND_CFLAGS  for frontend implementations. (properties, thumbnailer)
# FRONTEND_LIBS
# SHELL_CFLAGS     for shell implementation.
# SHELL_LIBS

POPPLER_REQUIRED=0.3.3
DBUS_GLIB_REQUIRED=0.33
GTK_REQUIRED=2.6.0
LIBGNOMEUI_REQUIRED=2.6.0

PKG_CHECK_MODULES(LIB, gtk+-2.0 >= $GTK_REQUIRED libgnomeui-2.0 >= $LIBGNOMEUI_REQUIRED)
PKG_CHECK_MODULES(BACKEND, gtk+-2.0 >= $GTK_REQUIRED)
PKG_CHECK_MODULES(FRONTEND_CORE, gtk+-2.0 >= $GTK_REQUIRED libgnomeui-2.0 >= $LIBGNOMEUI_REQUIRED libglade-2.0 poppler-glib >= $POPPLER_REQUIRED)
PKG_CHECK_MODULES(POPPLER, poppler-glib >= $POPPLER_REQUIRED)
PKG_CHECK_MODULES(SHELL_CORE, gtk+-2.0 >= $GTK_REQUIRED libgnomeui-2.0 >= $LIBGNOMEUI_REQUIRED gnome-vfs-2.0 libgnomeprint-2.2 >= 2.5.1 libgnomeprintui-2.2 libglade-2.0 gconf-2.0 poppler-glib >= $POPPLER_REQUIRED)

GLIB_GENMARSHAL=`$PKG_CONFIG --variable=glib_genmarshal glib-2.0`  
AC_SUBST(GLIB_GENMARSHAL)

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

	DBUS_VERSION=`$PKG_CONFIG --modversion dbus-glib-1 | sed 's/0.\([[0-9]]*\)/\1/'`
	AC_DEFINE_UNQUOTED(DBUS_VERSION, $DBUS_VERSION, [DBUS version.])
fi

SHELL_CFLAGS="$SHELL_CORE_CFLAGS $DBUS_CFLAGS"
SHELL_LIBS="$SHELL_CORE_LIBS $DBUS_LIBS -ltiff -lz"
AC_SUBST(SHELL_CFLAGS)
AC_SUBST(SHELL_LIBS)

FRONTEND_CFLAGS="$FRONTEND_CORE_CFLAGS"
FRONTEND_LIBS="$FRONTEND_CORE_LIBS -ltiff -lz"
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

dnl Check for functions not present in gtk 2.6
evince_save_LIBS=$LIBS
LIBS="$LIBS $FRONTEND_CORE_LIBS"
AC_CHECK_FUNCS(gtk_icon_view_get_visible_range)
AC_CHECK_FUNCS(g_file_set_contents)
LIBS=$evince_save_LIBS

dnl GConf configuration
AM_GCONF_SOURCE_2

AC_PATH_PROG([GCONFTOOL], [gconftool-2], [no])
if test "x$GCONFTOOL" = "xno"; then
	AC_MSG_ERROR([gconftool-2 executable not found in your path - should be installed with GConf])
fi

dnl ================== ggv checks ===================================================
AC_ARG_WITH(gs-pkg,
            [  --with-gs=dir       Directory Where GhostScript package is installed.])

if test "x$with_gs" = "x"; then
        AC_PATH_PROG(GS_PROG, gs)
        if test -z "$GS_PROG"; then
                AC_MSG_ERROR(Unable to find GhostScript in the PATH. Provide the full path for GhostScript(--with-gs=PATH). You need to have Ghostscript installed in order to run evince)
        fi
else
        GS_PROG=$with_gs
fi

AC_DEFINE_UNQUOTED(GS_PATH, "$GS_PROG", [Path to the 'gs' executable.])


dnl check for GS version
AC_MSG_CHECKING(for Ghostscript version...)
GS_VERSION=`gs --version | head -n 1 | sed 's/\([[0-9]]*\)\.\([[0-9]]*\).*/\1/'`
AC_MSG_RESULT(found $GS_VERSION)
if test "$GS_VERSION" -lt "7"; then
	AC_MSG_ERROR([You need Ghostscript version >= 7 in order to run evince])
fi
AA_PARMS="-sDEVICE=x11alpha -dNOPLATFONTS"
AC_DEFINE_UNQUOTED(ALPHA_PARAMS, "$AA_PARMS", [Anti-aliasing parameters for Ghostscript.])
AC_MSG_RESULT(Antialiasing parameters for Ghostscript: $AA_PARMS)
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
    AC_CHECK_HEADERS([libdjvu/ddjvuapi.h],enable_djvu=yes,enable_djvu=no,)

    if test "x$enable_djvu" = "xyes"; then
	AC_CHECK_LIB([djvulibre],ddjvu_context_create,enable_djvu=yes,enable_djvu=no,"-lpthread")
	AC_CHECK_LIB([djvulibre],ddjvu_document_get_pageinfo,enable_djvu=yes,enable_djvu=no,"-lpthread")
    fi

    if test "x$enable_djvu" = "xyes"; then
        AC_DEFINE([ENABLE_DJVU], [1], [Enable djvu viewer support.])
    else
	AC_MSG_WARN([	
** Djvu support is disabled since a recent version of the djvulibre 
** library was not found. You need at least djvulibre-3.5.15 which 
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
	AC_CHECK_LIB([t1lib],T1_InitLib,enable_type1_fonts=yes,enable_type1_fonts=no)
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

dnl ================== End of dvi checks ===================================================

dnl =================== Mime types list ====================================================

EVINCE_MIME_TYPES="application/pdf;application/postscript;application/x-gzpostscript"

if test "x$enable_dvi" = "xyes"; then
	EVINCE_MIME_TYPES="$EVINCE_MIME_TYPES;application/x-dvi"
fi
if test "x$enable_djvu" = "xyes"; then
	EVINCE_MIME_TYPES="$EVINCE_MIME_TYPES;image/vnd.djvu"
fi
if test "x$enable_tiff" = "xyes"; then
	EVINCE_MIME_TYPES="$EVINCE_MIME_TYPES;image/tiff"
fi
AC_SUBST(EVINCE_MIME_TYPES)

AC_CONFIG_FILES([
Makefile
cut-n-paste/Makefile
cut-n-paste/recent-files/Makefile
cut-n-paste/zoom-control/Makefile
cut-n-paste/toolbar-editor/Makefile
data/Makefile
data/evince.desktop.in
lib/Makefile
pdf/Makefile
pixbuf/Makefile
tiff/Makefile
ps/Makefile
djvu/Makefile
dvi/Makefile
dvi/mdvi-lib/Makefile
po/Makefile.in
backend/Makefile
properties/Makefile
shell/Makefile
thumbnailer/Makefile
help/Makefile
])

AC_OUTPUT
