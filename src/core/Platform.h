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
};

#endif // PLATFORM_H
