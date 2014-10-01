/* swc: libswc/shell.c
 *
 * Copyright (c) 2013 Michael Forney
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

#include "shell.h"
#include "internal.h"
#include "shell_surface.h"

#include <wayland-server.h>

static struct
{
    struct wl_global * global;
} shell;

static void get_shell_surface(struct wl_client * client,
                              struct wl_resource * resource, uint32_t id,
                              struct wl_resource * surface_resource)
{
    struct swc_surface * surface = wl_resource_get_user_data(surface_resource);
    struct shell_surface * shell_surface;

    shell_surface = shell_surface_new(client, wl_resource_get_version(resource),
                                      id, surface);

    if (!shell_surface)
        wl_resource_post_no_memory(resource);
}

static const struct wl_shell_interface shell_implementation = {
    &get_shell_surface
};

static void bind_shell(struct wl_client * client, void * data,
                       uint32_t version, uint32_t id)
{
    struct wl_resource * resource;

    if (version >= 1)
        version = 1;

    resource = wl_resource_create(client, &wl_shell_interface, version, id);
    wl_resource_set_implementation(resource, &shell_implementation, NULL, NULL);
}

bool swc_shell_initialize()
{
    shell.global = wl_global_create(swc.display, &wl_shell_interface, 1,
                                    NULL, &bind_shell);

    return shell.global;
}

void swc_shell_finalize()
{
    wl_global_destroy(shell.global);
}

