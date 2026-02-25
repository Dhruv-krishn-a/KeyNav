#include "X11Overlay.h"
#include <iostream>
#include <algorithm>
#include <cstring>
#include <cstdlib>
#include <array>
#include <cmath>
#include <X11/extensions/Xrandr.h>

namespace {

struct Rgba {
    double r;
    double g;
    double b;
    double a;
};

double clampValue(double value, double minValue, double maxValue) {
    return std::max(minValue, std::min(value, maxValue));
}

Rgba withAlpha(const Rgba& color, double alpha) {
    return {color.r, color.g, color.b, alpha};
}

Rgba tileColorForIndex(int index) {
    static const std::array<Rgba, 9> palette = {{
        {0.91, 0.30, 0.27, 0.0}, // coral
        {0.95, 0.56, 0.20, 0.0}, // amber
        {0.95, 0.78, 0.27, 0.0}, // gold
        {0.36, 0.76, 0.44, 0.0}, // green
        {0.22, 0.72, 0.73, 0.0}, // cyan
        {0.25, 0.48, 0.86, 0.0}, // blue
        {0.48, 0.42, 0.87, 0.0}, // indigo
        {0.79, 0.37, 0.81, 0.0}, // violet
        {0.88, 0.36, 0.53, 0.0}  // rose
    }};
    return palette[index % palette.size()];
}

std::string labelForIndex(int index) {
    static const std::array<const char*, 9> labels = {{"Q", "W", "E", "A", "S", "D", "Z", "X", "C"}};
    if (index >= 0 && index < (int)labels.size()) {
        return labels[index];
    }
    return std::to_string(index + 1);
}

bool queryActiveMonitorRect(Display* display, int screen, Rect& out) {
    if (!display) return false;

    Window root = RootWindow(display, screen);
    int monitorCount = 0;
    XRRMonitorInfo* monitors = XRRGetMonitors(display, root, True, &monitorCount);
    if (!monitors || monitorCount <= 0) {
        if (monitors) XRRFreeMonitors(monitors);
        return false;
    }

    int rootX = 0;
    int rootY = 0;
    int winX = 0;
    int winY = 0;
    unsigned int mask = 0;
    Window rootReturn = 0;
    Window childReturn = 0;
    bool havePointer = XQueryPointer(display, root, &rootReturn, &childReturn,
                                     &rootX, &rootY, &winX, &winY, &mask) != 0;

    int selectedIndex = -1;
    if (havePointer) {
        for (int i = 0; i < monitorCount; ++i) {
            const XRRMonitorInfo& m = monitors[i];
            if (rootX >= m.x && rootX < (m.x + (int)m.width) &&
                rootY >= m.y && rootY < (m.y + (int)m.height)) {
                selectedIndex = i;
                break;
            }
        }
    }
    if (selectedIndex < 0) {
        for (int i = 0; i < monitorCount; ++i) {
            if (monitors[i].primary) {
                selectedIndex = i;
                break;
            }
        }
    }
    if (selectedIndex < 0) selectedIndex = 0;

    const XRRMonitorInfo& selected = monitors[selectedIndex];
    out = {(double)selected.x, (double)selected.y, (double)selected.width, (double)selected.height};
    XRRFreeMonitors(monitors);
    return out.w > 0.0 && out.h > 0.0;
}

} // namespace

X11Overlay::X11Overlay(Display* d, int s) : display(d), screen(s) {}

X11Overlay::~X11Overlay() {
    destroyWindow();
}

bool X11Overlay::initialize() {
    createWindow();
    return true;
}

