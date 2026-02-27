#include "EvdevInput.h"
#include "../../core/Engine.h"
#include <iostream>
#include "../../core/Logger.h"
#include <fcntl.h>
#include <unistd.h>
#include <linux/input.h>
#include <sys/ioctl.h>
#include <sys/stat.h>
#include <glob.h>
#include <poll.h>
#include <algorithm>
#include <cerrno>
#include <cstring>
#include <cmath>
#include <dirent.h>
#include <linux/uinput.h>

#ifndef NLONGS
#define NLONGS(x) (((x) + 8 * sizeof(long) - 1) / (8 * sizeof(long)))
#endif
#ifndef TEST_BIT
#define TEST_BIT(bit, array) ((array)[(bit) / (8 * sizeof(long))] & (1L << ((bit) % (8 * sizeof(long)))))
#endif

EvdevInput::EvdevInput(Engine* e) : engine(e) {}

EvdevInput::~EvdevInput() {
    running = false;
    if (inputThread.joinable()) inputThread.join();
    closeDevices();
    destroyVirtualMouse();
}

bool EvdevInput::initialize(int screenW, int screenH) {
    sWidth = screenW;
    sHeight = screenH;
    openDevices();
    setupVirtualMouse(screenW, screenH);

    if (deviceFds.empty()) {
        LOG_ERROR("EvdevInput: No keyboard devices found. Are you running with sudo?");
        return false;
    }
    
    running = true;
    inputThread = std::thread(&EvdevInput::eventLoop, this);
    return true;
}

void EvdevInput::destroyVirtualMouse() {
    if (virtualMouseFd >= 0) {
        ioctl(virtualMouseFd, UI_DEV_DESTROY);
        close(virtualMouseFd);
        virtualMouseFd = -1;
    }
}

void EvdevInput::setupVirtualMouse(int w, int h) {
    virtualMouseFd = open("/dev/uinput", O_WRONLY | O_NONBLOCK);
    if (virtualMouseFd < 0) {
        fprintf(stderr, "EvdevInput: ERROR: Failed to open /dev/uinput: %s\n", strerror(errno));
        return;
    }

    // Declare event types
    if (ioctl(virtualMouseFd, UI_SET_EVBIT, EV_SYN) < 0) fprintf(stderr, "EvdevInput: ERROR: UI_SET_EVBIT EV_SYN failed: %s\n", strerror(errno));
    if (ioctl(virtualMouseFd, UI_SET_EVBIT, EV_KEY) < 0) fprintf(stderr, "EvdevInput: ERROR: UI_SET_EVBIT EV_KEY failed: %s\n", strerror(errno));
    if (ioctl(virtualMouseFd, UI_SET_EVBIT, EV_ABS) < 0) fprintf(stderr, "EvdevInput: ERROR: UI_SET_EVBIT EV_ABS failed: %s\n", strerror(errno));

    // Declare specific buttons
    if (ioctl(virtualMouseFd, UI_SET_KEYBIT, BTN_LEFT) < 0) fprintf(stderr, "EvdevInput: ERROR: UI_SET_KEYBIT BTN_LEFT failed: %s\n", strerror(errno));
    if (ioctl(virtualMouseFd, UI_SET_KEYBIT, BTN_RIGHT) < 0) fprintf(stderr, "EvdevInput: ERROR: UI_SET_KEYBIT BTN_RIGHT failed: %s\n", strerror(errno));
    if (ioctl(virtualMouseFd, UI_SET_KEYBIT, BTN_MIDDLE) < 0) fprintf(stderr, "EvdevInput: ERROR: UI_SET_KEYBIT BTN_MIDDLE failed: %s\n", strerror(errno));

    // Declare specific absolute axes
    if (ioctl(virtualMouseFd, UI_SET_ABSBIT, ABS_X) < 0) fprintf(stderr, "EvdevInput: ERROR: UI_SET_ABSBIT ABS_X failed: %s\n", strerror(errno));
    if (ioctl(virtualMouseFd, UI_SET_ABSBIT, ABS_Y) < 0) fprintf(stderr, "EvdevInput: ERROR: UI_SET_ABSBIT ABS_Y failed: %s\n", strerror(errno));

    struct uinput_user_dev uidev;
    memset(&uidev, 0, sizeof(uidev));
    snprintf(uidev.name, UINPUT_MAX_NAME_SIZE, "KeyNav Virtual Mouse");
    uidev.id.bustype = BUS_USB;
    uidev.id.vendor  = 0x1234; // Using a more distinct vendor/product ID
    uidev.id.product = 0x5678;
    uidev.id.version = 1;

    // Use inclusive absolute ranges [0..max], so max should be last valid pixel.
    uidev.absmin[ABS_X] = 0;
    uidev.absmax[ABS_X] = std::max(0, w - 1);
    uidev.absmin[ABS_Y] = 0;
    uidev.absmax[ABS_Y] = std::max(0, h - 1);

    if (write(virtualMouseFd, &uidev, sizeof(uidev)) < 0) {
        fprintf(stderr, "EvdevInput: ERROR: Failed to write device info: %s\n", strerror(errno));
        return;
    }
    if (ioctl(virtualMouseFd, UI_DEV_CREATE) < 0) {
        fprintf(stderr, "EvdevInput: ERROR: Failed to create uinput device: %s\n", strerror(errno));
        return;
    }
    
    LOG_INFO("EvdevInput: Virtual Mouse device created successfully.");
}

