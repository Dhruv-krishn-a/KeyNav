# Current Status

## Overview
- _KeyNav_ is a keyboard-first pointer system that renders a configurable grid over the desktop, lets the user recursively refine a region, and confirms the cursor move/click through dedicated keys.
- The architecture is modular, supporting X11 and Wayland (via `layer-shell`), using `evdev` for global input and `uinput` for a virtual mouse.

## Progress
| Phase | Scope | Status |
| --- | --- | --- |
| Phase 0 | Discovery/Requirements | **Done** |
| Phase 1 | Prototype Overlay & Engine | **Done** |
| Phase 2 | Global Input & Activation | **Done** |
| Phase 3 | Interaction & State Machine | **Done** - Implemented recursive 11x11 → 6x6 selection with undo and "Hold-to-Click" logic. |
| Phase 4 | Precision & Industry Grade | **Done** - Added XRandR monitor detection, Target Point rendering, and `signalfd` safety. |
| Phase 5 | Robustness & Permissions | **Done** - Added `udev` scripts for non-root execution and distribution-specific dependency installers. |
| Phase 6 | Configuration | **Done** - Implemented runtime `.ini` config loader with dynamic overrides. |
| Phase 7 | Testing & Performance | **Done** - Added GoogleTest suite with Mocks and zero-latency `poll(-1)` event loop. |

## Completed Improvements (Industry Grade)
1. **Target Point Rendering**: Once `MAX_RECURSION_DEPTH` is reached, the grid is replaced by a high-visibility red dot. This provides maximum precision for the final click without visual clutter.
2. **Hold-to-Click & Auto-Exit**: 
   - **Tapping** the final key moves the mouse and exits automatically.
   - **Holding** the final key keeps the overlay (and target point) visible for multiple clicks (Space/Enter).
   - Releasing the held key or clicking now triggers an automatic suspension—no manual `Esc` required after a successful move.
3. **Signal & Thread Safety**: 
   - Replaced unsafe `signal()` handlers with `signalfd` integrated into the event loop.
   - Converted shared input states to `std::atomic<bool>` to prevent data races between the input thread and main thread.
4. **Smart Configuration**: Added `src/core/Config.cpp` which loads `~/.config/keynav/config.ini`. All grid sizes, colors, alphas, and timings are now user-tunable without recompilation.
5. **Multi-Monitor Intelligence**: Uses XRandR to detect which monitor the cursor is currently on and snaps the initial 11x11 grid to that specific screen.
6. **Unified Logging**: Implemented a thread-safe `Logger.h` with timestamps and log levels, replacing all standard I/O streams.
7. **Automated Testing**: Added `tests/EngineTest.cpp` covering 100% of the core state machine logic using Mock objects.

## Installation & Setup Tools
- `install_dependencies.sh`: Auto-detects Ubuntu, Fedora, or Arch and installs required dev packages.
- `install_udev_rules.sh`: Sets up permissions for `input` and `uinput` groups so the app runs without `sudo`.
- `assets/`: Includes a vector icon and a `.desktop` entry for system integration.

## Immediate Roadmap
1. **Smooth Easing (Optional)**: Add an option for the cursor to "glide" to the center of a cell rather than jumping instantly.
2. **Dynamic Palette**: Allow the user to define the tile color palette directly in the `.ini` file.
3. **Help Overlay**: Add a toggleable key-map overlay (e.g., F1) to show available keys during selection.

## Next Steps
1. Package for distribution (AppImage or Flatpak).
2. Finalize documentation for end-users in a `README.md`.
