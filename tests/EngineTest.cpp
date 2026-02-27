#include <gtest/gtest.h>
#include "../src/core/Engine.h"
#include "../src/core/Platform.h"
#include "../src/core/Input.h"
#include "../src/core/Overlay.h"
#include "../src/core/Config.h"

// --- Mocks ---
class MockPlatform : public Platform {
public:
    int cursorX = 0, cursorY = 0;
    int clicks = 0;

    bool initialize() override { return true; }
    void run() override {}
    void exit() override {}
    void releaseModifiers() override {}
    void getScreenSize(int& w, int& h) override { w = 1920; h = 1080; }
    void moveCursor(int x, int y) override { cursorX = x; cursorY = y; }
    void clickMouse(int button, int count) override { clicks += count; }
};

class MockOverlay : public Overlay {
public:
    int updates = 0;
    bool isVisible = false;

    void show() override { isVisible = true; }
    void hide() override { isVisible = false; }
    void updateGrid(int rows, int cols, double x, double y, double w, double h, bool showPoint) override { updates++; }
    bool getBounds(Rect& out) override { out = {0, 0, 1920, 1080}; return true; }
};

class MockInput : public Input {
public:
    bool grabbed = false;

    bool initialize(int w, int h) override { return true; }
    void grabKeyboard() override { grabbed = true; }
    void ungrabKeyboard() override { grabbed = false; }
    void moveMouse(int x, int y, int sw, int sh) override {}
    void clickMouse(int button, int count) override {}
};

// --- Test Fixture ---
class EngineTest : public ::testing::Test {
protected:
    Engine engine;
    MockPlatform platform;
    MockOverlay overlay;
    MockInput input;

    void SetUp() override {
        Config::LEVEL0_GRID_ROWS = 10;
        Config::LEVEL0_GRID_COLS = 10;
        Config::LEVEL1_GRID_ROWS = 5;
        Config::LEVEL1_GRID_COLS = 5;
        
        engine.setPlatform(&platform);
        engine.setOverlay(&overlay);
        engine.setInput(&input);
        engine.initialize();
    }
};

// --- Tests ---
TEST_F(EngineTest, ActivationState) {
    engine.onActivate();
    EXPECT_TRUE(input.grabbed);
    EXPECT_TRUE(overlay.isVisible);
    EXPECT_GT(overlay.updates, 0);
}

TEST_F(EngineTest, DeactivationState) {
    engine.onActivate();
    engine.onDeactivate();
    EXPECT_FALSE(input.grabbed);
    EXPECT_FALSE(overlay.isVisible);
}

TEST_F(EngineTest, UndoFromFirstChar) {
    engine.onActivate();
    // Press first char 'a' -> selects row 0
    engine.onChar('a', false);
    
    engine.onUndo(); // Should revert to waiting for first char
    
    // Press 'b' (row 1) and 'b' (col 1) -> should work normally if undo succeeded
    engine.onChar('b', false);
    engine.onChar('b', false);
    
    // Grid 10x10 -> cell size 192, 108. 'b','b' is col 1, row 1.
    // Center is 192 + 96 = 288, 108 + 54 = 162.
    EXPECT_EQ(platform.cursorX, 192 + 96);
    EXPECT_EQ(platform.cursorY, 108 + 54);
}

TEST_F(EngineTest, ClickAction) {
    engine.onActivate();
    engine.onClick(1, 1, true); // Left click, single, deactivate
    
    EXPECT_EQ(platform.clicks, 1);
    EXPECT_FALSE(input.grabbed); // Because deactivate = true
}

int main(int argc, char **argv) {
    ::testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}
