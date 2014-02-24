/* swc: libswc/compositor.c
 *
 * Copyright (c) 2013, 2014 Michael Forney
 *
 * Based in part upon compositor.c from weston, which is:
 *
 *     Copyright © 2010-2011 Intel Corporation
 *     Copyright © 2008-2011 Kristian Høgsberg
 *     Copyright © 2012 Collabora, Ltd.
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

#include "swc.h"
#include "compositor.h"
#include "data_device_manager.h"
#include "drm.h"
#include "internal.h"
#include "launch.h"
#include "output.h"
#include "pointer.h"
#include "region.h"
#include "screen.h"
#include "seat.h"
#include "shm.h"
#include "surface.h"
#include "util.h"
#include "view.h"

#include <stdlib.h>
#include <stdio.h>
#include <assert.h>
#include <wld/wld.h>
#include <wld/drm.h>
#include <xkbcommon/xkbcommon-keysyms.h>

struct target
{
    struct wld_surface * surface;
    struct wld_buffer * next_buffer, * current_buffer;
    struct view * view;
    struct wl_listener view_listener;
    uint32_t mask;

    struct wl_listener screen_listener;
};

static bool handle_motion(struct pointer_handler * handler, uint32_t time,
                          wl_fixed_t x, wl_fixed_t y);
static void perform_update(void * data);

static struct pointer_handler pointer_handler = {
    .motion = &handle_motion
};

static struct
{
    struct wl_list views;
    pixman_region32_t damage, opaque;

    /* A mask of screens that have been repainted but are waiting on a page
     * flip. */
    uint32_t pending_flips;

    /* A mask of screens that are scheduled to be repainted on the next
     * idle. */
    uint32_t scheduled_updates;

    bool active, updating;

    struct wl_global * global;
} compositor;

const struct swc_compositor swc_compositor = {
    .pointer_handler = &pointer_handler
};

static void handle_screen_event(struct wl_listener * listener, void * data)
{
    struct swc_event * event = data;

    if (event->type == SWC_SCREEN_DESTROYED)
    {
        struct target * target
            = CONTAINER_OF(listener, typeof(*target), screen_listener);

        wld_destroy_surface(target->surface);
        free(target);
    }
}

static struct target * target_get(struct screen * screen)
{
    struct wl_listener * listener
        = wl_signal_get(&screen->base.event_signal, &handle_screen_event);
    struct target * target;

    return listener ? CONTAINER_OF(listener, typeof(*target), screen_listener)
                    : NULL;
}

static void handle_screen_view_event(struct wl_listener * listener, void * data)
{
    struct swc_event * event = data;
    struct view_event_data * event_data = event->data;
    struct target * target
        = CONTAINER_OF(listener, typeof(*target), view_listener);

    switch (event->type)
    {
        case VIEW_EVENT_FRAME:
        {
            struct compositor_view * view;

            compositor.pending_flips &= ~target->mask;

            wl_list_for_each(view, &compositor.views, link)
            {
                if (view->base.screens & target->mask)
                    view_frame(&view->base, event_data->frame.time);
            }

            if (target->current_buffer)
                wld_surface_release(target->surface, target->current_buffer);

            target->current_buffer = target->next_buffer;

            /* If we had scheduled updates that couldn't run because we were
             * waiting on a page flip, run them now. */
            if (compositor.scheduled_updates && !compositor.updating)
                perform_update(NULL);
            break;
        }
    }
}

static bool target_swap_buffers(struct target * target)
{
    target->next_buffer = wld_surface_take(target->surface);

    if (!view_attach(target->view, target->next_buffer))
    {
        ERROR("Failed to attach next frame to screen\n");
        return false;
    }

    return true;
}

static struct target * target_new(struct screen * screen)
{
    struct target * target;

    if (!(target = malloc(sizeof *target)))
        goto error0;

    target->surface = wld_create_surface(swc.drm->context,
                                         screen->base.geometry.width,
                                         screen->base.geometry.height,
                                         WLD_FORMAT_XRGB8888,
                                         WLD_DRM_FLAG_SCANOUT);

    if (!target->surface)
        goto error1;

    target->view = &screen->planes.framebuffer.view;
    target->view_listener.notify = &handle_screen_view_event;
    wl_signal_add(&target->view->event_signal, &target->view_listener);
    target->current_buffer = NULL;
    target->mask = screen_mask(screen);
    target_swap_buffers(target);

    target->screen_listener.notify = &handle_screen_event;
    wl_signal_add(&screen->base.event_signal, &target->screen_listener);

    return target;

error1:
    free(target);
error0:
    return NULL;
}

/* Rendering {{{ */

