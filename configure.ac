dnl Process this file with autoconf to produce a configure script.

AC_PREREQ(2.59)
AC_INIT(evince, 0.1.4)
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
AC_PROG_RANLIB
AC_PROG_INTLTOOL

GNOME_DEBUG_CHECK

ALL_LINGUAS="ca cs da de en_CA en_GB ja no nb nl pt_BR sv zh_CN"

AM_GLIB_GNU_GETTEXT

GETTEXT_PACKAGE=AC_PACKAGE_NAME
AC_SUBST(GETTEXT_PACKAGE)
AC_DEFINE_UNQUOTED(GETTEXT_PACKAGE,"$GETTEXT_PACKAGE", [Gettext package.])

PKG_CHECK_MODULES(LIBEVPRIVATE, gtk+-2.0 >= 2.4.0)
PKG_CHECK_MODULES(RECENT_FILES, gtk+-2.0 >= 2.4.0 libgnomeui-2.0 >= 2.4.0)
PKG_CHECK_MODULES(SHELL, gtk+-2.0 >= 2.6.0 libgnomeui-2.0 gnome-vfs-2.0 libgnomeprint-2.2 libgnomeprintui-2.2 libglade-2.0 gconf-2.0)
PKG_CHECK_MODULES(DVI, gtk+-2.0 >= 2.6.0)
PKG_CHECK_MODULES(GTK, gtk+-2.0 >= 2.6.0)
PKG_CHECK_MODULES(PS, gtk+-2.0 >= 2.6.0 gnome-vfs-2.0 libgnomeui-2.0)

GLIB_GENMARSHAL=`$PKG_CONFIG --variable=glib_genmarshal glib-2.0`  
AC_SUBST(GLIB_GENMARSHAL)

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

AM_GCONF_SOURCE_2

AC_PATH_PROG([GCONFTOOL], [gconftool-2], [no])
if test "x$GCONFTOOL" = "xno"; then
	AC_MSG_ERROR([gconftool-2 executable not found in your path - should be installed with GConf])
fi

dnl ================== Xpdf aconf.h.in checks ==================================

AH_TEMPLATE([HAVE_LIBSM], [Define to 1 if you have session management.])
AH_TEMPLATE([ENABLE_NLS], [Use Native Language Support.])
AH_TEMPLATE([HAVE_CATGETS], [Use catgets functions.])
AH_TEMPLATE([HAVE_GETTEXT], [Use gettext functions.])

dnl Optional features.
AH_TEMPLATE([A4_PAPER],
            [Use A4 paper size instead of Letter for PostScript output.])
AC_ARG_ENABLE(a4-paper,
              AC_HELP_STRING([--enable-a4-paper],
                             [use A4 paper size instead of Letter for 
                              PostScript output]),
              AC_DEFINE(A4_PAPER))

dnl never define NO_TEXT_SELECT, just remember this Xpdf setting.
AH_TEMPLATE([NO_TEXT_SELECT], [Do not allow text selection.])

AH_TEMPLATE([OPI_SUPPORT],
            [Include support for OPI comments.])
AC_ARG_ENABLE(opi,
              AC_HELP_STRING([--enable-opi],
                             [include support for OPI comments]),
              AC_DEFINE(OPI_SUPPORT))

dnl enable these unconditionally.
AC_DEFINE([MULTITHREADED], [1], [Enable multithreading support.])
AC_DEFINE([TEXTOUT_WORD_LIST], [1], [Enable word list support.])
	      
AH_TEMPLATE([APPDEFDIR],
            [Directory with the Xpdf app-defaults file.])
AC_ARG_WITH(appdef-dir,
            AC_HELP_STRING([--with-appdef-dir],
                           [set app-defaults directory]),
            AC_DEFINE_UNQUOTED(APPDEFDIR, "$with_appdef_dir"))

dnl Path to xpdfrc.
dnl This ugly kludge to get the sysconfdir path is needed because
dnl autoconf doesn't actually set the prefix variable until later.
if test "$sysconfdir" = '${prefix}/etc'; then
  if test "x$prefix" = xNONE; then
    system_xpdfrc="$ac_default_prefix/etc/xpdfrc"
  else
    system_xpdfrc="$prefix/etc/xpdfrc"
  fi
else
  system_xpdfrc="$sysconfdir/xpdfrc"
fi
AC_DEFINE_UNQUOTED(SYSTEM_XPDFRC, "$system_xpdfrc",
                   [Full path for the system-wide xpdfrc file.])

dnl Checks for header files.
AC_HEADER_DIRENT

dnl Switch over to C++.  This will make the checks below a little
dnl bit stricter (requiring function prototypes in include files).
dnl (99% of xpdf is written in C++.)
AC_LANG_CPLUSPLUS

