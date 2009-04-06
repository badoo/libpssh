#!/bin/sh
# $Id: autogen.sh,v 1.3 2007/12/24 10:02:18 tony Exp $

aclocal
libtoolize --force
autoheader
automake -a
autoconf
# ./configure --enable-debug --libdir=/home/rushba/prg/pssh/src/.libs LDFLAGS=-L/usr/local/lib
