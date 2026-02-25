#ifndef X11OVERLAY_H
#define X11OVERLAY_H

#include "../../core/Overlay.h"
#include "../../core/Types.h"
#include <string>
#include <vector>
#include <X11/Xlib.h>
#include <X11/Xutil.h>
#include <X11/Xatom.h>
#include <cairo.h>
#include <cairo-xlib.h>
#include <mutex>

class X11Overlay : public Overlay {
public:
    X11Overlay(Display* d, int screen);
    ~X11Overlay();

    bool initialize();
    void show() override;
    void hide() override;
    void updateGrid(int rows, int cols, double x, double y, double w, double h) override;
    bool getBounds(Rect& out) override;

    Window getWindow() const { return window; }

    // Handle X11 Expose events from the platform loop
    void handleExpose();

private:
    void createWindow();
    void destroyWindow();
    void render();
    void renderLocked();

    Display* display;
    int screen;
    Window window = 0;
    
    // Cairo state
    cairo_surface_t* surface = nullptr;
    cairo_t* cr = nullptr;
    int surfaceW = 0;
    int surfaceH = 0;
    
    // Grid state
    int gridRows = 3;
    int gridCols = 3;
    Rect currentRect;
    bool runningOnWayland = false;
    
    bool isVisible = false;
    std::mutex overlayMutex;
};

#endif // X11OVERLAY_H
