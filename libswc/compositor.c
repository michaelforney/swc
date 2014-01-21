#include "swc.h"
#include "buffer.h"
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

struct screen
{
    struct wld_surface * surface;
    struct wld_buffer * next_buffer, * current_buffer;
    struct swc_view * view;
    struct wl_listener view_listener;
    struct wl_listener event_listener;
};

struct view
{
    struct swc_view base;
    struct wl_listener event_listener;
    struct swc_compositor * compositor;
    struct swc_surface * surface;
    struct wld_buffer * wld;

    /* The box that the surface covers (including it's border). */
    pixman_box32_t extents;

    /* The region that is covered by opaque regions of surfaces above this
     * surface. */
    pixman_region32_t clip;

    struct
    {
        uint32_t width;
        uint32_t color;
        bool damaged;
    } border;

    struct wl_list link;
};

static void perform_update(void * data);

static void buffer_destroy(void * data)
{
    struct swc_buffer * buffer = data;

    swc_buffer_finalize(buffer);
    free(buffer);
}

struct swc_buffer * buffer_get(struct wld_buffer * wld)
{
    if (wld->destroy_data)
        return wld->data;

    struct swc_buffer * buffer;

    if (!(buffer = malloc(sizeof *buffer)))
        goto error0;

    if (!swc_buffer_initialize(buffer, wld))
        goto error1;

    wld->data = buffer;
    wld->destroy_data = &buffer_destroy;

    return buffer;

error1:
    free(buffer);
error0:
    return NULL;
}

static void handle_screen_event(struct wl_listener * listener, void * data)
{
    struct swc_event * event = data;

    if (event->type == SWC_SCREEN_DESTROYED)
    {
        struct screen * screen
            = CONTAINER_OF(listener, typeof(*screen), event_listener);

        wld_destroy_surface(screen->surface);
        free(screen);
    }
}

static struct screen * screen_get(struct swc_screen * base)
{
    struct wl_listener * listener
        = wl_signal_get(&base->event_signal, &handle_screen_event);
    struct screen * screen;

    return listener ? CONTAINER_OF(listener, typeof(*screen), event_listener)
        : NULL;
}

static void handle_screen_view_event(struct wl_listener * listener, void * data)
{
    struct swc_event * event = data;
    struct swc_view_event_data * event_data = event->data;

    switch (event->type)
    {
        case SWC_VIEW_EVENT_FRAME:
        {
            struct swc_screen_internal * base = CONTAINER_OF
                (event_data->view, typeof(*base), planes.framebuffer.view);
            struct screen * screen;
            struct view * view;

            if (!(screen = screen_get(&base->base)))
                return;

            swc.compositor->pending_flips &= ~swc_screen_mask(base);

            wl_list_for_each(view, &swc.compositor->views, link)
            {
                if (view->base.screens & swc_screen_mask(base))
                    swc_view_frame(&view->base, event_data->frame.time);
            }

            if (screen->current_buffer)
                wld_surface_release(screen->surface, screen->current_buffer);

            screen->current_buffer = screen->next_buffer;

            /* If we had scheduled updates that couldn't run because we were
             * waiting on a page flip, run them now. */
            if (swc.compositor->scheduled_updates)
                perform_update(swc.compositor);
            break;
        }
    }
}

static bool screen_swap_buffers(struct screen * screen)
{
    struct swc_buffer * buffer;

    screen->next_buffer = wld_surface_take(screen->surface);
    buffer = buffer_get(screen->next_buffer);

    if (!swc_view_attach(screen->view, buffer))
    {
        ERROR("Failed to attach next frame to screen\n");
        return false;
    }

    return true;
}

static struct screen * screen_new(struct swc_screen_internal * base)
{
    struct screen * screen;

    if (!(screen = malloc(sizeof *screen)))
        goto error0;

    screen->surface = wld_create_surface(swc.drm->context,
                                         base->base.geometry.width,
                                         base->base.geometry.height,
                                         WLD_FORMAT_XRGB8888);

