# Desktop application

*NMEA Simulator X* is the Qt Widgets host of the simulation engine. It runs the same
`SimulationRunner` as the [command-line tool](cli.md) and reads and writes the same
[profile files](profile.md).

## Command line

```
NMEASimulatorX [profile.json]
```

When a profile path is given and the file exists it is loaded at start-up. Otherwise the last
profile used is reopened, and if there is none the built-in default profile is used.

## Window layout

| Area | Contents | Can be hidden |
| --- | --- | --- |
| Dashboard (central) | Instrument tiles with override controls | No |
| Console (dock, bottom) | Sentences as sent, with pause, filter and clear | Yes, *View* menu |
| Outputs (dock, right) | One row per configured output: description, state, clients, sentences, bytes, last error | Yes, *View* menu |
| Status bar | Run state and profile name on the left, sentence counter on the right; errors appear here for ten seconds | No |

Docks can be moved to any edge, stacked, floated or closed. Geometry and dock layout are
saved on exit and restored at the next start.

## Actions and shortcuts

| Menu | Action | Shortcut | Effect |
| --- | --- | --- | --- |
| File | New profile | ++ctrl+n++ | Replaces the current profile with the default one |
| File | Open profile... | ++ctrl+o++ | Loads a JSON profile; the simulation restarts if it was running |
| File | Save profile | ++ctrl+s++ | Writes the current profile to its file, asking for a name the first time |
| File | Save profile as... | ++ctrl+shift+s++ | Writes the current profile to a new file |
| File | Quit | ++ctrl+q++ | Stops the simulation and closes the window |
| Simulation | Start / Stop | ++f5++ | Opens every enabled output and starts ticking, or closes everything |
| Simulation | Pause | ++f6++ | Freezes the simulated clock and the vessel; outputs stay open |
| Simulation | Steering mode | | Arrow keys move the rudder instead of the heading |
| Simulation | Start automatically on launch | | Starts the simulation as soon as the window opens |
| View | Console, Outputs | | Shows or hides the panel |
| Help | About | | Version and project link |

On macOS the ++ctrl++ shortcuts use ++cmd++.

## Keyboard control of the vessel

The main window must have focus; click on the dashboard if the arrow keys do not respond.

| Key | Parameter | Step | With ++shift++ |
| --- | --- | --- | --- |
| ++up++ / ++down++ | Speed over ground | 0.1 kn | 1 kn |
| ++left++ / ++right++ | Heading (steering mode off) | 1° | 10° |
| ++left++ / ++right++ | Rudder angle (steering mode on) | 1° | 10° |

A nudge sets an override on the parameter, which pins it at the new value until the override
is cleared from the tile.

## Dashboard tiles

| Tile | Shows | Override |
| --- | --- | --- |
| Position | Latitude and longitude in degrees and decimal minutes | No |
| Time (UTC) | Simulated clock | No |
| GNSS | Fix state, satellites in use, HDOP | *Fix* check box, *Satellites* spin box (0 to 12) |
| Heading | True heading | Yes, 0 to 359.9°; disabled in steering mode |
| Course over ground | Derived from heading and drift | No |
| Rate of turn | Degrees per minute | No |
| Speed over ground | kn | Yes |
| Speed through water | kn | Yes |
| Rudder | Rudder angle | Yes, -45 to 45°; enabled only in steering mode |
| Depth | Below transducer, m | Yes |
| Water temperature | °C | Yes |
| Altitude | m | Yes |
| True wind direction | Degrees true | Yes |
| True wind speed | kn | Yes |
| Apparent wind | Angle and speed derived from true wind and vessel motion | No |

An active override stops the random drift of that parameter. Clearing it lets the value drift
again from where it is.

## Console

The console buffers sentences and repaints a few times per second so that high output rates
do not slow the interface. It keeps the last 2000 lines. The filter matches the registry id
(for example `RMC`) or any part of the sentence text (for example `$HC`), case-insensitively,
and applies to new sentences only. *Pause* stops new sentences from being added without
affecting the simulation.

## Outputs

The table refreshes twice a second and whenever an output changes state. The state column
uses the names listed in the [transport reference](transports.md). An output that fails to
open is reported in the status bar and in the *Last error* column while the run continues on
the other outputs.

## Preferences

Application preferences are separate from profiles and are stored with `QSettings` in the
platform's native location:

| Platform | Location |
| --- | --- |
| Windows | Registry key `HKEY_CURRENT_USER\Software\NMEASimulatorX\NMEASimulatorX` |
| macOS | `~/Library/Preferences/io.github.dimitrios-kafetzis.NMEASimulatorX.plist` |
| Linux | `~/.config/NMEASimulatorX/NMEASimulatorX.conf` |

| Key | Meaning |
| --- | --- |
| `profile/last_path` | Profile reopened at the next start |
| `window/geometry`, `window/state` | Window size, position and dock layout |
| `simulation/autostart` | Whether the simulation starts on launch |

The default folder offered by the profile dialogs is the `profiles` sub-folder of the
application's configuration directory.
