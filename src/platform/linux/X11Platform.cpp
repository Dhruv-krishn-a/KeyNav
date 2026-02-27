#include "X11Platform.h"
#include "X11Overlay.h"
#include "WaylandOverlay.h"
#include "X11Input.h"
#include "EvdevInput.h"
#include "../../core/Logger.h"
#include <iostream>
#include <poll.h>
#include <cstring>
#include <cstdlib>
#include <string>
#include <glib.h>
#include <X11/extensions/XTest.h>
#include <X11/extensions/Xrandr.h>
#include <X11/keysym.h>
#include <sys/signalfd.h>
#include <signal.h>
#include <unistd.h>

int x11ErrorHandler(Display* d, XErrorEvent* e) {
    char buffer[1024];
    XGetErrorText(d, e->error_code, buffer, 1024);
    LOG_ERROR("X11 Error: ", buffer, " (Opcode: ", (int)e->request_code, ")");
    return 0;
}

X11Platform::X11Platform(Engine* e, bool evdev) : engine(e), useEvdev(evdev) {}

X11Platform::~X11Platform() {
    if (sigFd >= 0) close(sigFd);
    if (display) XCloseDisplay(display);
}

void X11Platform::setupSignalHandling() {
    sigset_t mask;
    sigemptyset(&mask);
    sigaddset(&mask, SIGINT);
    sigaddset(&mask, SIGTERM);

    // Block the signals so they don't hit their default asynchronous handlers
    if (sigprocmask(SIG_BLOCK, &mask, nullptr) == -1) {
        LOG_ERROR("X11Platform: sigprocmask failed");
    }

    // Create a file descriptor that we can poll to receive these signals synchronously
    sigFd = signalfd(-1, &mask, SFD_NONBLOCK | SFD_CLOEXEC);
    if (sigFd == -1) {
        LOG_ERROR("X11Platform: signalfd failed");
    }
}

void X11Platform::processSignal() {
    if (sigFd < 0) return;
    struct signalfd_siginfo fdsi;
    ssize_t s = read(sigFd, &fdsi, sizeof(struct signalfd_siginfo));
    if (s != sizeof(struct signalfd_siginfo)) return;

    if (fdsi.ssi_signo == SIGINT || fdsi.ssi_signo == SIGTERM) {
        LOG_INFO("X11Platform: Received shutdown signal (", fdsi.ssi_signo, "). Exiting gracefully...");
        if (input) input->ungrabKeyboard();
        isRunning = false;
    }
}

bool X11Platform::initialize() {
    XInitThreads();
    XSetErrorHandler(x11ErrorHandler);

    display = XOpenDisplay(NULL);
    if (!display) {
        LOG_ERROR("X11Platform: Cannot open display");
        return false;
    }
    screen = DefaultScreen(display);

    setupSignalHandling();

    const char* sessionType = std::getenv("XDG_SESSION_TYPE");
    const char* waylandDisplay = std::getenv("WAYLAND_DISPLAY");
    const bool runningOnWayland = (waylandDisplay && waylandDisplay[0] != '\0') ||
                                  (sessionType && std::string(sessionType) == "wayland");

    if (useEvdev && runningOnWayland) {
        waylandOverlay = std::make_unique<WaylandOverlay>();

        if (waylandOverlay->initialize()) {
            overlay = waylandOverlay.get();
            usingWaylandOverlay = true;
            LOG_INFO("Using Native Wayland Layer-Shell Overlay");
        } else {
            waylandOverlay.reset();
            LOG_ERROR("Wayland overlay initialization failed. Your compositor might not support wlr-layer-shell.");
            LOG_ERROR("ACTION REQUIRED: Try running without the --evdev flag to use the X11/XWayland fallback mode.");
            return false;
        }
    }

    if (!overlay) {
        x11Overlay = std::make_unique<X11Overlay>(display, screen);
        if (!x11Overlay->initialize()) return false;
        overlay = x11Overlay.get();
    }

    if (useEvdev) {
        LOG_INFO("Using Evdev Input Backend (Requires sudo/uinput)");
        input = std::make_unique<EvdevInput>(engine);
    } else {
        LOG_INFO("Using X11 Input Backend");
        input = std::make_unique<X11Input>(display, engine);
    }
    
    int w = DisplayWidth(display, screen);
    int h = DisplayHeight(display, screen);
    if (!input->initialize(w, h)) {
        LOG_ERROR("Failed to initialize input backend.");
        // Strict Init Contract: If primary input fails, KeyNav shouldn't run.
        return false; 
    }

    engine->setPlatform(this);
    engine->setOverlay(overlay);
    engine->setInput(input.get());

    return true;
}

