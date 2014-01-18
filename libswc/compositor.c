#include "swc.h"
#include "buffer.h"
#include "compositor.h"
#include "data_device_manager.h"
#include "drm.h"
#include "internal.h"
#include "output.h"
#include "pointer.h"
#include "region.h"
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

    bool mapped;

    struct wl_listener surface_event_listener;
};

/* Rendering {{{ */

struct render_target
{
    struct wld_buffer * buffer;
    struct swc_rectangle geometry;
};

static void repaint_surface(struct render_target * target,
                            struct swc_surface * surface,
                            pixman_region32_t * damage)
{
    pixman_region32_t surface_damage;
    pixman_region32_t border_damage;
    pixman_region32_t surface_region;
    struct view * view = (void *) surface->view;

    if (!surface->state.buffer)
        return;

    pixman_region32_init_with_extents(&surface_damage, &view->extents);
    pixman_region32_init(&border_damage);
    pixman_region32_init_rect
        (&surface_region, surface->geometry.x, surface->geometry.y,
         surface->geometry.width, surface->geometry.height);

    pixman_region32_intersect(&surface_damage, &surface_damage, damage);
    pixman_region32_subtract(&surface_damage, &surface_damage, &view->clip);
    pixman_region32_subtract(&border_damage, &surface_damage, &surface_region);
    pixman_region32_intersect(&surface_damage, &surface_damage,
                              &surface_region);

    pixman_region32_fini(&surface_region);

    if (pixman_region32_not_empty(&surface_damage))
    {
        DEBUG("\tDRM surface %u { x: %d, y: %d, w: %u, h: %u }\n",
              wl_resource_get_id(surface->resource),
              surface->geometry.x, surface->geometry.y,
              surface->geometry.width, surface->geometry.height);

        pixman_region32_translate(&surface_damage,
                                  -surface->geometry.x, -surface->geometry.y);
        wld_copy_region(swc.drm->renderer, surface->state.buffer->wld,
                        surface->geometry.x - target->geometry.x,
                        surface->geometry.y - target->geometry.y,
                        &surface_damage);
    }

    pixman_region32_fini(&surface_damage);

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
                             struct wl_list * surfaces)
{
    struct swc_surface * surface;

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

    wl_list_for_each_reverse(surface, surfaces, link)
    {
        if (swc_rectangle_overlap(&target->geometry, &surface->geometry))
            repaint_surface(target, surface, damage);
    }

    wld_flush(swc.drm->renderer);
}

static void renderer_attach(struct view * view, struct swc_buffer * buffer)
{
}

static void renderer_flush_surface(struct swc_surface * surface)
{
}

/* }}} */

/* Surface Views {{{ */

/**
 * Adds damage from the region below a surface, taking into account it's clip
 * region, to the region specified by `damage'.
 */
