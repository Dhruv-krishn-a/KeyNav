# Current Status

## Overview
- _KeyNav_ is a keyboard-first pointer system that renders a configurable grid (3×3 default) over the desktop, lets the user recursively refine a region via number keys, and then confirms the cursor move/click through a dedicated key.
- The architecture keeps input capture, command/state engine, rendering, and cursor control modular so the same engine can support multiple platforms (X11 and Wayland already present, with evdev input and uinput-driven virtual mouse injected for clicks).

## Progress
| Phase | Scope | Status |
| --- | --- | --- |
| Phase 0 (Discovery/Requirements) | Defined goals: Linux X11 prototype → Wayland, config + logging separation, activation/undo/cancel semantics, grid variants, non-functional budgets. | Done (implicit from repo structure and documentation) |
| Phase 1 (Prototype) | Overlay + subdivision engine + cursor movement through platform abstraction. | Done: `Engine` tracks rectangles, `Overlay` renders grid, `Platform`/`Input` stubs move cursor. |
| Phase 2 (Global Input & Activation) | Global keyboard hooks (evdev), activation key (Alt+G / Right Ctrl), keygrab, input normalization to engine with logs/deactivation. | Done: `EvdevInput` grabs keyboards, feeds `Engine` commands, `Platform` builds virtual mouse. |
| Phase 3 (Interaction/State Machine) | Recursive selection, undo (Backspace) and confirm (Enter/Space) with deterministic state machine and click integration. | Mostly done: overlay refines cells via Engine, undo/backspace works, confirm key triggers `onClick` which hides/re-shows overlay. Need to polish confirm/exit behavior. |

## Immediate Improvements
1. **Confirm flow cleanup** – ensure Enter/Space both close cleanly when desired, re-show overlay for multi-click sessions, and never leave keyboard grabbed when not active. Confirm should optionally allow “click without exit” via config flag or modifier.
2. **Feedback & UX polish** – highlight current active cell, display depth (level) or cursor preview dot so users know where the system thinks the next cursor center is; consider brief animation when hover/click occurs.
3. **Input edge cases** – handle repeated keypresses/long-press, make sure IME/composed input isn’t hijacked, and add timeout/cancel if mouse moves outside overlay.
4. **Config & remapping** – move keybindings, grid size, activation key, click behavior into a config manager (JSON/TOML) so Phase 6 is ready to hook in.
5. **Wayland readiness** – finalize Wayland overlay bounds + input backend so that the global grab respects compositor policies and the virtual mouse translates coordinates per monitor.

## Remaining Work by Phase
### Phase 3 (Interaction & State Machine)
- Document the deterministic state transitions (Idle → OverlayVisible → Selecting → Confirmed/Cancelled).
- Add explicit confirm key flag and ensure `onClick` only hides overlay when configured to exit.
- Add undo/cancel visual cue and ensure backspace/escape restore the previous rectangle.
- Improve overlay/state synchronization (show highlight of active cell, draw split progression).

### Phase 4 (Precision/Smoothing)
- Decide between instant jump vs snapping/smooth interpolation; implement optional easing or preview indicator.
- Add multi-monitor coordinate mapping inside `Engine`/`Platform` for DPI and scaling awareness.
- Build optional “click on confirm” flag and allow configurable offsets (top-left, center, etc.).

### Phase 5 (Robustness, Permissions & Wayland)
- Harden Wayland overlay (GTK layer-shell) to update bounds on main thread and keep pointer focus outside overlay.
- Document required permissions/privileges and how to grant them.
- Add graceful fallback if `uinput` or keyboard grab fails (notify user, run in non-grabbed mode).

### Phase 6 (Accessibility & Config)
- Implement config manager with live reload (JSON/TOML) for keybindings, grid size, animations.
- Add help overlay, high-contrast theme, audio cues for actions, and large-label mode.
- Introduce accessible UI options (e.g., sticky confirm, audible feedback, hold-to-activate).

### Phase 7 (Testing, Performance & Telemetry)
- Unit test grid subdivision, rect math, and state transitions.
- Add integration tests for input → overlay → cursor loop using mocks or recorded events.
- Measure latency/CPU, document in report, and add optional telemetry/logging toggles.

### Phase 8 (Packaging & Release)
- Assemble packaging scripts: AppImage/Flatpak for Linux, guidance for Wayland.
- Document install steps and release procedures (signing, changelog, release notes).

### Phase 9 (Maintenance & Community)
- Write CONTRIBUTING.md, issues triage playbook, and roadmap updates.
- Collect user feedback, schedule accessibility testing, and iterate.

## Short-Term Focus
- Stabilize confirm/exit so Enter/Space both produce a click and exit cleanly while still allowing “stay active” sessions via config.
- Improve overlay visibility (highlight active cells, animation) so the new user sees deterministic flow.
- Complete Wayland coordinate math and ensure virtual mouse clicks map per monitor.

## Next Steps
1. Rebuild after confirming `onClick` behavior works end-to-end and add tests for the confirm path.  
2. Audit config hooks (grid size, activation keys) and wire them to `Current.md` roadmap as soon as Phase 6 is ready.  
3. Write `Requirements.md`/`Architecture.md` if not yet done so future contributors can see the high-level direction noted above.
