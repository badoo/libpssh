AC_INIT(libpssh, [0.1], rushba@eyelinkmedia.com)
AC_CONFIG_SRCDIR([src])
AC_CONFIG_HEADER([src/pssh_config.h])

AM_INIT_AUTOMAKE([foreign])

AC_PROG_CC
AC_PROG_LIBTOOL

dnl check for how to do large files
AC_SYS_LARGEFILE
dnl ************************************************************
dnl option to switch on compiler debug options
dnl
AC_MSG_CHECKING([whether to enable pedantic and debug compiler options])
AC_ARG_ENABLE(debug,
AC_HELP_STRING([--enable-debug],[Enable pedantic and debug options])
AC_HELP_STRING([--disable-debug],[Disable debug options]),
[ case "$enableval" in
  no)
       AC_MSG_RESULT(no)
       ;;
  *)   AC_MSG_RESULT(yes)

    CPPFLAGS="$CPPFLAGS -DPSSHDEBUG -O0"
    CFLAGS="$CFLAGS -g3 -O0"

    dnl set compiler "debug" options to become more picky, and remove
    dnl optimize options from CFLAGS
    CURL_CC_DEBUG_OPTS
       ;;
  esac
 ],
       AC_MSG_RESULT(no)
)

DEFAULT_INSTALL_PREFIX="/usr/local"
STANDARD_PREFIXES="/usr /usr/local /opt /local"

dnl PSSH_ADD_LIBPATH(/path/to/lib, name) {{{
AC_DEFUN([PSSH_ADD_LIBPATH],[
    test -n "$ld_runpath_switch" && LDFLAGS="$LDFLAGS $ld_runpath_switch$1"

    LDFLAGS="$LDFLAGS -L$1"
    LIBS="$LIBS -l$2"
])
dnl }}}

dnl PSSH_RUNPATH_SWITCH {{{
AC_DEFUN([PSSH_RUNPATH_SWITCH],[
AC_MSG_CHECKING([if compiler supports -R])
AC_CACHE_VAL(pssh_cv_cc_dashr,[
  SAVE_LIBS=$LIBS
  LIBS="-R /usr/$LIBDIR $LIBS"
  AC_LINK_IFELSE([AC_LANG_PROGRAM([[]], [[]])],[pssh_cv_cc_dashr=yes],[pssh_cv_cc_dashr=no])
  LIBS=$SAVE_LIBS])
AC_MSG_RESULT([$pssh_cv_cc_dashr])
if test $pssh_cv_cc_dashr = "yes"; then
  ld_runpath_switch=-R
else
  AC_MSG_CHECKING([if compiler supports -Wl,-rpath,])
  AC_CACHE_VAL(pssh_cv_cc_rpath,[
    SAVE_LIBS=$LIBS
    LIBS="-Wl,-rpath,/usr/$LIBDIR $LIBS"
    AC_LINK_IFELSE([AC_LANG_PROGRAM([[]], [[]])],[pssh_cv_cc_rpath=yes],[pssh_cv_cc_rpath=no])
    LIBS=$SAVE_LIBS])
  AC_MSG_RESULT([$pssh_cv_cc_rpath])
  if test $pssh_cv_cc_rpath = "yes"; then
    ld_runpath_switch=-Wl,-rpath,
  else
    dnl something innocuous
    ld_runpath_switch=-L
  fi
fi
])
dnl }}}

PSSH_RUNPATH_SWITCH

dnl {{{ --with-libdir
AC_ARG_WITH(libdir,
  [AC_HELP_STRING(
    [--with-libdir],
    [look for libraries in .../NAME rather than .../lib])
  ],
  [LIBDIR=$with_libdir],
  [LIBDIR=lib]
)
dnl }}}

dnl {{{ --with-event
AC_ARG_WITH(event,
  [AC_HELP_STRING(
    [--with-event],
    [specify libevent install prefix])
  ],
  [ ],
  [with_event=yes]
)

if test "x$with_event" = "xno"; then
    AC_MSG_ERROR([can't continue without libevent])
else
    AC_MSG_CHECKING([libevent install prefix])

    if test "x$with_event" = "xyes"; then
        for i in `echo "$STANDARD_PREFIXES"`; do
            if test -f "$i/include/event.h"; then
                LIBEVENT_DIR="$i"
                break;
            fi
        done
    else
        if test -f "$with_event/include/event.h"; then
            LIBEVENT_DIR="$with_event"
            break;
        else
            AC_MSG_ERROR([Can't find libevent headers under $with_event directory]);
        fi
    fi

    if test "x$LIBEVENT_DIR" = "x"; then
        AC_MSG_ERROR([Unable to locate libevent headers, please use --with-event=<DIR>]);
    fi

    AC_MSG_RESULT([$LIBEVENT_DIR])
    PSSH_ADD_LIBPATH([$LIBEVENT_DIR/$LIBDIR], [event])

    AC_CHECK_LIB(
      [event],
      [evdns_init],
      [AC_DEFINE([HAVE_LIBEVENT],[1],[Define if you have libevent])],
      [AC_MSG_ERROR([libevent not found. May be older version? Check config.log for more details])]
    )

    CFLAGS="$CFLAGS -I$LIBEVENT_DIR/include"
fi
dnl }}}

dnl {{{ --with-ssh2
AC_ARG_WITH(ssh2,
  [AC_HELP_STRING(
    [--with-ssh2],
    [specify libssh2 install prefix])
  ],
  [ ],
  [with_ssh2=yes]
)

if test "x$with_ssh2" = "xno"; then
    AC_MSG_ERROR([can't continue without libssh2])
else
    AC_MSG_CHECKING([libssh2 install prefix])

    if test "x$with_ssh2" = "xyes"; then
        for i in `echo "$STANDARD_PREFIXES"`; do
            if test -f "$i/include/libssh2.h"; then
                LIBSSH2_DIR="$i"
                break;
            fi
        done
    else
        if test -f "$with_ssh2/include/libssh2.h"; then
            LIBSSH2_DIR="$with_ssh2"
            break;
        else
            AC_MSG_ERROR([Can't find libssh2 headers under $with_ssh2 directory]);
        fi
    fi

    if test "x$LIBSSH2_DIR" = "x"; then
        AC_MSG_ERROR([Unable to locate libssh2 headers, please use --with-ssh2=<DIR>]);
    fi

    AC_MSG_RESULT([$LIBSSH2_DIR])

    PSSH_ADD_LIBPATH([$LIBSSH2_DIR/$LIBDIR], [ssh2])
    AC_CHECK_LIB(
      [ssh2],
      [libssh2_session_init_ex],
      [AC_DEFINE([HAVE_LIBEVENT],[1],[Define if you have libevent])],
      [AC_MSG_ERROR([libssh2 not found. Check config.log for more details])]
    )

    CFLAGS="$CFLAGS -I$LIBSSH2_DIR/include"
fi
dnl }}}

CURL_CHECK_NONBLOCKING_SOCKET

AC_CONFIG_FILES([Makefile src/Makefile example/Makefile])
AC_OUTPUT
