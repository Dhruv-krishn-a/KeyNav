#include <iostream>
#include <cstring>
#include <csignal>
#include <atomic>
#include "core/Engine.h"
#include "platform/linux/X11Platform.h"

X11Platform* g_platform = nullptr;

void signalHandler(int signum) {
    // Only ungrab and set stop flag. Main loop will exit gracefully.
    if (g_platform) {
        g_platform->emergencyExit();
    }
}

int main(int argc, char* argv[]) {
    std::cout << "Starting KeyNav (Phase 2 - Global Input)..." << std::endl;
    
    // Register signal handler
    signal(SIGINT, signalHandler);
    
    bool useEvdev = false;
    for (int i = 1; i < argc; ++i) {
        if (std::strcmp(argv[i], "--evdev") == 0) {
            useEvdev = true;
        }
    }
    
    if (useEvdev) {
        std::cout << "Mode: Evdev (Wayland Compatible - Requires sudo/input group)" << std::endl;
        std::cout << "Activation: Alt + G or RIGHT CTRL" << std::endl;
    } else {
        std::cout << "Mode: X11 (Default - May fail on Wayland)" << std::endl;
        std::cout << "Activation: Alt + G" << std::endl;
    }
    
    Engine engine;
    engine.initialize();
    
    X11Platform platform(&engine, useEvdev);
    g_platform = &platform;
    
    if (!platform.initialize()) {
        std::cerr << "Failed to initialize platform." << std::endl;
        return 1;
    }
    
    // Engine runs the platform loop
    platform.run();
    
    return 0;
}
