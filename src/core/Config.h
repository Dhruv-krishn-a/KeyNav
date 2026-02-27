#ifndef CONFIG_H
#define CONFIG_H

#include <chrono>
#include <vector>

namespace Config {
    // Load configuration from disk (e.g. ~/.config/keynav/config.ini)
    void loadConfig();

    // Grid settings
    extern int LEVEL0_GRID_ROWS;
    extern int LEVEL0_GRID_COLS;
    extern int LEVEL1_GRID_ROWS;
    extern int LEVEL1_GRID_COLS;
    extern int MAX_RECURSION_DEPTH;

    // Overlay bounds tolerance (pixels)
    extern double OVERLAY_BOUNDS_EPSILON;

    // Timing
    extern std::chrono::milliseconds OVERLAY_SETTLE_POLL_INTERVAL;
    extern int OVERLAY_SETTLE_MAX_RETRIES;

    extern std::chrono::milliseconds POST_UNGRAB_DELAY;

    extern std::chrono::milliseconds CLICK_PRESS_RELEASE_DELAY;
    extern std::chrono::milliseconds DOUBLE_CLICK_DELAY;

    // UI Styling
    extern double OVERLAY_FILL_ALPHA;
    
    struct Rgba { double r, g, b, a; };
    
    // Default palette
    extern std::vector<Rgba> PALETTE;
}

#endif // CONFIG_H