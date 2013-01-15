#!/bin/sh

set -ex

rm -rf autom4te.cache

libtoolize
aclocal -I m4 --force
autoconf -f -W all
automake -f -a -c -W all

rm -rf autom4te.cache

