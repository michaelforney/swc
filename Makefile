# swc: Makefile

.PHONY: all
all: build

# Defaults for config.mk
PREFIX          ?= /usr
BINDIR          ?= $(PREFIX)/bin
LIBDIR          ?= $(PREFIX)/lib
INCLUDEDIR      ?= $(PREFIX)/include
DATADIR         ?= $(PREFIX)/share
PKGCONFIGDIR    ?= $(LIBDIR)/pkgconfig

PKG_CONFIG      ?= pkg-config
WAYLAND_SCANNER ?= wayland-scanner

VERSION_MAJOR   := 0
VERSION_MINOR   := 0
VERSION         := $(VERSION_MAJOR).$(VERSION_MINOR)

TARGETS         := swc.pc
SUBDIRS         := launch libswc protocol cursor example
CLEAN_FILES     := $(TARGETS)

include config.mk

ifeq ($(if $(V),$(V),0), 0)
    define quiet
        @echo "  $1	$@"
        @$(if $2,$2,$($1))
    endef
else
    quiet = $(if $2,$2,$($1))
endif

FINAL_CFLAGS = $(CFLAGS) -fvisibility=hidden -std=gnu99
FINAL_CPPFLAGS = $(CPPFLAGS) -D_GNU_SOURCE # Required for mkostemp

# Warning/error flags
FINAL_CFLAGS += -Werror=implicit-function-declaration -Werror=implicit-int \
                -Werror=pointer-sign -Werror=pointer-arith \
                -Wall -Wno-missing-braces

ifeq ($(ENABLE_DEBUG),1)
    FINAL_CPPFLAGS += -DENABLE_DEBUG=1
    FINAL_CFLAGS += -g
endif

compile     = $(call quiet,CC) $(FINAL_CPPFLAGS) $(FINAL_CFLAGS) -I . -c -o $@ $< \
              -MMD -MP -MF .deps/$(basename $<).d -MT $(basename $@).o -MT $(basename $@).lo
link        = $(call quiet,CCLD,$(CC)) $(FINAL_CFLAGS) -o $@ $^
pkgconfig   = $(sort $(foreach pkg,$(1),$(if $($(pkg)_$(3)),$($(pkg)_$(3)), \
                                           $(shell $(PKG_CONFIG) --$(2) $(pkg)))))

include $(SUBDIRS:%=%/local.mk)

$(foreach dir,BIN LIB INCLUDE PKGCONFIG,$(DESTDIR)$($(dir)DIR)) $(DESTDIR)$(DATADIR)/swc:
	mkdir -p "$@"

.PHONY: build
build: $(SUBDIRS:%=build-%) $(TARGETS)

swc.pc: swc.pc.in
	$(call quiet,GEN,sed)                   \
	    -e "s:@VERSION@:$(VERSION):"        \
	    -e "s:@PREFIX@:$(PREFIX):"          \
	    -e "s:@LIBDIR@:$(LIBDIR):"          \
	    -e "s:@INCLUDEDIR@:$(INCLUDEDIR):"  \
	    $< > $@

.PHONY: install-swc.pc
install-swc.pc: swc.pc | $(DESTDIR)$(PKGCONFIGDIR)
	install -m0644 $< "$(DESTDIR)$(PKGCONFIGDIR)"

.PHONY: install
install: $(SUBDIRS:%=install-%) $(TARGETS:%=install-%)

.PHONY: clean
clean:
	rm -f $(CLEAN_FILES)

-include .deps/*/*.d

