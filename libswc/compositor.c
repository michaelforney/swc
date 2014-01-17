#include "swc.h"
#include "buffer.h"
#include "compositor.h"
#include "data_device_manager.h"
#include "drm.h"
#include "internal.h"
#include "output.h"
#include "pointer.h"
#include "region.h"
#include "screen.h"
#include "seat.h"
#include "surface.h"
#include "util.h"
#include "view.h"

#include <stdlib.h>
#include <stdio.h>
#include <assert.h>
#include <wld/wld.h>
#include <wld/drm.h>
#include <xkbcommon/xkbcommon-keysyms.h>

struct view
{
    struct swc_view base;
    struct wl_listener event_listener;
    struct swc_compositor * compositor;
    struct swc_surface * surface;

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

/* Rendering {{{ */

struct render_target
{
    struct wld_buffer * buffer;
    struct swc_rectangle geometry;
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
        wld_copy_region(swc.drm->renderer, view->base.buffer->wld,
                        geometry->x - target->geometry.x,
                        geometry->y - target->geometry.y, &view_damage);
    }

    pixman_region32_fini(&view_damage);

    /* Draw border */
    if (pixman_region32_not_empty(&border_damage))
    {
        DEBUG("\t\tRedrawing border\n");

        pixman_region32_translate(&border_damage,
                                  -target->geometry.x, -target->geometry.y);
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
          target->geometry.x, target->geometry.y,
          target->geometry.width, target->geometry.height);

    /* Paint base damage black. */
    if (pixman_region32_not_empty(base_damage))
    {
        pixman_region32_translate(base_damage,
                                  -target->geometry.x, -target->geometry.y);
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
    return true;
}

static void renderer_flush_view(struct view * view)
{
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

static bool update(struct swc_view * base)
{
    struct view * view = (void *) base;
    struct swc_screen_internal * screen;

    if (!view->base.visible)
        return false;

    wl_list_for_each(screen, &swc.screens, link)
    {
        if (view->base.screens & swc_screen_mask(screen))
            swc_compositor_schedule_update(view->compositor, screen);
    }

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

static void repaint_screen(struct swc_compositor * compositor,
                           struct swc_screen_internal * screen,
                           pixman_region32_t * damage)
{
    struct swc_output * output
        = CONTAINER_OF(screen->outputs.next, typeof(*output), link);
    pixman_region32_t base_damage;
    struct render_target target;

    pixman_region32_init(&base_damage);
    pixman_region32_subtract(&base_damage, damage, &compositor->opaque);

    target.buffer = swc_plane_get_buffer(&output->framebuffer_plane);
    target.geometry.x = screen->base.geometry.x + output->framebuffer_plane.x;
    target.geometry.y = screen->base.geometry.y + output->framebuffer_plane.y;
    target.geometry.width = target.buffer->width;
    target.geometry.height = target.buffer->height;
    target.mask = swc_screen_mask(screen);

    renderer_repaint(&target, damage, &base_damage, &compositor->views);

    pixman_region32_fini(&base_damage);

    if (!swc_plane_flip(&output->framebuffer_plane))
        fprintf(stderr, "Plane flip failed\n");
}

static void update_output_damage(struct swc_output * output,
                                 pixman_region32_t * damage)
{
    struct swc_rectangle * geometry = &output->screen->base.geometry;

    pixman_region32_union
        (&output->current_damage, &output->current_damage, damage);
    pixman_region32_intersect_rect
        (&output->current_damage, &output->current_damage,
         geometry->x, geometry->y, geometry->width, geometry->height);
}

static void flush_output_damage(struct swc_output * output,
                                pixman_region32_t * damage)
{
    /* The total damage is composed of the damage from the new frame, and the
     * damage from the last frame. */
    pixman_region32_union(damage,
                          &output->current_damage, &output->previous_damage);

    /* We must save the damage from the previous frame because the back buffer
     * is also damaged in this region. */
    pixman_region32_copy(&output->previous_damage, &output->current_damage);
    pixman_region32_clear(&output->current_damage);
}

static void perform_update(void * data)
{
    struct swc_compositor * compositor = data;
    struct swc_screen_internal * screen;
    struct swc_output * output;
    pixman_region32_t damage;
    uint32_t updates = compositor->scheduled_updates
                     & ~compositor->pending_flips;

    if (!updates)
        return;

    DEBUG("Performing update\n");

    calculate_damage(compositor);
    pixman_region32_init(&damage);

    wl_list_for_each(screen, &swc.screens, link)
    {
        if (!(compositor->scheduled_updates & swc_screen_mask(screen)))
            continue;

        output = CONTAINER_OF(screen->outputs.next, typeof(*output), link);
        update_output_damage(output, &compositor->damage);

        /* Don't repaint the output if it is waiting for a page flip. */
        if (compositor->pending_flips & swc_screen_mask(screen))
            continue;

        flush_output_damage(output, &damage);
        repaint_screen(compositor, screen, &damage);
    }

    pixman_region32_fini(&damage);
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

static void handle_drm_event(struct wl_listener * listener, void * data)
{
    struct swc_event * event = data;
    struct swc_compositor * compositor;

    compositor = CONTAINER_OF(listener, typeof(*compositor), drm_listener);

    switch (event->type)
    {
        case SWC_DRM_PAGE_FLIP:
        {
            struct swc_drm_event_data * event_data = event->data;
            struct view * view;

            compositor->pending_flips &= ~SWC_OUTPUT_MASK(event_data->output);

            if (compositor->pending_flips == 0)
            {
                wl_list_for_each(view, &compositor->views, link)
                    swc_view_frame(&view->base, event_data->time);
            }

            /* If we had scheduled updates that couldn't run because we were
             * waiting on a page flip, run them now. */
            if (compositor->scheduled_updates)
                perform_update(compositor);

            break;
        }
    }
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
    uint32_t keysym;

    compositor->display = display;
    compositor->drm_listener.notify = &handle_drm_event;
    compositor->scheduled_updates = 0;
    compositor->pending_flips = 0;
    compositor->pointer_handler = (struct swc_pointer_handler) {
        .focus = &handle_focus,
        .motion = &handle_motion
    };

    wl_signal_add(&swc.drm->event_signal, &compositor->drm_listener);

    pixman_region32_init(&compositor->damage);
    pixman_region32_init(&compositor->opaque);
    wl_list_init(&compositor->views);

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

void swc_compositor_add_globals(struct swc_compositor * compositor,
                                struct wl_display * display)
{
    wl_global_create(display, &wl_compositor_interface, 3, compositor,
                     &bind_compositor);
}

void swc_compositor_schedule_update(struct swc_compositor * compositor,
                                    struct swc_screen_internal * screen)
{
    bool update_scheduled = compositor->scheduled_updates != 0;

    if (compositor->scheduled_updates & swc_screen_mask(screen))
        return;

    compositor->scheduled_updates |= swc_screen_mask(screen);

    if (!update_scheduled)
    {
        struct wl_event_loop * event_loop;

        event_loop = wl_display_get_event_loop(compositor->display);
        wl_event_loop_add_idle(event_loop, &perform_update, compositor);
    }
}

