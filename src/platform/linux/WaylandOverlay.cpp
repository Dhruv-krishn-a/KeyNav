#include "WaylandOverlay.h"
#include <algorithm>
#include <array>
#include <cmath>
#include <iostream>
#include <string>
#include <glib.h>

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

std::string labelForIndex(int index, int cols) {
    if (cols == 6) {
        if (index >= 0 && index < 26) {
            char c = 'A' + index;
            return std::string(1, c);
        } else if (index >= 26 && index < 36) {
            char c = '0' + (index - 26);
            return std::string(1, c);
        }
    } else { // Assume 11x11
        int r = index / cols;
        int c = index % cols;
        if (r < 11 && c < 11) {
            char rowChar = 'A' + r;
            char colChar = 'A' + c;
            return std::string{rowChar, colChar};
        }
    }
    return "";
}

} // namespace

WaylandOverlay::WaylandOverlay() {}

WaylandOverlay::~WaylandOverlay() {
    if (window) {
        gtk_widget_destroy(window);
        window = nullptr;
    }
}

bool WaylandOverlay::initialize() {
    if (initialized) return true;

    // Force a native Wayland GDK backend for layer-shell behavior.
    g_setenv("GDK_BACKEND", "wayland", TRUE);

    if (!gtk_init_check(nullptr, nullptr)) {
        std::cerr << "WaylandOverlay: gtk_init_check failed" << std::endl;
        return false;
    }

    GdkDisplay* gdkDisplay = gdk_display_get_default();
    const char* displayName = gdkDisplay ? gdk_display_get_name(gdkDisplay) : nullptr;
    if (!displayName || std::string(displayName).find("wayland") == std::string::npos) {
        std::cerr << "WaylandOverlay: GDK backend is not Wayland (" 
                  << (displayName ? displayName : "unknown") << ")" << std::endl;
        return false;
    }

    window = gtk_window_new(GTK_WINDOW_TOPLEVEL);
    if (!window) {
        std::cerr << "WaylandOverlay: Failed to create GTK window" << std::endl;
        return false;
    }

    gtk_window_set_title(GTK_WINDOW(window), "KeyNav Overlay");
    gtk_window_set_decorated(GTK_WINDOW(window), FALSE);
    gtk_window_set_accept_focus(GTK_WINDOW(window), FALSE);
    gtk_window_set_skip_taskbar_hint(GTK_WINDOW(window), TRUE);
    gtk_window_set_skip_pager_hint(GTK_WINDOW(window), TRUE);
    gtk_window_set_keep_above(GTK_WINDOW(window), TRUE);
    gtk_widget_set_app_paintable(window, TRUE);

    GdkScreen* gdkScreen = gtk_widget_get_screen(window);
    if (gdkScreen) {
        GdkVisual* visual = gdk_screen_get_rgba_visual(gdkScreen);
        if (visual && gdk_screen_is_composited(gdkScreen)) {
            gtk_widget_set_visual(window, visual);
        }
    }

    gtk_layer_init_for_window(GTK_WINDOW(window));
    gtk_layer_set_layer(GTK_WINDOW(window), GTK_LAYER_SHELL_LAYER_OVERLAY);
    gtk_layer_set_anchor(GTK_WINDOW(window), GTK_LAYER_SHELL_EDGE_LEFT, TRUE);
    gtk_layer_set_anchor(GTK_WINDOW(window), GTK_LAYER_SHELL_EDGE_RIGHT, TRUE);
    gtk_layer_set_anchor(GTK_WINDOW(window), GTK_LAYER_SHELL_EDGE_TOP, TRUE);
    gtk_layer_set_anchor(GTK_WINDOW(window), GTK_LAYER_SHELL_EDGE_BOTTOM, TRUE);
    // -1 => place relative to the full output, not the "usable/work area".
    gtk_layer_set_exclusive_zone(GTK_WINDOW(window), -1);
    gtk_layer_set_keyboard_mode(GTK_WINDOW(window), GTK_LAYER_SHELL_KEYBOARD_MODE_NONE);
    gtk_layer_set_namespace(GTK_WINDOW(window), "KeyNav");

    g_signal_connect(window, "draw", G_CALLBACK(WaylandOverlay::drawCallback), this);
    g_signal_connect(window, "configure-event", G_CALLBACK(WaylandOverlay::configureCallback), this);

    updateMonitorAndBoundsOnMainThread();
    currentRect = bounds;
    initialized = true;
    return true;
}

void WaylandOverlay::show() {
    g_idle_add(WaylandOverlay::idleShow, this);
}

void WaylandOverlay::hide() {
    g_idle_add(WaylandOverlay::idleHide, this);
}

void WaylandOverlay::updateGrid(int rows, int cols, double x, double y, double w, double h) {
    {
        std::lock_guard<std::mutex> lock(stateMutex);
        gridRows = std::max(rows, 1);
        gridCols = std::max(cols, 1);
        currentRect = {x, y, w, h};
    }
    g_idle_add(WaylandOverlay::idleQueueDraw, this);
}

bool WaylandOverlay::getBounds(Rect& out) {
    std::lock_guard<std::mutex> lock(stateMutex);
    out = bounds;
    return out.w > 0.0 && out.h > 0.0;
}

