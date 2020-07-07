# swc: Makefile

.PHONY: all
all: build

# Defaults for config.mk
PREFIX          ?= /usr/local
BINDIR          ?= $(PREFIX)/bin
LIBDIR          ?= $(PREFIX)/lib
INCLUDEDIR      ?= $(PREFIX)/include
DATADIR         ?= $(PREFIX)/share
PKGCONFIGDIR    ?= $(LIBDIR)/pkgconfig

OBJCOPY         ?= objcopy
PKG_CONFIG      ?= pkg-config
WAYLAND_SCANNER ?= wayland-scanner

VERSION_MAJOR   := 0
VERSION_MINOR   := 0
VERSION         := $(VERSION_MAJOR).$(VERSION_MINOR)

TARGETS         := swc.pc
SUBDIRS         := launch libswc protocol cursor example
CLEAN_FILES     := $(TARGETS)

include config.mk

# Dependencies
PACKAGES :=           \
    libdrm            \
    pixman-1          \
    wayland-server    \
    wayland-protocols \
    wld               \
    xkbcommon

ifeq ($(ENABLE_XWAYLAND),1)
PACKAGES +=         \
    xcb             \
    xcb-composite   \
    xcb-ewmh        \
    xcb-icccm
endif

ifneq ($(shell uname),NetBSD)
    PACKAGES += libinput
    ifeq ($(ENABLE_LIBUDEV),1)
        PACKAGES += libudev
    endif
endif

libinput_CONSTRAINTS        := --atleast-version=0.4
wayland-server_CONSTRAINTS  := --atleast-version=1.6.0

define check
    ifeq ($$(origin $(1)_EXISTS),undefined)
        $(1)_EXISTS = $$(shell $$(PKG_CONFIG) --exists $$($(1)_CONSTRAINTS) $(1) && echo yes)
    endif
    ifneq ($$($(1)_EXISTS),yes)
        $$(error Could not find package $(1) $$($(1)_CONSTRAINTS))
    endif
endef

$(foreach pkg,$(PACKAGES),$(eval $(call check,$(pkg))))

FINAL_CFLAGS = $(CFLAGS) -fvisibility=hidden -std=c11
FINAL_CPPFLAGS = $(CPPFLAGS) -D_GNU_SOURCE # Required for mkostemp

# Warning/error flags
FINAL_CFLAGS += -Werror=implicit-function-declaration -Werror=implicit-int \
                -Werror=pointer-sign -Werror=pointer-arith \
                -Wall -Wno-missing-braces

ifeq ($(ENABLE_DEBUG),1)
    FINAL_CPPFLAGS += -DENABLE_DEBUG=1
    FINAL_CFLAGS += -g
endif

ifeq ($(if $(V),$(V),0),0)
    quiet = @echo '  $1 $@';
endif

Q_AR      = $(call quiet,AR     )
Q_CC      = $(call quiet,CC     )
Q_CCLD    = $(call quiet,CCLD   )
Q_GEN     = $(call quiet,GEN    )
Q_OBJCOPY = $(call quiet,OBJCOPY)
Q_SYM     = $(call quiet,SYM    )

compile   = $(Q_CC)$(CC) $(FINAL_CPPFLAGS) $(FINAL_CFLAGS) -I . -c -o $@ $< \
            -MMD -MP -MF .deps/$(basename $<).d -MT $(basename $@).o -MT $(basename $@).lo
link      = $(Q_CCLD)$(CC) $(LDFLAGS) -o $@ $^
pkgconfig = $(foreach pkg,$(1),$(if $($(pkg)_$(3)),$($(pkg)_$(3)), \
                                    $(shell $(PKG_CONFIG) --$(2) $(pkg))))

include $(SUBDIRS:%=%/local.mk)

$(foreach dir,BIN LIB INCLUDE PKGCONFIG,$(DESTDIR)$($(dir)DIR)) $(DESTDIR)$(DATADIR)/swc:
	mkdir -p $@

.PHONY: build
build: $(SUBDIRS:%=build-%) $(TARGETS)

REQUIRES          := wayland-server
REQUIRES_PRIVATE  := $(filter-out $(REQUIRES),$(libswc_PACKAGES))
SWC_PC_VARS       := VERSION PREFIX LIBDIR INCLUDEDIR DATADIR REQUIRES REQUIRES_PRIVATE

swc.pc: swc.pc.in
	$(Q_GEN)sed $(foreach var,$(SWC_PC_VARS),-e 's:@$(var)@:$($(var)):') $< >$@

.PHONY: install-swc.pc
install-swc.pc: swc.pc | $(DESTDIR)$(PKGCONFIGDIR)
	install -m 644 $< $(DESTDIR)$(PKGCONFIGDIR)

.PHONY: install
install: $(SUBDIRS:%=install-%) $(TARGETS:%=install-%)

.PHONY: clean
clean:
	rm -f $(CLEAN_FILES)

-include .deps/*/*.d

