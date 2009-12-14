# *****************************************************************************
# Versioning
# *****************************************************************************

m4_define([ev_major_version],[2])
m4_define([ev_minor_version],[28])
m4_define([ev_micro_version],[2])
m4_define([ev_extra_version],[])
m4_define([ev_version],[ev_major_version.ev_minor_version.ev_micro_version()ev_extra_version])

# The evince API version
m4_define([ev_api_version], [2.25])

# Libtool versioning. The backend and view libraries have separate versions.
# Before making a release, the libtool version should be modified.
# The string is of the form C:R:A.
# - If interfaces have been changed or added, but binary compatibility has
#   been preserved, change to C+1:0:A+1
# - If binary compatibility has been broken (eg removed or changed interfaces)
#   change to C+1:0:0
# - If the interface is the same as the previous version, change to C:R+1:A

# Libtool version of the backend library
m4_define([ev_document_lt_current],[1])
m4_define([ev_document_lt_revision],[0])
m4_define([ev_document_lt_age],[0])
m4_define([ev_document_lt_version_info],[ev_document_lt_current:ev_document_lt_revision:ev_document_lt_age])
m4_define([ev_document_lt_current_minus_age],[m4_eval(ev_document_lt_current - ev_document_lt_age)])

# Libtool version of the view library
m4_define([ev_view_lt_current],[1])
m4_define([ev_view_lt_revision],[0])
m4_define([ev_view_lt_age],[0])
m4_define([ev_view_lt_version_info],[ev_view_lt_current:ev_view_lt_revision:ev_view_lt_age])
m4_define([ev_view_lt_current_minus_age],[m4_eval(ev_view_lt_current - ev_view_lt_age)])

# Binary version for the document backends
m4_define([ev_binary_version],[ev_document_lt_current])

# *****************************************************************************

AC_PREREQ([2.57])
AC_INIT([Evince],[ev_version],[http://bugzilla.gnome.org/enter_bug.cgi?product=evince],[evince])
AM_INIT_AUTOMAKE([1.9 foreign dist-bzip2 no-dist-gzip])

AC_CONFIG_HEADERS([config.h])
AC_CONFIG_MACRO_DIR([m4])

if test -z "$enable_maintainer_mode"; then
  enable_maintainer_mode=yes
fi
AM_MAINTAINER_MODE([enable])

m4_ifdef([AM_SILENT_RULES],[AM_SILENT_RULES([yes])])

AM_PROG_LIBTOOL

AC_ISC_POSIX
AC_PROG_CC
AM_PROG_CC_STDC
AM_PROG_CC_C_O
AC_PROG_CXX
AC_STDC_HEADERS

AC_PATH_PROG([GLIB_GENMARSHAL], [glib-genmarshal])
AC_PATH_PROG([GLIB_MKENUMS],[glib-mkenums])

GNOME_COMMON_INIT
GNOME_DOC_INIT
GNOME_MAINTAINER_MODE_DEFINES
GNOME_COMPILE_WARNINGS
GNOME_CXX_WARNINGS

GTK_DOC_CHECK(1.0)

IT_PROG_INTLTOOL([0.35.0])

GETTEXT_PACKAGE=evince
AC_SUBST(GETTEXT_PACKAGE)
AC_DEFINE_UNQUOTED([GETTEXT_PACKAGE],["$GETTEXT_PACKAGE"],[Gettext package])
AM_GLIB_GNU_GETTEXT

m4_pattern_allow([AM_V_GEN])dnl Make autoconf not complain about the rule below
EV_INTLTOOL_EVINCE_BACKEND_RULE='%.evince-backend:   %.evince-backend.in   $(INTLTOOL_MERGE) $(wildcard $(top_srcdir)/po/*.po) ; $(AM_V_GEN) LC_ALL=C $(INTLTOOL_MERGE) -d -u -c $(top_builddir)/po/.intltool-merge-cache $(top_srcdir)/po $< [$]@'
AC_SUBST([EV_INTLTOOL_EVINCE_BACKEND_RULE])

# Check which platform to use

AC_MSG_CHECKING([for which platform to build])
AC_ARG_WITH([platform],
  [AS_HELP_STRING([--with-platform=gnome|win32|hildon (default: gnome)])],
  [case "$withval" in
    gnome|win32|hildon) ;;
    *) AC_MSG_ERROR([invalid argument "$withval" for --with-platform]) ;;
   esac],
  [case "$host" in
     *-*-mingw*|*-*-cygwin*) with_platform="win32" ;;
     *) with_platform=gnome ;;
   esac])

