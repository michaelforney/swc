#include "swc.h"
#include "compositor.h"
#include "compositor_surface.h"
#include "cursor_surface.h"
#include "data_device_manager.h"
#include "output.h"
#include "region.h"
#include "surface.h"
#include "util.h"

#include <stdlib.h>
#include <stdio.h>

static const char default_seat[] = "seat0";

static void calculate_damage(struct swc_compositor * compositor)
{
    struct swc_surface * surface;
    struct swc_compositor_surface_state * state;
    pixman_region32_t surface_opaque;

    pixman_region32_clear(&compositor->opaque);
    pixman_region32_init(&surface_opaque);

    /* Go through surfaces top-down to calculate clipping regions. */
    wl_list_for_each(surface, &compositor->surfaces, link)
    {
        state = surface->class_state;

        /* Clip the surface by the opaque region covering it. */
        pixman_region32_copy(&state->clip, &compositor->opaque);

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
            swc_renderer_flush(&compositor->renderer, surface);

            /* Translate surface damage to global coordinates. */
            pixman_region32_translate(&surface->state.damage,
                                      surface->geometry.x,
                                      surface->geometry.y);

            /* Add the surface damage to the compositor damage. */
            pixman_region32_union(&compositor->damage, &compositor->damage,
                                  &surface->state.damage);
            pixman_region32_clear(&surface->state.damage);
        }

        if (state->border.damaged)
        {
            pixman_region32_t border_region, surface_region;

            pixman_region32_init_with_extents(&border_region, &state->extents);
            pixman_region32_init_rect
                (&surface_region, surface->geometry.x, surface->geometry.y,
                 surface->geometry.width, surface->geometry.height);

            pixman_region32_subtract(&border_region, &border_region,
                                     &surface_region);

            pixman_region32_union(&compositor->damage, &compositor->damage,
                                  &border_region);

            pixman_region32_fini(&border_region);
            pixman_region32_fini(&surface_region);

            state->border.damaged = false;
        }
    }

    pixman_region32_fini(&surface_opaque);
}

static void repaint_output(struct swc_compositor * compositor,
                           struct swc_output * output)
{
    pixman_region32_t damage, previous_damage, base_damage;

    pixman_region32_init(&damage);
    pixman_region32_init(&previous_damage);
    pixman_region32_init(&base_damage);

    pixman_region32_intersect_rect
        (&damage, &compositor->damage, output->geometry.x, output->geometry.y,
         output->geometry.width, output->geometry.height);

    /* We must save the damage from the previous frame because the back buffer
     * is also damaged in this region. */
    pixman_region32_copy(&previous_damage, &output->previous_damage);
    pixman_region32_copy(&output->previous_damage, &damage);

    /* The total damage is composed of the damage from the new frame, and the
     * damage from the last frame. */
    pixman_region32_union(&damage, &damage, &previous_damage);

    pixman_region32_subtract(&base_damage, &damage, &compositor->opaque);

    swc_renderer_set_target(&compositor->renderer, &output->framebuffer_plane);
    swc_renderer_repaint(&compositor->renderer, &damage, &base_damage,
                         &compositor->surfaces);

    pixman_region32_subtract(&compositor->damage, &compositor->damage, &damage);

    pixman_region32_fini(&damage);
    pixman_region32_fini(&previous_damage);
    pixman_region32_fini(&base_damage);

    if (!swc_plane_flip(&output->framebuffer_plane))
        fprintf(stderr, "Plane flip failed\n");
}

static void perform_update(void * data)
{
    struct swc_compositor * compositor = data;
    struct swc_output * output;
    uint32_t updates = compositor->scheduled_updates
                       & ~compositor->pending_flips;

    if (updates)
    {
        printf("performing update\n");
        calculate_damage(compositor);

        wl_list_for_each(output, &compositor->outputs, link)
        {
            if (updates & SWC_OUTPUT_MASK(output))
                repaint_output(compositor, output);
        }

        compositor->pending_flips |= updates;
        compositor->scheduled_updates &= ~updates;
    }

}

static void handle_focus(struct swc_pointer * pointer)
{
    struct swc_seat * seat;
    struct swc_compositor * compositor;
    struct swc_surface * surface;
    int32_t x, y;

    seat = CONTAINER_OF(pointer, typeof(*seat), pointer);
    compositor = CONTAINER_OF(seat, typeof(*compositor), seat);

    wl_list_for_each(surface, &compositor->surfaces, link)
    {
        pixman_region32_t region;

        pixman_region32_init_rect
            (&region, surface->geometry.x, surface->geometry.y,
             surface->geometry.width, surface->geometry.height);

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
                    swc_surface_send_frame_callbacks(surface, event_data->time);
            }

            /* If we had scheduled updates that couldn't run because we were
             * waiting on a page flip, run them now. */
            if (compositor->scheduled_updates)
                perform_update(compositor);

            break;
        }
    }
}

