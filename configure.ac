dnl Process this file with autoconf to produce a configure script.

AC_PREREQ(2.57)
AC_INIT(evince, 0.3.2)
AM_INIT_AUTOMAKE(AC_PACKAGE_NAME, AC_PACKAGE_VERSION)

dnl make sure we keep ACLOCAL_FLAGS around for maintainer builds to work
AC_SUBST(ACLOCAL_AMFLAGS, "$ACLOCAL_FLAGS")

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

GNOME_DEBUG_CHECK

ALL_LINGUAS="bg ca cs da de el en_CA en_GB es fi fr hu ja ko lt nb nl no pt_BR ru rw sk sv th uk wa zh_CN zh_TW"

AM_GLIB_GNU_GETTEXT

GETTEXT_PACKAGE=AC_PACKAGE_NAME
AC_SUBST(GETTEXT_PACKAGE)
AC_DEFINE_UNQUOTED(GETTEXT_PACKAGE,"$GETTEXT_PACKAGE", [Gettext package.])

POPPLER_REQUIRED=0.3.3
DBUS_GLIB_REQUIRED=0.33

PKG_CHECK_MODULES(LIBEVPRIVATE, gtk+-2.0 >= 2.4.0)
PKG_CHECK_MODULES(TOOLBAR_EDITOR, gtk+-2.0 >= 2.4.0 libgnomeui-2.0 >= 2.4.0)
PKG_CHECK_MODULES(RECENT_FILES, gtk+-2.0 >= 2.4.0 libgnomeui-2.0 >= 2.4.0)
PKG_CHECK_MODULES(ZOOM_CONTROL, gtk+-2.0 >= 2.4.0)
PKG_CHECK_MODULES(SHELL, gtk+-2.0 >= 2.6.0 libgnomeui-2.0 >= 2.6.0 gnome-vfs-2.0 libgnomeprint-2.2 >= 2.5.1 libgnomeprintui-2.2 libglade-2.0 gconf-2.0 poppler-glib >= $POPPLER_REQUIRED)
PKG_CHECK_MODULES(THUMBNAILER, gtk+-2.0 >= 2.6.0 gnome-vfs-2.0 poppler-glib >= $POPPLER_REQUIRED)
PKG_CHECK_MODULES(PROPERTIES, gtk+-2.0 >= 2.6.0 libglade-2.0 poppler-glib >= $POPPLER_REQUIRED)
PKG_CHECK_MODULES(DVI, gtk+-2.0 >= 2.6.0)
PKG_CHECK_MODULES(GTK, gtk+-2.0 >= 2.6.0)
PKG_CHECK_MODULES(PS, gtk+-2.0 >= 2.6.0 gnome-vfs-2.0 libgnomeui-2.0)
PKG_CHECK_MODULES(POPPLER_GLIB, poppler-glib >= $POPPLER_REQUIRED)

GLIB_GENMARSHAL=`$PKG_CONFIG --variable=glib_genmarshal glib-2.0`  
AC_SUBST(GLIB_GENMARSHAL)

AC_ARG_ENABLE([dbus],
        AS_HELP_STRING([--enable-dbus],[Enable DBUS (default=no)]),
        [enable_dbus=$enableval],
        [enable_dbus=no])

AC_MSG_RESULT([$enable_dbus])

DBUS_VERSION=
if test "x$enable_dbus" = "xyes" ; then
        AC_DEFINE([ENABLE_DBUS],[1],[Define if DBUS support is enabled])
	AC_DEFINE([ENABLE_METADATA],[1],[Define if metadata support is enabled])
        PKG_CHECK_MODULES([DBUS], [dbus-glib-1 >= $DBUS_GLIB_REQUIRED])

	DBUS_VERSION=`$PKG_CONFIG --modversion dbus-glib-1 | sed 's/0.\([[0-9]]*\)/\1/'`
	AC_DEFINE_UNQUOTED(DBUS_VERSION, $DBUS_VERSION, [DBUS version.])
