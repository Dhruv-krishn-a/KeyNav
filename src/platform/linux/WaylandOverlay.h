#ifndef WAYLANDOVERLAY_H
#define WAYLANDOVERLAY_H

#include "../../core/Overlay.h"
#include "../../core/Types.h"
#include <gtk/gtk.h>
#include <gtk-layer-shell.h>
#include <mutex>

class WaylandOverlay : public Overlay {
public:
    WaylandOverlay();
    ~WaylandOverlay();

    bool initialize();

    void show() override;
    void hide() override;
    void updateGrid(int rows, int cols, double x, double y, double w, double h, bool showPoint = false) override;
    bool getBounds(Rect& out) override;
    void setGlobalOrigin(int x, int y);

private:
    static gboolean drawCallback(GtkWidget* widget, cairo_t* cr, gpointer data);
    static gboolean configureCallback(GtkWidget* widget, GdkEvent* event, gpointer data);
    static gboolean idleShow(gpointer data);
    static gboolean idleHide(gpointer data);
    static gboolean idleQueueDraw(gpointer data);

    void showOnMainThread();
    void hideOnMainThread();
    void queueDrawOnMainThread();
    void updateMonitorAndBoundsOnMainThread();

    GtkWidget* window = nullptr;
    bool initialized = false;
    bool visible = false;
    int globalOriginX = 0;
    int globalOriginY = 0;

    int gridRows = 3;
    int gridCols = 3;
    bool showTargetPoint = false;
    Rect currentRect{0.0, 0.0, 1.0, 1.0};
    Rect bounds{0.0, 0.0, 1.0, 1.0};

    std::mutex stateMutex;
};

#endif // WAYLANDOVERLAY_H
