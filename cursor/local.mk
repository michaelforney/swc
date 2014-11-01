# swc: cursor/local.mk

dir := cursor

$(dir)_TARGETS := $(dir)/convert_font $(dir)/cursor_data.h

$(dir)/convert_font: $(dir)/convert_font.o
	$(link)

$(dir)/cursor_data.h: $(dir)/cursor.pcf $(dir)/convert_font
	$(Q_GEN)cursor/convert_font $< $@ 2>/dev/null

CLEAN_FILES += $(dir)/convert_font.o

include common.mk