AC_MSG_RESULT([$with_platform])

if test "$with_platform" = "win32"; then
  AC_CHECK_TOOL([WINDRES],[windres])
  AC_MSG_CHECKING([for native Win32])
  case "$host" in
    *-*-mingw*)
      os_win32=yes
      ;;
    *)
      os_win32=no
      ;;
  esac
  AC_MSG_RESULT([$os_win32])
fi

AM_CONDITIONAL([PLATFORM_HILDON],[test "$with_platform" = "hildon"])
AM_CONDITIONAL([PLATFORM_WIN32],[test "$with_platform" = "win32"])

dnl Specify required versions of dependencies
DBUS_GLIB_REQUIRED=0.70
GTK_REQUIRED=2.14.0
GLIB_REQUIRED=2.18.0
KEYRING_REQUIRED=2.22.0
AC_SUBST([GLIB_REQUIRED])
AC_SUBST([GTK_REQUIRED])

GNOME_ICON_THEME_REQUIRED=2.17.1
LIBXML_REQUIRED=2.5.0

dnl Check dependencies

# LIB_CFLAGS       for helpers and generic widgets. (libdocument, cut-and-paste)
# BACKEND_CFLAGS   for backend implementations.
# FRONTEND_CFLAGS  for frontend implementations. (properties, thumbnailer)
# FRONTEND_LIBS
# SHELL_CFLAGS     for shell implementation.
# SHELL_LIBS

PKG_CHECK_MODULES(LIBDOCUMENT, gtk+-2.0 >= $GTK_REQUIRED gio-2.0 >= $GLIB_REQUIRED)
PKG_CHECK_MODULES(LIBVIEW, gtk+-2.0 >= $GTK_REQUIRED gthread-2.0 gio-2.0 >= $GLIB_REQUIRED)
PKG_CHECK_MODULES(BACKEND, gtk+-2.0 >= $GTK_REQUIRED)
PKG_CHECK_MODULES(FRONTEND_CORE, gtk+-2.0 >= $GTK_REQUIRED gthread-2.0 gio-2.0 >= $GLIB_REQUIRED)
PKG_CHECK_MODULES(PREVIEWER, gtk+-2.0 >= $GTK_REQUIRED gthread-2.0 gio-2.0 >= $GLIB_REQUIRED)

SHELL_PLATFORM_PKGS=
case "$with_platform" in
  hildon) AC_DEFINE([PLATFORM_HILDON],[1],[Define if building for the hildon platform])
          SHELL_PLATFORM_PKGS="hildon-1 hildon-fm-2 libosso"
          ;;
  *) SHELL_PLATFORM_PKGS="gnome-icon-theme >= $GNOME_ICON_THEME_REQUIRED"
     ;;
esac

PKG_CHECK_MODULES([SHELL_CORE],[libxml-2.0 >= $LIBXML_REQUIRED gtk+-2.0 >= $GTK_REQUIRED gio-2.0 >= $GLIB_REQUIRED gthread-2.0 $SHELL_PLATFORM_PKGS])

# *********
# SM client
# *********

GDK_TARGET="$($PKG_CONFIG --variable target gdk-2.0)"

SMCLIENT_PKGS=
AC_MSG_CHECKING([which smclient backend to use])
AC_ARG_WITH([smclient],
  [AS_HELP_STRING([--with-smclient-backend],[which smclient backend to use (no|xsmp|win32|quartz)])],
  [],
  [case "$GDK_TARGET" in
    x11) case "$with_platform" in
           gnome) with_smclient=xsmp SMCLIENT_PKGS="sm >= 1.0.0" ;;
           *) with_smclient=no ;;
         esac ;;
    win32|quartz) with_smclient=$GDK_TARGET ;;
    *) with_smclient=no ;;
   esac])
AC_MSG_RESULT([$with_smclient])

