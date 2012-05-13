# *****************************************************************************
# Versioning
# *****************************************************************************

m4_define([ev_major_version],[3])
m4_define([ev_minor_version],[4])
m4_define([ev_micro_version],[0])
m4_define([ev_extra_version],[])
m4_define([ev_version],[ev_major_version.ev_minor_version.ev_micro_version()ev_extra_version])

# The evince API version
m4_define([ev_api_version], [3.0])

# Libtool versioning. The backend and view libraries have separate versions.
# Before making a release, the libtool version should be modified.
# The string is of the form C:R:A.
# - If interfaces have been changed or added, but binary compatibility has
#   been preserved, change to C+1:0:A+1
# - If binary compatibility has been broken (eg removed or changed interfaces)
#   change to C+1:0:0
# - If the interface is the same as the previous version, change to C:R+1:A

# Libtool version of the backend library
m4_define([ev_document_lt_current],[4])
m4_define([ev_document_lt_revision],[0])
m4_define([ev_document_lt_age],[0])
m4_define([ev_document_lt_version_info],[ev_document_lt_current:ev_document_lt_revision:ev_document_lt_age])
m4_define([ev_document_lt_current_minus_age],[m4_eval(ev_document_lt_current - ev_document_lt_age)])

# Libtool version of the view library
m4_define([ev_view_lt_current],[3])
m4_define([ev_view_lt_revision],[0])
m4_define([ev_view_lt_age],[0])
m4_define([ev_view_lt_version_info],[ev_view_lt_current:ev_view_lt_revision:ev_view_lt_age])
m4_define([ev_view_lt_current_minus_age],[m4_eval(ev_view_lt_current - ev_view_lt_age)])

# Binary version for the document backends
m4_define([ev_binary_version],[ev_document_lt_current])

# *****************************************************************************

