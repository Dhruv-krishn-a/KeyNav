#include "Engine.h"
#include "Platform.h"
#include "Overlay.h"
#include "Input.h"
#include "Config.h"
#include "Logger.h"
#include <chrono>
#include <thread>
#include <cmath>

Engine::Engine() {}

Engine::~Engine() {}

void Engine::initialize() {
    state.mode = EngineMode::Inactive;
    state.gridRows = Config::LEVEL0_GRID_ROWS;
    state.gridCols = Config::LEVEL0_GRID_COLS;
    state.recursionDepth = 0;
    state.lastPressedChar = '\0';
    state.showPoint = false;
}

void Engine::run() {
    if (platform) {
        platform->run();
    }
}

void Engine::onActivate() {
    if (state.mode != EngineMode::Inactive) return;
    
    state.mode = EngineMode::Level0_FirstChar;
    state.firstChar = '\0';
    state.gridRows = Config::LEVEL0_GRID_ROWS;
    state.gridCols = Config::LEVEL0_GRID_COLS;
    state.recursionDepth = 0;
    state.showPoint = false;

    // Start with full root-screen rect.
    int w, h;
    platform->getScreenSize(w, h);
    state.currentRect = {0.0, 0.0, (double)w, (double)h};
    state.history.clear();

    overlay->show();

    // Overlay geometry can settle asynchronously
    Rect bestBounds = state.currentRect;
    double bestArea = bestBounds.w * bestBounds.h;
    for (int i = 0; i < Config::OVERLAY_SETTLE_MAX_RETRIES; ++i) {
        Rect candidate;
        if (overlay->getBounds(candidate) && candidate.w > 1.0 && candidate.h > 1.0) {
            const double area = candidate.w * candidate.h;
            if (area > bestArea) {
                bestArea = area;
                bestBounds = candidate;
            }
        }
        std::this_thread::sleep_for(Config::OVERLAY_SETTLE_POLL_INTERVAL);
    }

    auto nearValue = [](double a, double b, double eps) {
        return std::abs(a - b) <= eps;
    };

    const double fullArea = state.currentRect.w * state.currentRect.h;
    const double areaRatio = (fullArea > 0.0) ? (bestArea / fullArea) : 0.0;
    const bool touchesXEdge = nearValue(bestBounds.x, 0.0, Config::OVERLAY_BOUNDS_EPSILON) ||
                              nearValue(bestBounds.x + bestBounds.w, (double)w, Config::OVERLAY_BOUNDS_EPSILON);
    const bool touchesYEdge = nearValue(bestBounds.y, 0.0, Config::OVERLAY_BOUNDS_EPSILON) ||
                              nearValue(bestBounds.y + bestBounds.h, (double)h, Config::OVERLAY_BOUNDS_EPSILON);
    const bool plausibleMonitorRect = (areaRatio >= 0.90) || (touchesXEdge && touchesYEdge);
    if (plausibleMonitorRect) {
        state.currentRect = bestBounds;
    }

    if (std::abs(state.currentRect.x) <= 2.0) state.currentRect.x = 0.0;
    if (std::abs(state.currentRect.y) <= 2.0) state.currentRect.y = 0.0;
    if (std::abs((state.currentRect.x + state.currentRect.w) - w) <= 2.0) {
        state.currentRect.w = (double)w - state.currentRect.x;
    }
    if (std::abs((state.currentRect.y + state.currentRect.h) - h) <= 2.0) {
        state.currentRect.h = (double)h - state.currentRect.y;
    }

    updateOverlay();

    input->grabKeyboard();
    platform->releaseModifiers();
    LOG_INFO("Engine: Activated");
}

void Engine::onDeactivate() {
    if (state.mode == EngineMode::Inactive) return;
    
    LOG_INFO("Engine: Deactivating...");
    state.mode = EngineMode::Inactive;
    overlay->hide();
    input->ungrabKeyboard();
    platform->releaseModifiers();
    LOG_INFO("Engine: Deactivated");
}

void Engine::onExit() {
    if (state.mode != EngineMode::Inactive) {
        onDeactivate();
    } else {
        platform->releaseModifiers();
    }
    if (platform) {
        platform->exit();
    }
}