static void damage_below_surface(struct swc_surface * surface)
{
    struct view * view = (void *) surface->view;
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
static void damage_surface(struct swc_surface * surface)
{
    struct view * view = (void *) surface->view;
    printf("damaging surface\n");

    pixman_region32_fini(&surface->state.damage);
    pixman_region32_init_rect(&surface->state.damage, 0, 0,
                              surface->geometry.width,
                              surface->geometry.height);
    view->border.damaged = true;
}

static void update_extents(struct swc_surface * surface)
{
    struct view * view = (void *) surface->view;

    view->extents.x1 = surface->geometry.x - view->border.width;
    view->extents.y1 = surface->geometry.y - view->border.width;
    view->extents.x2 = surface->geometry.x + surface->geometry.width
        + view->border.width;
    view->extents.y2 = surface->geometry.y + surface->geometry.height
        + view->border.width;

    /* Damage border. */
    view->border.damaged = true;
}

static void update_outputs(struct swc_surface * surface)
{
    struct view * view = (void *) surface->view;
    struct swc_compositor * compositor = view->compositor;
    uint32_t old_outputs = surface->outputs, new_outputs = 0,
             entered_outputs, left_outputs, changed_outputs;
    struct swc_output * output;
    struct wl_client * client;
    struct wl_resource * resource;

    if (view->mapped)
    {
        wl_list_for_each(output, &compositor->outputs, link)
        {
            if (swc_rectangle_overlap(&output->geometry, &surface->geometry))
                new_outputs |= SWC_OUTPUT_MASK(output);
        }
    }

    if (new_outputs == old_outputs)
        return;

    entered_outputs = new_outputs & ~old_outputs;
    left_outputs = old_outputs & ~new_outputs;
    changed_outputs = old_outputs ^ new_outputs;

    wl_list_for_each(output, &compositor->outputs, link)
    {
        if (!(changed_outputs & SWC_OUTPUT_MASK(output)))
            continue;

        client = wl_resource_get_client(surface->resource);
        resource = wl_resource_find_for_client(&output->resources, client);

        if (resource)
        {
            if (entered_outputs & SWC_OUTPUT_MASK(output))
                wl_surface_send_enter(surface->resource, resource);
            else if (left_outputs & SWC_OUTPUT_MASK(output))
                wl_surface_send_leave(surface->resource, resource);
        }
    }

    surface->outputs = new_outputs;
}

static void update(struct swc_view * view);

static void handle_surface_event(struct wl_listener * listener, void * data)
{
    struct view * view
        = CONTAINER_OF(listener, typeof(*view), surface_event_listener);
    struct swc_event * event = data;
    struct swc_surface_event_data * event_data = event->data;
    struct swc_surface * surface = event_data->surface;

    switch (event->type)
    {
        case SWC_SURFACE_EVENT_TYPE_RESIZE:
            damage_below_surface(surface);

            update_extents(surface);
            update(&view->base);
            update_outputs(surface);

            break;
    }
}

static void remove_(struct swc_view * base)
{
    struct view * view = (void *) base;

    swc_compositor_surface_hide(view->surface);
    wl_list_remove(&view->surface_event_listener.link);
    pixman_region32_fini(&view->clip);
    free(view);
}

static void attach(struct swc_view * base, struct swc_buffer * buffer)
{
    struct view * view = (void *) base;

    renderer_attach(view, buffer);
}

static void update(struct swc_view * base)
{
    struct view * view = (void *) base;
    struct swc_compositor * compositor = view->compositor;
    struct swc_output * output;

    if (!view->mapped)
        return;

    wl_list_for_each(output, &compositor->outputs, link)
    {
        if (view->surface->outputs & SWC_OUTPUT_MASK(output))
            swc_compositor_schedule_update(compositor, output);
    }
}

static void move(struct swc_view * base, int32_t x, int32_t y)
{
    struct view * view = (void *) base;
    struct swc_surface * surface = view->surface;

    if (x == surface->geometry.x && y == surface->geometry.y)
        return;

    if (view->mapped)
        damage_below_surface(surface);

    surface->geometry.x = x;
    surface->geometry.y = y;

    update_extents(surface);

    if (view->mapped)
    {
        /* Assume worst-case no clipping until we draw the next frame (in case
         * the surface gets moved again before that). */
        pixman_region32_init(&view->clip);

        damage_below_surface(surface);
        update(&view->base);
        update_outputs(surface);
        update(&view->base);
    }
}

const struct swc_view_impl view_impl = {
    .remove = &remove_,
    .attach = &attach,
    .update = &update,
    .move = &move
};

bool swc_compositor_add_surface(struct swc_compositor * compositor,
                                struct swc_surface * surface)
{
    struct view * view;

    view = malloc(sizeof *view);

    if (!view)
        return false;

    swc_view_initialize(&view->base, &view_impl);
    view->compositor = compositor;
    view->surface = surface;
    view->mapped = false;
    view->extents.x1 = surface->geometry.x;
    view->extents.y1 = surface->geometry.y;
    view->extents.x2 = surface->geometry.x + surface->geometry.width;
    view->extents.y2 = surface->geometry.y + surface->geometry.height;
    view->border.width = 0;
    view->border.color = 0x000000;
    view->border.damaged = false;
    view->surface_event_listener.notify = &handle_surface_event;
    wl_signal_add(&surface->event_signal, &view->surface_event_listener);
    pixman_region32_init(&view->clip);
    swc_surface_set_view(surface, &view->base);

    return true;
}

void swc_compositor_surface_show(struct swc_surface * surface)
{
    struct view * view = (void *) surface->view;
    struct swc_compositor * compositor = view->compositor;

    if (view->base.impl != &view_impl)
        return;

    if (view->mapped)
        return;

    printf("showing surface %u\n", wl_resource_get_id(surface->resource));

    view->mapped = true;

    /* Assume worst-case no clipping until we draw the next frame (in case the
     * surface gets moved before that. */
    pixman_region32_clear(&view->clip);

    damage_surface(surface);
    update_outputs(surface);
    update(&view->base);
    wl_list_insert(&compositor->surfaces, &surface->link);
}

void swc_compositor_surface_hide(struct swc_surface * surface)
{
    struct view * view = (void *) surface->view;

    if (view->base.impl != &view_impl)
        return;

    if (!view->mapped)
        return;

    /* Update all the outputs the surface was on. */
    update(&view->base);

    view->mapped = false;

    damage_below_surface(surface);
    update_outputs(surface);
    wl_list_remove(&surface->link);
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

    update_extents(surface);
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
    struct swc_surface * surface;
    struct view * view;
    pixman_region32_t surface_opaque;

    pixman_region32_clear(&compositor->opaque);
    pixman_region32_init(&surface_opaque);

    /* Go through surfaces top-down to calculate clipping regions. */
    wl_list_for_each(surface, &compositor->surfaces, link)
    {
        view = (void *) surface->view;

        /* Clip the surface by the opaque region covering it. */
        pixman_region32_copy(&view->clip, &compositor->opaque);

        /* Translate the opaque region to global coordinates. */
        pixman_region32_copy(&surface_opaque, &surface->state.opaque);
        pixman_region32_translate(&surface_opaque, surface->geometry.x,
                                  surface->geometry.y);

        /* Add the surface's opaque region to the accumulated opaque
         * region. */
        pixman_region32_union(&compositor->opaque, &compositor->opaque,
                              &surface_opaque);

        if (pixman_region32_not_empty(&surface->state.damage))
        {
            renderer_flush_surface(surface);

            /* Translate surface damage to global coordinates. */
            pixman_region32_translate(&surface->state.damage,
                                      surface->geometry.x,
                                      surface->geometry.y);

            /* Add the surface damage to the compositor damage. */
            pixman_region32_union(&compositor->damage, &compositor->damage,
                                  &surface->state.damage);
            pixman_region32_clear(&surface->state.damage);
        }

        if (view->border.damaged)
        {
            pixman_region32_t border_region, surface_region;

            pixman_region32_init_with_extents(&border_region, &view->extents);
            pixman_region32_init_rect
                (&surface_region, surface->geometry.x, surface->geometry.y,
                 surface->geometry.width, surface->geometry.height);

            pixman_region32_subtract(&border_region, &border_region,
                                     &surface_region);

            pixman_region32_union(&compositor->damage, &compositor->damage,
                                  &border_region);

            pixman_region32_fini(&border_region);
            pixman_region32_fini(&surface_region);

            view->border.damaged = false;
        }
    }

    pixman_region32_fini(&surface_opaque);
}

static void repaint_output(struct swc_compositor * compositor,
                           struct swc_output * output,
                           pixman_region32_t * damage)
{
    pixman_region32_t base_damage;
    struct render_target target;

    pixman_region32_init(&base_damage);
    pixman_region32_subtract(&base_damage, damage, &compositor->opaque);

    target.buffer = swc_plane_get_buffer(&output->framebuffer_plane);
    target.geometry.x = output->framebuffer_plane.output->geometry.x
        + output->framebuffer_plane.x;
    target.geometry.y = output->framebuffer_plane.output->geometry.y
        + output->framebuffer_plane.y;
    target.geometry.width = target.buffer->width;
    target.geometry.height = target.buffer->height;

    renderer_repaint(&target, damage, &base_damage,
                     &compositor->surfaces);

    pixman_region32_fini(&base_damage);

    if (!swc_plane_flip(&output->framebuffer_plane))
        fprintf(stderr, "Plane flip failed\n");
}

static void update_output_damage(struct swc_output * output,
                                 pixman_region32_t * damage)
{
    pixman_region32_union
        (&output->current_damage, &output->current_damage, damage);
    pixman_region32_intersect_rect
        (&output->current_damage, &output->current_damage,
         output->geometry.x, output->geometry.y,
         output->geometry.width, output->geometry.height);
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
    struct swc_output * output;
    pixman_region32_t damage;
    uint32_t updates = compositor->scheduled_updates
                     & ~compositor->pending_flips;

    if (!updates)
        return;

    DEBUG("Performing update\n");

    calculate_damage(compositor);
    pixman_region32_init(&damage);

    wl_list_for_each(output, &compositor->outputs, link)
    {
        if (!(compositor->scheduled_updates & SWC_OUTPUT_MASK(output)))
            continue;

        update_output_damage(output, &compositor->damage);

        /* Don't repaint the output if it is waiting for a page flip. */
        if (compositor->pending_flips & SWC_OUTPUT_MASK(output))
            continue;

        flush_output_damage(output, &damage);
        repaint_output(compositor, output, &damage);
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
    struct swc_surface * surface;
    int32_t x, y;

    wl_list_for_each(surface, &compositor->surfaces, link)
    {
        x = wl_fixed_to_int(pointer->x);
        y = wl_fixed_to_int(pointer->y);

        if (swc_rectangle_contains_point(&surface->geometry, x, y)
            && pixman_region32_contains_point(&surface->state.input,
                                              x - surface->geometry.x,
                                              y - surface->geometry.y, NULL))
        {
            swc_pointer_set_focus(pointer, surface);
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
            struct swc_surface * surface;

            compositor->pending_flips &= ~SWC_OUTPUT_MASK(event_data->output);

            if (compositor->pending_flips == 0)
            {
                wl_list_for_each(surface, &compositor->surfaces, link)
                    swc_view_frame(surface->view, event_data->time);
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
    struct swc_compositor * compositor = wl_resource_get_user_data(resource);
    struct swc_surface * surface;
    struct swc_output * output;

    printf("compositor.create_surface\n");

    output = CONTAINER_OF(compositor->outputs.next, typeof(*output), link);

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
    struct wl_list * outputs;
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

    outputs = swc_drm_create_outputs();

    if (outputs)
    {
        wl_list_init(&compositor->outputs);
        wl_list_insert_list(&compositor->outputs, outputs);
        free(outputs);
    }
    else
    {
        printf("could not create outputs\n");
        return false;
    }

    pixman_region32_init(&compositor->damage);
    pixman_region32_init(&compositor->opaque);
    wl_list_init(&compositor->surfaces);

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
    struct swc_output * output, * tmp;

    wl_list_for_each_safe(output, tmp, &compositor->outputs, link)
    {
        swc_output_finish(output);
        free(output);
    }
}

void swc_compositor_add_globals(struct swc_compositor * compositor,
                                struct wl_display * display)
{
    struct swc_output * output;

    wl_global_create(display, &wl_compositor_interface, 3, compositor,
                     &bind_compositor);

    wl_list_for_each(output, &compositor->outputs, link)
    {
        swc_output_add_globals(output, display);
    }
}

void swc_compositor_schedule_update(struct swc_compositor * compositor,
                                    struct swc_output * output)
{
    bool update_scheduled = compositor->scheduled_updates != 0;

    if (compositor->scheduled_updates & SWC_OUTPUT_MASK(output))
        return;

    compositor->scheduled_updates |= SWC_OUTPUT_MASK(output);

    if (!update_scheduled)
    {
        struct wl_event_loop * event_loop;

        event_loop = wl_display_get_event_loop(compositor->display);
        wl_event_loop_add_idle(event_loop, &perform_update, compositor);
    }
}

