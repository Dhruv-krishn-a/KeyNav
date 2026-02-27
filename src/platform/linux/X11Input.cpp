#include "X11Input.h"
#include "../../core/Engine.h"
#include <X11/keysym.h>
#include <iostream>
#include "../../core/Logger.h"

X11Input::X11Input(Display* d, Engine* e) : display(d), engine(e) {}

X11Input::~X11Input() {
    if (keyboardGrabbed) ungrabKeyboard();
    // Ungrab activation key? XCloseDisplay usually handles it, but good practice:
    // XUngrabKey(display, activationKeyCode, activationModifiers, DefaultRootWindow(display));
}

bool X11Input::initialize(int screenW, int screenH) {
    // Setup Activation Key: Alt + G
    activationKeySym = XK_g;
    activationModifiers = Mod1Mask; // Alt
    activationKeyCode = XKeysymToKeycode(display, activationKeySym);

    if (activationKeyCode == 0) {
        LOG_ERROR("X11Input: Failed to map activation key.");
        return false;
    }

    LOG_INFO("Key Mapped: G -> ", (int)activationKeyCode, " with modifiers: ", activationModifiers);

    grabActivationKey();
    return true;
}

void X11Input::grabActivationKey() {
    Window root = DefaultRootWindow(display);
    
    // Grab key with various modifier combinations (NumLock, CapsLock, ScrollLock issues)
    // Common ignore masks
    unsigned int modifiers[] = { 0, LockMask, Mod2Mask, LockMask | Mod2Mask };
    
    for (unsigned int m : modifiers) {
        int result = XGrabKey(display, activationKeyCode, activationModifiers | m, root,
                 True, GrabModeAsync, GrabModeAsync);
        // XGrabKey returns void? No, int usually (1 for request sent). Errors are async.
        // We rely on error handler.
    }
    XSync(display, False); // Force errors to be reported immediately
    
    LOG_INFO("Global Hotkey Initialized (Check for X11 errors above).");
}

void X11Input::grabKeyboard() {
    if (keyboardGrabbed) return;
    
    int result = XGrabKeyboard(display, DefaultRootWindow(display), True,
                               GrabModeAsync, GrabModeAsync, CurrentTime);
                               
    if (result == GrabSuccess) {
        keyboardGrabbed = true;
        // LOG_INFO("Keyboard grabbed.");
    } else {
        LOG_ERROR("Failed to grab keyboard. Result: ", result);
    }
}

void X11Input::ungrabKeyboard() {
    if (!keyboardGrabbed) return;
    
    XUngrabKeyboard(display, CurrentTime);
    keyboardGrabbed = false;
    // LOG_INFO("Keyboard ungrabbed.");
}

void X11Input::handleEvent(XEvent& event) {
    if (event.type != KeyPress && event.type != KeyRelease) return;
    
    // Debug log
    if (event.type == KeyPress) {
        LOG_INFO("Event: KeyPress ", event.xkey.keycode, " state: ", event.xkey.state);
    }

    KeySym key = XLookupKeysym(&event.xkey, 0);

    // If keyboard is grabbed (Active Mode)
    if (keyboardGrabbed) {
        bool pressed = (event.type == KeyPress);
        bool released = (event.type == KeyRelease);

        bool isAutoRepeat = false;
        if (released && XEventsQueued(display, QueuedAfterReading)) {
            XEvent nextEvent;
            XPeekEvent(display, &nextEvent);
            if (nextEvent.type == KeyPress && nextEvent.xkey.time == event.xkey.time && nextEvent.xkey.keycode == event.xkey.keycode) {
                isAutoRepeat = true;
            }
        }

        if (pressed) {
            if (key == XK_Escape) {
                engine->onDeactivate();
            } else if (key == XK_BackSpace) {
                engine->onControlKey("backspace");
            } else if (key == XK_Return) {
                engine->onControlKey("enter");
            } else if (key == XK_space) {
                engine->onControlKey("space");
            } else if (key == XK_f) {
                engine->onClick(1, 1, false); // Left click, STAY
            } else if (key >= XK_a && key <= XK_z) {
                bool shift = (event.xkey.state & ShiftMask) != 0;
                engine->onChar('a' + (key - XK_a), shift);
            } else if (key >= XK_A && key <= XK_Z) {
                engine->onChar('a' + (key - XK_A), true);
            } else if (key >= XK_0 && key <= XK_9) {
                engine->onChar('0' + (key - XK_0), false);
            }
        } else if (released && !isAutoRepeat) {
            if (key >= XK_a && key <= XK_z) {
                engine->onKeyRelease('a' + (key - XK_a));
            } else if (key >= XK_A && key <= XK_Z) {
                engine->onKeyRelease('a' + (key - XK_A));
            }
        }
        // Swallow other keys
    } 
    // If not grabbed (Idle Mode), check for activation
    else {
        // Check if it matches our activation key
        // Note: XGrabKey delivers the event to the root window (or our window if we asked).
        // The event loop in Platform needs to route it here.
        if (event.xkey.keycode == activationKeyCode) {
            // Check modifiers (ignoring Lock/Mod2 noise is handled by grab, but check just in case)
             engine->onActivate();
        }
    }
}