if test "$with_smclient" != "no"; then
  AC_DEFINE([WITH_SMCLIENT],[1],[Define if smclient is enabled])

  PKG_CHECK_MODULES([SMCLIENT],[gtk+-2.0 gthread-2.0 $SMCLIENT_PKGS])
  AC_SUBST([SMCLIENT_CFLAGS])
  AC_SUBST([SMCLIENT_LIBS])
fi

AM_CONDITIONAL([WITH_SMCLIENT],[test "$with_smclient" != "no"])
AM_CONDITIONAL([WITH_SMCLIENT_XSMP],[test "$with_smclient" = "xsmp"])
AM_CONDITIONAL([WITH_SMCLIENT_WIN32],[test "$with_smclient" = "win32"])
AM_CONDITIONAL([WITH_SMCLIENT_QUARTZ],[test "$with_smclient" = "quartz"])

# ***

BACKEND_LIBTOOL_FLAGS="-module -avoid-version -no-undefined -export-symbols \$(top_srcdir)/backend/backend.symbols"
AC_SUBST(BACKEND_LIBTOOL_FLAGS)

dnl ===== Check special functions
evince_save_LIBS=$LIBS
LIBS="$LIBS $BACKEND_LIBS"
AC_CHECK_FUNCS(cairo_format_stride_for_width)
AC_CHECK_FUNCS(gtk_print_operation_get_n_pages_to_print)
LIBS=$evince_save_LIBS

# ******************
# GKT+ Unix Printing
# ******************

AC_MSG_CHECKING([whether gtk+-unix-print support is requested])
AC_ARG_WITH([gtk-unix-print],
  [AS_HELP_STRING([--without-gtk-unix-print],[disable the use of gtk-unix-print])],
  [],[case "$os_win32" in
        yes) with_gtk_unix_print=no ;;
        *) with_gtk_unix_print=yes ;;
      esac])
AC_MSG_RESULT([$with_gtk_unix_print])

if test "$with_gtk_unix_print" = "yes"; then
   PKG_CHECK_MODULES(GTKUNIXPRINT, [gtk+-unix-print-2.0 >= $GTK_REQUIRED])
   AC_DEFINE([GTKUNIXPRINT_ENABLED], [1], [Define if gtk+-unix-print is enabled.])
fi

# *********************
# GNOME Keyring support
# *********************

AC_ARG_WITH(keyring,
        [AS_HELP_STRING([--without-keyring],[disable the use of gnome-keyring])],
        [],
        [case "$with_platform" in
           gnome) with_keyring=yes ;;
           hildon|win32) with_keyring=no ;;
         esac])

AM_CONDITIONAL([WITH_KEYRING],[test "$with_keyring" = "yes"])

if test "$with_keyring" = "yes"; then
        PKG_CHECK_MODULES(KEYRING, gnome-keyring-1 >= $KEYRING_REQUIRED)
        AC_DEFINE([WITH_KEYRING],[1],[Define if KEYRING support is enabled])
fi

GLIB_GENMARSHAL=`$PKG_CONFIG --variable=glib_genmarshal glib-2.0`  
AC_SUBST(GLIB_GENMARSHAL)

# ****
# DBUS
# ****

AC_ARG_ENABLE([dbus],
  [AS_HELP_STRING([--enable-dbus], [Compile with support for dbus])],
  [],
  [case "$with_platform" in
      gnome) enable_dbus=yes ;;
      hildon) enable_dbus=no ;;
    esac])

if test "$enable_dbus" = "yes"; then
  PKG_CHECK_MODULES([DBUS],[dbus-glib-1 >= $DBUS_GLIB_REQUIRED],[],
                    [AC_MSG_ERROR([DBUS not found; use --disable-dbus to disable DBUS support])])

  AC_PATH_PROG([DBUS_BINDING_TOOL], [dbus-binding-tool], [no])
  if test x$DBUS_BINDING_TOOL = "xno" ; then
          AC_MSG_ERROR([dbus-binding-tool executable not found in your path - should be installed with dbus glib bindings])
  fi

  AC_DEFINE([ENABLE_DBUS],[1],[Define if DBUS support is enabled])
fi

AC_SUBST([DBUS_CFLAGS])
AC_SUBST([DBUS_LIBS])

AM_CONDITIONAL([ENABLE_DBUS], [test "$enable_dbus" = "yes"])

dnl ========= Check for GConf