dnl Look for header that defines select() and fd_set.
AC_MSG_CHECKING([select() and fd_set in sys/select.h and sys/bsdtypes.h])
AC_TRY_COMPILE([#include <stdlib.h>
#include <stddef.h>
#include <unistd.h>
#include <sys/types.h>],
  [fd_set fds;
select(0, NULL, NULL, NULL, NULL);], xpdf_ok=yes, xpdf_ok=no)
if test $xpdf_ok = yes; then
  AC_MSG_RESULT([not needed])
else
  AC_TRY_COMPILE([#include <stdlib.h>
#include <stddef.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/select.h>],
    [fd_set fds;
select(0, NULL, NULL, NULL, NULL);], xpdf_ok=yes, xpdf_ok=no)
  if test $xpdf_ok = yes; then
    AC_DEFINE(HAVE_SYS_SELECT_H, 1, [Have sys/select.h.])
    AC_MSG_RESULT([need sys/select.h])
  else
    AC_TRY_COMPILE([#include <stdlib.h>
#include <stddef.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/bsdtypes.h>],
      [fd_set fds;
select(0, NULL, NULL, NULL, NULL);], xpdf_ok=yes, xpdf_ok=no)
    if test $xpdf_ok = yes; then
      AC_DEFINE(HAVE_SYS_BSDTYPES_H, 1, [Have sys/bsdtypes.h.])
      AC_MSG_RESULT([need sys/bsdtypes.h])
    else
      AC_MSG_RESULT([problem])
    fi
  fi
fi

dnl Look for header that defines FD_ZERO.
AC_MSG_CHECKING([FD_ZERO and strings.h or bstring.h])
AC_TRY_COMPILE([#include <stdlib.h>
#include <sys/types.h>
#ifdef HAVE_SYS_SELECT_H
#include <sys/select.h>
#endif],
[fd_set fds; FD_ZERO(&fds);], xpdf_ok=yes, xpdf_ok=no)
if test $xpdf_ok = yes; then
  AC_MSG_RESULT([not needed])
else
  AC_TRY_COMPILE([#include <stdlib.h>
#include <sys/types.h>
#include <strings.h>
#ifdef HAVE_SYS_SELECT_H
#include <sys/select.h>
#endif],
    [fd_set fds; FD_ZERO(&fds);], xpdf_ok=yes, xpdf_ok=no)
  if test $xpdf_ok = yes; then
    AC_DEFINE(HAVE_STRINGS_H, 1, [Have strings.h.])
    AC_MSG_RESULT([need strings.h])
  else
    AC_TRY_COMPILE([#include <stdlib.h>
#include <sys/types.h>
#include <bstring.h>
#ifdef HAVE_SYS_SELECT_H
#include <sys/select.h>
#endif],
      [fd_set fds; FD_ZERO(&fds);], xpdf_ok=yes, xpdf_ok=no)
    if test $xpdf_ok = yes; then
      AC_DEFINE(HAVE_BSTRING_H, 1, [Have bstring.h.])
      AC_MSG_RESULT([need bstring.h])
    else
      AC_MSG_RESULT([problem])
    fi
  fi
fi

dnl Look for rewinddir.
AC_CHECK_FUNCS(rewinddir)
if test $ac_cv_func_rewinddir = no; then
  AC_CHECK_LIB(cposix, rewinddir)
fi

dnl Checks for library functions.
AC_CHECK_FUNCS(popen)
dnl # This should use 'AC_CHECK_FUNCS(mkstemp)' but that fails if
dnl # the mkstemp exists in the library but isn't declared in the
dnl # include file (e.g., in cygwin 1.1.2).
AC_CACHE_CHECK([for mkstemp],
xpdf_cv_func_mkstemp,
[AC_TRY_LINK([#include <stdlib.h>
#include <unistd.h>],
[mkstemp("foo");],
xpdf_cv_func_mkstemp=yes, xpdf_cv_func_mkstemp=no)])
if test "$xpdf_cv_func_mkstemp" = yes; then
  AC_DEFINE(HAVE_MKSTEMP, 1, [Have mkstemp().])
fi
dnl Check for mkstemps, just like mkstemp.
AC_CACHE_CHECK([for mkstemps],
xpdf_cv_func_mkstemps,
[AC_TRY_LINK([#include <stdlib.h>
#include <unistd.h>],
[mkstemps("foo", 0);],
xpdf_cv_func_mkstemps=yes, xpdf_cv_func_mkstemps=no)])
if test "$xpdf_cv_func_mkstemps" = yes; then
  AC_DEFINE(HAVE_MKSTEMPS, 1, [Have mkstemps().])
fi

dnl Check select argument type: on HP-UX before version 10, select
dnl takes (int *) instead of (fd_set *).
AC_CACHE_CHECK([whether select takes fd_set arguments],
xpdf_cv_func_select_arg,
[AC_TRY_COMPILE([#include <sys/types.h>
#include <sys/time.h>
#include <unistd.h>
#ifdef HAVE_SYS_SELECT_H
#include <sys/select.h>
#endif],
[fd_set fds;
select(1, &fds, &fds, &fds, 0);],
xpdf_cv_func_select_arg=yes, xpdf_cv_func_select_arg=no)])
if test "$xpdf_cv_func_select_arg" != yes; then
  AC_DEFINE(SELECT_TAKES_INT, 1, [select() takes int, not fd_set arguments.])
fi

dnl Back to C for the library tests.
AC_LANG_C

dnl Check for fseeko/ftello or fseek64/ftell64
dnl The LARGEFILE and FSEEKO macros have to be called in C, not C++, mode.
AC_SYS_LARGEFILE
AC_FUNC_FSEEKO
AC_CHECK_FUNCS(fseek64, xpdf_cv_func_fseek64=yes, xpdf_cv_func_fseek64=no)
AC_CHECK_FUNCS(ftell64, xpdf_cv_func_ftell64=yes, xpdf_cv_func_ftell64=no)
if test "$xpdf_cv_func_fseek64" = yes -a "$xpdf_cv_func_ftell64" = yes; then
  AC_DEFINE(HAVE_FSEEK64, 1, [Have fseek64().])
fi

dnl Check for freetype headers
FREETYPE_LIBS=
FREETYPE_CFLAGS=
AC_PATH_PROG(FREETYPE_CONFIG, freetype-config, no)
if test "x$FREETYPE_CONFIG" != "xno" ; then
  FREETYPE_CFLAGS=`$FREETYPE_CONFIG --cflags`
  FREETYPE_LIBS=`$FREETYPE_CONFIG --libs`
  AC_DEFINE(HAVE_FREETYPE_H, 1, [Have FreeType2 include files])

  vers=`$FREETYPE_CONFIG --version 2>/dev/null | sed -e 's/libfreetype //' | awk 'BEGIN { FS = "."; } { printf "%d", ($1 * 1000 + $2) * 1000 + $3;}'`
  if test -n "$vers" && test "$vers" -le 9005003; then
    AC_DEFINE_UNQUOTED(HAVE_FREETYPE_217_OR_OLDER, 1, [Defines if your system has the freetype library 2.1.7 or older])
  else
    AC_DEFINE_UNQUOTED(HAVE_FREETYPE_217_OR_OLDER, 0, [Defines if your system has the freetype library 2.1.7 or older])
  fi

fi

AC_SUBST(FREETYPE_CFLAGS)
AC_SUBST(FREETYPE_LIBS)

dnl ================== End of xpdf checks ===========================================

dnl ================== ggv checks ===================================================
AC_ARG_WITH(gs-pkg,
            [  --with-gs=dir       Directory Where GhostScript package is installed.])

if test "x$with_gs" = "x"; then
        AC_PATH_PROG(GS_PROG, gs)
        if test -z "$GS_PROG"; then
                AC_MSG_ERROR(Unable to find GhostScript in the PATH. Provide the full path for GhostScript(--with-gs=PATH). You need to have Ghostscript installed in order to run GGV)
        fi
else
        GS_PROG=$with_gs
fi

AC_DEFINE_UNQUOTED(GS_PATH, "$GS_PROG", [Path to the 'gs' executable.])


dnl check for GS version and define appropriate alpha parameters
AC_MSG_CHECKING(for Ghostscript version...)
GS_VERSION=`$GS_PROG --version | head -n 1`
AC_MSG_RESULT(found $GS_VERSION)
dnl GS tends to get a lot of BadMatch errors with the *AlphaBits parameters
dnl so I've commented their use out
dnl if test "$GS_VERSION" -gt "5"; then
dnl     AA_PARMS="-sDEVICE=x11 -dTextAlphaBits=4 -dGraphicsAlphaBits=4"
dnl else
dnl     AA_PARMS="-sDEVICE=x11alpha"
dnl fi
AA_PARMS="-sDEVICE=x11alpha -dNOPLATFONTS"
AC_DEFINE_UNQUOTED(ALPHA_PARAMS, "$AA_PARMS", [Anti-aliasing parameters for Ghostscript.])
AC_MSG_RESULT(Antialiasing parameters for Ghostscript: $AA_PARMS)


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

dnl ======================== End of ggv checks =================================

AC_OUTPUT([
Makefile
cut-n-paste/Makefile
cut-n-paste/recent-files/Makefile
data/Makefile
lib/Makefile
pdf/Makefile
pdf/goo/Makefile
pdf/fofi/Makefile
pdf/splash/Makefile
pdf/xpdf/Makefile
pixbuf/Makefile
ps/Makefile
po/Makefile.in
backend/Makefile
shell/Makefile
])
