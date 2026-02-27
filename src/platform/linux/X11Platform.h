#ifndef X11PLATFORM_H
#define X11PLATFORM_H

#include "../../core/Platform.h"
#include "../../core/Engine.h"
#include "../../core/Input.h"
#include "../../core/Overlay.h"
#include <X11/Xlib.h>
#include <atomic>
#include <memory>

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

    void getScreenSize(int& w, int& h) override;
    
    void processX11Events();
    void setupSignalHandling();
    void processSignal();

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

    void clickMouse(int button, int count) override;

    Display* getDisplay() const { return display; }

private:
    Engine* engine;
    Display* display = nullptr;
    int screen = 0;
    int sigFd = -1;
    std::atomic<bool> isRunning{false};
    bool useEvdev = false;
    bool usingWaylandOverlay = false;

    Overlay* overlay = nullptr;
    std::unique_ptr<X11Overlay> x11Overlay;
    std::unique_ptr<WaylandOverlay> waylandOverlay;
    std::unique_ptr<Input> input;
};

#endif // X11PLATFORM_H