void EvdevInput::moveMouse(int x, int y, int screenW, int screenH) {
    if (virtualMouseFd < 0) return;

    int srcW = std::max(1, screenW);
    int srcH = std::max(1, screenH);
    int dstW = std::max(1, sWidth);
    int dstH = std::max(1, sHeight);

    x = std::clamp(x, 0, srcW - 1);
    y = std::clamp(y, 0, srcH - 1);

    int mappedX = 0;
    int mappedY = 0;
    if (srcW > 1 && dstW > 1) {
        mappedX = (int)std::lround((double)x * (double)(dstW - 1) / (double)(srcW - 1));
    }
    if (srcH > 1 && dstH > 1) {
        mappedY = (int)std::lround((double)y * (double)(dstH - 1) / (double)(srcH - 1));
    }

    struct input_event ev;
    memset(&ev, 0, sizeof(ev));
    
    ev.type = EV_ABS;
    ev.code = ABS_X;
    ev.value = mappedX;
    write(virtualMouseFd, &ev, sizeof(ev));

    ev.type = EV_ABS;
    ev.code = ABS_Y;
    ev.value = mappedY;
    write(virtualMouseFd, &ev, sizeof(ev));

    ev.type = EV_SYN;
    ev.code = SYN_REPORT;
    ev.value = 0;
    write(virtualMouseFd, &ev, sizeof(ev));
}

void EvdevInput::clickMouse(int button, int count) {
    if (virtualMouseFd < 0) {
        LOG_ERROR("EvdevInput: Cannot click, virtual mouse FD is invalid!");
        return;
    }

    int btnCode = BTN_LEFT;
    if (button == 2) btnCode = BTN_MIDDLE;
    else if (button == 3) btnCode = BTN_RIGHT;

    LOG_INFO("EvdevInput: Virtual Click - Code: ", btnCode, " Count: ", count);

    struct input_event ev;
    memset(&ev, 0, sizeof(ev));

    for (int i = 0; i < count; ++i) {
        // Press
        ev.type = EV_KEY;
        ev.code = btnCode;
        ev.value = 1;
        write(virtualMouseFd, &ev, sizeof(ev));

        ev.type = EV_SYN;
        ev.code = SYN_REPORT;
        ev.value = 0;
        write(virtualMouseFd, &ev, sizeof(ev));

        // Add a small delay between press and release to simulate a real click
        std::this_thread::sleep_for(std::chrono::milliseconds(40));

        // Release
        ev.type = EV_KEY;
        ev.code = btnCode;
        ev.value = 0;
        write(virtualMouseFd, &ev, sizeof(ev));

        ev.type = EV_SYN;
        ev.code = SYN_REPORT;
        ev.value = 0;
        write(virtualMouseFd, &ev, sizeof(ev));

        // If double-clicking, add a small pause between the clicks
        if (count > 1 && i < count - 1) {
            std::this_thread::sleep_for(std::chrono::milliseconds(50));
        }
    }
}

