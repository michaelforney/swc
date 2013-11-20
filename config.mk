# swc: config.mk

PREFIX          = /usr/local
BINDIR          = $(PREFIX)/bin
LIBDIR          = $(PREFIX)/lib
INCLUDEDIR      = $(PREFIX)/include
PKGCONFIGDIR    = $(LIBDIR)/pkgconfig

CC              = gcc
CPPFLAGS        = -D_GNU_SOURCE # Required for mkostemp
CFLAGS          = -O2 -pipe -g
PKG_CONFIG      = pkg-config
WAYLAND_SCANNER = wayland-scanner

ENABLE_STATIC   = 1
ENABLE_SHARED   = 1

