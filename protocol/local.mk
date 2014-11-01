# swc: protocol/local.mk

dir := protocol

PROTOCOL_EXTENSIONS =           \
    $(dir)/swc.xml              \
    $(dir)/wayland-drm.xml      \
    $(dir)/xdg-shell.xml

$(dir)_TARGETS := $(PROTOCOL_EXTENSIONS:%.xml=%-protocol.c) \
                  $(PROTOCOL_EXTENSIONS:%.xml=%-server-protocol.h)
$(dir)_PACKAGES := wayland-server

$(dir)/%-protocol.c: $(dir)/%.xml
	$(Q_GEN)$(WAYLAND_SCANNER) code <$< >$@

$(dir)/%-server-protocol.h: $(dir)/%.xml
	$(Q_GEN)$(WAYLAND_SCANNER) server-header <$< >$@

install-protocol: | $(DESTDIR)$(DATADIR)/swc
	install -m0644 protocol/swc.xml "$(DESTDIR)$(DATADIR)/swc"

include common.mk

