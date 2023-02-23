#!/bin/sh

mkdir -p build-aux

autoheader
aclocal
autoconf
automake --add-missing