void X11Overlay::createWindow() {
    const char* sessionType = std::getenv("XDG_SESSION_TYPE");
    const char* waylandDisplay = std::getenv("WAYLAND_DISPLAY");
    runningOnWayland = (waylandDisplay && waylandDisplay[0] != '\0') ||
                       (sessionType && std::string(sessionType) == "wayland");

    XVisualInfo vinfo;
    if (!XMatchVisualInfo(display, screen, 32, TrueColor, &vinfo)) {
        std::cerr << "No 32-bit visual found" << std::endl;
        return;
    }

    Rect monitorRect{0.0, 0.0, (double)DisplayWidth(display, screen), (double)DisplayHeight(display, screen)};
    if (!runningOnWayland) {
        queryActiveMonitorRect(display, screen, monitorRect);
    }
    int screenX = (int)monitorRect.x;
    int screenY = (int)monitorRect.y;
    int screenW = (int)monitorRect.w;
    int screenH = (int)monitorRect.h;

    XSetWindowAttributes attrs;
    attrs.colormap = XCreateColormap(display, RootWindow(display, screen), vinfo.visual, AllocNone);
    attrs.background_pixel = 0;
    attrs.border_pixel = 0;
    // Always unmanaged to avoid WM tiling/floating geometry and ensure
    // this overlay can be pinned exactly to monitor bounds.
    attrs.override_redirect = True;
    attrs.save_under = True;        // Helps with performance/transparency

    window = XCreateWindow(display, RootWindow(display, screen),
                           screenX, screenY, screenW, screenH,
                           0, vinfo.depth, InputOutput, vinfo.visual,
                           CWColormap | CWBackPixel | CWBorderPixel | CWOverrideRedirect | CWSaveUnder, &attrs);

    XStoreName(display, window, "KeyNav Overlay");

    // Avoid taking focus when shown.
    XWMHints wmHints;
    std::memset(&wmHints, 0, sizeof(wmHints));
    wmHints.flags = InputHint;
    wmHints.input = False;
    XSetWMHints(display, window, &wmHints);

    // Motif hints to remove decorations (backup for override_redirect)
    struct {
        unsigned long flags;
        unsigned long functions;
        unsigned long decorations;
        long input_mode;
        unsigned long status;
    } mwmhints;
    mwmhints.flags = 2; // MWM_HINTS_DECORATIONS
    mwmhints.decorations = 0; // No decorations
    Atom prop = XInternAtom(display, "_MOTIF_WM_HINTS", False);
    XChangeProperty(display, window, prop, prop, 32, PropModeReplace, (unsigned char*)&mwmhints, 5);

    // Keep only top-layer/taskbar skip hints. Do not request fullscreen state
    // because several compositors handle Xwayland fullscreen/transparency poorly.
    Atom wm_state = XInternAtom(display, "_NET_WM_STATE", False);
    Atom above = XInternAtom(display, "_NET_WM_STATE_ABOVE", False);
    Atom skipTaskbar = XInternAtom(display, "_NET_WM_STATE_SKIP_TASKBAR", False);
    Atom skipPager = XInternAtom(display, "_NET_WM_STATE_SKIP_PAGER", False);
    Atom states[3] = {above, skipTaskbar, skipPager};
    XChangeProperty(display, window, wm_state, XA_ATOM, 32, PropModeReplace, (unsigned char*)states, 3);

    // Keep compositing enabled for this ARGB overlay so transparent regions
    // reveal real window contents (not just the root wallpaper).
    Atom bypass = XInternAtom(display, "_NET_WM_BYPASS_COMPOSITOR", False);
    long bypass_val = 0;
    XChangeProperty(display, window, bypass, XA_CARDINAL, 32, PropModeReplace, (unsigned char*)&bypass_val, 1);

    // Set class to ensure it's treated as a system overlay
    XClassHint classHint;
    char name[] = "KeyNav";
    char cls[] = "KeyNav";
    classHint.res_name = name;
    classHint.res_class = cls;
    XSetClassHint(display, window, &classHint);

    // Only listen for Expose events. Input is handled globally now.
    XSelectInput(display, window, ExposureMask | StructureNotifyMask);

    surface = cairo_xlib_surface_create(display, window, vinfo.visual, screenW, screenH);
    cr = cairo_create(surface);
    surfaceW = screenW;
    surfaceH = screenH;

    // Initial rect
    currentRect = monitorRect;
}

