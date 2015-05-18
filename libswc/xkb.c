/* swc: libswc/xkb.c
 *
 * Copyright (c) 2013, 2014 Michael Forney
 *
 * Permission is hereby granted, free of charge, to any person obtaining a copy
 * of this software and associated documentation files (the "Software"), to deal
 * in the Software without restriction, including without limitation the rights
 * to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
 * copies of the Software, and to permit persons to whom the Software is
 * furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included in
 * all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
 * AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
 * OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
 * SOFTWARE.
 */

#include "xkb.h"
#include "util.h"

#include <stdlib.h>
#include <fcntl.h>
#include <unistd.h>
#include <string.h>
#include <sys/mman.h>

static const char keymap_file_template[] = "swc-xkb-keymap-XXXXXX";

bool
xkb_initialize(struct xkb *xkb)
{
	xkb->context = xkb_context_new(0);

	if (!xkb->context) {
		ERROR("Could not create XKB context\n");
		goto error0;
	}

	xkb->keymap.map = xkb_keymap_new_from_names(xkb->context, NULL, 0);

	if (!xkb->keymap.map) {
		ERROR("Could not create XKB keymap\n");
		goto error1;
	}

	xkb->state = xkb_state_new(xkb->keymap.map);

	if (!xkb->state) {
		ERROR("Could not create XKB state\n");
		goto error2;
	}

	if (!xkb_update_keymap(xkb)) {
		ERROR("Could not update XKB keymap\n");
		goto error3;
	}

	return true;

error3:
	xkb_state_unref(xkb->state);
error2:
	xkb_keymap_unref(xkb->keymap.map);
error1:
	xkb_context_unref(xkb->context);
error0:
	return false;
}

void
xkb_finalize(struct xkb *xkb)
{
	munmap(xkb->keymap.area, xkb->keymap.size);
	close(xkb->keymap.fd);
	xkb_state_unref(xkb->state);
	xkb_keymap_unref(xkb->keymap.map);
	xkb_context_unref(xkb->context);
}

bool
xkb_reset_state(struct xkb *xkb)
{
	struct xkb_state *state;

	if (!(state = xkb_state_new(xkb->keymap.map))) {
		ERROR("Failed to allocate new XKB state\n");
		return false;
	}

	xkb_state_unref(xkb->state);
	xkb->state = state;
	return true;
}

bool
xkb_update_keymap(struct xkb *xkb)
{
	const char *keymap_directory = getenv("XDG_RUNTIME_DIR") ?: "/tmp";
	char *keymap_string;
	char keymap_path[strlen(keymap_directory) + 1 + sizeof keymap_file_template];

	xkb->indices.ctrl = xkb_keymap_mod_get_index(xkb->keymap.map, XKB_MOD_NAME_CTRL);
	xkb->indices.alt = xkb_keymap_mod_get_index(xkb->keymap.map, XKB_MOD_NAME_ALT);
	xkb->indices.super = xkb_keymap_mod_get_index(xkb->keymap.map, XKB_MOD_NAME_LOGO);
	xkb->indices.shift = xkb_keymap_mod_get_index(xkb->keymap.map, XKB_MOD_NAME_SHIFT);

	/* In order to send the keymap to clients, we must first convert it to a
     * string and then mmap it to a file. */
	keymap_string = xkb_keymap_get_as_string(xkb->keymap.map, XKB_KEYMAP_FORMAT_TEXT_V1);

	if (!keymap_string) {
		WARNING("Could not get XKB keymap as a string\n");
		goto error0;
	}

	sprintf(keymap_path, "%s/%s", keymap_directory, keymap_file_template);

	xkb->keymap.size = strlen(keymap_string) + 1;
	xkb->keymap.fd = mkostemp(keymap_path, O_CLOEXEC);

	if (xkb->keymap.fd == -1) {
		WARNING("Could not create XKB keymap file\n");
		goto error1;
	}

	unlink(keymap_path);

	if (posix_fallocate(xkb->keymap.fd, 0, xkb->keymap.size) != 0) {
		WARNING("Could not resize XKB keymap file\n");
		goto error2;
	}

	xkb->keymap.area = mmap(NULL, xkb->keymap.size, PROT_READ | PROT_WRITE, MAP_SHARED, xkb->keymap.fd, 0);

	if (xkb->keymap.area == MAP_FAILED) {
		WARNING("Could not mmap XKB keymap string\n");
		goto error2;
	}

	strcpy(xkb->keymap.area, keymap_string);

	free(keymap_string);

	return true;

error2:
	close(xkb->keymap.fd);
error1:
	free(keymap_string);
error0:
	return false;
}