AC_PREREQ([2.57])
AC_INIT([Evince],[ev_version],[http://bugzilla.gnome.org/enter_bug.cgi?product=evince],[evince])
AM_INIT_AUTOMAKE([1.10 foreign dist-bzip2 no-dist-gzip])

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

AC_PROG_SED
AC_PATH_PROG([GLIB_GENMARSHAL], [glib-genmarshal])
AC_PATH_PROG([GLIB_MKENUMS],[glib-mkenums])

GNOME_COMPILE_WARNINGS
GNOME_CXX_WARNINGS

IT_PROG_INTLTOOL([0.35.0])

GETTEXT_PACKAGE=evince
AC_SUBST(GETTEXT_PACKAGE)
AC_DEFINE_UNQUOTED([GETTEXT_PACKAGE],["$GETTEXT_PACKAGE"],[Gettext package])
AM_GLIB_GNU_GETTEXT

m4_pattern_allow([AM_V_GEN])dnl Make autoconf not complain about the rule below
EV_INTLTOOL_EVINCE_BACKEND_RULE='%.evince-backend:   %.evince-backend.in   $(INTLTOOL_MERGE) $(wildcard $(top_srcdir)/po/*.po) ; $(AM_V_GEN) LC_ALL=C $(INTLTOOL_MERGE) -d -u -c $(top_builddir)/po/.intltool-merge-cache $(top_srcdir)/po $< [$]@'
AC_SUBST([EV_INTLTOOL_EVINCE_BACKEND_RULE])

GLIB_GSETTINGS

# Check which platform to use

AC_MSG_CHECKING([for which platform to build])
AC_ARG_WITH([platform],
  [AS_HELP_STRING([--with-platform=gnome|win32],
                  [Setting platform (default: gnome)])],
  [case "$withval" in
    gnome|win32) ;;
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

AM_CONDITIONAL([PLATFORM_WIN32],[test "$with_platform" = "win32"])

dnl Specify required versions of dependencies
CAIRO_REQUIRED=1.10.0
GLIB_REQUIRED=2.25.11
KEYRING_REQUIRED=2.22.0
GTK_REQUIRED=3.0.2
NAUTILUS_REQUIRED=2.91.4

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

AC_CHECK_LIBM
AC_SUBST(LIBM)

have_zlib=yes
AC_CHECK_HEADERS([zlib.h],
	[AC_CHECK_LIB([z], [inflate],
		[AC_CHECK_LIB([z], [crc32], [have_zlib=yes], [have_zlib=no])],
		[have_zlib=no])],
	[have_zlib=no])

if test x$have_zlib = xno; then
	AC_MSG_ERROR([No sufficient zlib library found on your system.])
fi

ZLIB_LIBS=-lz
AC_SUBST(ZLIB_LIBS)

PKG_CHECK_MODULES(LIBDOCUMENT, gtk+-3.0 >= $GTK_REQUIRED gio-2.0 >= $GLIB_REQUIRED gmodule-no-export-2.0 >= $GLIB_REQUIRED gmodule-2.0)
PKG_CHECK_MODULES(LIBVIEW, gtk+-3.0 >= $GTK_REQUIRED gail-3.0 >= $GTK_REQUIRED gthread-2.0 gio-2.0 >= $GLIB_REQUIRED)
PKG_CHECK_MODULES(BACKEND, cairo >= $CAIRO_REQUIRED gtk+-3.0 >= $GTK_REQUIRED)
PKG_CHECK_MODULES(FRONTEND_CORE, gtk+-3.0 >= $GTK_REQUIRED gthread-2.0 gio-2.0 >= $GLIB_REQUIRED gmodule-no-export-2.0 >= $GLIB_REQUIRED)

SHELL_PLATFORM_PKGS=
case "$with_platform" in
  gnome)
        # Evince has a rather soft run-time dependency on hicolor-icon-theme.
        # If the hicolor theme is not available, Evince fails to display some
        # icons. Because we cannot check for it at run-time, we instead
        # would like to require the icon theme at compile-time. But, because
        # the hicolor-icon-theme does not have a pkgconfig file, on gnome we
        # require the gnome icon theme instead.
        SHELL_PLATFORM_PKGS="gnome-icon-theme >= $GNOME_ICON_THEME_REQUIRED"
        # The totem-screensaver and egg_smclient code use x11 directly.
        SHELL_PLATFORM_PKGS="$SHELL_PLATFORM_PKGS x11"
        ;;
  *)
        # On all other platforms we issue a warning about the runtime
        # dependency.
        AC_MSG_WARN([Evince has a soft run-time dependency on hicolor-icon-theme. You are advised to have this theme installed when running Evince.]);
        SHELL_PLATFORM_PKGS=""
        ;;
esac

PKG_CHECK_MODULES([SHELL_CORE],[libxml-2.0 >= $LIBXML_REQUIRED gtk+-3.0 >= $GTK_REQUIRED gio-2.0 >= $GLIB_REQUIRED gmodule-no-export-2.0 >= $GLIB_REQUIRED gthread-2.0 $SHELL_PLATFORM_PKGS])

# *********
# SM client
# *********

GDK_TARGET="$($PKG_CONFIG --variable targets gdk-3.0)"

AC_MSG_CHECKING([which smclient backend to use])
AC_ARG_WITH([smclient],
  [AS_HELP_STRING([--with-smclient-backend=no|xsmp|win32|quartz],
		  [Setting smclient backend (default:auto)])],
  [],
  [case "$GDK_TARGET" in
    x11*) case "$with_platform" in
           gnome) with_smclient=xsmp ;;
           *) with_smclient=no ;;
         esac ;;
    win32|quartz) with_smclient=$GDK_TARGET ;;
    *) with_smclient=no ;;
   esac])
   
AC_MSG_RESULT([$with_smclient])

