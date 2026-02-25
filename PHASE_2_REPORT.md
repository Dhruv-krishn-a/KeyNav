# KeyNav Prototype - Phase 2 (Global Input)

## Status
Phase 2 is complete. The application has been refactored into a modular architecture and now supports global input capture.

## Key Features
- **Global Activation**: Press **`Alt + G`** anywhere to activate the overlay.
- **Modal Interaction**: Once active, the application grabs the keyboard.
    - **`1-9`**: Select grid cell (recursively).
    - **`Backspace`**: Undo last selection.
    - **`Escape`**: Cancel/Close overlay.
- **Cursor Movement**: The mouse cursor moves to the center of the selected cell immediately.
- **Architecture**:
    - `Engine`: Core logic and state machine.
    - `X11Platform`: Manages Display and Event Loop.
    - `X11Input`: Handles global hotkeys and keyboard grabbing.
    - `X11Overlay`: Purely passive renderer.

## How to Run
1.  Navigate to the project root: `cd /home/dhruv/KeyNav`
2.  Run the executable:
    ```bash
    ./build/KeyNav
    ```
3.  The terminal will show "KeyNav Platform Running...".
4.  Switch to any other window (e.g., a browser).
5.  Press **`Alt + G`**. The overlay should appear.
6.  Use number keys to navigate. Press `Escape` to close.

## Notes
- If `Alt + G` conflicts with your system, you can change it in `src/platform/linux/X11Input.cpp`.
- The application logs to stdout, so keep the terminal visible to see debug messages ("Engine: Activated", etc.).


## How to build

Fron the project root folder.

```
cmake -S . -B build && cmake --build build -j4
```

## How to run (Currently prototype only)

From the project root folder 

```
./build/KeyNav --evdev
```

