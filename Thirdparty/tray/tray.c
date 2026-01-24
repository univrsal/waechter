/*
 * Copyright (c) 2026, Alex <uni@vrsal.cc>
 * SPDX-License-Identifier: BSD-3-Clause
 */
// #define STB_IMAGE_IMPLEMENTATION
#include "stb_image.h"

#include <X11/Xlib.h>
#include <X11/Xutil.h>
#include <X11/Xatom.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>

#include "xtray.h"

/* System tray protocol opcodes */
#define SYSTEM_TRAY_REQUEST_DOCK   0
#define SYSTEM_TRAY_BEGIN_MESSAGE  1
#define SYSTEM_TRAY_CANCEL_MESSAGE 2

/* Global state */
static Display *display;
static int screen;
static Window icon_win;
static GC gc;
static XImage *ximage = NULL;
static int icon_width = 0, icon_height = 0;
static int running = 1;

/* Original image data */
static unsigned char *orig_image = NULL;
static int orig_width, orig_height;

/*
 * Find the system tray window for the given screen
 */
static Window get_tray_window(void)
{
    char atom_name[64];
    snprintf(atom_name, sizeof(atom_name), "_NET_SYSTEM_TRAY_S%d", screen);

    Atom selection_atom = XInternAtom(display, atom_name, False);
    return XGetSelectionOwner(display, selection_atom);
}

/*
 * Send a dock request to embed our window in the system tray
 */
static void send_dock_request(Window tray)
{
    XEvent ev;
    memset(&ev, 0, sizeof(ev));

    Atom opcode_atom = XInternAtom(display, "_NET_SYSTEM_TRAY_OPCODE", False);

    ev.xclient.type = ClientMessage;
    ev.xclient.window = tray;
    ev.xclient.message_type = opcode_atom;
    ev.xclient.format = 32;
    ev.xclient.data.l[0] = CurrentTime;
    ev.xclient.data.l[1] = SYSTEM_TRAY_REQUEST_DOCK;
    ev.xclient.data.l[2] = icon_win;
    ev.xclient.data.l[3] = 0;
    ev.xclient.data.l[4] = 0;

    XSendEvent(display, tray, False, NoEventMask, &ev);
    XFlush(display);
}

/*
 * Create/recreate the XImage with the icon scaled to the given size
 */
static void create_scaled_ximage(int width, int height)
{
    if (ximage) {
        XDestroyImage(ximage);  /* This frees the data buffer too */
        ximage = NULL;
    }

    Visual *visual = DefaultVisual(display, screen);
    int depth = DefaultDepth(display, screen);

    /* Allocate pixel buffer */
    uint32_t *pixels = malloc(width * height * sizeof(uint32_t));
    if (!pixels) {
        fprintf(stderr, "Failed to allocate pixel buffer\n");
        return;
    }

    /* Scale and convert image: RGBA -> native X11 format (typically BGRA) */
    for (int y = 0; y < height; y++) {
        for (int x = 0; x < width; x++) {
            /* Simple nearest-neighbor scaling */
            int src_x = x * orig_width / width;
            int src_y = y * orig_height / height;
            int src_idx = (src_y * orig_width + src_x) * 4;

            unsigned char r = orig_image[src_idx + 0];
            unsigned char g = orig_image[src_idx + 1];
            unsigned char b = orig_image[src_idx + 2];
            unsigned char a = orig_image[src_idx + 3];

            /* Pre-multiply alpha for better blending */
            r = (r * a) / 255;
            g = (g * a) / 255;
            b = (b * a) / 255;

            /* Pack as ARGB (X11 32-bit format on little-endian) */
            pixels[y * width + x] = ((uint32_t)a << 24) |
                                    ((uint32_t)r << 16) |
                                    ((uint32_t)g << 8)  |
                                    ((uint32_t)b);
        }
    }

    ximage = XCreateImage(display, visual, depth, ZPixmap, 0,
                          (char *)pixels, width, height, 32, width * 4);

    if (!ximage) {
        fprintf(stderr, "Failed to create XImage\n");
        free(pixels);
        return;
    }

    icon_width = width;
    icon_height = height;
}

/*
 * Draw the icon to the window
 */
static void draw_icon(void)
{
    if (ximage && icon_win) {
        XPutImage(display, icon_win, gc, ximage,
                  0, 0, 0, 0, icon_width, icon_height);
        XFlush(display);
    }
}

/*
 * Handle left mouse button click
 */
static void on_left_click(int x, int y)
{
    printf("Left click detected at position (%d, %d)!\n", x, y);

    /*
     * Add your click handling logic here, for example:
     * - Show a popup menu
     * - Toggle application visibility
     * - Display a notification
     */
}

