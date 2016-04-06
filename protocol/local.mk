# swc: protocol/local.mk

dir := protocol
wayland_protocols := $(call pkgconfig,wayland-protocols,variable=pkgdatadir,DATADIR)

PROTOCOL_EXTENSIONS =           \
    $(dir)/swc.xml              \
    $(dir)/wayland-drm.xml      \
    $(wayland_protocols)/unstable/xdg-shell/xdg-shell-unstable-v5.xml

$(dir)_PACKAGES := wayland-server

define protocol_rules

$(dir)/$$(basename $$(notdir $(1)))-protocol.c: $(1)
	$$(Q_GEN)$$(WAYLAND_SCANNER) code <$$< >$$@
$(dir)/$$(basename $$(notdir $(1)))-server-protocol.h: $(1)
	$$(Q_GEN)$$(WAYLAND_SCANNER) server-header <$$< >$$@

CLEAN_FILES += $(foreach type,protocol.c server-protocol.h, \
                 $(dir)/$$(basename $$(notdir $(1)))-$(type))

endef

$(eval $(foreach extension,$(PROTOCOL_EXTENSIONS),$(call protocol_rules,$(extension))))

install-$(dir): | $(DESTDIR)$(DATADIR)/swc
	install -m 644 protocol/swc.xml $(DESTDIR)$(DATADIR)/swc

include common.mk