    if (!screen->surface)
        goto error1;

    screen->view = &base->planes.framebuffer.view;
    screen->view_listener.notify = &handle_screen_view_event;
    wl_signal_add(&screen->view->event_signal, &screen->view_listener);
    screen->event_listener.notify = &handle_screen_event;
    wl_signal_add(&base->base.event_signal, &screen->event_listener);
    screen->current_buffer = NULL;
    screen_swap_buffers(screen);

    return screen;

error1:
    free(screen);
error0:
    return NULL;
}

/* Rendering {{{ */

struct render_target
{
    struct wld_surface * surface;
    const struct swc_rectangle * geometry;
    uint32_t mask;
};

static void repaint_view(struct render_target * target, struct view * view,
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
        wld_copy_region(swc.drm->renderer, view->wld,
                        geometry->x - target->geometry->x,
                        geometry->y - target->geometry->y, &view_damage);
    }

    pixman_region32_fini(&view_damage);

    /* Draw border */
    if (pixman_region32_not_empty(&border_damage))
    {
        DEBUG("\t\tRedrawing border\n");

        pixman_region32_translate(&border_damage,
                                  -target->geometry->x, -target->geometry->y);
        wld_fill_region(swc.drm->renderer, view->border.color, &border_damage);
    }

    pixman_region32_fini(&border_damage);
}

static void renderer_repaint(struct render_target * target,
                             pixman_region32_t * damage,
                             pixman_region32_t * base_damage,
                             struct wl_list * views)
{
    struct view * view;

    DEBUG("Rendering to target { x: %d, y: %d, w: %u, h: %u }\n",
          target->geometry->x, target->geometry->y,
          target->geometry->width, target->geometry->height);

    wld_set_target_surface(swc.drm->renderer, target->surface);

    /* Paint base damage black. */
    if (pixman_region32_not_empty(base_damage))
    {
        pixman_region32_translate(base_damage,
                                  -target->geometry->x, -target->geometry->y);
        wld_fill_region(swc.drm->renderer, 0xff000000, base_damage);
    }

    wl_list_for_each_reverse(view, views, link)
    {
        if (view->base.screens & target->mask)
            repaint_view(target, view, damage);
    }

    wld_flush(swc.drm->renderer);
}

static bool renderer_attach(struct view * view, struct swc_buffer * buffer)
{
    struct wld_buffer * wld;
    bool was_proxy = view->base.buffer && view->wld != view->base.buffer->wld;
    bool needs_proxy = buffer
        && !(wld_capabilities(swc.drm->renderer,
                              buffer->wld) & WLD_CAPABILITY_READ);
    bool resized = view->wld && buffer
        && (view->wld->width != buffer->wld->width
            || view->wld->height != buffer->wld->height);

    if (buffer)
    {
        /* Create a proxy buffer if necessary (for example a hardware buffer
         * backing a SHM buffer). */
        if (needs_proxy)
        {
            if (!was_proxy || resized)
            {
                DEBUG("Creating a proxy buffer\n");
                wld = wld_create_buffer(swc.drm->context,
                                        buffer->wld->width, buffer->wld->height,
                                        buffer->wld->format);

                if (!wld)
                    return false;
            }
            else
            {
                /* Otherwise we can keep the original proxy buffer. */
                wld = view->wld;
            }
        }
        else
            wld = buffer->wld;
    }
    else
        wld = NULL;

    /* If we no longer need a proxy buffer, or the original buffer is of a
     * different size, destroy the old proxy image. */
    if (view->wld && ((!needs_proxy && was_proxy) || (needs_proxy && resized)))
        wld_destroy_buffer(view->wld);

    view->wld = wld;

    return true;
}

static void renderer_flush_view(struct view * view)
{
    if (view->wld == view->base.buffer->wld)
        return;

    wld_set_target_buffer(swc.shm->renderer, view->wld);
    wld_copy_region(swc.shm->renderer, view->base.buffer->wld,
                    0, 0, &view->surface->state.damage);
    wld_flush(swc.shm->renderer);
}