int xtray_init(const unsigned char* image_data, unsigned int image_data_size)
{
    /* Load image using stb_image (force RGBA) */
    int channels;
    orig_image = stbi_load_from_memory(image_data, image_data_size, &orig_width, &orig_height, &channels, 4);
    if (!orig_image) {
        fprintf(stderr, "Failed to load image: %s\n",
                 stbi_failure_reason());
        return 1;
    }

    /* Open X display */
    display = XOpenDisplay(NULL);
    if (!display) {
        fprintf(stderr, "Cannot open X display\n");
        stbi_image_free(orig_image);
        return 1;
    }

    screen = DefaultScreen(display);
    Window root = RootWindow(display, screen);

    /* Find the system tray */
    Window tray = get_tray_window();
    if (tray == None) {
        fprintf(stderr, "No system tray found. Is a panel/tray running?\n");
        XCloseDisplay(display);
        stbi_image_free(orig_image);
        return 1;
    }

    printf("Found system tray window: 0x%lx\n", tray);

    /* Create the icon window */
    int initial_size = 24;  /* Standard tray icon size */

    XSetWindowAttributes attrs;
    attrs.background_pixel = BlackPixel(display, screen);
    attrs.event_mask = ExposureMask | ButtonPressMask |
                       ButtonReleaseMask | StructureNotifyMask;

    icon_win = XCreateWindow(display, root,
                             0, 0, initial_size, initial_size,
                             0,
                             CopyFromParent,       /* depth */
                             InputOutput,          /* class */
                             CopyFromParent,       /* visual */
                             CWBackPixel | CWEventMask,
                             &attrs);

    /* Request docking BEFORE mapping the window to avoid it appearing separately */
    send_dock_request(tray);

    /* Set window class hints (some trays use this) */
    XClassHint class_hint;
    class_hint.res_name = "tray_icon";
    class_hint.res_class = "TrayIcon";
    XSetClassHint(display, icon_win, &class_hint);

    /* Prevent window from appearing in taskbar */
    Atom state_atom = XInternAtom(display, "_NET_WM_STATE", False);
    Atom skip_taskbar = XInternAtom(display, "_NET_WM_STATE_SKIP_TASKBAR", False);
    Atom skip_pager = XInternAtom(display, "_NET_WM_STATE_SKIP_PAGER", False);
    Atom atoms[2] = { skip_taskbar, skip_pager };
    XChangeProperty(display, icon_win, state_atom, XA_ATOM, 32,
                    PropModeReplace, (unsigned char *)atoms, 2);

    /* Set window type to notification to help window managers ignore it */
    Atom type_atom = XInternAtom(display, "_NET_WM_WINDOW_TYPE", False);
    Atom notification_type = XInternAtom(display, "_NET_WM_WINDOW_TYPE_NOTIFICATION", False);
    XChangeProperty(display, icon_win, type_atom, XA_ATOM, 32,
                    PropModeReplace, (unsigned char *)&notification_type, 1);

    /* Create graphics context */
    gc = XCreateGC(display, icon_win, 0, NULL);

    /* Create initial scaled image */
    create_scaled_ximage(initial_size, initial_size);

    /* Map the window */
    XMapWindow(display, icon_win);
    XFlush(display);

    printf("Tray icon created. Left-click to interact, Ctrl+C to quit.\n");

    return 0;
}

void xtray_poll()
{
	/* Main event loop */
	XEvent event;

	while (running) {
		XNextEvent(display, &event);

		switch (event.type) {
			case Expose:
				/* Redraw on expose (wait for last in sequence) */
				if (event.xexpose.count == 0) {
					draw_icon();
				}
				break;

			case ConfigureNotify:
				/* Window was resized (tray may resize us) */
				if (event.xconfigure.width != icon_width ||
					event.xconfigure.height != icon_height) {
					printf("Icon resized to %dx%d\n",
						   event.xconfigure.width, event.xconfigure.height);
					create_scaled_ximage(event.xconfigure.width,
										event.xconfigure.height);
					draw_icon();
					}
				break;

			case ButtonPress:
				if (event.xbutton.button == Button1) {
					on_left_click(event.xbutton.x, event.xbutton.y);
				} else if (event.xbutton.button == Button3) {
					printf("Right click - could show context menu\n");
				}
				break;

			case DestroyNotify:
				printf("Window destroyed, exiting...\n");
				running = 0;
				break;

			case ReparentNotify:
				printf("Window reparented (docked in tray)\n");
				break;
		}
	}
}

void xtray_cleanup()
{
	/* Signal the event loop to stop */
	running = 0;

	/* Send a dummy event to unblock XNextEvent if it's waiting */
	if (display && icon_win) {
		XEvent dummy;
		memset(&dummy, 0, sizeof(dummy));
		dummy.type = ClientMessage;
		dummy.xclient.window = icon_win;
		XSendEvent(display, icon_win, False, 0, &dummy);
		XFlush(display);
	}

	/* Cleanup */
	if (ximage) {
		XDestroyImage(ximage);  /* Frees pixel data too */
	}
	if (display && gc) {
		XFreeGC(display, gc);
	}
	if (display && icon_win) {
		XDestroyWindow(display, icon_win);
	}
	if (display) {
		XCloseDisplay(display);
	}

	if (orig_image) {
		stbi_image_free(orig_image);
	}
}
