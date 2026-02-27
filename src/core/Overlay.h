#ifndef OVERLAY_H
#define OVERLAY_H

#include "Types.h"

// Interface for overlay renderer
class Overlay {
public:
    virtual ~Overlay() {}
    virtual void show() = 0;
    virtual void hide() = 0;
    virtual void updateGrid(int rows, int cols, double x, double y, double w, double h, bool showPoint = false) = 0;
    virtual bool getBounds(Rect& out) = 0;
    // ... other visual updates
};

#endif // OVERLAY_H
