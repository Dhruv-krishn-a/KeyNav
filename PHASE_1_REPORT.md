# KeyNav Prototype - Phase 1

## Status
Phase 1 (Prototype) is complete. The application successfully compiles and implements the core requirements:
- **Transparent Overlay:** Renders a 3x3 grid over the screen.
- **Recursive Selection:** Pressing keys 1-9 refines the selection area.
- **Cursor Movement:** The system cursor moves to the center of the selected cell immediately.
- **Undo:** Pressing `Backspace` undoes the last selection, moving the cursor back to the center of the previous area.
- **Exit:** Pressing `Escape` closes the overlay.

## How to Run
The executable is located in the `build` directory.

1.  Open a terminal.
2.  Navigate to the project root: `cd /home/dhruv/KeyNav`
3.  Run the executable:
    ```bash
    ./build/KeyNav
    ```

## Notes
- The overlay attempts to grab input focus immediately upon launch (`XSetInputFocus`).
- Ensure you have a running X11 session.
- The grid and labels will scale down as you refine your selection.
