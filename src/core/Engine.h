#ifndef ENGINE_H
#define ENGINE_H

#include <vector>
#include <string>
#include "Types.h"

// Forward declarations
class Platform;
class Overlay;
class Input;

enum class EngineMode {
    Inactive,
    Level0_FirstChar,
    Level0_SecondChar,
    Level1_Recursive
};

struct EngineState {
    EngineMode mode = EngineMode::Inactive;
    Rect currentRect;
    std::vector<Rect> history;
    int gridRows = 10;
    int gridCols = 10;
    char firstChar = '\0';
    char lastPressedChar = '\0';
    bool showPoint = false;
    int recursionDepth = 0;
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
    void onChar(char c, bool shiftPressed);
    void onKeyRelease(char c);
    void onControlKey(const std::string& key);
    void onUndo();
    void onClick(int button, int count, bool deactivate = true);
    void onExit(); 
    
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
