swc
===
swc is a small Wayland compositor implemented as a library.

It has been designed primary with tiling window managers in mind. Additionally,
notable features include:

* Easy to follow code base
* XWayland support
* Can place borders around windows

Dependencies
------------
* wayland
* libdrm
* libevdev
* libxkbcommon
* pixman
* [wld](http://github.com/michaelforney/wld)
* libudev
* linux[>=3.12] (for EVIOCREVOKE)

libudev is currently required for input hotplugging support. I'd like to get rid
of this dependency, but it seems that libudev is currently the only way to get
this functionality.

For XWayland support, the following are also required:
* libxcb
* xcb-util-wm

Implementing a window manager using swc
---------------------------------------
You must implement two callback functions, `new_window` and `new_screen`, which
get called when a new window or screen is created. In `new_window`, you should
allocate your own window window structure, and register a listener for the
window's event signal. More information can be found in `swc.h`.

    void new_window(struct swc_window * window)
    {
        /* TODO: Implement */
    }

    void new_screen(struct swc_screen * screen)
    {
        /* TODO: Implement */
    }

Create a `struct swc_manager` containing pointers to these functions.

    const struct swc_manager manager = { &new_window, &new_screen };

In your startup code, you must create a Wayland display.

    display = wl_display_create();

Then call `swc_initialize`.

    swc_initialize(display, NULL, manager);

Finally, run the main event loop.

    wl_display_run(display);

Why not write a Weston shell plugin?
------------------------------------
In my opinion the goals of Weston and swc are rather orthogonal. Weston seeks to
demonstrate many of the features possible with the Wayland protocol, with
various types of backends and shells supported, while swc aims to provide only
what's necessary to get windows displayed on the screen.

I've seen several people look at Wayland, and ask "How can I implement a tiling
window manager using the Wayland protocol?", only to be turned off by the
response "Write a weston shell plugin". Hopefully it is less intimidating to
implement a window manager using swc.

TODO
----
* VT switching
* XWayland copy-paste integration
* Better multi-screen support, including mirroring and screen arrangement
* Mouse button bindings
* Resizing/moving windows by dragging
* Touchpad support

Future Plans
------------
* I'd like to revisit `wld` and set it up in such a way that an EGL backend can
  be implemented so more hardware is supported.
* I also want to revisit the planes implementation, allowing full screen
  surfaces to be scanned out directly, or to a hardware plane.

