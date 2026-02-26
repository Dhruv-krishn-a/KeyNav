#include "X11Input.h"
#include "../../core/Engine.h"
#include <X11/keysym.h>
#include <iostream>

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
        std::cerr << "X11Input: Failed to map activation key." << std::endl;
        return false;
    }

    std::cout << "Key Mapped: G -> " << (int)activationKeyCode << " with modifiers: " << activationModifiers << std::endl;

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
    
    std::cout << "Global Hotkey Initialized (Check for X11 errors above)." << std::endl;
}

void X11Input::grabKeyboard() {
    if (keyboardGrabbed) return;
    
    int result = XGrabKeyboard(display, DefaultRootWindow(display), True,
                               GrabModeAsync, GrabModeAsync, CurrentTime);
                               
    if (result == GrabSuccess) {
        keyboardGrabbed = true;
        // std::cout << "Keyboard grabbed." << std::endl;
    } else {
        std::cerr << "Failed to grab keyboard. Result: " << result << std::endl;
    }
}

void X11Input::ungrabKeyboard() {
    if (!keyboardGrabbed) return;
    
    XUngrabKeyboard(display, CurrentTime);
    keyboardGrabbed = false;
    // std::cout << "Keyboard ungrabbed." << std::endl;
}

void X11Input::handleEvent(XEvent& event) {
    if (event.type != KeyPress && event.type != KeyRelease) return;
    
    // Debug log
    if (event.type == KeyPress) {
        std::cout << "Event: KeyPress " << event.xkey.keycode << " state: " << event.xkey.state << std::endl;
    }

    // We only care about KeyPress for logic
    if (event.type == KeyRelease) return;

    KeySym key = XLookupKeysym(&event.xkey, 0);

    // If keyboard is grabbed (Active Mode)
    if (keyboardGrabbed) {
        if (key == XK_Escape) {
            engine->onDeactivate();
        } else if (key >= XK_1 && key <= XK_9) {
            engine->onKeyPress(key - XK_1); // 0-8
        } else if (key == XK_BackSpace) {
            engine->onUndo();
        } else if (key == XK_Return) {
            engine->onClick(1, 1, true); // Left click, single, close
        } else if (key == XK_space) {
            engine->onClick(1, 2, true); // Left click, double, close
        } else if (key == XK_r) {
            engine->onClick(3, 1, true); // Right click, single, close
        } else if (key == XK_m) {
            engine->onClick(2, 1, true); // Middle click, single, close
        } else if (key == XK_f) {
            engine->onClick(1, 1, false); // Left click, single, STAY
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