void X11Overlay::destroyWindow() {
    std::lock_guard<std::mutex> lock(overlayMutex);

    if (cr) cairo_destroy(cr);
    if (surface) cairo_surface_destroy(surface);
    if (window) XDestroyWindow(display, window);
    cr = nullptr;
    surface = nullptr;
    window = 0;
    surfaceW = 0;
    surfaceH = 0;
}

void X11Overlay::show() {
    std::lock_guard<std::mutex> lock(overlayMutex);

    isVisible = true;
    
    // Force-reposition before and after mapping.
    Rect monitorRect{0.0, 0.0, (double)DisplayWidth(display, screen), (double)DisplayHeight(display, screen)};
    if (!runningOnWayland) {
        queryActiveMonitorRect(display, screen, monitorRect);
    }
    int screenX = (int)monitorRect.x;
    int screenY = (int)monitorRect.y;
    int screenW = (int)monitorRect.w;
    int screenH = (int)monitorRect.h;
    int requestX = screenX;
    int requestY = screenY;
    int requestW = screenW;
    int requestH = screenH;
    XMoveResizeWindow(display, window, requestX, requestY, requestW, requestH);

    XMapRaised(display, window);

    // Some WMs/Xwayland setups apply geometry asynchronously.
    // Iteratively overscan if the compositor insets/shrinks this window.
    for (int i = 0; i < 5; ++i) {
        XSync(display, False);
        XMoveResizeWindow(display, window, requestX, requestY, requestW, requestH);
        XRaiseWindow(display, window);
        XSync(display, False);

        XWindowAttributes attrs;
        Window child = 0;
        int actualX = 0;
        int actualY = 0;
        int actualW = 0;
        int actualH = 0;
        if (!XGetWindowAttributes(display, window, &attrs)) continue;
        actualW = attrs.width;
        actualH = attrs.height;
        if (!XTranslateCoordinates(display, window, RootWindow(display, screen), 0, 0, &actualX, &actualY, &child)) {
            continue;
        }

        const int gapL = actualX - screenX;
        const int gapT = actualY - screenY;
        const int gapR = (screenX + screenW) - (actualX + actualW);
        const int gapB = (screenY + screenH) - (actualY + actualH);

        if (std::abs(gapL) <= 1 && std::abs(gapT) <= 1 &&
            std::abs(gapR) <= 1 && std::abs(gapB) <= 1) {
            break;
        }

        // Expand request only where compositor is leaving positive gaps.
        if (gapL > 0) {
            requestX -= gapL;
            requestW += gapL;
        }
        if (gapT > 0) {
            requestY -= gapT;
            requestH += gapT;
        }
        if (gapR > 0) requestW += gapR;
        if (gapB > 0) requestH += gapB;

        requestW = std::max(requestW, screenW);
        requestH = std::max(requestH, screenH);
    }
    XFlush(display);
}

void X11Overlay::hide() {
    std::lock_guard<std::mutex> lock(overlayMutex);

    isVisible = false;
    XUnmapWindow(display, window);
    XFlush(display);
}

void X11Overlay::updateGrid(int rows, int cols, double x, double y, double w, double h) {
    std::lock_guard<std::mutex> lock(overlayMutex);

    gridRows = rows;
    gridCols = cols;
    currentRect = {x, y, w, h};
    renderLocked();
}

bool X11Overlay::getBounds(Rect& out) {
    std::lock_guard<std::mutex> lock(overlayMutex);

    if (!window) return false;

    XWindowAttributes attrs;
    if (!XGetWindowAttributes(display, window, &attrs)) return false;

    Window child = 0;
    int absX = 0;
    int absY = 0;
    if (!XTranslateCoordinates(display, window, RootWindow(display, screen), 0, 0, &absX, &absY, &child)) {
        return false;
    }

    out = {(double)absX, (double)absY, (double)attrs.width, (double)attrs.height};
    return true;
}