void WaylandOverlay::setGlobalOrigin(int x, int y) {
    std::lock_guard<std::mutex> lock(stateMutex);
    globalOriginX = x;
    globalOriginY = y;
    bounds.x = (double)x;
    bounds.y = (double)y;
}

void WaylandOverlay::updateMonitorAndBoundsOnMainThread() {
    GdkDisplay* display = gdk_display_get_default();
    if (!display || !window) return;

    GdkMonitor* monitor = nullptr;

    GdkSeat* seat = gdk_display_get_default_seat(display);
    if (seat) {
        GdkDevice* pointer = gdk_seat_get_pointer(seat);
        if (pointer) {
            int px = 0;
            int py = 0;
            gdk_device_get_position(pointer, nullptr, &px, &py);
            monitor = gdk_display_get_monitor_at_point(display, px, py);
        }
    }

    if (!monitor) {
        monitor = gdk_display_get_primary_monitor(display);
    }
    if (!monitor && gdk_display_get_n_monitors(display) > 0) {
        monitor = gdk_display_get_monitor(display, 0);
    }

    if (monitor) {
        gtk_layer_set_monitor(GTK_WINDOW(window), monitor);
        GdkRectangle geometry;
        gdk_monitor_get_geometry(monitor, &geometry);
        {
            std::lock_guard<std::mutex> lock(stateMutex);
            globalOriginX = geometry.x;
            globalOriginY = geometry.y;
        }
    }

    int w = gtk_widget_get_allocated_width(window);
    int h = gtk_widget_get_allocated_height(window);
    if (w > 0 && h > 0) {
        std::lock_guard<std::mutex> lock(stateMutex);
        bounds.x = (double)globalOriginX;
        bounds.y = (double)globalOriginY;
        bounds.w = (double)w;
        bounds.h = (double)h;
    }
}

void WaylandOverlay::showOnMainThread() {
    if (!initialized || !window) return;
    updateMonitorAndBoundsOnMainThread();
    visible = true;
    gtk_widget_show(window);
    gtk_widget_queue_draw(window);
}

void WaylandOverlay::hideOnMainThread() {
    if (!window) return;
    visible = false;
    gtk_widget_hide(window);
}

void WaylandOverlay::queueDrawOnMainThread() {
    if (!window || !visible) return;
    gtk_widget_queue_draw(window);
}

gboolean WaylandOverlay::idleShow(gpointer data) {
    static_cast<WaylandOverlay*>(data)->showOnMainThread();
    return G_SOURCE_REMOVE;
}

gboolean WaylandOverlay::idleHide(gpointer data) {
    static_cast<WaylandOverlay*>(data)->hideOnMainThread();
    return G_SOURCE_REMOVE;
}

gboolean WaylandOverlay::idleQueueDraw(gpointer data) {
    static_cast<WaylandOverlay*>(data)->queueDrawOnMainThread();
    return G_SOURCE_REMOVE;
}

gboolean WaylandOverlay::configureCallback(GtkWidget* widget, GdkEvent* /*event*/, gpointer data) {
    auto* self = static_cast<WaylandOverlay*>(data);
    int w = gtk_widget_get_allocated_width(widget);
    int h = gtk_widget_get_allocated_height(widget);
    if (w > 0 && h > 0) {
        std::lock_guard<std::mutex> lock(self->stateMutex);
        self->bounds.x = (double)self->globalOriginX;
        self->bounds.y = (double)self->globalOriginY;
        self->bounds.w = (double)w;
        self->bounds.h = (double)h;
    }
    return FALSE;
}

