#include "Config.h"
#include "Logger.h"
#include <fstream>
#include <sstream>
#include <string>
#include <cstdlib>
#include <algorithm>

namespace Config {
    int LEVEL0_GRID_ROWS = 11;
    int LEVEL0_GRID_COLS = 11;
    int LEVEL1_GRID_ROWS = 6;
    int LEVEL1_GRID_COLS = 6;
    int MAX_RECURSION_DEPTH = 1;

    double OVERLAY_BOUNDS_EPSILON = 3.0;
    std::chrono::milliseconds OVERLAY_SETTLE_POLL_INTERVAL(8);
    int OVERLAY_SETTLE_MAX_RETRIES = 12;
    std::chrono::milliseconds POST_UNGRAB_DELAY(50);
    std::chrono::milliseconds CLICK_PRESS_RELEASE_DELAY(40);
    std::chrono::milliseconds DOUBLE_CLICK_DELAY(50);

    double OVERLAY_FILL_ALPHA = 0.30;
    
    std::vector<Rgba> PALETTE = {
        {0.91, 0.30, 0.27, 0.0}, // coral
        {0.95, 0.56, 0.20, 0.0}, // amber
        {0.95, 0.78, 0.27, 0.0}, // gold
        {0.36, 0.76, 0.44, 0.0}, // green
        {0.22, 0.72, 0.73, 0.0}, // cyan
        {0.25, 0.48, 0.86, 0.0}, // blue
        {0.48, 0.42, 0.87, 0.0}, // indigo
        {0.79, 0.37, 0.81, 0.0}, // violet
        {0.88, 0.36, 0.53, 0.0}  // rose
    };

    void loadConfig() {
        const char* home = std::getenv("HOME");
        if (!home) return;

        std::string configPath = std::string(home) + "/.config/keynav/config.ini";
        std::ifstream file(configPath);
        if (!file.is_open()) {
            LOG_INFO("Config file not found at ", configPath, ", using defaults.");
            return;
        }

        LOG_INFO("Loading config from ", configPath);
        std::string line;
        while (std::getline(file, line)) {
            // Remove comments
            size_t commentPos = line.find('#');
            if (commentPos != std::string::npos) {
                line = line.substr(0, commentPos);
            }

                        // Trim whitespace
                        line.erase(0, line.find_first_not_of(" \t\r\n"));
                        line.erase(line.find_last_not_of(" \t\r\n") + 1);
            if (line.empty() || line[0] == '[') continue;

            size_t eqPos = line.find('=');
            if (eqPos == std::string::npos) continue;

            std::string key = line.substr(0, eqPos);
            std::string val = line.substr(eqPos + 1);

            key.erase(key.find_last_not_of(" 	") + 1);
            val.erase(0, val.find_first_not_of(" 	"));

            try {
                if (key == "level0_rows") LEVEL0_GRID_ROWS = std::stoi(val);
                else if (key == "level0_cols") LEVEL0_GRID_COLS = std::stoi(val);
                else if (key == "level1_rows") LEVEL1_GRID_ROWS = std::stoi(val);
                else if (key == "level1_cols") LEVEL1_GRID_COLS = std::stoi(val);
                else if (key == "max_recursion") MAX_RECURSION_DEPTH = std::stoi(val);
                else if (key == "overlay_alpha") OVERLAY_FILL_ALPHA = std::stod(val);
            } catch (const std::exception& e) {
                LOG_ERROR("Failed to parse config key '", key, "': ", e.what());
            }
        }
    }
}