AC_MSG_CHECKING([whether GConf support is requested])
AC_ARG_WITH([gconf],
  [AS_HELP_STRING([--without-gconf],[disable the use of gconf])],
  [],
  [case "$os_win32" in
     yes) with_gconf=no ;;
     *) with_gconf=yes ;;
   esac])
AC_MSG_RESULT([$with_gconf])

AM_CONDITIONAL([WITH_GCONF],[test "$with_gconf" = "yes"])

if test "$with_gconf" = "yes"; then
   PKG_CHECK_MODULES([GCONF],[gconf-2.0])
   AC_DEFINE([WITH_GCONF],[1],[Define if GConf support is enabled])

   AM_GCONF_SOURCE_2

   AC_PATH_PROG([GCONFTOOL], [gconftool-2], [false])
   if test "$GCONFTOOL" = "false"; then
      AC_MSG_ERROR([gconftool-2 executable not found in your path - should be installed with GConf])
   fi
else
   AM_CONDITIONAL([GCONF_SCHEMAS_INSTALL],false)
fi

dnl Debug mode

AC_ARG_ENABLE([debug],
	AC_HELP_STRING([--enable-debug],
			[Turn on evince debug mode]),,
		[enable_debug=no])
		
if test "x$enable_debug" = "xyes"; then
   DEBUG_FLAGS="-DEV_ENABLE_DEBUG"
fi

AC_ARG_ENABLE([tests],
	AC_HELP_STRING([--enable-tests], [Disable tests]),
	enable_tests="$enableval",
	enable_tests=yes)

AM_CONDITIONAL(ENABLE_TESTS, test x$enable_tests = xyes)

LIBDOCUMENT_CFLAGS="$LIBDOCUMENT_CFLAGS $DEBUG_FLAGS"
LIBDOCUMENT_LIBS="$LIBDOCUMENT_LIBS"
AC_SUBST(LIBDOCUMENT_CFLAGS)
AC_SUBST(LIBDOCUMENT_LIBS)

LIBVIEW_CFLAGS="$LIBVIEW_CFLAGS $DEBUG_FLAGS"
LIBVIEW_LIBS="$LIBVIEW_LIBS"
AC_SUBST(LIBVIEW_CFLAGS)
AC_SUBST(LIBVIEW_LIBS)

BACKEND_CFLAGS="$BACKEND_CFLAGS -DGDK_MULTIHEAD_SAFE -DGTK_MULTIHEAD_SAFE $DEBUG_FLAGS"
AC_SUBST(BACKEND_CFLAGS)
AC_SUBST(BACKEND_LIBS)

SHELL_CFLAGS="$SHELL_CORE_CFLAGS $GTKUNIXPRINT_CFLAGS $DBUS_CFLAGS $KEYRING_CFLAGS $GCONF_CFLAGS -DGDK_MULTIHEAD_SAFE -DGTK_MULTIHEAD_SAFE $DEBUG_FLAGS"
SHELL_LIBS="$SHELL_CORE_LIBS $GTKUNIXPRINT_CFLAGS $DBUS_LIBS $KEYRING_LIBS $GCONF_LIBS -lz"
AC_SUBST(SHELL_CFLAGS)
AC_SUBST(SHELL_LIBS)

FRONTEND_CFLAGS="$FRONTEND_CORE_CFLAGS $DEBUG_FLAGS"
FRONTEND_LIBS="$FRONTEND_CORE_LIBS -lz"
AC_SUBST(FRONTEND_CFLAGS)
AC_SUBST(FRONTEND_LIBS)

# Check for Nautilus property page build
AC_ARG_ENABLE([nautilus],
  [AS_HELP_STRING([--disable-nautilus],[Build the nautilus extensions])],
  [],
  [case "$with_platform" in
     gnome) enable_nautilus=yes ;;
     *) enable_nautilus=no ;;
    esac])

if test "$enable_nautilus" = "yes" ; then
  PKG_CHECK_MODULES([NAUTILUS],[gtk+-x11-2.0 $MM gthread-2.0 libnautilus-extension],
                    [],[AC_MSG_ERROR([libnautilus-extension not found; use --disable-nautilus to disable the nautilus extensions])])
  NAUTILUS_EXTENSION_DIR=`$PKG_CONFIG --variable=extensiondir libnautilus-extension`
  AC_SUBST([nautilusextensiondir],[$NAUTILUS_EXTENSION_DIR])
  AC_SUBST(NAUTILUS_CFLAGS)
  AC_SUBST(NAUTILUS_LIBS)

  AC_DEFINE([HAVE_NAUTILUS],[1], [defined if you build the nautilus plugin])
