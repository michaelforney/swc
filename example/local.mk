# swc: example/local.mk

dir := example

$(dir)_PACKAGES = wayland-server xkbcommon
$(dir)_CFLAGS = -Ilibswc

$(dir): $(dir)/wm

$(dir)/wm: $(dir)/wm.o libswc/libswc.so
	$(link) $(example_PACKAGE_LIBS) -lm

CLEAN_FILES += $(dir)/wm.o $(dir)/wm

include common.mk