static void repaint_view(struct target * target, struct compositor_view * view,
                         pixman_region32_t * damage)
{
    pixman_region32_t view_region, view_damage, border_damage;
    const struct swc_rectangle * geometry = &view->base.geometry;

    if (!view->base.buffer)
        return;

    pixman_region32_init_rect(&view_region, geometry->x, geometry->y,
                              geometry->width, geometry->height);
    pixman_region32_init_with_extents(&view_damage, &view->extents);
    pixman_region32_init(&border_damage);

    pixman_region32_intersect(&view_damage, &view_damage, damage);
    pixman_region32_subtract(&view_damage, &view_damage, &view->clip);
    pixman_region32_subtract(&border_damage, &view_damage, &view_region);
    pixman_region32_intersect(&view_damage, &view_damage, &view_region);

    pixman_region32_fini(&view_region);

    if (pixman_region32_not_empty(&view_damage))
    {
        pixman_region32_translate(&view_damage, -geometry->x, -geometry->y);
        wld_copy_region(swc.drm->renderer, view->buffer,
                        geometry->x - target->view->geometry.x,
                        geometry->y - target->view->geometry.y, &view_damage);
    }

    pixman_region32_fini(&view_damage);

    /* Draw border */
    if (pixman_region32_not_empty(&border_damage))
    {
        DEBUG("\t\tRedrawing border\n");

        pixman_region32_translate(&border_damage,
                                  -target->view->geometry.x,
                                  -target->view->geometry.y);
        wld_fill_region(swc.drm->renderer, view->border.color, &border_damage);
    }

    pixman_region32_fini(&border_damage);
}

static void renderer_repaint(struct target * target,
                             pixman_region32_t * damage,
                             pixman_region32_t * base_damage,
                             struct wl_list * views)
{
    struct compositor_view * view;

    DEBUG("Rendering to target { x: %d, y: %d, w: %u, h: %u }\n",
          target->view->geometry.x, target->view->geometry.y,
          target->view->geometry.width, target->view->geometry.height);

    wld_set_target_surface(swc.drm->renderer, target->surface);

    /* Paint base damage black. */
    if (pixman_region32_not_empty(base_damage))
    {
        pixman_region32_translate(base_damage,
                                  -target->view->geometry.x,
                                  -target->view->geometry.y);
        wld_fill_region(swc.drm->renderer, 0xff000000, base_damage);
    }

    wl_list_for_each_reverse(view, views, link)
    {
        if (view->base.screens & target->mask)
            repaint_view(target, view, damage);
    }

    wld_flush(swc.drm->renderer);
}

static bool renderer_attach(struct compositor_view * view,
                            struct wld_buffer * client_buffer)
{
    struct wld_buffer * buffer;
    bool was_proxy = view->buffer != view->base.buffer;
    bool needs_proxy = client_buffer
        && !(wld_capabilities(swc.drm->renderer,
                              client_buffer) & WLD_CAPABILITY_READ);
    bool resized = view->buffer && client_buffer
        && (view->buffer->width != client_buffer->width
            || view->buffer->height != client_buffer->height);

    if (client_buffer)
    {
        /* Create a proxy buffer if necessary (for example a hardware buffer
         * backing a SHM buffer). */
        if (needs_proxy)
        {
            if (!was_proxy || resized)
            {
                DEBUG("Creating a proxy buffer\n");
                buffer = wld_create_buffer(swc.drm->context,
                                           client_buffer->width,
                                           client_buffer->height,
                                           client_buffer->format, WLD_FLAG_MAP);

                if (!buffer)
                    return false;
            }
            else
            {
                /* Otherwise we can keep the original proxy buffer. */
                buffer = view->buffer;
            }
        }
        else
            buffer = client_buffer;
    }
    else
        buffer = NULL;

    /* If we no longer need a proxy buffer, or the original buffer is of a
     * different size, destroy the old proxy image. */
    if (view->buffer && ((!needs_proxy && was_proxy)
                         || (needs_proxy && resized)))
    {
        wld_buffer_unreference(view->buffer);
    }

    view->buffer = buffer;

    return true;
}

static void renderer_flush_view(struct compositor_view * view)
{
    if (view->buffer == view->base.buffer)
        return;

    wld_set_target_buffer(swc.shm->renderer, view->buffer);
    wld_copy_region(swc.shm->renderer, view->base.buffer,
                    0, 0, &view->surface->state.damage);
    wld_flush(swc.shm->renderer);
}

/* }}} */

/* Surface Views {{{ */

/**
 * Adds damage from the region below a view, taking into account it's clip
 * region, to the region specified by `damage'.
 */
