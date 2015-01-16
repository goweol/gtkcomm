#!/bin/sh
# Run this to generate all the initial makefiles, etc.

srcdir=`dirname $0`
test -z "$srcdir" && srcdir=.

PKG_NAME="GTK+ Communication Program"

(test -f $srcdir/configure.in \
  && test -f $srcdir/src/pcMain.c) || {
    echo "**Error**: Directory "\`$srcdir\'" does not look like the"
    echo " top-level '$PKG_NAME' directory"
    exit 1
}

aclocal $ACLOCAL_FLAGS
autoheader
automake --add-missing --copy
autoconf
./configure --enable-maintainer-mode $@
