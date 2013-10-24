#include "xkb.h"

#include <stdlib.h>
#include <fcntl.h>
#include <unistd.h>
#include <string.h>
#include <sys/mman.h>

const struct xkb_rule_names rule_names = {
    .layout = "us,us",
    .variant = "dvorak,",
    .options = "grp:alt_shift_toggle"
};

static const char keymap_file_template[] = "swc-xkb-keymap-XXXXXX";

bool swc_xkb_initialize(struct swc_xkb * xkb)
{
    xkb->context = xkb_context_new(0);

    if (!xkb->context)
    {
        printf("could not create XKB context\n");
        goto error_base;
    }

    xkb->keymap.map = xkb_keymap_new_from_names(xkb->context, &rule_names, 0);

    if (!xkb->keymap.map)
    {
        printf("could not create XKB keymap\n");
        goto error_context;
    }

    xkb->state = xkb_state_new(xkb->keymap.map);

    if (!swc_xkb_update_keymap(xkb))
    {
        printf("could not update XKB keymap\n");
        goto error_state;
    }

    return true;

  error_state:
    xkb_state_unref(xkb->state);
  error_keymap:
    xkb_keymap_unref(xkb->keymap.map);
  error_context:
    xkb_context_unref(xkb->context);
  error_base:
    return false;
}

void swc_xkb_finish(struct swc_xkb * xkb)
{
    munmap(xkb->keymap.area, xkb->keymap.size);
    close(xkb->keymap.fd);
    xkb_state_unref(xkb->state);
    xkb_keymap_unref(xkb->keymap.map);
    xkb_context_unref(xkb->context);
}

bool swc_xkb_update_keymap(struct swc_xkb * xkb)
{
    char * keymap_string;

    xkb->indices.ctrl
        = xkb_keymap_mod_get_index(xkb->keymap.map, XKB_MOD_NAME_CTRL);
    xkb->indices.alt
        = xkb_keymap_mod_get_index(xkb->keymap.map, XKB_MOD_NAME_ALT);
    xkb->indices.super
        = xkb_keymap_mod_get_index(xkb->keymap.map, XKB_MOD_NAME_LOGO);
    xkb->indices.shift
        = xkb_keymap_mod_get_index(xkb->keymap.map, XKB_MOD_NAME_SHIFT);

    printf("indices { ctrl: %x, alt: %x, super: %x, shift: %x }\n",
        xkb->indices.ctrl, xkb->indices.alt, xkb->indices.super, xkb->indices.shift);

    /* Keymap string */
    {
        const char * keymap_directory = getenv("XDG_RUNTIME_DIR") ?: "/tmp";
        char keymap_path[strlen(keymap_directory) + 1
                         + sizeof keymap_file_template];

        /* In order to send the keymap to clients, we must first convert it to a
         * string and then mmap it to a file. */
        keymap_string = xkb_keymap_get_as_string(xkb->keymap.map,
                                                 XKB_KEYMAP_FORMAT_TEXT_V1);

        if (!keymap_string)
        {
            printf("could not get XKB keymap as a string\n");
            goto error_base;
        }

        sprintf(keymap_path, "%s/%s", keymap_directory, keymap_file_template);

        xkb->keymap.size = strlen(keymap_string) + 1;
        xkb->keymap.fd = mkostemp(keymap_path, O_CLOEXEC);

        if (xkb->keymap.fd == -1)
        {
            printf("could not create XKB keymap file\n");
            goto error_string;
        }

        unlink(keymap_path);

        if (ftruncate(xkb->keymap.fd, xkb->keymap.size) == -1)
        {
            printf("could not resize XKB keymap file\n");
            goto error_fd;
        }

        xkb->keymap.area = mmap(NULL, xkb->keymap.size, PROT_READ | PROT_WRITE,
            MAP_SHARED, xkb->keymap.fd, 0);

        if (xkb->keymap.area == MAP_FAILED)
        {
            printf("could not mmap XKB keymap string\n");
            goto error_fd;
        }

        strcpy(xkb->keymap.area, keymap_string);

        free(keymap_string);
    }

    return true;

  error_fd:
    close(xkb->keymap.fd);
  error_string:
    free(keymap_string);
  error_base:
    return false;
}

void swc_xkb_update_key_indices(struct swc_xkb * xkb)
{
}