static void damage_below_view(struct compositor_view * view)
{
    pixman_region32_t damage_below;

    pixman_region32_init_with_extents(&damage_below, &view->extents);
    pixman_region32_subtract(&damage_below, &damage_below, &view->clip);
    pixman_region32_union(&compositor.damage, &compositor.damage,
                          &damage_below);
    pixman_region32_fini(&damage_below);
}

/**
 * Completely damages the surface and its border.
 */
static void damage_view(struct compositor_view * view)
{
    damage_below_view(view);
    view->border.damaged = true;
}

static void update_extents(struct compositor_view * view)
{
    view->extents.x1 = view->base.geometry.x - view->border.width;
    view->extents.y1 = view->base.geometry.y - view->border.width;
    view->extents.x2 = view->base.geometry.x + view->base.geometry.width
        + view->border.width;
    view->extents.y2 = view->base.geometry.y + view->base.geometry.height
        + view->border.width;

    /* Damage border. */
    view->border.damaged = true;
}

static void schedule_updates(uint32_t screens)
{
    if (compositor.scheduled_updates == 0)
        wl_event_loop_add_idle(swc.event_loop, &perform_update, NULL);

    if (screens == -1)
    {
        struct screen * screen;

        screens = 0;
        wl_list_for_each(screen, &swc.screens, link)
            screens |= screen_mask(screen);
    }

    compositor.scheduled_updates |= screens;
}

static bool update(struct view * base)
{
    struct compositor_view * view = (void *) base;

    if (!compositor.active || !view->visible)
        return false;

    schedule_updates(view->base.screens);

    return true;
}

static bool attach(struct view * base, struct wld_buffer * buffer)
{
    struct compositor_view * view = (void *) base;

    if (!renderer_attach(view, buffer))
        return false;

    if (view->visible && view->base.buffer)
    {
        damage_below_view(view);
        update(&view->base);
    }

    view_set_size_from_buffer(&view->base, buffer);

    return true;
}

static bool move(struct view * base, int32_t x, int32_t y)
{
    struct compositor_view * view = (void *) base;

    if (view->visible)
    {
        damage_below_view(view);
        update(&view->base);
    }

    view_set_position(&view->base, x, y);

    return true;
}

const static struct view_impl view_impl = {
    .update = &update,
    .attach = &attach,
    .move = &move
};

static void handle_view_event(struct wl_listener * listener, void * data)
{
    struct compositor_view * view
        = CONTAINER_OF(listener, typeof(*view), event_listener);
    struct swc_event * event = data;

    switch (event->type)
    {
        case VIEW_EVENT_MOVED:
            update_extents(view);

            if (view->visible)
            {
                /* Assume worst-case no clipping until we draw the next frame (in case
                 * the surface gets moved again before that). */
                pixman_region32_init(&view->clip);

                damage_below_view(view);
                view_update_screens(&view->base);
                update(&view->base);
            }
            break;
        case VIEW_EVENT_RESIZED:
            update_extents(view);

            if (view->visible)
            {
                damage_below_view(view);
                view_update_screens(&view->base);
                update(&view->base);
            }
            break;
    }
}

struct compositor_view * swc_compositor_create_view
    (struct swc_surface * surface)
{
    struct compositor_view * view;

    view = malloc(sizeof *view);

    if (!view)
        return NULL;

    view_initialize(&view->base, &view_impl);
    view->event_listener.notify = &handle_view_event;
    wl_signal_add(&view->base.event_signal, &view->event_listener);
    view->surface = surface;
    view->buffer = NULL;
    view->visible = false;
    view->extents.x1 = 0;
    view->extents.y1 = 0;
    view->extents.x2 = 0;
    view->extents.y2 = 0;
    view->border.width = 0;
    view->border.color = 0x000000;
    view->border.damaged = false;
    pixman_region32_init(&view->clip);
    swc_surface_set_view(surface, &view->base);

    return view;
}

void compositor_view_destroy(struct compositor_view * view)
{
    compositor_view_hide(view);
    swc_surface_set_view(view->surface, NULL);
    view_finalize(&view->base);
    pixman_region32_fini(&view->clip);
    free(view);
}

void compositor_view_show(struct compositor_view * view)
{
    if (view->visible)
        return;

    /* Assume worst-case no clipping until we draw the next frame (in case the
     * surface gets moved before that. */
    pixman_region32_clear(&view->clip);

    view->visible = true;
    view_update_screens(&view->base);

    damage_view(view);
    update(&view->base);
    wl_list_insert(&compositor.views, &view->link);
}