/* }}} */

/* Surface Views {{{ */

/**
 * Adds damage from the region below a view, taking into account it's clip
 * region, to the region specified by `damage'.
 */
static void damage_below_view(struct view * view)
{
    struct swc_compositor * compositor = view->compositor;
    pixman_region32_t damage_below;

    pixman_region32_init_with_extents(&damage_below, &view->extents);
    pixman_region32_subtract(&damage_below, &damage_below, &view->clip);
    pixman_region32_union(&compositor->damage, &compositor->damage,
                          &damage_below);
    pixman_region32_fini(&damage_below);
}

/**
 * Completely damages the surface and its border.
 */
static void damage_view(struct view * view)
{
    damage_below_view(view);
    view->border.damaged = true;
}

static void update_extents(struct view * view)
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

static void schedule_updates(struct swc_compositor * compositor, uint32_t screens)
{
    if (compositor->scheduled_updates == 0)
        wl_event_loop_add_idle(swc.event_loop, &perform_update, NULL);

    compositor->scheduled_updates |= screens;
}

static bool update(struct swc_view * base)
{
    struct view * view = (void *) base;

    if (!view->base.visible)
        return false;

    schedule_updates(view->compositor, view->base.screens);

    return true;
}

static void remove_(struct swc_view * base)
{
    struct view * view = (void *) base;

    swc_compositor_surface_hide(view->surface);
    pixman_region32_fini(&view->clip);
    free(view);
}

static bool attach(struct swc_view * base, struct swc_buffer * buffer)
{
    struct view * view = (void *) base;

    if (!renderer_attach(view, buffer))
        return false;

    return true;
}

static bool move(struct swc_view * base, int32_t x, int32_t y)
{
    struct view * view = (void *) base;

    if (view->base.visible)
    {
        damage_below_view(view);
        update(&view->base);
    }

    return true;
}

static void resize(struct swc_view * base)
{
    struct view * view = (void *) base;

    if (view->base.visible)
    {
        damage_below_view(view);
        update(&view->base);
    }
}

const static struct swc_view_impl view_impl = {
    .update = &update,
    .attach = &attach,
    .move = &move,
    .resize = &resize,
    .remove = &remove_,
};

static void handle_view_event(struct wl_listener * listener, void * data)
{
    struct view * view = CONTAINER_OF(listener, typeof(*view), event_listener);
    struct swc_event * event = data;

    switch (event->type)
    {
        case SWC_VIEW_EVENT_MOVED:
            update_extents(view);

            if (view->base.visible)
            {
                /* Assume worst-case no clipping until we draw the next frame (in case
                 * the surface gets moved again before that). */
                pixman_region32_init(&view->clip);

                damage_below_view(view);
                update(&view->base);
            }
            break;
        case SWC_VIEW_EVENT_RESIZED:
            update_extents(view);

            if (view->base.visible)
            {
                damage_below_view(view);
                update(&view->base);
            }
            break;
    }
}

bool swc_compositor_add_surface(struct swc_compositor * compositor,
                                struct swc_surface * surface)
{
    struct view * view;

    view = malloc(sizeof *view);

    if (!view)
        return false;

    swc_view_initialize(&view->base, &view_impl);
    view->base.visible = false;
    view->event_listener.notify = &handle_view_event;
    wl_signal_add(&view->base.event_signal, &view->event_listener);
    view->compositor = compositor;
    view->surface = surface;
    view->wld = NULL;
    view->extents.x1 = 0;
    view->extents.y1 = 0;
    view->extents.x2 = 0;
    view->extents.y2 = 0;
    view->border.width = 0;
    view->border.color = 0x000000;
    view->border.damaged = false;
    pixman_region32_init(&view->clip);
    swc_surface_set_view(surface, &view->base);

    return true;
}

