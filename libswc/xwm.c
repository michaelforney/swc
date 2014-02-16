/* swc: libswc/xwm.c
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

#include "xwm.h"
#include "compositor.h"
#include "internal.h"
#include "surface.h"
#include "swc.h"
#include "util.h"
#include "view.h"
#include "window.h"

#include <stdio.h>
#include <xcb/composite.h>
#include <xcb/xcb_ewmh.h>

struct xwl_window
{
    xcb_window_t id;
    struct window window;
    struct wl_listener surface_destroy_listener;
};

struct xwl_window_entry
{
    xcb_window_t id;
    bool override_redirect;
    struct xwl_window * xwl_window;
};

static struct
{
    xcb_connection_t * connection;
    xcb_ewmh_connection_t ewmh;
    xcb_screen_t * screen;
    struct wl_event_source * source;
    struct wl_array windows;
} xwm;

static void update_name(struct xwl_window * xwl_window)
{
    xcb_get_property_cookie_t wm_name_cookie;
    xcb_ewmh_get_utf8_strings_reply_t wm_name_reply;

    wm_name_cookie = xcb_ewmh_get_wm_name(&xwm.ewmh, xwl_window->id);
    xcb_ewmh_get_wm_name_reply(&xwm.ewmh, wm_name_cookie,
                               &wm_name_reply, NULL);

    window_set_title(&xwl_window->window,
                     wm_name_reply.strings, wm_name_reply.strings_len);

    xcb_ewmh_get_utf8_strings_reply_wipe(&wm_name_reply);
}

static struct xwl_window_entry * find_window(xcb_window_t id)
{
    struct xwl_window_entry * entry;

    wl_array_for_each(entry, &xwm.windows)
    {
        if (entry->id == id)
            return entry;
    }

    return NULL;
}

/* X event handlers */
void create_notify(xcb_create_notify_event_t * event)
{
    struct xwl_window_entry * entry;

    if (!(entry = wl_array_add(&xwm.windows, sizeof *entry)))
        return;

    entry->id = event->window;
    entry->override_redirect = event->override_redirect;
    entry->xwl_window = NULL;
}

void destroy_notify(xcb_destroy_notify_event_t * event)
{
    struct xwl_window_entry * entry;

    if (!(entry = find_window(event->window)))
        return;

    swc_array_remove(&xwm.windows, entry, sizeof *entry);
}

void map_request(xcb_map_request_event_t * event)
{
    xcb_map_window(xwm.connection, event->window);
}

void configure_request(xcb_configure_request_event_t * event)
{
}

void property_notify(xcb_property_notify_event_t * event)
{
    struct xwl_window_entry * entry;

    if (!(entry = find_window(event->window)) || !entry->xwl_window)
        return;

    if (event->atom == xwm.ewmh._NET_WM_NAME
        && event->state == XCB_PROPERTY_NEW_VALUE)
    {
        update_name(entry->xwl_window);
    }
}

static int connection_data(int fd, uint32_t mask, void * data)
{
    xcb_generic_event_t * event;
    uint32_t count = 0;

    while ((event = xcb_poll_for_event(xwm.connection)))
    {
        switch (event->response_type & ~0x80)
        {
            case XCB_CREATE_NOTIFY:
                create_notify((xcb_create_notify_event_t *) event);
                break;
            case XCB_DESTROY_NOTIFY:
                destroy_notify((xcb_destroy_notify_event_t *) event);
                break;
            case XCB_MAP_REQUEST:
                map_request((xcb_map_request_event_t *) event);
                break;
            case XCB_CONFIGURE_REQUEST:
                configure_request((xcb_configure_request_event_t *) event);
                break;
            case XCB_PROPERTY_NOTIFY:
                property_notify((xcb_property_notify_event_t *) event);
        }

        free(event);
        ++count;
    }

    xcb_flush(xwm.connection);

    return count;
}