if test "$with_smclient" != "no"; then
  AC_DEFINE([WITH_SMCLIENT],[1],[Define if smclient is enabled])

  case "$with_smclient" in
    xsmp) SMCLIENT_PKGS="sm >= 1.0.0 ice" ;;
    *) SMCLIENT_PKGS="" ;;
  esac

  PKG_CHECK_MODULES([SMCLIENT],[gtk+-3.0 gthread-2.0 $SMCLIENT_PKGS])
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
LIBS=$evince_save_LIBS

# ******************
# GKT+ Unix Printing
# ******************

AC_MSG_CHECKING([whether gtk+-unix-print support is requested])
AC_ARG_WITH([gtk-unix-print],
  [AS_HELP_STRING([--without-gtk-unix-print],
		  [Disable the use of gtk-unix-print])],
  [],[case "$os_win32" in
        yes) with_gtk_unix_print=no ;;
        *) with_gtk_unix_print=yes ;;
      esac])
      
AC_MSG_RESULT([$with_gtk_unix_print])

if test "$with_gtk_unix_print" = "yes"; then
   PKG_CHECK_MODULES(GTKUNIXPRINT, [gtk+-unix-print-3.0 >= $GTK_REQUIRED])
   AC_DEFINE([GTKUNIXPRINT_ENABLED], [1], [Define if gtk+-unix-print is enabled.])
fi

# *********************
# GNOME Keyring support
# *********************

AC_ARG_WITH(keyring,
        [AS_HELP_STRING([--without-keyring],
			[Disable the use of gnome-keyring])],
        [],
        [case "$with_platform" in
           gnome) with_keyring=yes ;;
           win32) with_keyring=no ;;
         esac])

AM_CONDITIONAL([WITH_KEYRING],[test "$with_keyring" = "yes"])

if test "$with_keyring" = "yes"; then
        PKG_CHECK_MODULES(KEYRING, gnome-keyring-1 >= $KEYRING_REQUIRED)
        AC_DEFINE([WITH_KEYRING],[1],[Define if KEYRING support is enabled])
fi

# ****
# DBUS
# ****

AC_ARG_ENABLE([dbus],
  [AS_HELP_STRING([--disable-dbus], [Disable support for dbus])],
  [],
  [case "$with_platform" in
      gnome) enable_dbus=yes ;;
    esac])

if test "$enable_dbus" = "yes"; then
  AC_DEFINE([ENABLE_DBUS],[1],[Define if DBUS support is enabled])

   PKG_CHECK_MODULES([EV_DAEMON], [gio-2.0 >= $GLIB_REQUIRED])
fi

AM_CONDITIONAL([ENABLE_DBUS], [test "$enable_dbus" = "yes"])

dnl ========= Check for Desktop Schemas
PKG_CHECK_MODULES([DESKTOP_SCHEMAS], [gsettings-desktop-schemas],
                  has_desktop_schemas=yes, has_desktop_schemas=no)
if test x$has_desktop_schemas = xyes; then
   AC_DEFINE([HAVE_DESKTOP_SCHEMAS], [1], [Whether GSettings Desktop Schemas are available])
fi

dnl Debug mode

AC_ARG_ENABLE([debug],
	AS_HELP_STRING([--enable-debug],
		       [Turn on evince debug mode]),
	[enable_debug=$enableval],
	[enable_debug=no])

if test "x$enable_debug" = "xyes"; then
   DEBUG_FLAGS="-DEV_ENABLE_DEBUG"
fi

AC_ARG_ENABLE([tests],
	AS_HELP_STRING([--enable-tests], [Enable the tests]),
	[enable_tests=$enableval],
	[enable_tests=yes])

AM_CONDITIONAL(ENABLE_TESTS, test x$enable_tests = xyes)

LIBDOCUMENT_CFLAGS="$LIBDOCUMENT_CFLAGS $DEBUG_FLAGS"
LIBDOCUMENT_LIBS="$LIBDOCUMENT_LIBS"
AC_SUBST(LIBDOCUMENT_CFLAGS)
AC_SUBST(LIBDOCUMENT_LIBS)

