dnl Process this file with autoconf to produce a configure script.

AC_PREREQ(2.59)
AC_INIT(evince, 0.1.0)
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

ALL_LINGUAS="de"

AM_GLIB_GNU_GETTEXT

GETTEXT_PACKAGE=AC_PACKAGE_NAME
AC_SUBST(GETTEXT_PACKAGE)
AC_DEFINE_UNQUOTED(GETTEXT_PACKAGE,"$GETTEXT_PACKAGE", [Gettext package.])

PKG_CHECK_MODULES(LIBEVPRIVATE, gtk+-2.0 >= 2.4.0)
PKG_CHECK_MODULES(RECENT_FILES, gtk+-2.0 >= 2.4.0 libgnomeui-2.0 >= 2.4.0 libbonobo-2.0)
PKG_CHECK_MODULES(SHELL, gtk+-2.0 >= 2.5.0 libgnomeui-2.0 gnome-vfs-2.0 libbonoboui-2.0 libgnomeprint-2.2 libgnomeprintui-2.2 libglade-2.0)

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

dnl ==========================================================================

AC_OUTPUT([
Makefile
cut-n-paste/Makefile
cut-n-paste/recent-files/Makefile
data/Makefile
po/Makefile.in
shell/Makefile
viewer/Makefile
])
