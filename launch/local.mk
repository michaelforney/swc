# swc: launch/local.mk

dir := launch

$(dir)_TARGETS  := $(dir)/swc-launch
$(dir)_PACKAGES := libdrm

ifeq ($(shell uname),NetBSD)
	DEVMAJOR_OBJ=devmajor-netbsd.o
else
	DEVMAJOR_OBJ=devmajor-linux.o
endif

$(dir)/swc-launch: $(dir)/$(DEVMAJOR_OBJ) $(dir)/launch.o $(dir)/protocol.o
	$(link) $(launch_PACKAGE_LIBS)

install-$(dir): $(dir)/swc-launch | $(DESTDIR)$(BINDIR)
	install -m 4755 launch/swc-launch $(DESTDIR)$(BINDIR)

CLEAN_FILES += $(dir)/launch.o
CLEAN_FILES += $(dir)/$(DEVMAJOR_OBJ)

include common.mk