LIBVIEW_CFLAGS="$LIBVIEW_CFLAGS $GTKUNIXPRINT_CFLAGS $DEBUG_FLAGS"
LIBVIEW_LIBS="$LIBVIEW_LIBS $GTKUNIXPRINT_LIBS -lm"
AC_SUBST(LIBVIEW_CFLAGS)
AC_SUBST(LIBVIEW_LIBS)

BACKEND_CFLAGS="$BACKEND_CFLAGS -DGDK_MULTIHEAD_SAFE -DGTK_MULTIHEAD_SAFE $DEBUG_FLAGS"
BACKEND_LIBS="$BACKEND_LIBS -lm"
AC_SUBST(BACKEND_CFLAGS)
AC_SUBST(BACKEND_LIBS)

SHELL_CFLAGS="$SHELL_CORE_CFLAGS $KEYRING_CFLAGS -DGDK_MULTIHEAD_SAFE -DGTK_MULTIHEAD_SAFE $DEBUG_FLAGS"
SHELL_LIBS="$SHELL_CORE_LIBS $KEYRING_LIBS -lz -lm"
AC_SUBST(SHELL_CFLAGS)
AC_SUBST(SHELL_LIBS)

FRONTEND_CFLAGS="$FRONTEND_CORE_CFLAGS $DEBUG_FLAGS"
FRONTEND_LIBS="$FRONTEND_CORE_LIBS -lz"
AC_SUBST(FRONTEND_CFLAGS)
AC_SUBST(FRONTEND_LIBS)

EV_DAEMON_CFLAGS="$EV_DAEMON_CFLAGS $DEBUG_FLAGS"
AC_SUBST([EV_DAEMON_CFLAGS])
AC_SUBST([EV_DAEMON_LIBS])

# Check for Nautilus property page build
AC_ARG_ENABLE([nautilus],
  [AS_HELP_STRING([--disable-nautilus],
		  [Disable build of nautilus extensions])],
  [],
  [case "$with_platform" in
     gnome) enable_nautilus=yes ;;
     *) enable_nautilus=no ;;
    esac])

