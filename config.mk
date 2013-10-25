# swc: config.mk

PREFIX          = /usr/local
BINDIR          = $(PREFIX)/bin
LIBDIR          = $(PREFIX)/lib
INCLUDEDIR      = $(PREFIX)/include

CC              = gcc
CPPFLAGS        = -D_GNU_SOURCE # Required for mkostemp
CFLAGS          = -O2 -pipe
PKG_CONFIG      = pkg-config
WAYLAND_SCANNER = wayland-scanner

ENABLE_STATIC   = 1
ENABLE_SHARED   = 1

# Dependencies
SWC_PACKAGES    =   \
    wayland-server  \
    libudev         \
    libevdev        \
    xkbcommon       \
    libdrm          \
    pixman-1        \
    wld
LAUNCH_PACKAGES =   \
    libdrm

SWC_PACKAGE_CFLAGS      = $$($(PKG_CONFIG) --cflags $(SWC_PACKAGES))
SWC_PACKAGE_LIBS        = $$($(PKG_CONFIG) --libs $(SWC_PACKAGES))
LAUNCH_PACKAGE_CFLAGS   = $$($(PKG_CONFIG) --cflags $(LAUNCH_PACKAGES))
LAUNCH_PACKAGE_LIBS     = $$($(PKG_CONFIG) --libs $(LAUNCH_PACKAGES))

