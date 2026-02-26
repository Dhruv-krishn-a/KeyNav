#ifndef INPUT_H
#define INPUT_H

// Interface for input manager
class Input {
public:
    virtual ~Input() {}
    virtual bool initialize(int screenW = 0, int screenH = 0) = 0;
    virtual void grabKeyboard() = 0; // Modal
    virtual void ungrabKeyboard() = 0;
    
    // Optional virtual mouse support (for Wayland/Evdev)
    virtual void moveMouse(int x, int y, int screenW, int screenH) {}
    virtual void clickMouse(int button, int count) {}
    // ... other methods to send keycodes to engine
};

#endif // INPUT_H
