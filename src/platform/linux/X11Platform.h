#ifndef X11PLATFORM_H
#define X11PLATFORM_H

#include "../../core/Platform.h"
#include "../../core/Engine.h"
#include "../../core/Input.h"
#include "../../core/Overlay.h"
#include <X11/Xlib.h>
#include <atomic>

class X11Overlay; // Forward decl
class X11Input;   // Forward decl
class WaylandOverlay; // Forward decl

class X11Platform : public Platform {
public:
    X11Platform(Engine* engine, bool useEvdev = false);
    ~X11Platform();

    bool initialize() override;
    void run() override;
    void exit() override;
    
    // Release modifiers using XTest (useful when ungrabbing evdev)
    void releaseModifiers() override;

    // Call from signal handler to ensure ungrab. 
    // MUST BE ASYNC-SIGNAL-SAFE (no X11 calls).
    void emergencyExit() {
        if (input) input->ungrabKeyboard();
        isRunning = false;
    }

    void getScreenSize(int& w, int& h) override;
    
    void moveCursor(int x, int y) override {
        if (useEvdev && input) {
            int w = DisplayWidth(display, screen);
            int h = DisplayHeight(display, screen);
            input->moveMouse(x, y, w, h);
        } else {
            XWarpPointer(display, None, RootWindow(display, screen), 0, 0, 0, 0, x, y);
            XFlush(display);
        }
    }

    Display* getDisplay() const { return display; }

private:
    Engine* engine;
    Display* display = nullptr;
    int screen = 0;
    std::atomic<bool> isRunning{false};
    bool useEvdev = false;
    bool usingWaylandOverlay = false;

    Overlay* overlay = nullptr;
    X11Overlay* x11Overlay = nullptr;
    WaylandOverlay* waylandOverlay = nullptr;
    Input* input = nullptr;        // Abstract Input
};

#endif // X11PLATFORM_H