fi

AM_CONDITIONAL([ENABLE_NAUTILUS],[test "$enable_nautilus" = "yes"])

# Check for thumbnailer build

AC_ARG_ENABLE([thumbnailer],
  [AS_HELP_STRING([--disable-thumbnailer],[disable the GNOME thumbnailer])],
  [],
  [case "$with_platform" in
     gnome) enable_thumbnailer=yes ;;
     *) enable_thumbnailer=no ;;
    esac])

AM_CONDITIONAL([ENABLE_THUMBNAILER],[test "$enable_thumbnailer" = "yes"])

# ***************
# Print Previewer
# ***************

AC_ARG_ENABLE([previewer],
  [AS_HELP_STRING([--disable-previewer],[disable the GNOME Document Previewer])],
  [],
  [case "$with_platform" in
      gnome) enable_previewer=yes ;;
      *) enable_previewer=no ;;
    esac])

if test x$enable_previewer = "xyes" ; then
  PKG_CHECK_MODULES([PREVIEWER],[gtk+-2.0 >= $GTK_REQUIRED gtk+-unix-print-2.0 >= $GTK_REQUIRED gthread-2.0 gio-2.0 >= $GLIB_REQUIRED])
fi

AM_CONDITIONAL([ENABLE_PREVIEWER],[test "$enable_previewer" = "yes"])
PREVIEWER_CFLAGS="$PREVIEWER_CFLAGS $GTKUNIXPRINT_CFLAGS $DEBUG_FLAGS"
PREVIEWER_LIBS="$PREVIEWER_LIBS $GTKUNIXPRINT_LIBS -lz"
AC_SUBST(PREVIEWER_CFLAGS)
AC_SUBST(PREVIEWER_LIBS)

# ***
# GIR
# ***

AC_MSG_CHECKING([whether GObject introspection is requested])
AC_ARG_ENABLE([introspection],
        AS_HELP_STRING([--enable-introspection],[Enable GObject introspection]),
        [],[enable_introspection=no])
AC_MSG_RESULT([$enable_introspection])

G_IR_SCANNER=
G_IR_COMPILER=
G_IR_GENERATE=
GIRDIR=
GIRTYPELIBDIR=

if test "$enable_introspection" = "yes"; then
  GOBJECT_INTROSPECTION_REQUIRED=0.6
  PKG_CHECK_MODULES([GOBJECT_INTROSPECTION],[gobject-introspection-1.0 >= $GOBJECT_INTROSPECTION_REQUIRED])

  G_IR_SCANNER="$($PKG_CONFIG --variable=g_ir_scanner gobject-introspection-1.0)"
  G_IR_COMPILER="$($PKG_CONFIG --variable=g_ir_compiler gobject-introspection-1.0)"
  G_IR_GENERATE="$($PKG_CONFIG --variable=g_ir_generate gobject-introspection-1.0)"
  GIRDIR="$($PKG_CONFIG --variable=girdir gobject-introspection-1.0)"
  GIRTYPELIBDIR="$($PKG_CONFIG --variable=typelibdir gobject-introspection-1.0)"
fi

AC_SUBST([G_IR_SCANNER])
AC_SUBST([G_IR_COMPILER])
AC_SUBST([G_IR_GENERATE])
AC_SUBST([GIRDIR])
AC_SUBST([GIRTYPELIBDIR])

