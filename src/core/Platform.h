#ifndef PLATFORM_H
#define PLATFORM_H

// Interface for platform-specific implementations
class Platform {
public:
    virtual ~Platform() {}
    virtual bool initialize() = 0;
    virtual void run() = 0;
    virtual void exit() = 0;
    virtual void releaseModifiers() = 0;
    virtual void getScreenSize(int& w, int& h) = 0;
    virtual void moveCursor(int x, int y) = 0;
    virtual void clickMouse(int button, int count) = 0; // button: 1=Left, 2=Middle, 3=Right; count: 1=Single, 2=Double
};

#endif // PLATFORM_H
