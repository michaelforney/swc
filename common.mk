# swc: common.mk

.PHONY: build-$(dir)
build-$(dir): $($(dir)_TARGETS)

.PHONY: install-$(dir)
install-$(dir):

.PHONY: uninstall-$(dir)
uninstall-$(dir):

.deps/$(dir):
	@mkdir -p "$@"

$(dir)/%: dir := $(dir)

$(dir)/%.o: $(dir)/%.c | .deps/$(dir)
	$(compile) $($(dir)_CFLAGS) $($(dir)_PACKAGE_CFLAGS)

$(dir)/%.lo: $(dir)/%.c | .deps/$(dir)
	$(compile) -fPIC $($(dir)_CFLAGS) $($(dir)_PACKAGE_CFLAGS)

ifdef $(dir)_PACKAGES
    ifndef $(dir)_PACKAGE_CFLAGS
        $(dir)_PACKAGE_CFLAGS := $(call pkgconfig,$($(dir)_PACKAGES),cflags,CFLAGS)
    endif
    ifndef $(dir)_PACKAGE_LIBS
        $(dir)_PACKAGE_LIBS := $(call pkgconfig,$($(dir)_PACKAGES),libs,LIBS)
    endif
endif

CLEAN_FILES += $($(dir)_TARGETS)

