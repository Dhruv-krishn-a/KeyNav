#include "X11Platform.h"
#include "X11Overlay.h"
#include "WaylandOverlay.h"
#include "X11Input.h"
#include "EvdevInput.h"
#include <iostream>
#include <poll.h>
#include <cstring>
#include <cstdlib>
#include <string>
#include <glib.h>
#include <X11/extensions/XTest.h>
#include <X11/keysym.h>

int x11ErrorHandler(Display* d, XErrorEvent* e) {
    char buffer[1024];
    XGetErrorText(d, e->error_code, buffer, 1024);
    std::cerr << "X11 Error: " << buffer << " (Opcode: " << (int)e->request_code << ")" << std::endl;
    return 0;
}

X11Platform::X11Platform(Engine* e, bool evdev) : engine(e), useEvdev(evdev) {}

X11Platform::~X11Platform() {
    if (input) delete input;
    if (overlay) delete overlay;
    if (display) XCloseDisplay(display);
}

bool X11Platform::initialize() {
    XInitThreads(); // Critical for multi-threaded X11 access (Evdev thread -> Engine -> Overlay)
    XSetErrorHandler(x11ErrorHandler);

    display = XOpenDisplay(NULL);
    if (!display) {
        std::cerr << "X11Platform: Cannot open display" << std::endl;
        return false;
    }
    screen = DefaultScreen(display);

    const char* sessionType = std::getenv("XDG_SESSION_TYPE");
    const char* waylandDisplay = std::getenv("WAYLAND_DISPLAY");
    const bool runningOnWayland = (waylandDisplay && waylandDisplay[0] != '\0') ||
                                  (sessionType && std::string(sessionType) == "wayland");

    // Create Overlay backend
    if (useEvdev && runningOnWayland) {
        waylandOverlay = new WaylandOverlay();

        if (waylandOverlay->initialize()) {
            overlay = waylandOverlay;
            usingWaylandOverlay = true;
            std::cout << "Using Native Wayland Layer-Shell Overlay" << std::endl;
        } else {
            delete waylandOverlay;
            waylandOverlay = nullptr;
            std::cerr << "Wayland overlay initialization failed." << std::endl;
            return false;
        }
    }

    if (!overlay) {
        x11Overlay = new X11Overlay(display, screen);
        if (!x11Overlay->initialize()) return false;
        overlay = x11Overlay;
    }

    if (useEvdev) {
        std::cout << "Using Evdev Input Backend (Requires sudo/uinput)" << std::endl;
        input = new EvdevInput(engine);
    } else {
        std::cout << "Using X11 Input Backend" << std::endl;
        input = new X11Input(display, engine);
    }
    
    int w = DisplayWidth(display, screen);
    int h = DisplayHeight(display, screen);
    if (!input->initialize(w, h)) {
        std::cerr << "Failed to initialize input backend." << std::endl;
        // Don't fail completely, maybe overlay works
        // return false; 
    }

    // Connect to Engine
    engine->setPlatform(this);
    engine->setOverlay(overlay);
    engine->setInput(input);

    return true;
}

void X11Platform::run() {
    isRunning = true;
    XEvent event;

    std::string activationKey = useEvdev ? "Alt+G or RIGHT CTRL" : "Alt+G";
    std::cout << "KeyNav Platform Running (" << activationKey << " to Activate)..." << std::endl;

    int x11Fd = ConnectionNumber(display);

    while (isRunning) {
        if (usingWaylandOverlay) {
            while (g_main_context_iteration(nullptr, false)) {}
        }

        // Use poll to wait for X events OR timeout, allowing us to check isRunning
        struct pollfd pfd;
        pfd.fd = x11Fd;
        pfd.events = POLLIN;

        int pollTimeoutMs = usingWaylandOverlay ? 10 : 100;
        int ret = poll(&pfd, 1, pollTimeoutMs);
        if (ret < 0) {
            if (errno != EINTR) std::cerr << "X11Platform: poll error: " << strerror(errno) << std::endl;
            break; 
        }

        // Process all pending X events
        while (XPending(display)) {
            XNextEvent(display, &event);

            // Route Events
            if (event.type == Expose && x11Overlay) {
                x11Overlay->handleExpose();
            } 
            else if (event.type == KeyPress || event.type == KeyRelease) {
                // Only route to X11Input if we are using it
                if (!useEvdev) {
                    static_cast<X11Input*>(input)->handleEvent(event);
                }
            }
        }
    }

    std::cout << "X11Platform: Run loop exiting..." << std::endl;
    // Final cleanup of modifiers on exit
    releaseModifiers();
}

void X11Platform::exit() {
    isRunning = false;
}

void X11Platform::getScreenSize(int& w, int& h) {
    Rect bounds;
    if (overlay && overlay->getBounds(bounds)) {
        if (bounds.w >= 64.0 && bounds.h >= 64.0) {
            w = (int)bounds.w;
            h = (int)bounds.h;
            return;
        }
    }

    // Get the actual physical screen size from the root window
    w = DisplayWidth(display, screen);
    h = DisplayHeight(display, screen);
}

void X11Platform::clickMouse(int button, int count) {
    if (useEvdev && input) {
        input->clickMouse(button, count);
    } else {
        // X11 button mapping: 1=Left, 2=Middle, 3=Right
        for (int i = 0; i < count; ++i) {
            XTestFakeButtonEvent(display, button, True, CurrentTime);
            XTestFakeButtonEvent(display, button, False, CurrentTime);
        }
        XFlush(display);
    }
}

void X11Platform::releaseModifiers() {
    // Release Alt and Ctrl keys via XTest to ensure no stuck modifiers
    // This is especially needed after an evdev grab which may have masked release events.
    
    KeySym keys[] = { 
        XK_Alt_L, XK_Alt_R, 
        XK_Control_L, XK_Control_R, 
        XK_Meta_L, XK_Meta_R, 
        XK_Super_L, XK_Super_R,
        XK_Shift_L, XK_Shift_R,
        XK_g, XK_G,
        XK_Escape
    };
    
    for (KeySym k : keys) {
        KeyCode kc = XKeysymToKeycode(display, k);
        if (kc) {
            XTestFakeKeyEvent(display, kc, False, CurrentTime);
        }
    }
    XFlush(display);
}
