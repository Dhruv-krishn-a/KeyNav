#ifndef ENGINE_H
#define ENGINE_H

#include <vector>
#include "Types.h"

// Forward declarations
class Platform;
class Overlay;
class Input;

struct EngineState {
    bool active = false;
    Rect currentRect;
    std::vector<Rect> history;
    int gridRows = 3;
    int gridCols = 3;
};

class Engine {
public:
    Engine();
    ~Engine();

    void initialize();
    void run();

    // Callbacks from Platform/Input
    void onActivate(); 
    void onDeactivate(); 
    void onKeyPress(int keyIndex); // 0-8 for cells 1-9
    void onUndo(); 
    void onExit(); // New exit method
    
    // Dependencies
    void setPlatform(Platform* p) { platform = p; }
    void setOverlay(Overlay* o) { overlay = o; }
    void setInput(Input* i) { input = i; }

private:
    void updateOverlay();
    void resetSelection();

    Platform* platform = nullptr;
    Overlay* overlay = nullptr;
    Input* input = nullptr;
    EngineState state;
};

#endif // ENGINE_H