void compositor_view_hide(struct compositor_view * view)
{
    if (!view->visible)
        return;

    /* Update all the screens the view was on. */
    update(&view->base);
    damage_below_view(view);

    wl_list_remove(&view->link);
    view_set_screens(&view->base, 0);
    view->visible = false;
}

void compositor_view_set_border_width(struct compositor_view * view,
                                      uint32_t width)
{
    if (view->border.width == width)
        return;

    view->border.width = width;
    view->border.damaged = true;

    /* XXX: Damage above surface for transparent surfaces? */

    update_extents(view);
    update(&view->base);
}

void compositor_view_set_border_color(struct compositor_view * view,
                                      uint32_t color)
{
    if (view->border.color == color)
        return;

    view->border.color = color;
    view->border.damaged = true;

    /* XXX: Damage above surface for transparent surfaces? */

    update(&view->base);
}

/* }}} */

static void calculate_damage()
{
    struct compositor_view * view;
    pixman_region32_t surface_opaque, * surface_damage;

    pixman_region32_clear(&compositor.opaque);
    pixman_region32_init(&surface_opaque);

    /* Go through views top-down to calculate clipping regions. */
    wl_list_for_each(view, &compositor.views, link)
    {
        /* Clip the surface by the opaque region covering it. */
        pixman_region32_copy(&view->clip, &compositor.opaque);

        /* Translate the opaque region to global coordinates. */
        pixman_region32_copy(&surface_opaque, &view->surface->state.opaque);
        pixman_region32_translate(&surface_opaque,
                                  view->base.geometry.x, view->base.geometry.y);

        /* Add the surface's opaque region to the accumulated opaque
         * region. */
        pixman_region32_union(&compositor.opaque, &compositor.opaque,
                              &surface_opaque);

        surface_damage = &view->surface->state.damage;

        if (pixman_region32_not_empty(surface_damage))
        {
            renderer_flush_view(view);

            /* Translate surface damage to global coordinates. */
            pixman_region32_translate
                (surface_damage, view->base.geometry.x, view->base.geometry.y);

            /* Add the surface damage to the compositor damage. */
            pixman_region32_union(&compositor.damage, &compositor.damage,
                                  surface_damage);
            pixman_region32_clear(surface_damage);
        }

        if (view->border.damaged)
        {
            pixman_region32_t border_region, view_region;

            pixman_region32_init_with_extents(&border_region, &view->extents);
            pixman_region32_init_rect
                (&view_region, view->base.geometry.x, view->base.geometry.y,
                 view->base.geometry.width, view->base.geometry.height);

            pixman_region32_subtract(&border_region, &border_region,
                                     &view_region);

            pixman_region32_union(&compositor.damage, &compositor.damage,
                                  &border_region);

            pixman_region32_fini(&border_region);
            pixman_region32_fini(&view_region);

            view->border.damaged = false;
        }
    }

    pixman_region32_fini(&surface_opaque);
}

static void update_screen(struct screen * screen)
{
    struct target * target;
    const struct swc_rectangle * geometry = &screen->base.geometry;
    pixman_region32_t damage;

    if (!(compositor.scheduled_updates & screen_mask(screen)))
        return;

    if (!(target = target_get(screen)))
        return;

    pixman_region32_init(&damage);
    pixman_region32_intersect_rect(&damage, &compositor.damage,
                                   geometry->x, geometry->y,
                                   geometry->width, geometry->height);
    pixman_region32_translate(&damage, -geometry->x, -geometry->y);
    pixman_region32_union(&target->next_buffer->damage,
                          &target->next_buffer->damage, &damage);
    pixman_region32_fini(&damage);

    /* Don't repaint the screen if it is waiting for a page flip. */
    if (compositor.pending_flips & screen_mask(screen))
        return;

    pixman_region32_t * total_damage, base_damage;

    total_damage = wld_surface_damage(target->surface,
                                      &target->next_buffer->damage);
    pixman_region32_translate(total_damage, geometry->x, geometry->y);
    pixman_region32_init(&base_damage);
    pixman_region32_subtract(&base_damage, total_damage, &compositor.opaque);
    renderer_repaint(target, total_damage, &base_damage, &compositor.views);
    pixman_region32_fini(&base_damage);
    target_swap_buffers(target);
}

static void perform_update(void * data)
{
    struct screen * screen;
    uint32_t updates = compositor.scheduled_updates
                     & ~compositor.pending_flips;

    if (!compositor.active || !updates)
        return;

    DEBUG("Performing update\n");

    compositor.updating = true;
    calculate_damage();

    wl_list_for_each(screen, &swc.screens, link)
        update_screen(screen);

    /* XXX: Should assert that all damage was covered by some output */
    pixman_region32_clear(&compositor.damage);
    compositor.pending_flips |= updates;
    compositor.scheduled_updates &= ~updates;
    compositor.updating = false;
}