fi

AM_CONDITIONAL([ENABLE_DBUS], [test "x$enable_dbus" = "xyes"])
AM_CONDITIONAL([ENABLE_METADATA], [test "x$enable_dbus" = "xyes"])
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

dnl Compile with disable-deprecated switches

AC_ARG_ENABLE(deprecated,
AC_HELP_STRING([--disable-deprecated],
	       [Don't allow any deprecated GTK+/etc. features.]),
set_enable_deprecated="$enableval",[
if test -f $srcdir/autogen.sh; then
	is_cvs_version=true
	set_enable_deprecated=no
else
	set_enable_deprecated=yes
	fi
])
AC_MSG_CHECKING([whether to disable deprecated glib/gtk+/etc. features])
if test "$set_enable_deprecated" != "yes"; then
	AC_MSG_RESULT(yes)
	EVINCE_DISABLE_DEPRECATED="-DG_DISABLE_DEPRECATED -DGDK_DISABLE_DEPRECATED -DGTK_DISABLE_DEPRECATED -DGNOME_DISABLE_DEPRECATED -DBONOBO_DISABLE_DEPRECATED"
else
	AC_MSG_RESULT(no)
	EVINCE_DISABLE_DEPRECATED=""
fi
AC_SUBST(EVINCE_DISABLE_DEPRECATED)

evince_save_LIBS=$LIBS
LIBS="$LIBS $GTK_LIBS"
AC_CHECK_FUNCS(gtk_icon_view_get_visible_range)
LIBS=$evince_save_LIBS

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
** Djvu support is disabled since recent version of djvulibre 
** library was not found. To get proper djvu support you need to
** install development version of djvulibre. You can get it from
** anonymous CVS server on djvulibre.sf.net. Just use something
** like:

** cvs -d :pserver:anonymous@cvs.sourceforge.net:/cvsroot/djvu co djvulibre-3.5
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

dnl Turn on the additional warnings last, so -Werror doesn't affect other tests.
dnl stolen from nautilus and gnome-common

AC_ARG_ENABLE(more-warnings,
[  --enable-more-warnings  Maximum compiler warnings],
set_more_warnings="$enableval",[
if test -f $srcdir/autogen.sh; then
	is_cvs_version=true
	set_more_warnings=yes
else
	set_more_warnings=no
fi
])
AC_MSG_CHECKING(for more warnings, including -Werror)
if test "$GCC" = "yes" -a "$set_more_warnings" != "no"; then
	AC_MSG_RESULT([yes, using gcc])
	CFLAGS="\
	-Wall \
	-Wchar-subscripts -Wmissing-declarations -Wmissing-prototypes \
	-Wnested-externs -Wpointer-arith \
	-Wcast-align -Wsign-compare \
	-Werror \
	$CFLAGS"

dnl 	case " $CFLAGS " in
dnl 	    *[\ \	]-ansi[\ \	]*) ;;
dnl 	    *) CFLAGS="$CFLAGS -ansi" ;;
dnl 	esac
dnl 	case " $CFLAGS " in
dnl 	    *[\ \	]-pedantic[\ \	]*) ;;
dnl 	    *) CFLAGS="$CFLAGS -pedantic" ;;
dnl 	esac

	for option in -Wno-strict-aliasing -Wno-sign-compare; do
		SAVE_CFLAGS="$CFLAGS"
		CFLAGS="$CFLAGS $option"
		AC_MSG_CHECKING([whether gcc understands $option])
		AC_TRY_COMPILE([], [],
			has_option=yes,
			has_option=no,)
		if test $has_option = no; then
			CFLAGS="$SAVE_CFLAGS"
		fi
		AC_MSG_RESULT($has_option)
		unset has_option
		unset SAVE_CFLAGS
	done
	unset option
else
	AC_MSG_RESULT(no)
fi

AC_OUTPUT([
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
help/C/Makefile
])
