# swc: common.mk

.PHONY: build-$(dir)
build-$(dir): $($(dir)_TARGETS)

.PHONY: install-$(dir)
install-$(dir):

.deps/$(dir):
	@mkdir -p "$@"

$(dir)/%: dir := $(dir)

$(dir)/%.o: $(dir)/%.c | .deps/$(dir)
	$(compile) $($(dir)_CFLAGS) $($(dir)_PACKAGE_CFLAGS)

$(dir)/%.lo: $(dir)/%.c | .deps/$(dir)
	$(compile) -fPIC $($(dir)_CFLAGS) $($(dir)_PACKAGE_CFLAGS)

ifeq ($(origin $(dir)_PACKAGE_CFLAGS),undefined)
    $(dir)_PACKAGE_CFLAGS := $(call pkgconfig,$($(dir)_PACKAGES),cflags,CFLAGS)
endif
ifeq ($(origin $(dir)_PACKAGE_LIBS),undefined)
    $(dir)_PACKAGE_LIBS := $(call pkgconfig,$($(dir)_PACKAGES),libs,LIBS)
endif

CLEAN_FILES += $($(dir)_TARGETS)