bool handle_motion(struct pointer_handler * handler, uint32_t time,
                   wl_fixed_t fx, wl_fixed_t fy)
{
    struct compositor_view * view;
    struct swc_surface * surface = NULL;
    int32_t x, y;

    wl_list_for_each(view, &compositor.views, link)
    {
        x = wl_fixed_to_int(fx);
        y = wl_fixed_to_int(fy);

        if (swc_rectangle_contains_point(&view->base.geometry, x, y)
            && pixman_region32_contains_point(&view->surface->state.input,
                                              x - view->base.geometry.x,
                                              y - view->base.geometry.y, NULL))
        {
            surface = view->surface;
            break;
        }
    }

    pointer_set_focus(swc.seat->pointer, surface);

    return false;
}

static void handle_terminate(void * data, uint32_t time,
                             uint32_t value, uint32_t state)
{
    if (state == WL_KEYBOARD_KEY_STATE_PRESSED)
        wl_display_terminate(swc.display);
}

static void handle_switch_vt(void * data, uint32_t time,
                             uint32_t value, uint32_t state)
{
    uint8_t vt = value - XKB_KEY_XF86Switch_VT_1 + 1;

    if (state == WL_KEYBOARD_KEY_STATE_PRESSED)
        swc_launch_activate_vt(vt);
}

static void handle_launch_event(struct wl_listener * listener, void * data)
{
    struct swc_event * event = data;
    struct screen * screen;

    switch (event->type)
    {
        case SWC_LAUNCH_EVENT_ACTIVATED:
            compositor.active = true;
            schedule_updates(-1);
            wl_list_for_each(screen, &swc.screens, link)
                screen->planes.framebuffer.need_modeset = true;
            break;
        case SWC_LAUNCH_EVENT_DEACTIVATED:
            compositor.active = false;
            compositor.scheduled_updates = 0;
            break;
    }
}

static struct wl_listener launch_listener = {
    .notify = &handle_launch_event
};

static void create_surface(struct wl_client * client,
                           struct wl_resource * resource, uint32_t id)
{
    struct swc_surface * surface;

    /* Initialize surface. */
    surface = swc_surface_new(client, wl_resource_get_version(resource), id);

    if (!surface)
    {
        wl_resource_post_no_memory(resource);
        return;
    }
}

static void create_region(struct wl_client * client,
                          struct wl_resource * resource, uint32_t id)
{
    struct swc_region * region;

    region = swc_region_new(client, id);

    if (!region)
        wl_resource_post_no_memory(resource);
}

static struct wl_compositor_interface compositor_implementation = {
    .create_surface = &create_surface,
    .create_region = &create_region
};

static void bind_compositor(struct wl_client * client, void * data,
                            uint32_t version, uint32_t id)
{
    struct swc_compositor * compositor = data;
    struct wl_resource * resource;

    if (version >= 3)
        version = 3;

    resource = wl_resource_create(client, &wl_compositor_interface,
                                  version, id);
    wl_resource_set_implementation(resource, &compositor_implementation,
                                   compositor, NULL);
}

bool swc_compositor_initialize()
{
    struct screen * screen;
    uint32_t keysym;

    compositor.global = wl_global_create
        (swc.display, &wl_compositor_interface, 3, NULL, &bind_compositor);

    if (!compositor.global)
        return false;

    compositor.scheduled_updates = 0;
    compositor.pending_flips = 0;
    compositor.active = true;
    compositor.updating = false;
    pixman_region32_init(&compositor.damage);
    pixman_region32_init(&compositor.opaque);
    wl_list_init(&compositor.views);
    wl_signal_add(&swc.launch->event_signal, &launch_listener);

    wl_list_for_each(screen, &swc.screens, link)
        target_new(screen);

    swc_add_binding(SWC_BINDING_KEY, SWC_MOD_CTRL | SWC_MOD_ALT,
                    XKB_KEY_BackSpace, &handle_terminate, NULL);

    for (keysym = XKB_KEY_XF86Switch_VT_1;
         keysym <= XKB_KEY_XF86Switch_VT_12;
         ++keysym)
    {
        swc_add_binding(SWC_BINDING_KEY, SWC_MOD_ANY, keysym,
                        &handle_switch_vt, NULL);
    }


    return true;
}

void swc_compositor_finalize()
{
    pixman_region32_fini(&compositor.damage);
    pixman_region32_fini(&compositor.opaque);
    wl_global_destroy(compositor.global);
}

