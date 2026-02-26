#include "Engine.h"
#include "Platform.h"
#include "Overlay.h"
#include "Input.h"
#include <iostream>
#include <chrono>
#include <thread>
#include <cmath>

Engine::Engine() {}

Engine::~Engine() {}

void Engine::initialize() {
    state.active = false;
    state.gridRows = 3;
    state.gridCols = 3;
}

void Engine::run() {
    if (platform) {
        platform->run();
    }
}

void Engine::onActivate() {
    if (state.active) return;
    
    state.active = true;

    // Start with full root-screen rect.
    int w, h;
    platform->getScreenSize(w, h);
    state.currentRect = {0.0, 0.0, (double)w, (double)h};
    state.history.clear();

    overlay->show();

    // Overlay geometry can settle asynchronously (especially on Wayland/Xwayland).
    // Sample a few times and pick the largest stable bounds.
    Rect bestBounds = state.currentRect;
    double bestArea = bestBounds.w * bestBounds.h;
    for (int i = 0; i < 12; ++i) {
        Rect candidate;
        if (overlay->getBounds(candidate) && candidate.w > 1.0 && candidate.h > 1.0) {
            const double area = candidate.w * candidate.h;
            if (area > bestArea) {
                bestArea = area;
                bestBounds = candidate;
            }
        }
        std::this_thread::sleep_for(std::chrono::milliseconds(8));
    }

    auto nearValue = [](double a, double b, double eps) {
        return std::abs(a - b) <= eps;
    };

    // Accept the sampled bounds when they plausibly represent a monitor region.
    // Reject centered/partial transient geometries that cause visible margins.
    const double fullArea = state.currentRect.w * state.currentRect.h;
    const double areaRatio = (fullArea > 0.0) ? (bestArea / fullArea) : 0.0;
    const bool touchesXEdge = nearValue(bestBounds.x, 0.0, 3.0) ||
                              nearValue(bestBounds.x + bestBounds.w, (double)w, 3.0);
    const bool touchesYEdge = nearValue(bestBounds.y, 0.0, 3.0) ||
                              nearValue(bestBounds.y + bestBounds.h, (double)h, 3.0);
    const bool plausibleMonitorRect = (areaRatio >= 0.90) || (touchesXEdge && touchesYEdge);
    if (plausibleMonitorRect) {
        state.currentRect = bestBounds;
    }

    // Final edge snapping to avoid 1-2px border slivers.
    if (std::abs(state.currentRect.x) <= 2.0) state.currentRect.x = 0.0;
    if (std::abs(state.currentRect.y) <= 2.0) state.currentRect.y = 0.0;
    if (std::abs((state.currentRect.x + state.currentRect.w) - w) <= 2.0) {
        state.currentRect.w = (double)w - state.currentRect.x;
    }
    if (std::abs((state.currentRect.y + state.currentRect.h) - h) <= 2.0) {
        state.currentRect.h = (double)h - state.currentRect.y;
    }

    overlay->updateGrid(state.gridRows, state.gridCols,
                        state.currentRect.x, state.currentRect.y,
                        state.currentRect.w, state.currentRect.h);

    input->grabKeyboard();
    platform->releaseModifiers();
    std::cout << "Engine: Activated" << std::endl;
}

void Engine::onDeactivate() {
    if (!state.active) return;
    
    std::cout << "Engine: Deactivating..." << std::endl;
    state.active = false;
    overlay->hide();
    input->ungrabKeyboard();
    platform->releaseModifiers();
    std::cout << "Engine: Deactivated" << std::endl;
}

void Engine::onExit() {
    if (state.active) {
        onDeactivate();
    } else {
        platform->releaseModifiers();
    }
    if (platform) {
        platform->exit();
    }
}

void Engine::onKeyPress(int keyIndex) {
    if (!state.active) return;
    
    int r = keyIndex / state.gridCols;
    int c = keyIndex % state.gridCols;
    
    double cellW = state.currentRect.w / state.gridCols;
    double cellH = state.currentRect.h / state.gridRows;
    
    // Prevent infinite recursion
    if (cellW < 1.0 || cellH < 1.0) return;

    // Push history
    state.history.push_back(state.currentRect);
    
    // Update current rect
    state.currentRect.x += c * cellW;
    state.currentRect.y += r * cellH;
    state.currentRect.w = cellW;
    state.currentRect.h = cellH;
    
    updateOverlay();
    
    // Move cursor to center
    int centerX = (int)(state.currentRect.x + state.currentRect.w / 2);
    int centerY = (int)(state.currentRect.y + state.currentRect.h / 2);
    platform->moveCursor(centerX, centerY);
}

void Engine::onUndo() {
    if (!state.active || state.history.empty()) return;
    
    state.currentRect = state.history.back();
    state.history.pop_back();
    
    updateOverlay();
    
    // Move cursor to center of previous rect
    int centerX = (int)(state.currentRect.x + state.currentRect.w / 2);
    int centerY = (int)(state.currentRect.y + state.currentRect.h / 2);
    platform->moveCursor(centerX, centerY);
}

void Engine::onClick(int button, int count, bool deactivate) {
    if (!state.active) return;

    std::cout << "Engine: Click Request - Button: " << button << " Count: " << count << std::endl;

    bool shouldReShow = !deactivate;
    if (overlay) {
        overlay->hide();
    }

    // Move cursor to center before clicking
    int centerX = (int)(state.currentRect.x + state.currentRect.w / 2);
    int centerY = (int)(state.currentRect.y + state.currentRect.h / 2);
    platform->moveCursor(centerX, centerY);

    // Perform click
    platform->clickMouse(button, count);

    if (shouldReShow && overlay) {
        overlay->show();
    }
    
    if (deactivate) {
        // Small delay to ensure the OS/Application processes the click 
        // before we ungrab the keyboard and hide the overlay.
        std::this_thread::sleep_for(std::chrono::milliseconds(50));
        onDeactivate();
    }
}

void Engine::updateOverlay() {
    overlay->updateGrid(state.gridRows, state.gridCols, 
                        state.currentRect.x, state.currentRect.y, 
                        state.currentRect.w, state.currentRect.h);
}
