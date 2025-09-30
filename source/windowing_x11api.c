/*
 * windowing_x11api.c
 * ------------------
 * Windowing implementation for X11 (Linux).
 *
 * By: Marcus 2021, ported for X11 by [Nelson]
 */

#include "windowing.h"
#include <X11/Xlib.h>
#include <X11/Xutil.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

static Display *display = NULL;
static int screen;
static Window window;
static GC gc;
static int sInitialized = 0;
static int sWidth = 800;
static int sHeight = 600;
static int sActive = 1;

void WIN_Init() {
    // Nothing to do for X11 here
}

int WIN_Set(const char *title, int width, int height, int fullScreen, int scaleFactor, int minW, int minH) {
    sWidth = width;
    sHeight = height;

    display = XOpenDisplay(NULL);
    if (!display) {
        fprintf(stderr, "Cannot open X display\n");
        return WIN_FATAL_ERROR;
    }
    screen = DefaultScreen(display);

    XSetWindowAttributes attrs;
    attrs.background_pixel = WhitePixel(display, screen);
    attrs.border_pixel = BlackPixel(display, screen);
    attrs.event_mask = ExposureMask | KeyPressMask | KeyReleaseMask |
                       ButtonPressMask | ButtonReleaseMask | PointerMotionMask | StructureNotifyMask;

    window = XCreateWindow(
        display,
        RootWindow(display, screen),
        10, 10, sWidth, sHeight,
        1, // border width
        CopyFromParent, // depth
        InputOutput, // class
        CopyFromParent, // visual
        CWBackPixel | CWBorderPixel | CWEventMask,
        &attrs
    );

    XSetStandardProperties(display, window, title, title, None, NULL, 0, NULL);

    gc = XCreateGC(display, window, 0, NULL);

    XMapWindow(display, window);

    sInitialized = 1;
    return WIN_SUCCESS;
}

int WIN_HasWindow() {
    return sInitialized;
}

void WIN_Close() {
    if (display) {
        XDestroyWindow(display, window);
        XCloseDisplay(display);
        display = NULL;
        sInitialized = 0;
    }
}

void WIN_Update() {
    if (!display) return;
    XEvent event;
    while (XPending(display)) {
        XNextEvent(display, &event);
        switch (event.type) {
            case Expose:
                // Redraw window
                WIN_Redraw();
                break;
            case KeyPress:
                // TODO: call SYS_KeyDown(event.xkey.keycode);
                break;
            case KeyRelease:
                // TODO: call SYS_KeyUp(event.xkey.keycode);
                break;
            case ButtonPress:
                // TODO: call SYS_MouseDown(event.xbutton.button - 1);
                break;
            case ButtonRelease:
                // TODO: call SYS_MouseUp(event.xbutton.button - 1);
                break;
            case MotionNotify:
                // TODO: call SYS_MouseMove(event.xmotion.x, event.xmotion.y);
                break;
            case ConfigureNotify:
                sWidth = event.xconfigure.width;
                sHeight = event.xconfigure.height;
                break;
            case ClientMessage:
                // Handle window close
                WIN_Close();
                break;
        }
    }
}

void WIN_Redraw() {
    if (!display) return;
    // Fill window with white
    XSetForeground(display, gc, WhitePixel(display, screen));
    XFillRectangle(display, window, gc, 0, 0, sWidth, sHeight);
    XFlush(display);
}

int WIN_Active() {
    return sActive;
}

int WIN_Width() {
    return sWidth;
}

int WIN_Height() {
    return sHeight;
}
