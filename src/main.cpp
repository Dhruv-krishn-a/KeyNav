#include "core/Logger.h"
#include "core/Config.h"
#include <cstring>
#include "core/Engine.h"
#include "platform/linux/X11Platform.h"

int main(int argc, char* argv[]) {
    Config::loadConfig();
    LOG_INFO("Starting KeyNav (Phase 2 - Global Input)...");
    
    bool useEvdev = false;
    for (int i = 1; i < argc; ++i) {
        if (std::strcmp(argv[i], "--evdev") == 0) {
            useEvdev = true;
        }
    }
    
    if (useEvdev) {
        LOG_INFO("Mode: Evdev (Wayland Compatible - Requires sudo/input group)");
        LOG_INFO("Activation: Alt + G or RIGHT CTRL");
    } else {
        LOG_INFO("Mode: X11 (Default - May fail on Wayland)");
        LOG_INFO("Activation: Alt + G");
    }
    
    Engine engine;
    engine.initialize();
    
    X11Platform platform(&engine, useEvdev);
    
    if (!platform.initialize()) {
        LOG_ERROR("Failed to initialize platform.");
        return 1;
    }
    
    // Engine runs the platform loop
    platform.run();
    
    return 0;
}
