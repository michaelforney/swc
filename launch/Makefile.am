# launch/Makefile.am

AM_CFLAGS = $(drm_CFLAGS)

bin_PROGRAMS = swc-launch
noinst_LTLIBRARIES = liblaunch-protocol.la

swc_launch_SOURCES = launch.c
swc_launch_LDADD = liblaunch-protocol.la $(drm_LIBS)
swc_launch_LDFLAGS = -static

liblaunch_protocol_la_SOURCES = protocol.c protocol.h

