# launch/Makefile.am

AM_CFLAGS = $(drm_CFLAGS)

lib_LTLIBRARIES = libswc-launch.la
noinst_LTLIBRARIES = liblaunch-protocol.la

libswc_launch_la_SOURCES = \
	launch.c swc-launch.h

libswc_launch_la_LIBADD = $(drm_LIBS) liblaunch-protocol.la

liblaunch_protocol_la_SOURCES = \
	protocol.c protocol.h

include_HEADERS = swc-launch.h