void swc_compositor_surface_show(struct swc_surface * surface)
{
    struct view * view = (void *) surface->view;

    if (view->base.impl != &view_impl)
        return;

    if (view->base.visible)
        return;

    printf("showing surface %u\n", wl_resource_get_id(surface->resource));

    /* Assume worst-case no clipping until we draw the next frame (in case the
     * surface gets moved before that. */
    pixman_region32_clear(&view->clip);

    damage_view(view);
    swc_view_set_visibility(&view->base, true);
    update(&view->base);
    wl_list_insert(&view->compositor->views, &view->link);
}

void swc_compositor_surface_hide(struct swc_surface * surface)
{
    struct view * view = (void *) surface->view;

    if (view->base.impl != &view_impl)
        return;

    if (!view->base.visible)
        return;

    /* Update all the outputs the surface was on. */
    update(&view->base);

    damage_below_view(view);
    swc_view_set_visibility(&view->base, false);
    wl_list_remove(&view->link);
}

void swc_compositor_surface_set_border_width(struct swc_surface * surface,
                                             uint32_t width)
{
    struct view * view = (void *) surface->view;

    if (view->border.width == width)
        return;

    view->border.width = width;
    view->border.damaged = true;

    /* XXX: Damage above surface for transparent surfaces? */

    update_extents(view);
    update(&view->base);
}

void swc_compositor_surface_set_border_color(struct swc_surface * surface,
                                             uint32_t color)
{
    struct view * view = (void *) surface->view;

    if (view->border.color == color)
        return;

    view->border.color = color;
    view->border.damaged = true;

    /* XXX: Damage above surface for transparent surfaces? */

    update(&view->base);
}

/* }}} */