AM_CONDITIONAL([ENABLE_INTROSPECTION],[test "$enable_introspection" = "yes"])

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
    POPPLER_REQUIRED=0.11.0
    PKG_CHECK_MODULES(POPPLER, poppler-glib >= $POPPLER_REQUIRED libxml-2.0 >= $LIBXML_REQUIRED,enable_pdf=yes,enable_pdf=no)

    if test "x$enable_pdf" = "xyes"; then
	    AC_DEFINE([ENABLE_PDF], [1], [Enable pdf support.])
	    SHELL_LIBS="$SHELL_LIBS $POPPLER_LIBS"
	    SHELL_CFLAGS="$SHELL_CFLAGS $POPPLER_CFLAGS"

	    evince_save_LIBS=$LIBS
	    LIBS="$LIBS $POPPLER_LIBS"
	    AC_CHECK_FUNCS(poppler_page_render)
	    AC_CHECK_FUNCS(poppler_page_get_image)
	    LIBS=$evince_save_LIBS

	    PKG_CHECK_MODULES(CAIRO_PDF, cairo-pdf, enable_cairo_pdf=yes, enable_cairo_pdf=no)
	    if test x$enable_cairo_pdf = xyes; then
	            AC_DEFINE([HAVE_CAIRO_PDF], [1], [defined if cairo-pdf is available])
            fi

	    PKG_CHECK_MODULES(CAIRO_PS, cairo-ps, enable_cairo_ps=yes, enable_cairo_ps=no)
	    if test x$enable_cairo_ps = xyes; then
	            AC_DEFINE([HAVE_CAIRO_PS], [1], [defined if cairo-ps is available])
            fi
    else
	    AC_MSG_WARN("PDF support is disabled since poppler-glib library version $POPPLER_REQUIRED or newer not found")
    fi 
fi

AM_CONDITIONAL(ENABLE_PDF, test x$enable_pdf = xyes)
dnl ================== end of pdf checks ============================================

dnl libspectre (used by ps and dvi backends)
SPECTRE_REQUIRED=0.2.0 
PKG_CHECK_MODULES(SPECTRE, libspectre >= $SPECTRE_REQUIRED,have_spectre=yes,have_spectre=no)
AM_CONDITIONAL(HAVE_SPECTRE, test x$have_spectre = xyes)
if test "x$have_spectre" = "xyes"; then
   AC_DEFINE([HAVE_SPECTRE], [1], [Have libspectre])
fi

dnl ================== ps checks ====================================================
AC_ARG_ENABLE(ps,
  	[AC_HELP_STRING([--disable-ps], [Compile without PostScript backend])],enable_ps=$enableval,enable_ps="yes")

if test "x$enable_ps" = "xyes"; then
   if test "x$have_spectre" = "xyes"; then
      AC_DEFINE([ENABLE_PS], [1], [Enable support for PostScript files.])
   else
      enable_ps="no"
      AC_MSG_WARN([PS support is disabled since libspectre (version >= $SPECTRE_REQUIRED) is needed])
   fi
fi
AM_CONDITIONAL(ENABLE_PS, test x$enable_ps = xyes)
dnl ======================== End of ps checks ===================================

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
            [AC_HELP_STRING([--enable-djvu], [Compile with support of djvu viewer])],enable_djvu="$enableval",enable_djvu=yes)

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
            [AC_HELP_STRING([--enable-dvi], [Compile with support of dvi viewer])],enable_dvi="$enableval",enable_dvi=yes)

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
 	[AC_HELP_STRING([--enable-comics], [Compile with support for comic book archives])],enable_comics="$enableval",enable_comics=yes)
if test "x$enable_comics" = "xyes"; then
	if test "x$os_win32" = "xyes"; then
		# The comics backend is disabled on windows because:
		# 1) it uses unix functions from sys/wait.h.
		# 2) it uses external decompression tools not generally available on windows.
		enable_comics=no
		AC_MSG_WARN(The comics backend is not supported on windows.)
	else
		AC_DEFINE([ENABLE_COMICS], [1], [Enable support for comics.])
	fi
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
	EVINCE_MIME_TYPES="application/pdf;application/x-bzpdf;application/x-gzpdf;"
fi
if test "x$enable_ps" = "xyes" ; then
	EVINCE_MIME_TYPES="${EVINCE_MIME_TYPES}application/postscript;application/x-bzpostscript;application/x-gzpostscript;image/x-eps;image/x-bzeps;image/x-gzeps;"
fi
if test "x$enable_dvi" = "xyes"; then
	EVINCE_MIME_TYPES="${EVINCE_MIME_TYPES}application/x-dvi;application/x-bzdvi;application/x-gzdvi;"
fi
if test "x$enable_djvu" = "xyes"; then
	EVINCE_MIME_TYPES="${EVINCE_MIME_TYPES}image/vnd.djvu;"
