# swc: Makefile.am

ACLOCAL_AMFLAGS = -I m4

noinst_LTLIBRARIES = libswc.la

AM_CFLAGS = $(pixman_CFLAGS)

libswc_la_SOURCES = \
	compositor.c compositor.h \
	util.c util.h \
	output.c output.h \
	buffer.c buffer.h \
	surface.c surface.h \
	renderer.c renderer.h \
	seat.c seat.h \
	mode.c mode.h \
	tty.c tty.h \
	evdev_device.c evdev_device.h \
	xkb.c xkb.h \
	drm.c drm.h \
	egl.c egl.h

libswc_la_LIBADD = $(wayland_server_LIBS) $(udev_LIBS) $(xkbcommon_LIBS) \
	$(drm_LIBS) $(gbm_LIBS) $(egl_LIBS) $(pixman_LIBS) i915/libi915.la

# testclient_SOURCES = \
# 	testclient.c
# 
# testclient_LDADD = $(wayland_client_LIBS)

SUBDIRS = i915