static void calculate_damage(struct swc_compositor * compositor)
{
    struct view * view;
    pixman_region32_t surface_opaque, * surface_damage;

    pixman_region32_clear(&compositor->opaque);
    pixman_region32_init(&surface_opaque);

    /* Go through views top-down to calculate clipping regions. */
    wl_list_for_each(view, &compositor->views, link)
    {
        /* Clip the surface by the opaque region covering it. */
        pixman_region32_copy(&view->clip, &compositor->opaque);

        /* Translate the opaque region to global coordinates. */
        pixman_region32_copy(&surface_opaque, &view->surface->state.opaque);
        pixman_region32_translate(&surface_opaque,
                                  view->base.geometry.x, view->base.geometry.y);

        /* Add the surface's opaque region to the accumulated opaque
         * region. */
        pixman_region32_union(&compositor->opaque, &compositor->opaque,
                              &surface_opaque);

        surface_damage = &view->surface->state.damage;

        if (pixman_region32_not_empty(surface_damage))
        {
            renderer_flush_view(view);

            /* Translate surface damage to global coordinates. */
            pixman_region32_translate
                (surface_damage, view->base.geometry.x, view->base.geometry.y);

            /* Add the surface damage to the compositor damage. */
            pixman_region32_union(&compositor->damage, &compositor->damage,
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

            pixman_region32_union(&compositor->damage, &compositor->damage,
                                  &border_region);

            pixman_region32_fini(&border_region);
            pixman_region32_fini(&view_region);

            view->border.damaged = false;
        }
    }

    pixman_region32_fini(&surface_opaque);
}

static void update_screen(struct swc_compositor * compositor,
                          struct swc_screen_internal * base)
{
    struct screen * screen;
    const struct swc_rectangle * geometry = &base->base.geometry;
    pixman_region32_t damage;

    if (!(compositor->scheduled_updates & swc_screen_mask(base)))
        return;

    if (!(screen = screen_get(&base->base)))
        return;

    pixman_region32_init(&damage);
    pixman_region32_intersect_rect(&damage, &compositor->damage,
                                   geometry->x, geometry->y,
                                   geometry->width, geometry->height);
    pixman_region32_translate(&damage, -geometry->x, -geometry->y);
    pixman_region32_union(&screen->next_buffer->damage,
                          &screen->next_buffer->damage, &damage);

    /* Don't repaint the screen if it is waiting for a page flip. */
    if (compositor->pending_flips & swc_screen_mask(base))
        return;

    struct render_target target;
    pixman_region32_t * total_damage, base_damage;

    total_damage = wld_surface_damage(screen->surface,
                                      &screen->next_buffer->damage);
    pixman_region32_translate(total_damage, geometry->x, geometry->y);
    pixman_region32_init(&base_damage);
    pixman_region32_subtract(&base_damage, total_damage, &compositor->opaque);

    target.surface = screen->surface;
    target.mask = swc_screen_mask(base);
    target.geometry = geometry;

    renderer_repaint(&target, total_damage, &base_damage, &compositor->views);
    pixman_region32_fini(&base_damage);
    screen_swap_buffers(screen);
}

static void perform_update(void * data)
{
    struct swc_compositor * compositor = data;
    struct swc_screen_internal * screen;
    uint32_t updates = compositor->scheduled_updates
                     & ~compositor->pending_flips;

    if (!updates)
        return;

    DEBUG("Performing update\n");

    calculate_damage(compositor);

    wl_list_for_each(screen, &swc.screens, link)
        update_screen(compositor, screen);

    /* XXX: Should assert that all damage was covered by some output */
    pixman_region32_clear(&compositor->damage);
    compositor->pending_flips |= updates;
    compositor->scheduled_updates &= ~updates;
}

static void handle_focus(struct swc_pointer * pointer)
{
    /* XXX: Temporary hack */
    struct swc_compositor * compositor = swc.compositor;
    struct view * view;
    int32_t x, y;

    wl_list_for_each(view, &compositor->views, link)
    {
        x = wl_fixed_to_int(pointer->x);
        y = wl_fixed_to_int(pointer->y);

        if (swc_rectangle_contains_point(&view->base.geometry, x, y)
            && pixman_region32_contains_point(&view->surface->state.input,
                                              x - view->base.geometry.x,
                                              y - view->base.geometry.y, NULL))
        {
            swc_pointer_set_focus(pointer, view->surface);
            return;
        }
    }

    swc_pointer_set_focus(pointer, NULL);
}

static bool handle_motion(struct swc_pointer * pointer, uint32_t time)
{
    return false;
}

static void handle_terminate(uint32_t time, uint32_t value, void * data)
{
    struct wl_display * display = data;
    printf("handling terminate\n");
    wl_display_terminate(display);
}

static void handle_switch_vt(uint32_t time, uint32_t value, void * data)
{
    uint8_t vt = value - XKB_KEY_XF86Switch_VT_1 + 1;
    printf("handle switch vt%u\n", vt);
    swc_launch_activate_vt(vt);
}

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

bool swc_compositor_initialize(struct swc_compositor * compositor,
                               struct wl_display * display,
                               struct wl_event_loop * event_loop)
{
    struct swc_screen_internal * screen;
    uint32_t keysym;

    wl_global_create(swc.display, &wl_compositor_interface, 3, compositor,
                     &bind_compositor);

    compositor->display = display;
    compositor->scheduled_updates = 0;
    compositor->pending_flips = 0;
    compositor->pointer_handler = (struct swc_pointer_handler) {
        .focus = &handle_focus,
        .motion = &handle_motion
    };

    pixman_region32_init(&compositor->damage);
    pixman_region32_init(&compositor->opaque);
    wl_list_init(&compositor->views);

    wl_list_for_each(screen, &swc.screens, link)
        screen_new(screen);

    swc_add_key_binding(SWC_MOD_CTRL | SWC_MOD_ALT, XKB_KEY_BackSpace,
                        &handle_terminate, display);

    for (keysym = XKB_KEY_XF86Switch_VT_1;
         keysym <= XKB_KEY_XF86Switch_VT_12;
         ++keysym)
    {
        swc_add_key_binding(SWC_MOD_ANY, keysym,
                            &handle_switch_vt, NULL);
    }


    return true;
}

void swc_compositor_finish(struct swc_compositor * compositor)
{
}