void EvdevInput::openDevices() {
    DIR* dir = opendir("/dev/input");
    if (!dir) return;

    struct dirent* ent;
    std::vector<ino_t> openedInodes;

    while ((ent = readdir(dir)) != NULL) {
        if (strncmp(ent->d_name, "event", 5) != 0) continue;

        char path[PATH_MAX];
        snprintf(path, sizeof(path), "/dev/input/%s", ent->d_name);

        int fd = open(path, O_RDWR | O_NONBLOCK);
        if (fd < 0) continue;

        // Check if device has keys
        unsigned long keyBitmask[NLONGS(KEY_CNT)];
        memset(keyBitmask, 0, sizeof(keyBitmask));
        if (ioctl(fd, EVIOCGBIT(EV_KEY, sizeof(keyBitmask)), keyBitmask) >= 0) {
            // Check for a few standard keys to confirm it's a keyboard
            bool isKeyboard = TEST_BIT(KEY_A, keyBitmask) && 
                              TEST_BIT(KEY_Z, keyBitmask) && 
                              TEST_BIT(KEY_ENTER, keyBitmask);

            if (isKeyboard) {
                struct stat st;
                if (fstat(fd, &st) == 0) {
                    if (std::find(openedInodes.begin(), openedInodes.end(), st.st_ino) != openedInodes.end()) {
                        close(fd);
                        continue;
                    }
                    openedInodes.push_back(st.st_ino);
                }
                LOG_INFO("EvdevInput: Found Keyboard: ", path, " (fd: ", fd, ")");
                deviceFds.push_back(fd);
            } else {
                close(fd);
            }
        } else {
            close(fd);
        }
    }
    closedir(dir);
}

void EvdevInput::closeDevices() {
    ungrabKeyboard(); // Ensure ungrabbed
    for (int fd : deviceFds) {
        close(fd);
    }
    deviceFds.clear();
}

void EvdevInput::grabKeyboard() {
    if (grabbed) return;
    
    std::vector<int> successfullyGrabbed;
    for (int fd : deviceFds) {
        if (ioctl(fd, EVIOCGRAB, 1) == 0) {
            successfullyGrabbed.push_back(fd);
        } else {
            LOG_ERROR("EvdevInput: Failed to grab fd ", fd, " (", strerror(errno), "). Device will be ignored.");
        }
    }
    
    if (!successfullyGrabbed.empty()) {
        grabbed = true;
        // We don't replace deviceFds because we need them for passive monitoring when ungrabbed.
        // But the event loop should ideally only care about grabbed ones when in grabbed mode.
        LOG_INFO("EvdevInput: Keyboard grabbed (Exclusive Mode on ", successfullyGrabbed.size(), " devices)");
    }
}

void EvdevInput::injectKeyToPhysical(int code, int value) {
    struct input_event ev;
    memset(&ev, 0, sizeof(ev));
    ev.type = EV_KEY;
    ev.code = code;
    ev.value = value;
    
    struct input_event syn;
    memset(&syn, 0, sizeof(syn));
    syn.type = EV_SYN;
    syn.code = SYN_REPORT;
    syn.value = 0;

    for (int fd : deviceFds) {
        write(fd, &ev, sizeof(ev));
        write(fd, &syn, sizeof(syn));
    }
}

void EvdevInput::ungrabKeyboard() {
    if (!grabbed) return;

    for (int fd : deviceFds) {
        if (ioctl(fd, EVIOCGRAB, 0) != 0) {
            LOG_ERROR("EvdevInput: Failed to release grab on fd ", fd, ": ", strerror(errno));
        }
    }
    grabbed = false;

    // Emit "up" events to the PHYSICAL devices so the OS resets their state
    int keysToRelease[] = { 
        KEY_LEFTALT, KEY_RIGHTALT, 
        KEY_LEFTCTRL, KEY_RIGHTCTRL, 
        KEY_LEFTMETA, KEY_RIGHTMETA,
        KEY_LEFTSHIFT, KEY_RIGHTSHIFT,
        KEY_G, KEY_ESC 
    };
    
    for (int key : keysToRelease) {
        injectKeyToPhysical(key, 0); // 0 = Release
    }

    // Clear internal state tracking
    altPressed = false;
    ctrlPressed = false;

    LOG_INFO("EvdevInput: Keyboard released. (KeyNav is still running, press Activation Key to return or Ctrl+C to quit)");
}

