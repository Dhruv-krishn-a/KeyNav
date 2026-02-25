#ifndef X11INPUT_H
#define X11INPUT_H

#include "../../core/Input.h"
#include <X11/Xlib.h>

class Engine; // Forward decl

class X11Input : public Input {
public:
    X11Input(Display* d, Engine* e);
    ~X11Input();

    bool initialize(int screenW = 0, int screenH = 0) override;
    void grabKeyboard() override;
    void ungrabKeyboard() override;

    // Handle X11 KeyPress/KeyRelease
    void handleEvent(XEvent& event);

private:
    void grabActivationKey();
    
    Display* display;
    Engine* engine;
    bool keyboardGrabbed = false;
    
    // Configurable Activation Key (Phase 2: Alt+G)
    unsigned int activationModifiers;
    KeySym activationKeySym;
    KeyCode activationKeyCode;
};

#endif // X11INPUT_H
