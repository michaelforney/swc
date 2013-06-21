# swc: Makefile.am

ACLOCAL_AMFLAGS = -I m4
AM_CFLAGS = $(pixman_CFLAGS)

lib_LTLIBRARIES = libswc.la

libswc_la_SOURCES = \
	compositor.c compositor.h \
	util.c util.h \
	output.c output.h \
	buffer.c buffer.h \
	surface.c surface.h \
	region.c region.h \
	renderer.c renderer.h \
	keyboard.c keyboard.h \
	pointer.c pointer.h \
	seat.c seat.h \
	data_device_manager.c data_device_manager.h \
	data_device.c data_device.h \
	mode.c mode.h \
	tty.c tty.h \
	evdev_device.c evdev_device.h \
	xkb.c xkb.h \
	drm.c drm.h \
	egl.c egl.h

libswc_la_LIBADD = $(wayland_server_LIBS) $(udev_LIBS) $(xkbcommon_LIBS) \
	$(drm_LIBS) $(drm_intel_LIBS) $(gbm_LIBS) $(egl_LIBS) $(pixman_LIBS) \
	intel/libintel.la

SUBDIRS = launch intel

