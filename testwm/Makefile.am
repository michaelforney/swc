# testwm/Makefile.am

AM_CFLAGS = -I.. $(pixman_CFLAGS)

bin_PROGRAMS = testwm

testwm_SOURCES = \
	main.c \
	shell.c shell.h \
	shell_surface.c shell_surface.h

testwm_LDADD = ../libswc.la $(wayland_server_LIBS)