if test "$enable_nautilus" = "yes" ; then
  PKG_CHECK_MODULES([NAUTILUS],[gtk+-x11-3.0 $MM gthread-2.0 libnautilus-extension >= $NAUTILUS_REQUIRED],
                    [],[AC_MSG_ERROR([libnautilus-extension not found; use --disable-nautilus to disable the nautilus extensions])])
  NAUTILUS_EXTENSION_DIR=`$PKG_CONFIG --variable=extensiondir libnautilus-extension`
  NAUTILUS_EXTENSION_DIR=${NAUTILUS_EXTENSION_DIR#`$PKG_CONFIG --variable=libdir libnautilus-extension`}
  NAUTILUS_EXTENSION_DIR=${NAUTILUS_EXTENSION_DIR#/}
  AC_SUBST(NAUTILUS_EXTENSION_DIR)
  AC_SUBST(NAUTILUS_CFLAGS)
  AC_SUBST(NAUTILUS_LIBS)

  AC_DEFINE([HAVE_NAUTILUS],[1], [defined if you build the nautilus plugin])
fi

AM_CONDITIONAL([ENABLE_NAUTILUS],[test "$enable_nautilus" = "yes"])

# ***************
# Document Viewer
# ***************

AC_ARG_ENABLE(
  [viewer],
  [AS_HELP_STRING([--disable-viewer], [Disable GNOME Document viewer])],
  [],
  [enable_viewer=yes]
)
AM_CONDITIONAL([ENABLE_VIEWER],[test "$enable_viewer" = "yes"])

# ***************
# Thumbnailer
# ***************

AC_ARG_ENABLE([thumbnailer],
  [AS_HELP_STRING([--disable-thumbnailer],
		  [Disable GNOME thumbnailer])],
  [],
  [enable_thumbnailer=yes])

AM_CONDITIONAL([ENABLE_THUMBNAILER],[test "$enable_thumbnailer" = "yes"])

# ***************
# Print Previewer
# ***************

AC_ARG_ENABLE([previewer],
  [AS_HELP_STRING([--disable-previewer],
		  [Disable the GNOME Document Previewer])],
  [],
  [enable_previewer=yes])

if test x$enable_previewer = "xyes" ; then
  PKG_CHECK_MODULES([PREVIEWER],[gtk+-3.0 >= $GTK_REQUIRED gthread-2.0 gio-2.0 >= $GLIB_REQUIRED gmodule-no-export-2.0 >= $GLIB_REQUIRED])
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
        AS_HELP_STRING([--enable-introspection],
		       [Enable GObject introspection]),
       	[enable_introspection=$enableval],
	[enable_introspection=no])
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
AC_ARG_ENABLE([pdf],
  [AS_HELP_STRING([--disable-pdf],
		  [Disable the PDF support])],
  [enable_pdf=$enableval],
  [enable_pdf=yes])

if test "x$enable_pdf" = "xyes"; then
    POPPLER_REQUIRED=0.20.0
    PKG_CHECK_MODULES(POPPLER, poppler-glib >= $POPPLER_REQUIRED libxml-2.0 >= $LIBXML_REQUIRED,enable_pdf=yes,enable_pdf=no)

    if test "x$enable_pdf" = "xyes"; then
	    PKG_CHECK_MODULES(CAIRO_PDF, cairo-pdf, enable_cairo_pdf=yes, enable_cairo_pdf=no)
	    if test x$enable_cairo_pdf = xyes; then
	            AC_DEFINE([HAVE_CAIRO_PDF], [1], [defined if cairo-pdf is available])
            fi

	    PKG_CHECK_MODULES(CAIRO_PS, cairo-ps, enable_cairo_ps=yes, enable_cairo_ps=no)
	    if test x$enable_cairo_ps = xyes; then
	            AC_DEFINE([HAVE_CAIRO_PS], [1], [defined if cairo-ps is available])
            fi
    else
	    AC_MSG_ERROR("PDF support is disabled since poppler-glib library version $POPPLER_REQUIRED or newer not found")
    fi 
    evince_save_LIBS=$LIBS
    LIBS="$LIBS $POPPLER_LIBS"
    AC_CHECK_FUNCS(poppler_document_new_from_stream poppler_document_new_from_gfile)
    LIBS=$evince_save_LIBS
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
  	[AS_HELP_STRING([--disable-ps],
			[Disable the PostScript backend])],
	[enable_ps=$enableval],
	[enable_ps=yes])

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
            [AS_HELP_STRING([--disable-tiff],
			    [Disable the support of multipage tiff])],
	    [enable_tiff=$enableval],
	    [enable_tiff=yes])

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
            [AS_HELP_STRING([--disable-djvu],
			    [Disable the support of djvu viewer])],
	    [enable_djvu=$enableval],
	    [enable_djvu=yes])

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
            [AS_HELP_STRING([--disable-dvi],
	    [Disable the support of dvi viewer])],
	    [enable_dvi=$enableval],
	    [enable_dvi=yes])

AC_ARG_ENABLE(t1lib,
            [AS_HELP_STRING([--enable-t1lib],
	                    [Compile with support of t1lib for type1 fonts in dvi])],
	    [enable_type1_fonts=$enableval],
	    [enable_type1_fonts=no])

if test "x$enable_dvi" = "xyes"; then
    AC_C_CONST
    AC_C_INLINE
    AC_TYPE_SIZE_T
    AC_CHECK_SIZEOF(long, 4)
    AC_CHECK_SIZEOF(int, 4)
    AC_CHECK_SIZEOF(short, 2)
    AC_CHECK_SIZEOF(long long, 4)
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

