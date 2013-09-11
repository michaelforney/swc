/* swc: compositor_surface.c
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

#include "compositor_surface.h"
#include "compositor.h"
#include "util.h"

#include <stdio.h>
#include <stdlib.h>

static bool add(struct swc_surface * surface);
static void remove_(struct swc_surface * surface);
static void attach(struct swc_surface * surface, struct wl_resource * resource);
static void update(struct swc_surface * surface);
static void move(struct swc_surface * surface, int32_t x, int32_t y);

const struct swc_surface_class_interface swc_compositor_class_implementation = {
    .add = &add,
    .remove = &remove_,
    .attach = &attach,
    .update = &update,
    .move = &move
};

/**
 * Adds damage from the region below a surface, taking into account it's clip
 * region, to the region specified by `damage'.
 */
static void damage_below_surface(struct swc_surface * surface)
{
    struct swc_compositor * compositor = swc_container_of
        (surface->class, typeof(*compositor), compositor_class);
    struct swc_compositor_surface_state * state = surface->class_state;
    pixman_region32_t damage_below;

    pixman_region32_init_with_extents(&damage_below, &state->extents);
    pixman_region32_subtract(&damage_below, &damage_below, &state->clip);
    pixman_region32_union(&compositor->damage, &compositor->damage,
                          &damage_below);
    pixman_region32_fini(&damage_below);
}

/**
 * Completely damages the surface and its border.
 */
static void damage_surface(struct swc_surface * surface)
{
    struct swc_compositor_surface_state * state = surface->class_state;
    printf("damaging surface\n");

    pixman_region32_fini(&surface->state.damage);
    pixman_region32_init_rect(&surface->state.damage, 0, 0,
                              surface->geometry.width,
                              surface->geometry.height);
    state->border.damaged = true;
}

static void update_extents(struct swc_surface * surface)
{
    struct swc_compositor_surface_state * state = surface->class_state;

    state->extents.x1 = surface->geometry.x - state->border.width;
    state->extents.y1 = surface->geometry.y - state->border.width;
    state->extents.x2 = surface->geometry.x + surface->geometry.width
        + state->border.width;
    state->extents.y2 = surface->geometry.y + surface->geometry.height
        + state->border.width;

    /* Damage border. */
    state->border.damaged = true;
}

static void update_outputs(struct swc_surface * surface)
{
    struct swc_compositor * compositor = swc_container_of
        (surface->class, typeof(*compositor), compositor_class);
    struct swc_compositor_surface_state * state = surface->class_state;
    uint32_t old_outputs = surface->outputs, new_outputs = 0,
             entered_outputs, left_outputs, changed_outputs;
    struct swc_output * output;
    struct wl_client * client;
    struct wl_resource * resource;

    if (state->mapped)
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

/* Compositor class */
bool add(struct swc_surface * surface)
{
    struct swc_compositor * compositor = swc_container_of
        (surface->class, typeof(*compositor), compositor_class);
    struct swc_compositor_surface_state * state;

    state = malloc(sizeof *state);

    if (!state)
        return false;

    state->compositor = compositor;
    state->extents.x1 = surface->geometry.x;
    state->extents.y1 = surface->geometry.y;
    state->extents.x2 = surface->geometry.x + surface->geometry.width;
    state->extents.y2 = surface->geometry.y + surface->geometry.height;
    state->border.width = 0;
    state->border.color = 0x000000;
    state->border.damaged = false;
    state->mapped = false;

    wl_signal_add(&surface->event_signal, &state->event_listener);

    pixman_region32_init(&state->clip);

    surface->class_state = state;

    return true;
}

void remove_(struct swc_surface * surface)
{
    struct swc_compositor * compositor = swc_container_of
        (surface->class, typeof(*compositor), compositor_class);
    struct swc_compositor_surface_state * state = surface->class_state;

    swc_compositor_surface_hide(surface);

    wl_list_remove(&state->event_listener.link);
    pixman_region32_fini(&state->clip);

    free(state);
}

void attach(struct swc_surface * surface, struct wl_resource * resource)
{
    struct swc_compositor * compositor = swc_container_of
        (surface->class, typeof(*compositor), compositor_class);

    swc_renderer_attach(&compositor->renderer, surface, resource);
}

void update(struct swc_surface * surface)
{
    struct swc_compositor * compositor = swc_container_of
        (surface->class, typeof(*compositor), compositor_class);
    struct swc_compositor_surface_state * state = surface->class_state;
    struct swc_output * output;

    if (!state->mapped)
        return;

    wl_list_for_each(output, &compositor->outputs, link)
    {
        if (surface->outputs & SWC_OUTPUT_MASK(output))
            swc_compositor_schedule_update(compositor, output);
    }
}

void move(struct swc_surface * surface, int32_t x, int32_t y)
{
    struct swc_compositor * compositor = swc_container_of
        (surface->class, typeof(*compositor), compositor_class);
    struct swc_compositor_surface_state * state = surface->class_state;

    if (x == surface->geometry.x && y == surface->geometry.y)
        return;

    if (state->mapped)
        damage_below_surface(surface);

    surface->geometry.x = x;
    surface->geometry.y = y;

    update_extents(surface);

    if (state->mapped)
    {
        /* Assume worst-case no clipping until we draw the next frame (in case
         * the surface gets moved again before that). */
        pixman_region32_init(&state->clip);

        damage_below_surface(surface);
        update(surface);
        update_outputs(surface);
        update(surface);
    }
}

void swc_compositor_surface_show(struct swc_surface * surface)
{
    struct swc_compositor * compositor = swc_container_of
        (surface->class, typeof(*compositor), compositor_class);
    struct swc_compositor_surface_state * state = surface->class_state;

    if (surface->class->interface != &swc_compositor_class_implementation)
        return;

    if (state->mapped)
        return;

    printf("showing surface %u\n", wl_resource_get_id(surface->resource));

    state->mapped = true;

    /* Assume worst-case no clipping until we draw the next frame (in case the
     * surface gets moved before that. */
    pixman_region32_clear(&state->clip);

    damage_surface(surface);
    update_outputs(surface);
    update(surface);
    wl_list_insert(&compositor->surfaces, &surface->link);
}

void swc_compositor_surface_hide(struct swc_surface * surface)
{
    struct swc_compositor * compositor = swc_container_of
        (surface->class, typeof(*compositor), compositor_class);
    struct swc_compositor_surface_state * state = surface->class_state;

    if (surface->class->interface != &swc_compositor_class_implementation)
        return;

    if (!state->mapped)
        return;

    /* Update all the outputs the surface was on. */
    update(surface);

    state->mapped = false;

    damage_below_surface(surface);
    update_outputs(surface);
    wl_list_remove(&surface->link);
}

void swc_compositor_surface_set_border_width(struct swc_surface * surface,
                                             uint32_t width)
{
    struct swc_compositor_surface_state * state = surface->class_state;

    if (state->border.width == width)
        return;

    state->border.width = width;
    state->border.damaged = true;

    /* XXX: Damage above surface for transparent surfaces? */

    update_extents(surface);
    update(surface);
}

void swc_compositor_surface_set_border_color(struct swc_surface * surface,
                                             uint32_t color)
{
    struct swc_compositor_surface_state * state = surface->class_state;

    if (state->border.color == color)
        return;

    state->border.color = color;
    state->border.damaged = true;

    /* XXX: Damage above surface for transparent surfaces? */

    update(surface);
}

