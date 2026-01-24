/*
 * Copyright (c) 2026, Alex <uni@vrsal.cc>
 * SPDX-License-Identifier: BSD-3-Clause
 */
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
#define SYSTEM_TRAY_REQUEST_DOCK 0
#define SYSTEM_TRAY_BEGIN_MESSAGE 1
#define SYSTEM_TRAY_CANCEL_MESSAGE 2

/* Global state */
static Display* GDisplay;
static int      GScreen;
static Window   GIconWin;
static GC       GGc;
static XImage*  GXImage       = NULL;
static int      GIconWidth    = 0, GIconHeight = 0;
static int      bIsRunning    = 1;
static int	    bIsInitialized = 0;

/* Original image data */
static unsigned char* GOrigImage = NULL;
static int            GOrigWidth, GOrigHeight;

/*
 * Find the system tray window for the given screen
 */
static Window GetTrayWindow(void)
{
	char atomName[64];
	snprintf(atomName, sizeof(atomName), "_NET_SYSTEM_TRAY_S%d", GScreen);

	Atom selectionAtom = XInternAtom(GDisplay, atomName, False);
	return XGetSelectionOwner(GDisplay, selectionAtom);
}

/*
 * Send a dock request to embed our window in the system tray
 */
static void SendDockRequest(Window Tray)
{
	XEvent ev;
	memset(&ev, 0, sizeof(ev));

	Atom opcodeAtom = XInternAtom(GDisplay, "_NET_SYSTEM_TRAY_OPCODE", False);

	ev.xclient.type         = ClientMessage;
	ev.xclient.window       = Tray;
	ev.xclient.message_type = opcodeAtom;
	ev.xclient.format       = 32;
	ev.xclient.data.l[0]    = CurrentTime;
	ev.xclient.data.l[1]    = SYSTEM_TRAY_REQUEST_DOCK;
	ev.xclient.data.l[2]    = GIconWin;
	ev.xclient.data.l[3]    = 0;
	ev.xclient.data.l[4]    = 0;

	XSendEvent(GDisplay, Tray, False, NoEventMask, &ev);
	XFlush(GDisplay);
}

/*
 * Create/recreate the XImage with the icon scaled to the given size
 */
static void CreateScaledXImage(int Width, int Height)
{
	if (GXImage)
	{
		XDestroyImage(GXImage); /* This frees the data buffer too */
		GXImage = NULL;
	}

	Visual* visual = DefaultVisual(GDisplay, GScreen);
	int     depth  = DefaultDepth(GDisplay, GScreen);

	/* Allocate pixel buffer */
	uint32_t* pixels = malloc(Width * Height * sizeof(uint32_t));
	if (!pixels)
	{
		fprintf(stderr, "Failed to allocate pixel buffer\n");
		return;
	}

	/* Scale and convert image: RGBA -> native X11 format (typically BGRA) */
	for (int y = 0; y < Height; y++)
	{
		for (int x = 0; x < Width; x++)
		{
			/* Simple nearest-neighbor scaling */
			int srcX   = x * GOrigWidth / Width;
			int srcY   = y * GOrigHeight / Height;
			int srcIdx = (srcY * GOrigWidth + srcX) * 4;

			unsigned char r = GOrigImage[srcIdx + 0];
			unsigned char g = GOrigImage[srcIdx + 1];
			unsigned char b = GOrigImage[srcIdx + 2];
			unsigned char a = GOrigImage[srcIdx + 3];

			/* Pre-multiply alpha for better blending */
			r = (r * a) / 255;
			g = (g * a) / 255;
			b = (b * a) / 255;

			/* Pack as ARGB (X11 32-bit format on little-endian) */
			pixels[y * Width + x] = ((uint32_t)a << 24) | ((uint32_t)r << 16) | ((uint32_t)g << 8) | ((uint32_t)b);
		}
	}

	GXImage = XCreateImage(GDisplay, visual, depth, ZPixmap, 0, (char*)pixels, Width, Height, 32, Width * 4);

	if (!GXImage)
	{
		fprintf(stderr, "Failed to create XImage\n");
		free(pixels);
		return;
	}

	GIconWidth  = Width;
	GIconHeight = Height;
}

/*
 * Draw the icon to the window
 */
static void DrawIcon(void)
{
	if (GXImage && GIconWin)
	{
		XPutImage(GDisplay, GIconWin, GGc, GXImage, 0, 0, 0, 0, GIconWidth, GIconHeight);
		XFlush(GDisplay);
	}
}

/*
 * Handle left mouse button click
 */
static void OnLeftClick(int X, int Y)
{
	printf("Left click detected at position (%d, %d)!\n", X, Y);
}