fi
if test "x$enable_tiff" = "xyes"; then
	EVINCE_MIME_TYPES="${EVINCE_MIME_TYPES}image/tiff;"
fi
if test "x$enable_comics" = "xyes"; then
	EVINCE_MIME_TYPES="${EVINCE_MIME_TYPES}application/x-cbr;application/x-cbz;application/x-cb7;"
fi
if test "x$enable_pixbuf" = "xyes"; then
	EVINCE_MIME_TYPES="${EVINCE_MIME_TYPES}image/*;"
fi
if test "x$enable_impress" = "xyes"; then
	EVINCE_MIME_TYPES="${EVINCE_MIME_TYPES}application/vnd.sun.xml.impress;application/vnd.oasis.opendocument.presentation;"
fi
AC_SUBST(EVINCE_MIME_TYPES)

AC_CHECK_FUNC(localtime_r, AC_DEFINE(HAVE_LOCALTIME_R, 1, [Defines if localtime_r is available on your system]))

# Backends directory

AC_SUBST([backenddir],"\$(libdir)/evince/ev_binary_version/backends")
AC_SUBST([backend_binary_version],"ev_binary_version")

# Versioning

AC_SUBST([EV_MAJOR_VERSION],[ev_major_version])
AC_SUBST([EV_MINOR_VERSION],[ev_minor_version])
AC_SUBST([EV_MICRO_VERSION],[ev_micro_version])

AC_SUBST([EV_API_VERSION],[ev_api_version])
AC_SUBST([EV_BINARY_VERSION],[ev_binary_version])

AC_SUBST([EV_DOCUMENT_LT_VERSION_INFO],[ev_document_lt_version_info])
AC_SUBST([EV_DOCUMENT_LT_CURRENT_MINUS_AGE],[ev_document_lt_current_minus_age])
AC_SUBST([EV_VIEW_LT_VERSION_INFO],[ev_view_lt_version_info])
AC_SUBST([EV_VIEW_LT_CURRENT_MINUS_AGE],[ev_view_lt_current_minus_age])

# *****************************************************************************

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
cut-n-paste/gedit-message-area/Makefile
cut-n-paste/gimpcellrenderertoggle/Makefile
cut-n-paste/smclient/Makefile
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
data/icons/32x32/Makefile
data/icons/32x32/actions/Makefile
data/icons/48x48/Makefile
data/icons/48x48/apps/Makefile
data/icons/48x48/actions/Makefile
data/icons/scalable/Makefile
data/icons/scalable/apps/Makefile
help/Makefile
help/reference/Makefile
help/reference/libdocument/Makefile
help/reference/libdocument/version.xml
help/reference/libview/Makefile
help/reference/libview/version.xml
help/reference/shell/Makefile
help/reference/shell/version.xml
libdocument/Makefile
libdocument/ev-version.h
libmisc/Makefile
libview/Makefile
Makefile
po/Makefile.in
previewer/Makefile
properties/Makefile
shell/Makefile
test/Makefile
thumbnailer/Makefile
])

AC_CONFIG_FILES(evince-document-[]ev_api_version[].pc:evince-document.pc.in)
AC_CONFIG_FILES(evince-view-[]ev_api_version[].pc:evince-view.pc.in)

AC_OUTPUT

echo "
Configure summary:
        Platform...........:  $with_platform
        GConf Support......:  $with_gconf
	GTK+ Unix Print....:  $with_gtk_unix_print
	Keyring Support....:  $with_keyring
	DBUS Support.......:  $enable_dbus
	SM client support..:  $with_smclient
	Nautilus Plugin....:  $enable_nautilus
	Thumbnailer........:  $enable_thumbnailer
	Previewer..........:  $enable_previewer
	Gtk-Doc Support....:  $enable_gtk_doc
	Debug mode.........:  $enable_debug
	GObj. Introspection:  $enable_introspection
	Tests..............:  $enable_tests

	PDF Backend........:  $enable_pdf
	PostScript Backend.:  $enable_ps
	TIFF Backend.......:  $enable_tiff
	DJVU Backend.......:  $enable_djvu
	DVI Backend........:  $enable_dvi
	Pixbuf Backend.....:  $enable_pixbuf
	Comics Backend.....:  $enable_comics
	Impress Backend....:  $enable_impress
"