static void handle_pointer_event(struct wl_listener * listener, void * data)
{
    struct swc_event * event = data;
    struct swc_pointer_event_data * event_data = event->data;
    struct swc_compositor * compositor;

    compositor = CONTAINER_OF(listener, typeof(*compositor), pointer_listener);

    switch (event->type)
    {
        case SWC_POINTER_CURSOR_CHANGED:
            if (event_data->old)
                swc_surface_set_class(event_data->old, NULL);

            if (event_data->new)
                swc_surface_set_class(event_data->new, &compositor->cursor_class);
            break;
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
    struct swc_output * output;
    pixman_region32_t pointer_region;
    xkb_keysym_t keysym;

    compositor->display = display;
    compositor->drm_listener.notify = &handle_drm_event;
    compositor->pointer_listener.notify = &handle_pointer_event;
    compositor->scheduled_updates = 0;
    compositor->pending_flips = 0;
    compositor->compositor_class.interface
        = &swc_compositor_class_implementation;
    compositor->cursor_class.interface = &swc_cursor_class_implementation;
    compositor->pointer_handler = (struct swc_pointer_handler) {
        .focus = &handle_focus,
        .motion = &handle_motion
    };

    /* TODO: configurable seat */
    if (!swc_seat_initialize(&compositor->seat, default_seat))
    {
        printf("could not initialize seat\n");
        goto error_base;
    }

    swc_seat_add_event_sources(&compositor->seat, event_loop);
    compositor->seat.pointer.handler = &compositor->pointer_handler;
    wl_signal_add(&compositor->seat.pointer.event_signal,
                  &compositor->pointer_listener);

    /* TODO: configurable seat */
    if (!swc_drm_initialize(&compositor->drm, default_seat))
    {
        printf("could not initialize drm\n");
        goto error_seat;
    }

    wl_signal_add(&compositor->drm.event_signal, &compositor->drm_listener);
    swc_drm_add_event_sources(&compositor->drm, event_loop);

    if (!swc_renderer_initialize(&compositor->renderer, &compositor->drm))
    {
        printf("could not initialize renderer\n");
        goto error_drm;
    }

    outputs = swc_drm_create_outputs(&compositor->drm);

    if (outputs)
    {
        wl_list_init(&compositor->outputs);
        wl_list_insert_list(&compositor->outputs, outputs);
        free(outputs);
    }
    else
    {
        printf("could not create outputs\n");
        goto error_renderer;
    }

    /* Calculate pointer region */
    pixman_region32_init(&pointer_region);

    wl_list_for_each(output, &compositor->outputs, link)
    {
        pixman_region32_union_rect(&pointer_region, &pointer_region,
                                   output->geometry.x, output->geometry.y,
                                   output->geometry.width,
                                   output->geometry.height);
    }

    swc_pointer_set_region(&compositor->seat.pointer, &pointer_region);
    pixman_region32_fini(&pointer_region);

    pixman_region32_init(&compositor->damage);
    pixman_region32_init(&compositor->opaque);
    wl_list_init(&compositor->surfaces);
    wl_signal_init(&compositor->destroy_signal);

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

  error_renderer:
    swc_renderer_finalize(&compositor->renderer);
  error_drm:
    swc_drm_finish(&compositor->drm);
  error_seat:
    swc_seat_finish(&compositor->seat);
  error_base:
    return false;
}

void swc_compositor_finish(struct swc_compositor * compositor)
{
    struct swc_output * output, * tmp;

    wl_signal_emit(&compositor->destroy_signal, compositor);

    wl_list_for_each_safe(output, tmp, &compositor->outputs, link)
    {
        swc_output_finish(output);
        free(output);
    }

    swc_drm_finish(&compositor->drm);
    swc_seat_finish(&compositor->seat);
}

void swc_compositor_add_globals(struct swc_compositor * compositor,
                                struct wl_display * display)
{
    struct swc_output * output;

    wl_global_create(display, &wl_compositor_interface, 3, compositor,
                     &bind_compositor);

    swc_data_device_manager_add_globals(display);
    swc_seat_add_globals(&compositor->seat, display);
    swc_drm_add_globals(&compositor->drm, display);

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

