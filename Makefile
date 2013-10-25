# swc: Makefile

.PHONY: all
all: build

VERSION_MAJOR   := 0
VERSION_MINOR   := 0

SUBDIRS         := launch libswc protocol
CLEAN_FILES     :=

include config.mk
include $(SUBDIRS:%=%/Makefile.local)

ifeq ($(if $(V),$(V),0), 0)
    define quiet
        @echo "  $1	$@"
        @$(if $2,$2,$($1))
    endef
else
    quiet = $(if $2,$2,$($1))
endif

define check_deps
    @echo "Checking for dependencies of $1 ($2)"
    @$(PKG_CONFIG) --exists --print-errors $2
endef

compile = $(call quiet,CC) $(CFLAGS) $(CPPFLAGS) -I . -c -MMD -MP -MF .deps/$*.d -o $@ $<
link    = $(call quiet,CCLD,$(CC)) $(CFLAGS) -o $@ $^

.PHONY: check-dependencies
check-dependencies: $(SUBDIRS:%=check-dependencies-%)

.PHONY: build
build: $(SUBDIRS:%=build-%)

.PHONY: install
install: $(SUBDIRS:%=install-%)

$(DESTDIR)$(BINDIR) $(DESTDIR)$(LIBDIR) $(DESTDIR)$(INCLUDEDIR):
	mkdir -p "$@"

.PHONY: clean
clean:
	rm -f $(CLEAN_FILES)

