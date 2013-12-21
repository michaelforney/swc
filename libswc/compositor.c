#include "swc.h"
#include "compositor.h"
#include "compositor_surface.h"
#include "cursor_surface.h"
#include "data_device_manager.h"
#include "drm.h"
#include "drm_buffer.h"
#include "internal.h"
#include "output.h"
#include "pointer.h"
#include "region.h"
#include "seat.h"
#include "surface.h"
#include "util.h"

#include <stdlib.h>
#include <stdio.h>
#include <assert.h>
#include <wld/wld.h>
#include <wld/drm.h>
#include <xkbcommon/xkbcommon-keysyms.h>

/* Rendering {{{ */

struct buffer_state
{
    struct wld_drawable * drawable;
    /* Only used for SHM buffers */
    pixman_image_t * dst, * src;
    struct wl_listener destroy_listener;
};

struct render_target
{
    struct wld_drawable * drawable;
    pixman_rectangle32_t geometry;
};

static inline uint32_t pixman_format(uint32_t format)
{
    switch (format)
    {
        case WL_SHM_FORMAT_XRGB8888:
            return PIXMAN_x8r8g8b8;
        case WL_SHM_FORMAT_ARGB8888:
            return PIXMAN_a8r8g8b8;
    }

    return 0;
}

static inline uint32_t wld_format(uint32_t format)
{
    switch (format)
    {
        case WL_SHM_FORMAT_XRGB8888:
            return WLD_FORMAT_XRGB8888;
        case WL_SHM_FORMAT_ARGB8888:
            return WLD_FORMAT_ARGB8888;
    }

    return 0;
}

static void handle_buffer_destroy(struct wl_listener * listener, void * data)
{
    struct buffer_state * state
        = CONTAINER_OF(listener, typeof(*state), destroy_listener);

    if (state->dst)
        pixman_image_unref(state->dst);
    if (state->src)
        pixman_image_unref(state->src);
    wld_destroy_drawable(state->drawable);
    free(state);
}

static inline struct buffer_state * buffer_state(struct wl_resource * resource)
{
    struct wl_listener * listener
        = wl_resource_get_destroy_listener(resource, &handle_buffer_destroy);

    return listener ? CONTAINER_OF(listener, struct buffer_state,
                                   destroy_listener)
                    : NULL;
}

static void repaint_surface(struct render_target * target,
                            struct swc_surface * surface,
                            pixman_region32_t * damage)
{
    pixman_region32_t surface_damage;
    pixman_region32_t border_damage;
    pixman_region32_t surface_region;
    struct buffer_state * state;
    struct swc_compositor_surface_state * surface_state = surface->view_state;

    if (!surface->state.buffer)
        return;

    state = buffer_state(surface->state.buffer);
    assert(state);

    pixman_region32_init_with_extents(&surface_damage, &surface_state->extents);
    pixman_region32_init(&border_damage);
    pixman_region32_init_rect
        (&surface_region, surface->geometry.x, surface->geometry.y,
         surface->geometry.width, surface->geometry.height);

    pixman_region32_intersect(&surface_damage, &surface_damage, damage);
    pixman_region32_subtract(&surface_damage, &surface_damage,
                             &surface_state->clip);
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
        wld_copy_region(state->drawable, target->drawable, &surface_damage,
                        surface->geometry.x - target->geometry.x,
                        surface->geometry.y - target->geometry.y);
    }

    pixman_region32_fini(&surface_damage);

    /* Draw border */
    if (pixman_region32_not_empty(&border_damage))
    {
        DEBUG("\t\tRedrawing border\n");

        pixman_region32_translate(&border_damage,
                                  -target->geometry.x, -target->geometry.y);
        wld_fill_region(target->drawable, surface_state->border.color,
                        &border_damage);
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
        wld_fill_region(target->drawable, 0xff000000, base_damage);
    }

    wl_list_for_each_reverse(surface, surfaces, link)
    {
        if (swc_rectangle_overlap(&target->geometry, &surface->geometry))
            repaint_surface(target, surface, damage);
    }

    wld_flush(target->drawable);
}

void swc_compositor_attach(struct swc_surface * surface,
                           struct wl_resource * resource)
{
    struct buffer_state * state;
    struct wl_shm_buffer * shm_buffer;
    struct swc_drm_buffer * drm_buffer;

    if (!resource)
        return;

    /* Check if we have already seen this buffer. */
    if ((state = buffer_state(resource)))
        return;

    if (!(state = malloc(sizeof *state)))
        return;

    if ((shm_buffer = wl_shm_buffer_get(resource)))
    {
        uint32_t width = wl_shm_buffer_get_width(shm_buffer),
                 height = wl_shm_buffer_get_height(shm_buffer),
                 format = wl_shm_buffer_get_format(shm_buffer),
                 pitch = wl_shm_buffer_get_stride(shm_buffer);
        void * data = wl_shm_buffer_get_data(shm_buffer);

        state->drawable = wld_drm_create_drawable(swc.drm->context,
                                                  width, height,
                                                  wld_format(format));
        state->src = pixman_image_create_bits_no_clear(pixman_format(format),
                                                       width, height,
                                                       data, pitch);
        state->dst = wld_map(state->drawable);
    }
    else if ((drm_buffer = swc_drm_buffer_get(resource)))
    {
        if (!(state = malloc(sizeof *state)))
            return;

        state->drawable = drm_buffer->drawable;
        state->src = NULL;
        state->dst = NULL;
    }
    else
    {
        ERROR("Unsupported buffer type\n");
        return;
    }

    state->destroy_listener.notify = &handle_buffer_destroy;
    wl_resource_add_destroy_listener(resource, &state->destroy_listener);
}

static void renderer_flush_surface(struct swc_surface * surface)
{
    struct buffer_state * state;

    state = buffer_state(surface->state.buffer);
    assert(state);

    if (!state->src || !state->dst)
        return;

    pixman_image_set_clip_region32(state->src, &surface->state.damage);
    pixman_image_composite32(PIXMAN_OP_SRC, state->src, NULL, state->dst,
                             0, 0, 0, 0, 0, 0,
                             state->drawable->width, state->drawable->height);
}

/* }}} */

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
        state = surface->view_state;

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
                           struct swc_output * output,
                           pixman_region32_t * damage)
{
    pixman_region32_t base_damage;
    struct render_target target;

    pixman_region32_init(&base_damage);
    pixman_region32_subtract(&base_damage, damage, &compositor->opaque);

    target.drawable = swc_plane_get_buffer(&output->framebuffer_plane);
    target.geometry.x = output->framebuffer_plane.output->geometry.x
        + output->framebuffer_plane.x;
    target.geometry.y = output->framebuffer_plane.output->geometry.y
        + output->framebuffer_plane.y;
    target.geometry.width = target.drawable->width;
    target.geometry.height = target.drawable->height;

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
                swc_surface_set_view(event_data->old, NULL);

            if (event_data->new)
                swc_surface_set_view(event_data->new, &compositor->cursor_view);
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
    uint32_t keysym;

    compositor->display = display;
    compositor->drm_listener.notify = &handle_drm_event;
    compositor->pointer_listener.notify = &handle_pointer_event;
    compositor->scheduled_updates = 0;
    compositor->pending_flips = 0;
    compositor->compositor_view.impl = &swc_compositor_view_impl;
    compositor->cursor_view.impl = &swc_cursor_view_impl;
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