bool swc_xwm_initialize(int fd)
{
    const xcb_setup_t * setup;
    xcb_screen_iterator_t screen_iterator;
    uint32_t mask;
    uint32_t values[1];
    xcb_void_cookie_t change_attributes_cookie, redirect_subwindows_cookie;
    xcb_generic_error_t * error;
    xcb_intern_atom_cookie_t * ewmh_cookies;
    const xcb_query_extension_reply_t * composite_extension;

    xwm.connection = xcb_connect_to_fd(fd, NULL);

    if (xcb_connection_has_error(xwm.connection))
    {
        ERROR("xwm: Could not connect to X server\n");
        goto error0;
    }

    xcb_prefetch_extension_data(xwm.connection, &xcb_composite_id);
    ewmh_cookies = xcb_ewmh_init_atoms(xwm.connection, &xwm.ewmh);

    if (!ewmh_cookies)
    {
        ERROR("xwm: Failed to initialize EWMH atoms\n");
        goto error1;
    }

    setup = xcb_get_setup(xwm.connection);
    screen_iterator = xcb_setup_roots_iterator(setup);
    xwm.screen = screen_iterator.data;

    /* Try to select for substructure redirect. */
    mask = XCB_CW_EVENT_MASK;
    values[0] = XCB_EVENT_MASK_SUBSTRUCTURE_NOTIFY
              | XCB_EVENT_MASK_SUBSTRUCTURE_REDIRECT;
    change_attributes_cookie = xcb_change_window_attributes
        (xwm.connection, xwm.screen->root, mask, values);

    xwm.source = wl_event_loop_add_fd(swc.event_loop, fd, WL_EVENT_READABLE,
                                      &connection_data, NULL);
    wl_array_init(&xwm.windows);

    if (!xwm.source)
    {
        ERROR("xwm: Failed to create X connection event source\n");
        goto error2;
    }

    composite_extension = xcb_get_extension_data(xwm.connection, &xcb_composite_id);

    if (!composite_extension->present)
    {
        ERROR("xwm: X server does not have composite extension\n");
        goto error3;
    }

    redirect_subwindows_cookie = xcb_composite_redirect_subwindows_checked
            (xwm.connection, xwm.screen->root, XCB_COMPOSITE_REDIRECT_MANUAL);

    if ((error = xcb_request_check(xwm.connection, change_attributes_cookie)))
    {
        ERROR("xwm: Another window manager is running\n");
        free(error);
        goto error3;
    }

    if ((error = xcb_request_check(xwm.connection, redirect_subwindows_cookie)))
    {
        ERROR("xwm: Could not redirect subwindows of root for compositing\n");
        free(error);
        goto error3;
    }

    if (!xcb_ewmh_init_atoms_replies(&xwm.ewmh, ewmh_cookies, NULL))
    {
        ERROR("xwm: Failed to get EWMH atom replies\n");
        goto error3;
    }

    return true;

  error3:
    wl_event_source_remove(xwm.source);
  error2:
    xcb_ewmh_connection_wipe(&xwm.ewmh);
  error1:
    xcb_disconnect(xwm.connection);
  error0:
    return false;
}

void swc_xwm_finalize()
{
    wl_array_release(&xwm.windows);
    wl_event_source_remove(xwm.source);
    xcb_ewmh_connection_wipe(&xwm.ewmh);
    xcb_disconnect(xwm.connection);
}

static void configure(struct window * window,
                      const struct swc_rectangle * geometry)
{
    uint32_t mask, values[4];
    struct xwl_window * xwl_window
        = CONTAINER_OF(window, typeof(*xwl_window), window);

    mask = XCB_CONFIG_WINDOW_X | XCB_CONFIG_WINDOW_Y
         | XCB_CONFIG_WINDOW_WIDTH | XCB_CONFIG_WINDOW_HEIGHT;
    values[0] = geometry->x;
    values[1] = geometry->y;
    values[2] = geometry->width;
    values[3] = geometry->height;

    xcb_configure_window(xwm.connection, xwl_window->id, mask, values);
    xcb_flush(xwm.connection);
}

static void focus(struct window * window)
{
    xcb_window_t id = window ? CONTAINER_OF(window, struct xwl_window,
                                            window)->id
                             : XCB_NONE;

    xcb_set_input_focus(xwm.connection, XCB_INPUT_FOCUS_NONE,
                        id, XCB_CURRENT_TIME);
    xcb_flush(xwm.connection);
}

static const struct window_impl xwl_window_handler = {
    .configure = &configure,
    .focus = &focus
};

static void handle_surface_destroy(struct wl_listener * listener, void * data)
{
    struct xwl_window_entry * entry;
    struct xwl_window * xwl_window
        = CONTAINER_OF(listener, typeof(*xwl_window), surface_destroy_listener);

    window_finalize(&xwl_window->window);
    free(xwl_window);

    wl_array_for_each(entry, &xwm.windows)
    {
        if (entry->xwl_window == xwl_window)
        {
            entry->xwl_window = NULL;
            break;
        }
    }
}

void swc_xwm_manage_window(xcb_window_t id, struct swc_surface * surface)
{
    struct xwl_window_entry * entry;
    struct xwl_window * xwl_window;
    xcb_get_geometry_cookie_t geometry_cookie;
    xcb_get_geometry_reply_t * geometry_reply;

    if (!(entry = find_window(id)))
        return;

    if (!(xwl_window = malloc(sizeof *xwl_window)))
        return;

    geometry_cookie = xcb_get_geometry(xwm.connection, id);

    window_initialize(&xwl_window->window, &xwl_window_handler, surface);
    xwl_window->id = id;
    xwl_window->surface_destroy_listener.notify = &handle_surface_destroy;
    wl_resource_add_destroy_listener(surface->resource,
                                     &xwl_window->surface_destroy_listener);

    entry->xwl_window = xwl_window;

    if ((geometry_reply = xcb_get_geometry_reply(xwm.connection,
                                                 geometry_cookie, NULL)))
    {
        swc_view_move(surface->view, geometry_reply->x, geometry_reply->y);
        free(geometry_reply);
    }

    if (entry->override_redirect)
        compositor_view_show(xwl_window->window.view);
    else
    {
        uint32_t mask, values[1];

        mask = XCB_CW_EVENT_MASK;
        values[0] = XCB_EVENT_MASK_PROPERTY_CHANGE;
        xcb_change_window_attributes(xwm.connection, id, mask, values);
        mask = XCB_CONFIG_WINDOW_BORDER_WIDTH;
        values[0] = 0;
        xcb_configure_window(xwm.connection, id, mask, values);
        update_name(xwl_window);

        window_set_state(&xwl_window->window, SWC_WINDOW_STATE_NORMAL);
    }
}