void EvdevInput::eventLoop() {
    std::vector<struct pollfd> fds(deviceFds.size());
    for (size_t i = 0; i < deviceFds.size(); ++i) {
        fds[i].fd = deviceFds[i];
        fds[i].events = POLLIN;
    }

    struct input_event ev;
    
    while (running) {
        int ret = poll(fds.data(), fds.size(), 100); // 100ms timeout
        if (ret < 0) break; // Error
        if (ret == 0) continue; // Timeout

        for (size_t i = 0; i < fds.size(); ++i) {
            if (fds[i].revents & POLLIN) {
                while (read(fds[i].fd, &ev, sizeof(ev)) > 0) {
                    if (ev.type == EV_KEY) {
                        bool pressed = (ev.value == 1);
                        bool released = (ev.value == 0);
                        int code = ev.code;

                        // Track Modifiers state globally
                        if (code == KEY_LEFTALT || code == KEY_RIGHTALT) {
                            if (pressed) altPressed = true;
                            else if (released) altPressed = false;
                        }
                        if (code == KEY_LEFTCTRL || code == KEY_RIGHTCTRL) {
                            if (pressed) ctrlPressed = true;
                            else if (released) ctrlPressed = false;
                        }
                        if (code == KEY_LEFTSHIFT || code == KEY_RIGHTSHIFT) {
                            if (pressed) shiftPressed = true;
                            else if (released) shiftPressed = false;
                        }

                        // Logic
                        if (grabbed) {
                            // In grabbed mode, we process all keys and they are NOT seen by the OS
                            if (pressed) {
                                LOG_INFO("EvdevInput: Key Pressed: ", code, " (grabbed)");
                                if (code == KEY_ESC) {
                                    engine->onDeactivate();
                                }
                                else if (code == KEY_C && ctrlPressed) {
                                    LOG_INFO("EvdevInput: Ctrl+C detected while grabbed. Exiting...");
                                    engine->onExit();
                                }
                                else if (code == KEY_BACKSPACE) {
                                    engine->onControlKey("backspace");
                                }
                                else if (code == KEY_ENTER) {
                                    engine->onControlKey("enter");
                                }
                                else if (code == KEY_SPACE) {
                                    engine->onControlKey("space");
                                }
                                else if (code == KEY_F) {
                                    engine->onClick(1, 1, false); // Left click, STAY
                                }
                            }
                            
                            char c = '\0';
                            switch(code) {
                                case KEY_A: c = 'a'; break; case KEY_B: c = 'b'; break;
                                case KEY_C: c = 'c'; break; case KEY_D: c = 'd'; break;
                                case KEY_E: c = 'e'; break; case KEY_F: c = 'f'; break;
                                case KEY_G: c = 'g'; break; case KEY_H: c = 'h'; break;
                                case KEY_I: c = 'i'; break; case KEY_J: c = 'j'; break;
                                case KEY_K: c = 'k'; break; case KEY_L: c = 'l'; break;
                                case KEY_M: c = 'm'; break; case KEY_N: c = 'n'; break;
                                case KEY_O: c = 'o'; break; case KEY_P: c = 'p'; break;
                                case KEY_Q: c = 'q'; break; case KEY_R: c = 'r'; break;
                                case KEY_S: c = 's'; break; case KEY_T: c = 't'; break;
                                case KEY_U: c = 'u'; break; case KEY_V: c = 'v'; break;
                                case KEY_W: c = 'w'; break; case KEY_X: c = 'x'; break;
                                case KEY_Y: c = 'y'; break; case KEY_Z: c = 'z'; break;
                                case KEY_1: c = '1'; break; case KEY_2: c = '2'; break;
                                case KEY_3: c = '3'; break; case KEY_4: c = '4'; break;
                                case KEY_5: c = '5'; break; case KEY_6: c = '6'; break;
                                case KEY_7: c = '7'; break; case KEY_8: c = '8'; break;
                                case KEY_9: c = '9'; break; case KEY_0: c = '0'; break;
                            }
                            if (c != '\0') {
                                if (pressed) engine->onChar(c, shiftPressed);
                                else if (released) engine->onKeyRelease(c);
                            }
                        } else {
                            // Passive Monitoring (Activation)
                            // In this state, the OS also sees these keys! 
                            // We only look for the trigger to START grabbing.
                            if (pressed && (code == KEY_RIGHTCTRL || (altPressed && code == KEY_G))) {
                                LOG_INFO("EvdevInput: Activation Key Detected (", (code == KEY_RIGHTCTRL ? "RIGHT CTRL" : "Alt+G"), ")");
                                
                                // Before grabbing, we MUST "release" the activation keys in the OS's mind
                                // otherwise they will be stuck "down" forever because we grab before the "up" event.
                                if (code == KEY_RIGHTCTRL) injectKeyToPhysical(KEY_RIGHTCTRL, 0);
                                if (altPressed && code == KEY_G) {
                                    injectKeyToPhysical(KEY_LEFTALT, 0);
                                    injectKeyToPhysical(KEY_RIGHTALT, 0);
                                    injectKeyToPhysical(KEY_G, 0);
                                }

                                grabKeyboard();
                                engine->onActivate();
                            }
                        }
                    }
                }
            }
        }
    }
}
