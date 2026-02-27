#ifndef EVDEVINPUT_H
#define EVDEVINPUT_H

#include "../../core/Input.h"
#include <vector>
#include <string>
#include <thread>
#include <atomic>

class Engine;

class EvdevInput : public Input {
public:
    EvdevInput(Engine* e);
    ~EvdevInput();

    bool initialize(int screenW = 0, int screenH = 0) override;
    void grabKeyboard() override;
    void ungrabKeyboard() override;

    // Main loop for reading events (runs in separate thread)
    void eventLoop();

private:
        void openDevices();
    void closeDevices();
    void injectKeyToPhysical(int code, int value);
    
    // Virtual Mouse for Wayland support
    void setupVirtualMouse(int w, int h);
    void moveMouse(int x, int y, int screenW, int screenH) override;
    void clickMouse(int button, int count) override;
    void destroyVirtualMouse();

    Engine* engine;
    std::vector<int> deviceFds;
    int virtualMouseFd = -1;
    int sWidth = 0, sHeight = 0;
    std::thread inputThread;
    std::atomic<bool> running{false};
    std::atomic<bool> grabbed{false};
    
    // Key state tracking
    bool altPressed = false;
    bool ctrlPressed = false;
    bool shiftPressed = false;
};

#endif // EVDEVINPUT_H
