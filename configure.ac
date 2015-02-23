dnl Process this file with autoconf to produce a configure script

AC_INIT([gtkcomm], [0.8.2])
AC_CONFIG_SRCDIR([src/pcSetting.h])
AM_INIT_AUTOMAKE
AM_CONFIG_HEADER(config.h)
AM_MAINTAINER_MODE

AC_PROG_CC
AC_ISC_POSIX
AC_PROG_CPP
AC_STDC_HEADERS
AC_PROG_INSTALL
AC_PROG_LN_S
AC_PROG_MAKE_SET
AC_PROG_RANLIB

dnl Checks for typedefs, structures, and compiler characteristics.
AC_C_CONST
AC_TYPE_OFF_T
AC_TYPE_PID_T
AC_STRUCT_TM

GETTEXT_PACKAGE="gtkcomm"
AC_SUBST(GETTEXT_PACKAGE)
AC_DEFINE_UNQUOTED(GETTEXT_PACKAGE,"$GETTEXT_PACKAGE", "gettext pkg")

ALL_LINGUAS="ko"
AM_GLIB_GNU_GETTEXT
LIBS="$LIBS $INTLLIBS"

dnl Checks for library functions.
AC_TYPE_SIGNAL
AC_CHECK_FUNCS(setenv putenv)
AC_CHECK_FUNCS(connect,,[AC_CHECK_LIB(socket,connect)])

PKG_CHECK_MODULES(GLIB, glib-2.0 >= 2.0.6)
PKG_CHECK_MODULES(GTK, gtk+-2.0 >= 2.0.6)

AC_SUBST(GLIB_CFLAGS)
AC_SUBST(GLIB_LIBS)
AC_SUBST(GTK_CFLAGS)
AC_SUBST(GTK_LIBS)

AC_CHECK_LIB(util, openpty,
    [ UTIL_LIBS="-lutil"],
    [ echo ;
      echo "***************************************************************";
      echo "* Warning: Cannot find the util libraries, I guess you do not *";
      echo "* have glibc2. Upgrade your system. Anyway I'll try to use    *";
      echo "* gnomesupport library without checking.                      *";
      echo "***************************************************************";
      echo "sleep 3 seconds";
      sleep 3;
      UTIL_LIBS="-lgnomesupport"])

# Stealed from GTK+'s configure.in
AC_MSG_CHECKING(if user want to debug)
AC_ARG_ENABLE(debug,
              [  --enable-debug=[no/minimum/yes] turn on debugging [default=minimum]],,
	      enable_debug=minimum)

if test "x$enable_debug" = "xyes"; then
  test "$cflags_set" = set || CFLAGS="$CFLAGS -g"
  CFLAGS="$CFLAGS -DG_ENABLE_DEBUG -D_DEBUG_"
  CFLAGS=`echo "$CFLAGS" | sed 's/-fomit-frame-pointer//'`
  if test "x$CC" = "xgcc"; then
    # NOTE: -Wshadow -Wstrict-prototypes
    #       를 추가하고 싶지만 gtk+ header 쪽에서 경고가 많이 나와서...
    CFLAGS="$CFLAGS -W -Wall -Wparentheses"
  fi
else
  if test "x$enable_debug" = "xno"; then
    CFLAGS="$CFLAGS -DG_DISABLE_ASSERT -DG_DISABLE_CHECKS -DGTK_NO_CHECK_CASTS"
  else
    CFLAGS="$CFLAGS -DGTK_NO_CHECK_CASTS"
  fi
fi
AC_MSG_RESULT($enable_debug)

AC_CACHE_CHECK(if C compiler has \"__FILE__\" macro,
ac_cv_c_has_file_macro,
[AC_TRY_COMPILE([#include <stdio.h>],[printf("%s", __FILE__);],
  ac_cv_c_has_file_macro=yes,
  ac_cv_c_has_file_macro=no) ] )
if test "$ac_cv_c_has_file_macro" = "no" ; then
  AC_DEFINE(__FILE__, "FILE", [Define if compiler does not support __FILE__])
fi

AC_CACHE_CHECK(if C compiler has \"__LINE__\" macro,
ac_cv_c_has_line_macro,
[AC_TRY_COMPILE([#include <stdio.h>],[printf("%s", __LINE__);],
  ac_cv_c_has_line_macro=yes,
  ac_cv_c_has_line_macro=no) ] )
if test "$ac_cv_c_has_line_macro" = "no" ; then
  AC_DEFINE(__LINE__, "LINE", [Define if compiler does not support __LINE__])
fi

AC_CACHE_CHECK(if C compiler has \"__FUNCTION__\" macro,
ac_cv_c_has_function_macro,
[AC_TRY_COMPILE([#include <stdio.h>],[printf("%s", __FUNCTION__);],
  ac_cv_c_has_function_macro=yes,
  ac_cv_c_has_function_macro=no) ] )
if test "$ac_cv_c_has_function_macro" = "no" ; then
  AC_DEFINE(__FUNCTION__, "FUNCTION",
	    [Define if compiler does not support __FUNCTION__])
fi

AC_SUBST(UTIL_LIBS)

ACLOCAL="$ACLOCAL $ACLOCAL_FLAGS"

PKGDATADIR="${datarootdir}/${PACKAGE}"
BISON_DEFINE_FILE(PKGDATADIR, PKGDATADIR)

AC_SUBST(PACKAGE)
AC_SUBST(VERSION)
AC_DEFINE_UNQUOTED(PACKAGE, "$PACKAGE", "PACKAGE")
AC_DEFINE_UNQUOTED(VERSION, "$VERSION", "VERSION")

AC_OUTPUT([
Makefile
gtkcomm.spec
po/Makefile.in
src/Makefile
res/Makefile
res/gtkrc
res/script/startup.scr
doc/Makefile
])

echo "
Now type:
   make; make install
to compile and install GTK Communication Program.
"

# vim:set ts=8 sts=2 sw=2 tw=78 noet:
# Local Variables:
# mode: m4
# End:
