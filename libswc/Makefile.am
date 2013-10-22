# swc: libswc/Makefile.am

AM_CPPFLAGS = -I$(top_srcdir) -I$(PROTOCOL_DIR)
AM_CFLAGS = $(drm_CFLAGS) $(pixman_CFLAGS) $(wld_CFLAGS) $(libevdev_CFLAGS)

lib_LTLIBRARIES = libswc.la

libswc_la_SOURCES = \
    compositor.c compositor.h \
    util.c util.h \
    output.c output.h \
    plane.c plane.h \
    surface.c surface.h \
    compositor_surface.c compositor_surface.h \
    cursor_surface.c cursor_surface.h \
    region.c region.h \
    renderer.c renderer.h \
    input_focus.c input_focus.h \
    keyboard.c keyboard.h \
    pointer.c pointer.h \
    seat.c seat.h \
    data_device_manager.c data_device_manager.h \
    data_device.c data_device.h \
    data.c data.h \
    mode.c mode.h \
    evdev_device.c evdev_device.h \
    xkb.c xkb.h \
    drm.c drm.h \
    drm_buffer.c drm_buffer.h \
    ../protocol/wayland-drm-protocol.c

libswc_la_LIBADD = $(wayland_server_LIBS) $(udev_LIBS) $(libevdev_LIBS) \
                   $(xkbcommon_LIBS) $(drm_LIBS) $(pixman_LIBS) $(wld_LIBS) \
                   ../launch/liblaunch-protocol.la

