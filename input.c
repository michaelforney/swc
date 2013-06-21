#include "input.h"
#include "surface.h"

#include <assert.h>

#include "util.h"

static inline void focus(struct swc_input * input,
                         struct swc_surface * surface,
                         struct wl_resource * resource)
{
    if (resource)
    {
        input->handler->enter(input->handler, resource, surface);
    }

    input->focus.surface = surface;
    input->focus.resource = resource;
}

static inline void unfocus(struct swc_input * input)
{
    if (input->focus.resource)
    {
        input->handler->leave(input->handler, input->focus.resource,
                              input->focus.surface);
    }
}

bool swc_input_initialize(struct swc_input * input,
                          struct swc_input_handler * handler)
{
    input->focus.resource = NULL;
    input->focus.surface = NULL;
    input->handler = handler;

    wl_list_init(&input->resources);

    return true;
}

void swc_input_finish(struct swc_input * input)
{
    /* XXX: Destroy resources? */
}

void swc_input_add_resource(struct swc_input * input,
                            struct wl_resource * resource)
{
    /* If this new input resource corresponds to our focus, set it as our
     * focus. */
    if (input->focus.surface)
    {
        struct wl_client * client, * surface_client;

        client = wl_resource_get_client(resource);
        surface_client = wl_resource_get_client(input->focus.surface->resource);

        if (client == surface_client)
        {
            unfocus(input);
            focus(input, input->focus.surface, resource);
        }
    }

    wl_list_insert(&input->resources, wl_resource_get_link(resource));
}

void swc_input_remove_resource(struct swc_input * input,
                               struct wl_resource * resource)
{
    if (resource == input->focus.resource)
        input->focus.resource = NULL;

    swc_remove_resource(resource);
}

void swc_input_set_focus(struct swc_input * input,
                         struct swc_surface * surface)
{
    struct wl_client * client;
    struct wl_display * display;
    struct wl_resource * resource;
    uint32_t serial;

    if (surface == input->focus.surface)
        return;

    /* Unfocus previously focused surface. */
    unfocus(input);

    /* Focus new surface, if given. */
    if (surface)
    {
        client = wl_resource_get_client(surface->resource);
        resource = wl_resource_find_for_client(&input->resources, client);

        focus(input, surface, resource);
    }
    else
    {
        input->focus.surface = NULL;
        input->focus.resource = NULL;
    }

    return;
}

