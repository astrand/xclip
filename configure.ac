AC_INIT(xclip, 0.13)

AC_CONFIG_SRCDIR([xclip.c])

AC_PROG_CC
if test "$GCC" = yes; then
    CFLAGS="$CFLAGS -Wall"
fi

AC_PROG_INSTALL
AC_LANG_C
AC_HEADER_STDC
AC_PATH_XTRA
AC_CHECK_TOOL(STRIP, strip, :)
AC_CHECK_HEADER([X11/Xmu/Atoms.h], [], AC_MSG_ERROR([*** X11/Xmu/Atoms.h is missing ***]))
AC_CHECK_HEADER([X11/Intrinsic.h], [], AC_MSG_ERROR([*** X11/Intrinsic.h is missing ***]))
AC_CHECK_HEADER([iconv.h],
    AC_SEARCH_LIBS([iconv], [iconv],
        AC_DEFINE([HAVE_ICONV]), []), [])
AC_CHECK_LIB(Xmu, XmuClientWindow, [], AC_MSG_ERROR([*** libXmu not found ***]))

AC_OUTPUT(Makefile)

