dnl Process this file with autoconf to produce a configure script.

AC_PREREQ(2.59)
AC_INIT(evince, 0.1.7)
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

GNOME_DEBUG_CHECK

ALL_LINGUAS="bg ca cs da de el en_CA en_GB fi fr ja no nb nl pt_BR sv zh_CN zh_TW"

AM_GLIB_GNU_GETTEXT

GETTEXT_PACKAGE=AC_PACKAGE_NAME
AC_SUBST(GETTEXT_PACKAGE)
AC_DEFINE_UNQUOTED(GETTEXT_PACKAGE,"$GETTEXT_PACKAGE", [Gettext package.])

PKG_CHECK_MODULES(LIBEVPRIVATE, gtk+-2.0 >= 2.4.0)
PKG_CHECK_MODULES(RECENT_FILES, gtk+-2.0 >= 2.4.0 libgnomeui-2.0 >= 2.4.0)
PKG_CHECK_MODULES(SHELL, gtk+-2.0 >= 2.6.0 libgnomeui-2.0 gnome-vfs-2.0 libgnomeprint-2.2 libgnomeprintui-2.2 libglade-2.0 gconf-2.0 poppler >= 0.1.1)
PKG_CHECK_MODULES(THUMBNAILER, gtk+-2.0 >= 2.6.0 gnome-vfs-2.0 poppler >= 0.1.1)
PKG_CHECK_MODULES(DVI, gtk+-2.0 >= 2.6.0)
PKG_CHECK_MODULES(GTK, gtk+-2.0 >= 2.6.0)
PKG_CHECK_MODULES(PS, gtk+-2.0 >= 2.6.0 gnome-vfs-2.0 libgnomeui-2.0)
PKG_CHECK_MODULES(POPPLER, poppler >= 0.1.1)

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
pixbuf/Makefile
ps/Makefile
po/Makefile.in
backend/Makefile
shell/Makefile
thumbnailer/Makefile
])
