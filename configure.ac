dnl swc: configure.ac

AC_PREREQ([2.59])

AC_INIT([swc], [0.0.1], [mforney@mforney.org])
AM_INIT_AUTOMAKE([foreign -Wall -Werror])
AC_LANG([C])

m4_ifdef([AM_SILENT_RULES], [AM_SILENT_RULES([yes])])

AC_PROG_CC_C99
AC_USE_SYSTEM_EXTENSIONS

AM_PROG_AR
LT_INIT

AC_CONFIG_SRCDIR([libswc/compositor.c])
AC_CONFIG_MACRO_DIR([m4])

PKG_PROG_PKG_CONFIG([0.9.0])

dnl Check for libraries {{{
PKG_CHECK_MODULES([wayland_server], [wayland-server])
PKG_CHECK_MODULES([udev], [libudev])
PKG_CHECK_MODULES([xkbcommon], [xkbcommon])
PKG_CHECK_MODULES([drm], [libdrm])
PKG_CHECK_MODULES([drm_intel], [libdrm_intel])
PKG_CHECK_MODULES([intelbatch], [intelbatch])
PKG_CHECK_MODULES([gbm], [gbm])
PKG_CHECK_MODULES([egl], [egl])
PKG_CHECK_MODULES([pixman], [pixman-1])
PKG_CHECK_MODULES([wld], [wld])

PKG_CHECK_MODULES([wayland_client], [wayland-client])
dnl }}}

dnl Wayland protocol
AC_SUBST(PROTOCOL_DIR, ['$(top_builddir)/protocol'])
WAYLAND_SCANNER_RULES(['$(top_srcdir)/protocol'])

AC_CONFIG_FILES([Makefile libswc/Makefile launch/Makefile protocol/Makefile])
AC_OUTPUT