void X11Platform::processX11Events() {
    XEvent event;
    while (XPending(display)) {
        XNextEvent(display, &event);

        if (event.type == Expose && x11Overlay) {
            x11Overlay->handleExpose();
        } 
        else if (event.type == KeyPress || event.type == KeyRelease) {
            if (!useEvdev) {
                static_cast<X11Input*>(input.get())->handleEvent(event);
            }
        }
    }
}

void X11Platform::run() {
    isRunning = true;

    std::string activationKey = useEvdev ? "Alt+G or RIGHT CTRL" : "Alt+G";
    LOG_INFO("KeyNav Platform Running (", activationKey, " to Activate)...");

    int x11Fd = ConnectionNumber(display);

    if (usingWaylandOverlay) {
        // Integrate X11 events into GLib main loop
        GIOChannel* x11Channel = g_io_channel_unix_new(x11Fd);
        g_io_add_watch(x11Channel, G_IO_IN, [](GIOChannel*, GIOCondition, gpointer data) -> gboolean {
            auto* platform = static_cast<X11Platform*>(data);
            platform->processX11Events();
            return G_SOURCE_CONTINUE;
        }, this);
        g_io_channel_unref(x11Channel);

        // Integrate Signals into GLib main loop
        if (sigFd >= 0) {
            GIOChannel* sigChannel = g_io_channel_unix_new(sigFd);
            g_io_add_watch(sigChannel, G_IO_IN, [](GIOChannel*, GIOCondition, gpointer data) -> gboolean {
                auto* platform = static_cast<X11Platform*>(data);
                platform->processSignal();
                if (!platform->isRunning) g_main_context_wakeup(nullptr);
                return G_SOURCE_CONTINUE;
            }, this);
            g_io_channel_unref(sigChannel);
        }

        while (isRunning) {
            g_main_context_iteration(nullptr, true); // True = Blocking wait
        }
    } else {
        // Native Poll loop for X11 without GLib
        while (isRunning) {
            struct pollfd pfds[2];
            pfds[0].fd = x11Fd;
            pfds[0].events = POLLIN;
            
            pfds[1].fd = sigFd;
            pfds[1].events = POLLIN;

            int ret = poll(pfds, 2, -1); // Infinite wait (-1), wake on input or signal!
            if (ret < 0) {
                if (errno != EINTR) LOG_ERROR("X11Platform: poll error: ", strerror(errno));
                break; 
            }

            if (pfds[0].revents & POLLIN) {
                processX11Events();
            }
            if (sigFd >= 0 && (pfds[1].revents & POLLIN)) {
                processSignal();
            }
        }
    }

    LOG_INFO("X11Platform: Run loop exiting...");
    releaseModifiers();
}

void X11Platform::exit() {
    isRunning = false;
    if (usingWaylandOverlay) {
        g_main_context_wakeup(nullptr);
    }
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

    if (!usingWaylandOverlay) {
        Window root = RootWindow(display, screen);
        int monitorCount = 0;
        XRRMonitorInfo* monitors = XRRGetMonitors(display, root, True, &monitorCount);
        if (monitors && monitorCount > 0) {
            int rootX = 0, rootY = 0, winX = 0, winY = 0;
            unsigned int mask = 0;
            Window rootReturn = 0, childReturn = 0;
            bool havePointer = XQueryPointer(display, root, &rootReturn, &childReturn, &rootX, &rootY, &winX, &winY, &mask);

            int selectedIndex = -1;
            if (havePointer) {
                for (int i = 0; i < monitorCount; ++i) {
                    if (rootX >= monitors[i].x && rootX < (monitors[i].x + monitors[i].width) &&
                        rootY >= monitors[i].y && rootY < (monitors[i].y + monitors[i].height)) {
                        selectedIndex = i;
                        break;
                    }
                }
            }
            if (selectedIndex < 0) {
                for (int i = 0; i < monitorCount; ++i) {
                    if (monitors[i].primary) { selectedIndex = i; break; }
                }
            }
            if (selectedIndex < 0) selectedIndex = 0;

            w = monitors[selectedIndex].width;
            h = monitors[selectedIndex].height;
            XRRFreeMonitors(monitors);
            return;
        }
    }

    w = DisplayWidth(display, screen);
    h = DisplayHeight(display, screen);
}

void X11Platform::clickMouse(int button, int count) {
    if (useEvdev && input) {
        input->clickMouse(button, count);
    } else {
        for (int i = 0; i < count; ++i) {
            XTestFakeButtonEvent(display, button, True, CurrentTime);
            XTestFakeButtonEvent(display, button, False, CurrentTime);
        }
        XFlush(display);
    }
}

void X11Platform::releaseModifiers() {
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
