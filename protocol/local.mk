# swc: protocol/local.mk

dir := protocol

PROTOCOL_EXTENSIONS =           \
    $(dir)/swc.xml              \
    $(dir)/wayland-drm.xml

$(dir)_TARGETS := $(PROTOCOL_EXTENSIONS:%.xml=%-protocol.c) \
                  $(PROTOCOL_EXTENSIONS:%.xml=%-server-protocol.h)
$(dir)_PACKAGES := wayland-server

$(dir)/%-protocol.c: $(dir)/%.xml
	$(call quiet,GEN,$(WAYLAND_SCANNER)) code < $< > $@

$(dir)/%-server-protocol.h: $(dir)/%.xml
	$(call quiet,GEN,$(WAYLAND_SCANNER)) server-header < $< > $@

install-protocol: | $(DESTDIR)$(DATADIR)/swc
	install -m0644 protocol/swc.xml "$(DESTDIR)$(DATADIR)/swc"

uninstall-protocol: | $(DESTDIR)$(DATADIR)/swc
	rm -f "$(DESTDIR)$(DATADIR)/swc.xml"

include common.mk

