#include "compositor.h"

#include <stdio.h>
#include <stdlib.h>
#include <signal.h>
#include <unistd.h>
#include <sys/wait.h>

#include <wayland-server.h>
#include <xkbcommon/xkbcommon-keysyms.h>

const char socket_name[] = "wayland-0";

static void get_shell_surface(struct wl_client * client,
                              struct wl_resource * resource, uint32_t id,
                              struct wl_resource * surface)
{
    wl_client_add_object(client, &wl_shell_surface_interface, NULL, id, surface->data);
}

struct wl_shell_interface shell_interface = {
    .get_shell_surface = &get_shell_surface
};

void handle_terminate(uint32_t time, uint32_t value, void * data)
{
    struct wl_display * display = data;
    printf("handling terminate\n");
    wl_display_terminate(display);
}

void handle_switch_vt(uint32_t time, uint32_t value, void * data)
{
    struct swc_tty * tty = data;
    uint32_t vt = value - XKB_KEY_XF86Switch_VT_1 + 1;
    printf("handle switch vt%u\n", vt);
    swc_tty_switch_vt(tty, value - XKB_KEY_XF86Switch_VT_1 + 1);
}

void handle_test_shm(uint32_t time, uint32_t value, void * data)
{
    printf("handle test shm\n");
    if (fork() == 0)
    {
        printf("launching shm\n");
        execlp("/home/michael/scm/freedesktop/weston/clients/simple-shm", "simple-shm", NULL);
        exit(EXIT_SUCCESS);
    }
}

void handle_test_term(uint32_t time, uint32_t value, void * data)
{
    printf("handle test term\n");
    if (fork() == 0)
    {
        printf("launching term\n");
        execlp("weston-terminal", "weston-terminal", NULL);
        exit(EXIT_SUCCESS);
    }
}

void handle_test_info(uint32_t time, uint32_t value, void * data)
{
    printf("handle test term\n");
    if (fork() == 0)
    {
        printf("launching info\n");
        execlp("weston-info", "weston-info", NULL);
        exit(EXIT_SUCCESS);
    }
}

int handle_sigint(int signal_number, void * data)
{
    struct wl_display * display = data;
    printf("handle sigint\n");
    wl_display_terminate(display);

    return 1;
}

int handle_sigchld(int signal_number, void * data)
{
    printf("handle SIGCHLD\n");
    while (waitpid(-1, NULL, WNOHANG) != -1);
}

static void bind_shell(struct wl_client * client, void * data, uint32_t version, uint32_t id)
{
    wl_client_add_object(client, &wl_shell_interface, &shell_interface, id, NULL);
}

int main(int argc, char * argv[])
{
    struct wl_display * display;
    struct swc_compositor compositor;
    struct wl_event_loop * event_loop;
    struct wl_event_source * sigint_source, * sigchld_source;
    xkb_keysym_t keysym;

    display = wl_display_create();

    wl_display_init_shm(display);
    wl_data_device_manager_init(display);

    event_loop = wl_display_get_event_loop(display);
    sigint_source = wl_event_loop_add_signal(event_loop, SIGINT, &handle_sigint,
                                             display);

    sigchld_source = wl_event_loop_add_signal(event_loop, SIGCHLD, &handle_sigchld,
                                              NULL);

    swc_compositor_initialize(&compositor, display);
    swc_compositor_add_globals(&compositor, display);

    wl_display_add_global(display, &wl_shell_interface, NULL, &bind_shell);

    swc_compositor_add_key_binding(&compositor,
        MOD_CTRL | MOD_ALT, XKB_KEY_BackSpace, &handle_terminate, display);

    for (keysym = XKB_KEY_XF86Switch_VT_1;
         keysym <= XKB_KEY_XF86Switch_VT_12;
         ++keysym)
    {
        swc_compositor_add_key_binding(&compositor, MOD_ANY, keysym,
                                       &handle_switch_vt, &compositor.tty);
    }

    swc_compositor_add_key_binding(&compositor, MOD_SUPER, XKB_KEY_Return,
                                   &handle_test_term, NULL);

    swc_compositor_add_key_binding(&compositor, MOD_SUPER, XKB_KEY_1,
                                   &handle_test_info, NULL);

    swc_compositor_add_key_binding(&compositor, MOD_SUPER, XKB_KEY_2,
                                   &handle_test_shm, NULL);

    wl_display_add_socket(display, socket_name);
    setenv("WAYLAND_DISPLAY", socket_name, 1);

    wl_display_run(display);

    /* Cleanup */
    wl_event_source_remove(sigint_source);
    wl_event_source_remove(sigchld_source);
    swc_compositor_finish(&compositor);
    wl_display_destroy(display);

    return EXIT_SUCCESS;
}