gboolean WaylandOverlay::drawCallback(GtkWidget* widget, cairo_t* cr, gpointer data) {
    auto* self = static_cast<WaylandOverlay*>(data);

    Rect localBounds;
    Rect localRect;
    int rows = 3;
    int cols = 3;

    {
        std::lock_guard<std::mutex> lock(self->stateMutex);
        localBounds = self->bounds;
        localRect = self->currentRect;
        rows = self->gridRows;
        cols = self->gridCols;
    }

    const int surfaceW = gtk_widget_get_allocated_width(widget);
    const int surfaceH = gtk_widget_get_allocated_height(widget);
    if (surfaceW <= 0 || surfaceH <= 0 || rows <= 0 || cols <= 0) return FALSE;

    if (localBounds.w <= 0.0 || localBounds.h <= 0.0) {
        localBounds.w = (double)surfaceW;
        localBounds.h = (double)surfaceH;
    }

    Rect drawRect{
        localRect.x - localBounds.x,
        localRect.y - localBounds.y,
        localRect.w,
        localRect.h
    };

    drawRect.x = std::max(drawRect.x, -drawRect.w);
    drawRect.y = std::max(drawRect.y, -drawRect.h);
    drawRect.w = std::max(1.0, drawRect.w);
    drawRect.h = std::max(1.0, drawRect.h);

    if (std::abs(drawRect.x) <= 2.0) drawRect.x = 0.0;
    if (std::abs(drawRect.y) <= 2.0) drawRect.y = 0.0;
    if (std::abs((drawRect.x + drawRect.w) - surfaceW) <= 2.0) drawRect.w = std::max(1.0, (double)surfaceW - drawRect.x);
    if (std::abs((drawRect.y + drawRect.h) - surfaceH) <= 2.0) drawRect.h = std::max(1.0, (double)surfaceH - drawRect.y);

    const double surfaceArea = std::max(1.0, (double)surfaceW * (double)surfaceH);
    const double drawArea = drawRect.w * drawRect.h;
    const bool nearFullscreenArea = drawArea >= surfaceArea * 0.65;
    const bool touchesLeftOrRight = drawRect.x <= 2.0 || (drawRect.x + drawRect.w) >= ((double)surfaceW - 2.0);
    const bool touchesTopOrBottom = drawRect.y <= 2.0 || (drawRect.y + drawRect.h) >= ((double)surfaceH - 2.0);
    if (nearFullscreenArea && (!touchesLeftOrRight || !touchesTopOrBottom)) {
        drawRect = {0.0, 0.0, (double)surfaceW, (double)surfaceH};
    }

    cairo_save(cr);
    cairo_set_operator(cr, CAIRO_OPERATOR_CLEAR);
    cairo_paint(cr);
    cairo_restore(cr);

    cairo_set_antialias(cr, CAIRO_ANTIALIAS_NONE);
    cairo_set_line_join(cr, CAIRO_LINE_JOIN_MITER);
    cairo_set_line_cap(cr, CAIRO_LINE_CAP_SQUARE);

    const double minCell = std::min(drawRect.w / cols, drawRect.h / rows);
    double fontSizeMultiplier = (cols == 6) ? 0.35 : 0.25;
    const double fontSize = clampValue(minCell * fontSizeMultiplier, 12.0, 72.0);
    const double gridStroke = clampValue(minCell * 0.010, 1.0, 2.0);
    const double borderStroke = clampValue(minCell * 0.012, 1.2, 2.4);

    cairo_save(cr);
    cairo_rectangle(cr, 0.0, 0.0, (double)surfaceW, (double)surfaceH);
    cairo_clip(cr);

    for (int r = 0; r < rows; ++r) {
        const double y0 = drawRect.y + (drawRect.h * r) / rows;
        const double y1 = drawRect.y + (drawRect.h * (r + 1)) / rows;
        for (int c = 0; c < cols; ++c) {
            const double x0 = drawRect.x + (drawRect.w * c) / cols;
            const double x1 = drawRect.x + (drawRect.w * (c + 1)) / cols;

            const int index = r * cols + c;
            const Rgba fill = withAlpha(tileColorForIndex(index), 0.30);

            cairo_rectangle(cr, x0, y0, std::max(1.0, x1 - x0), std::max(1.0, y1 - y0));
            cairo_set_source_rgba(cr, fill.r, fill.g, fill.b, fill.a);
            cairo_fill(cr);
        }
    }

    cairo_set_source_rgba(cr, 0.92, 0.95, 1.0, 0.25);
    cairo_set_line_width(cr, gridStroke);
    for (int c = 1; c < cols; ++c) {
        const double x = drawRect.x + (drawRect.w * c) / cols;
        cairo_move_to(cr, x, drawRect.y);
        cairo_line_to(cr, x, drawRect.y + drawRect.h);
    }
    for (int r = 1; r < rows; ++r) {
        const double y = drawRect.y + (drawRect.h * r) / rows;
        cairo_move_to(cr, drawRect.x, y);
        cairo_line_to(cr, drawRect.x + drawRect.w, y);
    }
    cairo_stroke(cr);

    cairo_set_source_rgba(cr, 0.96, 0.97, 1.0, 0.75);
    cairo_set_line_width(cr, borderStroke);
    cairo_rectangle(cr, drawRect.x, drawRect.y, drawRect.w, drawRect.h);
    cairo_stroke(cr);

    cairo_select_font_face(cr, "Sans", CAIRO_FONT_SLANT_NORMAL, CAIRO_FONT_WEIGHT_BOLD);
    cairo_set_font_size(cr, fontSize);
    cairo_set_source_rgba(cr, 0.0, 0.0, 0.0, 1.0);

    for (int r = 0; r < rows; ++r) {
        const double y0 = drawRect.y + (drawRect.h * r) / rows;
        const double y1 = drawRect.y + (drawRect.h * (r + 1)) / rows;
        for (int c = 0; c < cols; ++c) {
            const double x0 = drawRect.x + (drawRect.w * c) / cols;
            const double x1 = drawRect.x + (drawRect.w * (c + 1)) / cols;
            const std::string label = labelForIndex(r * cols + c, cols);

            cairo_text_extents_t extents;
            cairo_text_extents(cr, label.c_str(), &extents);

            const double textX = x0 + ((x1 - x0) - extents.width) * 0.5 - extents.x_bearing;
            const double textY = y0 + ((y1 - y0) - extents.height) * 0.5 - extents.y_bearing;
            cairo_move_to(cr, textX, textY);
            cairo_show_text(cr, label.c_str());
        }
    }

    cairo_restore(cr);
    return FALSE;
}