dnl ================== comic book checks ===================================================
 
AC_ARG_ENABLE(comics,
 	[AS_HELP_STRING([--enable-comics],
	                [Compile with support for comic book archives])],
	[enable_comics=$enableval],
	[enable_comics=yes])
	
if test "x$enable_comics" = "xyes"; then
	AC_DEFINE([ENABLE_COMICS], [1], [Enable support for comics.])
fi
AM_CONDITIONAL(ENABLE_COMICS, test x$enable_comics = xyes)

dnl ================== End of comic book checks ============================================

dnl ================== XPS checks ===================================================

AC_ARG_ENABLE(xps,
	[AS_HELP_STRING([--enable-xps],
			[Compile with support for XPS documents.])],
	[enable_xps=$enableval],
	[enable_xps=yes])

if test "x$enable_xps" = "xyes"; then
   GXPS_REQUIRED=0.2.1
   PKG_CHECK_MODULES(GXPS, libgxps >= $GXPS_REQUIRED,enable_xps=yes,enable_xps=no)

   if test "x$enable_xps" = "xyes"; then
      AC_DEFINE([ENABLE_XPS], [1], [Enable support for XPS documents.])
   else
      enable_xps="no"
      AC_MSG_WARN(["XPS support is disabled since libgxps (version >= $GXPS_REQUIRED) is needed])
   fi
fi

AM_CONDITIONAL(ENABLE_XPS, test x$enable_xps = xyes)

dnl ================== End of XPS checks ===================================================

dnl =================== Mime types list ====================================================

if test "x$enable_pdf" = "xyes" ; then
	EVINCE_MIME_TYPES="application/pdf;application/x-bzpdf;application/x-gzpdf;application/x-xzpdf;"
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
	EVINCE_MIME_TYPES="${EVINCE_MIME_TYPES}application/x-cbr;application/x-cbz;application/x-cb7;application/x-cbt;"
fi
if test "x$enable_xps" = "xyes"; then
   	EVINCE_MIME_TYPES="${EVINCE_MIME_TYPES}application/oxps;application/vnd.ms-xpsdocument;"
fi
AC_SUBST(EVINCE_MIME_TYPES)

AC_CHECK_FUNC(localtime_r, AC_DEFINE(HAVE_LOCALTIME_R, 1, [Defines if localtime_r is available on your system]))

# *****************
# Help files
# *****************

GNOME_DOC_INIT([], gnome_doc_utils=yes, gnome_doc_utils=no)

AC_ARG_ENABLE(help,
    [AS_HELP_STRING([--disable-help], [Disable offline help files])],
    [enable_help=$enableval],
    [enable_help=yes])

if test "x$enable_help" = "xyes" && test "x$os_win32" = "xyes" ; then
    AC_MSG_WARN([Offline help is disabled (not implemented on Windows).])
    enable_help=no
fi

if test "x$enable_help" = "xyes" && test "x$gnome_doc_utils" == "xno"; then
    AC_MSG_ERROR(
       [gnome-doc-utils not found; use --disable-help to disable help files]
    )
fi

if test "x$enable_help" = "xyes"; then
    AC_DEFINE([OFFLINE_HELP_ENABLED], [1], [Enable offline help])
fi

AM_CONDITIONAL(ENABLE_HELP, test "x$enable_help" = "xyes")

# *****************
# API documentation
# *****************

GTK_DOC_CHECK([1.13],[--flavour no-tmpl])

AC_SUBST([GLIB_PREFIX],[$($PKG_CONFIG --variable=prefix glib-2.0)])
AC_SUBST([GTK_PREFIX],[$($PKG_CONFIG --variable=prefix gtk+-3.0)])

# ******************
# Backends directory
# ******************

AC_SUBST([backenddir],"\$(libdir)/evince/ev_binary_version/backends")
AC_SUBST([backend_binary_version],"ev_binary_version")

# **********
# Versioning
# **********

AC_SUBST([EV_MAJOR_VERSION],[ev_major_version])
AC_SUBST([EV_MINOR_VERSION],[ev_minor_version])
AC_SUBST([EV_MICRO_VERSION],[ev_micro_version])

AC_SUBST([EV_API_VERSION],[ev_api_version])
AC_SUBST([EV_API_VERSION_U],[AS_TR_SH([ev_api_version])])
AC_SUBST([EV_BINARY_VERSION],[ev_binary_version])

AC_SUBST([EV_DOCUMENT_LT_VERSION_INFO],[ev_document_lt_version_info])
AC_SUBST([EV_DOCUMENT_LT_CURRENT_MINUS_AGE],[ev_document_lt_current_minus_age])
AC_SUBST([EV_VIEW_LT_VERSION_INFO],[ev_view_lt_version_info])
AC_SUBST([EV_VIEW_LT_CURRENT_MINUS_AGE],[ev_view_lt_current_minus_age])

# *****************************************************************************
# *****************************************************************************

AC_CONFIG_FILES([
backend/Makefile
backend/comics/Makefile
backend/djvu/Makefile
backend/dvi/Makefile
backend/dvi/mdvi-lib/Makefile
backend/pdf/Makefile
backend/ps/Makefile
backend/tiff/Makefile
backend/xps/Makefile
cut-n-paste/Makefile
cut-n-paste/gimpcellrenderertoggle/Makefile
cut-n-paste/smclient/Makefile
cut-n-paste/toolbar-editor/Makefile
cut-n-paste/zoom-control/Makefile
cut-n-paste/totem-screensaver/Makefile
cut-n-paste/synctex/Makefile
data/evince.desktop.in
data/Makefile
data/icons/Makefile
data/icons/16x16/Makefile
data/icons/16x16/apps/Makefile
data/icons/16x16/actions/Makefile
data/icons/16x16/mimetypes/Makefile
data/icons/22x22/Makefile
data/icons/22x22/apps/Makefile
data/icons/22x22/actions/Makefile
data/icons/22x22/mimetypes/Makefile
data/icons/24x24/Makefile
data/icons/24x24/apps/Makefile
data/icons/24x24/actions/Makefile
data/icons/24x24/mimetypes/Makefile
data/icons/32x32/Makefile
data/icons/32x32/actions/Makefile
data/icons/32x32/mimetypes/Makefile
data/icons/48x48/Makefile
data/icons/48x48/apps/Makefile
data/icons/48x48/actions/Makefile
data/icons/256x256/Makefile
data/icons/256x256/apps/Makefile
data/icons/scalable/Makefile
data/icons/scalable/actions/Makefile
data/icons/scalable/mimetypes/Makefile
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

AC_MSG_NOTICE([

Evince configure summary
========================

Platform .................:  $with_platform
Debug mode ...............:  $enable_debug
Tests ....................:  $enable_tests


FRONTENDS

Viewer ...................:  $enable_viewer
Previewer ................:  $enable_previewer
Thumbnailer ..............:  $enable_thumbnailer
Nautilus Extensions.......:  $enable_nautilus


BACKENDS

PDF ......................:  $enable_pdf
PostScript ...............:  $enable_ps
TIFF .....................:  $enable_tiff
DJVU .....................:  $enable_djvu
DVI ......................:  $enable_dvi
Comics ...................:  $enable_comics
XPS ......................:  $enable_xps


FEATURES

Help .....................:  $enable_help
GTK Doc reference ........:  $enable_gtk_doc
GObject Introspection ....:  $enable_introspection
DBUS communication .......:  $enable_dbus
Keyring integration ......:  $with_keyring
Session Manager client ...:  $with_smclient
GTK+ Unix Print ..........:  $with_gtk_unix_print

])
