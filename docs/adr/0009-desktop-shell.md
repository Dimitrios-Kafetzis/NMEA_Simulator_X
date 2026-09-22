# 0009 Desktop shell: one main window with dock panels, tested offscreen

- Status: accepted
- Date: 2026-09-23

## Context and problem statement

Milestone M2 adds the desktop application on top of the headless engine from M1. The
reference application shows a dashboard, a map, a console and a settings page as separate
views. How should the Qt Widgets application be structured so that it stays small, works the
same on three platforms, and can be tested in CI where no display exists?

## Decision drivers

- Everything an operator needs during a run (instruments, map, sentence stream, output
  status) should be visible at once on a laptop screen.
- The window layout must be adjustable and remembered between sessions.
- Widgets must be exercised by automated tests on every platform, as ADR 0008 requires,
  without a window system.
- The application must not contain simulation logic; it is a host of `nmeasim::io`.

## Considered options

1. A stacked layout with a page per view and a navigation bar, as in the reference.
2. One `QMainWindow` with the dashboard as central widget and dockable panels for the map,
   the console and the outputs, with settings in a modal dialog.
3. Several top-level windows.

## Decision outcome

Option 2. The dashboard is the central widget; the console, outputs and map are
`QDockWidget` panels the operator can move, float or hide, and the layout is saved with
`QMainWindow::saveState` into `QSettings`. Simulation, sentence and output settings edit the
profile in a dialog, because they change the running configuration and deserve an explicit
apply step. Toolbar and menu actions are the only entry points to start, pause, stop and
steer, so keyboard shortcuts, menus and buttons cannot drift apart.

The window and its widgets are compiled into a static library, `nmeasim_app_lib`, and the
executable is a `main.cpp` that only sets application metadata. The test suite links the
library and drives the real `MainWindow` with Qt's `offscreen` platform plugin, so the same
tests run on Linux, Windows and macOS runners without a display. Every operator-visible error
is also raised as a signal and shown in the status bar; dialogs appear only when the window is
visible, so tests never block on a modal box.

### Consequences

- Application code lives in `src/app` and follows the same warning, sanitizer and formatting
  rules as the libraries.
- Tests can assert on the window, the console line count, the outputs table and the effect of
  keyboard events on the running simulation.
- Window geometry and dock layout are stored in the platform's native settings store; the
  simulation profile is never stored there.
- A panel that later needs its own window (for example a detached map on a second monitor)
  can float its dock without new code.

## More information

- [Architecture](../explanation/architecture.md)
- [Desktop application reference](../reference/desktop-app.md)