void Engine::onChar(char c, bool shiftPressed) {
    if (state.mode == EngineMode::Inactive) return;
    
    if (c >= 'A' && c <= 'Z') {
        c = c + ('a' - 'A');
    }

    if (state.mode == EngineMode::Level0_FirstChar) {
        if (c >= 'a' && c < 'a' + state.gridRows) {
            state.firstChar = c;
            state.mode = EngineMode::Level0_SecondChar;
        }
    } 
    else if (state.mode == EngineMode::Level0_SecondChar) {
        if (c >= 'a' && c < 'a' + state.gridCols) {
            int r = state.firstChar - 'a';
            int col = c - 'a';
            
            double cellW = state.currentRect.w / state.gridCols;
            double cellH = state.currentRect.h / state.gridRows;
            
            state.history.push_back(state.currentRect);

            state.currentRect.x += col * cellW;
            state.currentRect.y += r * cellH;
            state.currentRect.w = cellW;
            state.currentRect.h = cellH;
            
            int cursorX = (int)(state.currentRect.x + state.currentRect.w / 2);
            int cursorY = (int)(state.currentRect.y + state.currentRect.h / 2);
            platform->moveCursor(cursorX, cursorY);
            
            // Switch to level 1 recursive mode
            state.gridRows = Config::LEVEL1_GRID_ROWS;
            state.gridCols = Config::LEVEL1_GRID_COLS;
            state.mode = EngineMode::Level1_Recursive;
            state.recursionDepth = 0;
            updateOverlay();
        }
    }
    else if (state.mode == EngineMode::Level1_Recursive) {
        if (state.recursionDepth >= Config::MAX_RECURSION_DEPTH) {
            return; // Stop recursion after reached max depth
        }

        int index = -1;
        if (c >= 'a' && c <= 'z') index = c - 'a';
        else if (c >= '0' && c <= '9') index = 26 + (c - '0');
        
        if (index >= 0 && index < state.gridRows * state.gridCols) {
            int r = index / state.gridCols;
            int col = index % state.gridCols;
            
            double cellW = state.currentRect.w / state.gridCols;
            double cellH = state.currentRect.h / state.gridRows;
            
            if (cellW < 1.0 || cellH < 1.0) return;

            state.history.push_back(state.currentRect);

            state.currentRect.x += col * cellW;
            state.currentRect.y += r * cellH;
            state.currentRect.w = cellW;
            state.currentRect.h = cellH;
            
            int cursorX = (int)(state.currentRect.x + state.currentRect.w / 2);
            int cursorY = (int)(state.currentRect.y + state.currentRect.h / 2);
            platform->moveCursor(cursorX, cursorY);
            
            state.recursionDepth++;
            state.lastPressedChar = c; // Remember this key to handle release later

            if (state.recursionDepth >= Config::MAX_RECURSION_DEPTH) {
                state.showPoint = true;
            }

            updateOverlay();
        }
    }
}

void Engine::onKeyRelease(char c) {
    if (state.mode == EngineMode::Inactive) return;
    
    if (c >= 'A' && c <= 'Z') {
        c = c + ('a' - 'A');
    }

    // If the final recursion key is released, we deactivate the engine.
    if (state.mode == EngineMode::Level1_Recursive && 
        state.recursionDepth >= Config::MAX_RECURSION_DEPTH &&
        c == state.lastPressedChar) {
        onDeactivate();
    }
}

void Engine::onControlKey(const std::string& key) {
    if (state.mode == EngineMode::Inactive) return;

    if (key == "space") {
        onClick(1, 1, true); // Left click
    } else if (key == "enter") {
        onClick(3, 1, true); // Right click
    } else if (key == "backspace") {
        onUndo();
    }
}

void Engine::onUndo() {
    if (state.mode == EngineMode::Inactive) return;
    
    // Ensure overlay is visible when we back up from a final selection
    overlay->show();
    state.showPoint = false;

    if (state.mode == EngineMode::Level0_SecondChar) {
        state.mode = EngineMode::Level0_FirstChar;
        state.firstChar = '\0';
    } 
    else if (state.mode == EngineMode::Level1_Recursive) {
        if (!state.history.empty()) {
            state.currentRect = state.history.back();
            state.history.pop_back();
            state.recursionDepth--;
            
            // If we popped back to the initial full screen, revert to Level0
            if (state.history.empty() || state.recursionDepth < 0) {
                state.mode = EngineMode::Level0_FirstChar;
                state.firstChar = '\0';
                state.gridRows = Config::LEVEL0_GRID_ROWS;
                state.gridCols = Config::LEVEL0_GRID_COLS;
                state.recursionDepth = 0;
            }
        }
        
        int cursorX = (int)(state.currentRect.x + state.currentRect.w / 2);
        int cursorY = (int)(state.currentRect.y + state.currentRect.h / 2);
        platform->moveCursor(cursorX, cursorY);
        
        updateOverlay();
    }
}

void Engine::onClick(int button, int count, bool deactivate) {
    if (state.mode == EngineMode::Inactive) return;

    LOG_INFO("Engine: Click Request - Button: ", button, " Count: ", count);

    int centerX = (int)(state.currentRect.x + state.currentRect.w / 2);
    int centerY = (int)(state.currentRect.y + state.currentRect.h / 2);
    platform->moveCursor(centerX, centerY);

    if (deactivate) {
        onDeactivate(); // Ungrabs the keyboard and hides overlay
        // Critical: Give GTK/Wayland a moment to process the keyboard ungrab
        // before we inject the mouse click, otherwise GTK ignores the click.
        std::this_thread::sleep_for(Config::POST_UNGRAB_DELAY);
    } else {
        // If we are NOT deactivating (just a click while holding a key), 
        // we briefly hide the overlay to let the OS process the click target
        // correctly if it's sensitive to overlay windows.
        if (overlay) overlay->hide();
        std::this_thread::sleep_for(Config::POST_UNGRAB_DELAY);
    }

    platform->clickMouse(button, count);

    if (!deactivate && overlay) {
        overlay->show();
    }
}

void Engine::updateOverlay() {
    overlay->updateGrid(state.gridRows, state.gridCols, 
                        state.currentRect.x, state.currentRect.y, 
                        state.currentRect.w, state.currentRect.h,
                        state.showPoint);
}