void X11Overlay::handleExpose() {
    std::lock_guard<std::mutex> lock(overlayMutex);
    if (isVisible) renderLocked();
}

void X11Overlay::render() {
    std::lock_guard<std::mutex> lock(overlayMutex);
    renderLocked();
}

void X11Overlay::renderLocked() {
    if (!isVisible || !cr || gridRows <= 0 || gridCols <= 0) return;

    // Ensure the Cairo surface matches the window size
    XWindowAttributes attrs;
    if (XGetWindowAttributes(display, window, &attrs)) {
        if (!surface) {
            XVisualInfo vinfo;
            XMatchVisualInfo(display, screen, 32, TrueColor, &vinfo);
            surface = cairo_xlib_surface_create(display, window, vinfo.visual, attrs.width, attrs.height);
            cr = cairo_create(surface);
            surfaceW = attrs.width;
            surfaceH = attrs.height;
        } else if (attrs.width != surfaceW || attrs.height != surfaceH) {
            cairo_xlib_surface_set_size(surface, attrs.width, attrs.height);
            surfaceW = attrs.width;
            surfaceH = attrs.height;
        }
    }

    Rect windowRect{0.0, 0.0, (double)surfaceW, (double)surfaceH};
    Window child = 0;
    int absX = 0;
    int absY = 0;
    if (window && XTranslateCoordinates(display, window, RootWindow(display, screen), 0, 0, &absX, &absY, &child)) {
        windowRect.x = (double)absX;
        windowRect.y = (double)absY;
    }

    // Convert from root coordinates to this window's local coordinates.
    Rect drawRect{
        currentRect.x - windowRect.x,
        currentRect.y - windowRect.y,
        currentRect.w,
        currentRect.h
    };

    // Keep draw rect within sane values to avoid rendering artifacts.
    drawRect.x = std::max(drawRect.x, -drawRect.w);
    drawRect.y = std::max(drawRect.y, -drawRect.h);
    drawRect.w = std::max(1.0, drawRect.w);
    drawRect.h = std::max(1.0, drawRect.h);

    // Snap near-fullscreen rects to exact pixel bounds to avoid visible margins.
    if (std::abs(drawRect.x) <= 2.0) drawRect.x = 0.0;
    if (std::abs(drawRect.y) <= 2.0) drawRect.y = 0.0;
    if (std::abs((drawRect.x + drawRect.w) - surfaceW) <= 2.0) drawRect.w = std::max(1.0, (double)surfaceW - drawRect.x);
    if (std::abs((drawRect.y + drawRect.h) - surfaceH) <= 2.0) drawRect.h = std::max(1.0, (double)surfaceH - drawRect.y);

    // Guard against transient inset geometries captured during activation.
    // If the rect is still "mostly fullscreen" but detached from surface edges,
    // force a full-surface draw to avoid corner gaps.
    const double surfaceArea = std::max(1.0, (double)surfaceW * (double)surfaceH);
    const double drawArea = drawRect.w * drawRect.h;
    const bool nearFullscreenArea = drawArea >= surfaceArea * 0.65;
    const bool touchesLeftOrRight = drawRect.x <= 2.0 || (drawRect.x + drawRect.w) >= ((double)surfaceW - 2.0);
    const bool touchesTopOrBottom = drawRect.y <= 2.0 || (drawRect.y + drawRect.h) >= ((double)surfaceH - 2.0);
    if (nearFullscreenArea && (!touchesLeftOrRight || !touchesTopOrBottom)) {
        drawRect = {0.0, 0.0, (double)surfaceW, (double)surfaceH};
    }

    // Clear background
    cairo_save(cr);
    cairo_set_operator(cr, CAIRO_OPERATOR_CLEAR);
    cairo_paint(cr);
    cairo_restore(cr);

    // Rectangular production layout: edge-to-edge cells, no spacing.
    cairo_set_antialias(cr, CAIRO_ANTIALIAS_NONE);
    cairo_set_line_join(cr, CAIRO_LINE_JOIN_MITER);
    cairo_set_line_cap(cr, CAIRO_LINE_CAP_SQUARE);

    const double minCell = std::min(drawRect.w / gridCols, drawRect.h / gridRows);
    const double fontSize = clampValue(minCell * 0.14, 14.0, 42.0);
    const double gridStroke = clampValue(minCell * 0.010, 1.0, 2.0);
    const double borderStroke = clampValue(minCell * 0.012, 1.2, 2.4);

    // Clip drawing to visible surface bounds for robustness.
    cairo_save(cr);
    cairo_rectangle(cr, 0.0, 0.0, (double)surfaceW, (double)surfaceH);
    cairo_clip(cr);

    // Draw contiguous translucent cells (no gap).
    for (int r = 0; r < gridRows; ++r) {
        const double y0 = drawRect.y + (drawRect.h * r) / gridRows;
        const double y1 = drawRect.y + (drawRect.h * (r + 1)) / gridRows;
        for (int c = 0; c < gridCols; ++c) {
            const double x0 = drawRect.x + (drawRect.w * c) / gridCols;
            const double x1 = drawRect.x + (drawRect.w * (c + 1)) / gridCols;

            const int index = r * gridCols + c;
            const Rgba fill = withAlpha(tileColorForIndex(index), 0.22);

            cairo_rectangle(cr, x0, y0, std::max(1.0, x1 - x0), std::max(1.0, y1 - y0));
            cairo_set_source_rgba(cr, fill.r, fill.g, fill.b, fill.a);
            cairo_fill(cr);
        }
    }

    // Grid dividers.
    cairo_set_source_rgba(cr, 0.92, 0.95, 1.0, 0.55);
    cairo_set_line_width(cr, gridStroke);
    for (int c = 1; c < gridCols; ++c) {
        const double x = drawRect.x + (drawRect.w * c) / gridCols;
        cairo_move_to(cr, x, drawRect.y);
        cairo_line_to(cr, x, drawRect.y + drawRect.h);
    }
    for (int r = 1; r < gridRows; ++r) {
        const double y = drawRect.y + (drawRect.h * r) / gridRows;
        cairo_move_to(cr, drawRect.x, y);
        cairo_line_to(cr, drawRect.x + drawRect.w, y);
    }
    cairo_stroke(cr);

    // Outer border end-to-end.
    cairo_set_source_rgba(cr, 0.96, 0.97, 1.0, 0.75);
    cairo_set_line_width(cr, borderStroke);
    cairo_rectangle(cr, drawRect.x, drawRect.y, drawRect.w, drawRect.h);
    cairo_stroke(cr);

    // Key labels (smaller, readable).
    cairo_select_font_face(cr, "Sans", CAIRO_FONT_SLANT_NORMAL, CAIRO_FONT_WEIGHT_BOLD);
    cairo_set_font_size(cr, fontSize);
    cairo_set_source_rgba(cr, 0.98, 0.99, 1.0, 0.92);

    for (int r = 0; r < gridRows; ++r) {
        const double y0 = drawRect.y + (drawRect.h * r) / gridRows;
        const double y1 = drawRect.y + (drawRect.h * (r + 1)) / gridRows;
        for (int c = 0; c < gridCols; ++c) {
            const double x0 = drawRect.x + (drawRect.w * c) / gridCols;
            const double x1 = drawRect.x + (drawRect.w * (c + 1)) / gridCols;
            const std::string label = labelForIndex(r * gridCols + c);

            cairo_text_extents_t extents;
            cairo_text_extents(cr, label.c_str(), &extents);

            const double textX = x0 + ((x1 - x0) - extents.width) * 0.5 - extents.x_bearing;
            const double textY = y0 + ((y1 - y0) - extents.height) * 0.5 - extents.y_bearing;
            cairo_move_to(cr, textX, textY);
            cairo_show_text(cr, label.c_str());
        }
    }

    cairo_restore(cr);
    cairo_surface_flush(surface);
}
