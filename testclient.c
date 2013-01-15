#include <stdbool.h>
#include <stdio.h>
#include <wayland-client.h>

void registry_handle_global(void * data, struct wl_registry * wl_registry,
                            uint32_t name, const char * interface,
                            uint32_t version)
{
    printf("handle global\n");
    printf("\tinterface: %s\n", interface);
}

static const struct wl_registry_listener registry_listener = {
    .global = &registry_handle_global
};

int main(int argc, char * argv[])
{
    struct wl_display * display;
    struct wl_registry * registry;
    bool running = true;

    display = wl_display_connect(NULL);
    registry = wl_display_get_registry(display);

    wl_registry_add_listener(registry, &registry_listener, NULL);
    wl_display_dispatch(display);

    while (running)
    {
        wl_display_dispatch_pending(display);
        wl_display_flush(display);
    }
}