int XTrayInit(unsigned char const* ImageData, unsigned int ImageDataSize)
{
	if (bIsInitialized)
	{
		return 1;
	}
	/* Load image using stb_image (force RGBA) */
	int channels;
	GOrigImage = stbi_load_from_memory(ImageData, ImageDataSize, &GOrigWidth, &GOrigHeight, &channels, 4);
	if (!GOrigImage)
	{
		return 0;
	}

	/* Open X display */
	GDisplay = XOpenDisplay(NULL);
	if (!GDisplay)
	{
		stbi_image_free(GOrigImage);
		return 0;
	}

	GScreen    = DefaultScreen(GDisplay);
	Window root = RootWindow(GDisplay, GScreen);

	/* Find the system tray */
	Window tray = GetTrayWindow();
	if (tray == None)
	{
		XCloseDisplay(GDisplay);
		stbi_image_free(GOrigImage);
		return 0;
	}

	/* Create the icon window */
	int initialSize = 24; /* Standard tray icon size */

	XSetWindowAttributes attrs;
	attrs.background_pixel = BlackPixel(GDisplay, GScreen);
	attrs.event_mask       = ExposureMask | ButtonPressMask | ButtonReleaseMask | StructureNotifyMask;

	GIconWin = XCreateWindow(GDisplay, root, 0, 0, initialSize, initialSize, 0, CopyFromParent, /* depth */
		InputOutput,                                                                             /* class */
		CopyFromParent,                                                                          /* visual */
		CWBackPixel | CWEventMask, &attrs);

	/* Request docking BEFORE mapping the window to avoid it appearing separately */
	SendDockRequest(tray);

	/* Set window class hints (some trays use this) */
	XClassHint classHint;
	classHint.res_name  = "tray_icon";
	classHint.res_class = "TrayIcon";
	XSetClassHint(GDisplay, GIconWin, &classHint);

	/* Prevent window from appearing in taskbar */
	Atom stateAtom        = XInternAtom(GDisplay, "_NET_WM_STATE", False);
	Atom skipTaskbar      = XInternAtom(GDisplay, "_NET_WM_STATE_SKIP_TASKBAR", False);
	Atom skipPager        = XInternAtom(GDisplay, "_NET_WM_STATE_SKIP_PAGER", False);
	Atom atoms[2]         = { skipTaskbar, skipPager };
	XChangeProperty(GDisplay, GIconWin, stateAtom, XA_ATOM, 32, PropModeReplace, (unsigned char*)atoms, 2);

	/* Set window type to notification to help window managers ignore it */
	Atom typeAtom          = XInternAtom(GDisplay, "_NET_WM_WINDOW_TYPE", False);
	Atom notificationType = XInternAtom(GDisplay, "_NET_WM_WINDOW_TYPE_NOTIFICATION", False);
	XChangeProperty(GDisplay, GIconWin, typeAtom, XA_ATOM, 32, PropModeReplace, (unsigned char*)&notificationType, 1);

	/* Create graphics context */
	GGc = XCreateGC(GDisplay, GIconWin, 0, NULL);

	/* Create initial scaled image */
	CreateScaledXImage(initialSize, initialSize);

	/* Map the window */
	XMapWindow(GDisplay, GIconWin);
	XFlush(GDisplay);
	bIsInitialized = 1;

	return 1;
}

void XTrayEventLoop()
{
	/* Main event loop */
	XEvent event;

	while (bIsRunning)
	{
		XNextEvent(GDisplay, &event);

		switch (event.type)
		{
			case Expose:
				/* Redraw on expose (wait for last in sequence) */
				if (event.xexpose.count == 0)
				{
					DrawIcon();
				}
				break;

			case ConfigureNotify:
				/* Window was resized (tray may resize us) */
				if (event.xconfigure.width != GIconWidth || event.xconfigure.height != GIconHeight)
				{
					printf("Icon resized to %dx%d\n", event.xconfigure.width, event.xconfigure.height);
					CreateScaledXImage(event.xconfigure.width, event.xconfigure.height);
					DrawIcon();
				}
				break;

			case ButtonPress:
				if (event.xbutton.button == Button1)
				{
					OnLeftClick(event.xbutton.x, event.xbutton.y);
				}
				else if (event.xbutton.button == Button3)
				{
					printf("Right click - could show context menu\n");
				}
				break;

			case DestroyNotify:
				printf("Window destroyed, exiting...\n");
				bIsRunning = 0;
				break;

			case ReparentNotify:
				printf("Window reparented (docked in tray)\n");
				break;
			default:;
		}
	}
}

void XTrayCleanup()
{
	/* Signal the event loop to stop */
	bIsRunning = 0;

	/* Cleanup */
	if (GXImage)
	{
		XDestroyImage(GXImage); /* Frees pixel data too */
	}
	if (GDisplay && GGc)
	{
		XFreeGC(GDisplay, GGc);
	}
	if (GDisplay && GIconWin)
	{
		XDestroyWindow(GDisplay, GIconWin);
	}
	if (GDisplay)
	{
		XCloseDisplay(GDisplay);
	}

	if (GOrigImage)
	{
		stbi_image_free(GOrigImage);
	}
}
