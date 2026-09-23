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
| Map (dock, left) | Vessel, heading, course line and track on a slippy map | Yes, *View* menu |
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
| File | Open track... | ++ctrl+t++ | Switches the current profile to track mode with a GPX or KML file; the simulation restarts if it was running |
| File | Open log for replay... | ++ctrl+l++ | Switches the current profile to replay mode with a recorded or plain NMEA log |
| File | Record log... | ++ctrl+r++ | Starts recording every sentence to a log file, or stops the recording when unticked |
| File | Settings... | ++ctrl+comma++ | Opens the [settings dialog](#settings-dialog) for the current profile |
| File | Quit | ++ctrl+q++ | Stops the simulation and closes the window |
| Simulation | Start / Stop | ++f5++ | Opens every enabled output and starts ticking, or closes everything |
| Simulation | Pause | ++f6++ | Freezes the simulated clock and the vessel; outputs stay open |
| Simulation | Step | ++f7++ | Pauses and advances by one tick, or by one recorded sentence during a replay; starts the run paused when it is stopped |
| Simulation | Steering mode | | Arrow keys move the rudder instead of the heading; available in delta mode only |
| Simulation | Start automatically on launch | | Starts the simulation as soon as the window opens |
| View | Map, Console, Outputs | | Shows or hides the panel |
| View | Follow vessel on the map | ++home++ | Keeps the map centred on the vessel; dragging the map switches it off |
| View | Download map tiles | | Fetches missing tiles from the tile server; off uses the disk cache only |
| View | Clear map tile cache | | Deletes every cached tile from disk and memory |
| Help | About | | Version and project link |

On macOS the ++ctrl++ shortcuts use ++cmd++.

## Transport controls

The toolbar ends with a seek slider and a position label. Both are active while the profile
follows a track or replays a log, whose length is known, and disabled for the delta
simulation, which is endless.

| Control | Effect |
| --- | --- |
| *Pause* | Stops the clock; the outputs stay open |
| *Step* | One tick of the simulation (the profile's simulation step), or exactly the next recorded sentence during a replay |
| Slider | Drag or click to move to that point of the track or log. The vessel state and the map change at once; during a replay the sentences before the new position are applied to the state without being sent |
| Label | Elapsed and total time as `mm:ss` or `h:mm:ss` |

A track or log that is not set to loop ends the run by itself: the last sentence is sent,
the status bar reports *End of the track or log reached* and the outputs close.

## Modes

The [profile](profile.md#simulation) decides what drives the vessel:

| Mode | Vessel | Dashboard | Map |
| --- | --- | --- | --- |
| Delta simulation | Seed values that drift, with overrides and steering | Overrides active | Vessel and sailed track |
| Follow a track | A [GPX or KML file](track-files.md) | Overrides disabled | The loaded track is drawn in green under the sailed track |
| Replay a log | The sentences of a [log file](log-format.md), decoded into the state | Overrides disabled | Vessel and sailed track from the decoded positions |

Use *File → Open track...* or *Open log for replay...* to switch the current profile to
those modes with one file, or the *Mode* group of the settings dialog to set every option.
*File → New profile* or the settings dialog return to the delta simulation.

## Recording

*File → Record log...* asks for a file and records every emitted sentence, whatever the
profile outputs and their filters, in the [log format](log-format.md). The file is truncated
when the recording starts and continued across *Stop* and *Start* until the action is
unticked. The status bar and the action's tooltip show the file being written. To record only
some sentences, add a *Log* output with a filter in the settings dialog instead.

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
again from where it is. In track and replay mode every override control is disabled, because
the file drives the vessel.

## Map

The map shows the vessel as a yellow hull pointing along its true heading, a dashed blue
line along its course over ground, and a red track of the positions sailed since the profile
was applied (at most 5000 points). In track mode the loaded track or route is drawn in green
underneath, with a marker on every point when it has 500 points or fewer. The zoom level and the words *offline* and *free view*
appear in the top-left corner; the OpenStreetMap attribution is always drawn.

| Input | Effect |
| --- | --- |
| Drag with the left button | Pans the map and switches *Follow vessel* off |
| Mouse wheel | Zooms in or out around the pointer |
| ++plus++ / ++minus++ | Zooms in or out around the centre |
| ++home++ | Switches *Follow vessel* on and recentres |
| Double-click, or ++ctrl++ and click | Moves the vessel to that point |

Moving the vessel changes the running simulation immediately and also the start position of
the current profile, so saving the profile keeps the new place.

### Tiles

Tiles follow the slippy map scheme and come from a tile server given as a URL template
with `{z}`, `{x}` and `{y}` placeholders. The default is the OpenStreetMap server,
`https://tile.openstreetmap.org/{z}/{x}/{y}.png`, used under its
[tile usage policy](https://operations.osmfoundation.org/policies/tiles/): requests carry
a `User-Agent` naming this application, at most four downloads run at a time and every tile
is cached. Set another server through the `map/tile_url` preference.

Every downloaded tile is written to the tile cache directory below, so once an area has
been viewed it stays available without a network connection. When a tile is missing the map
shows the matching part of the nearest cached lower zoom level, or a grey square when there
is none. Untick *View → Download map tiles* to stop all network access; the map then uses
the cache only and shows *offline*.

| Platform | Tile cache directory |
| --- | --- |
| Windows | `%LOCALAPPDATA%\NMEASimulatorX\NMEASimulatorX\cache\tiles` |
| macOS | `~/Library/Caches/NMEASimulatorX/NMEASimulatorX/tiles` |
| Linux | `~/.cache/NMEASimulatorX/NMEASimulatorX/tiles` |

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

## Settings dialog

*File → Settings...* edits a copy of the current profile in three tabs. *OK* applies the
result as the current profile, restarting the simulation if it was running; *Cancel* discards
every change. The profile on disk is not touched until you save it.

### Simulation tab

| Group | Fields |
| --- | --- |
| Mode | *Vessel driven by*: delta simulation, follow a track or replay a log |
| Track | File (with *Browse...*), speed without timestamps, follow the track's own timestamps, start again at the end; enabled in track mode |
| Log replay | File (with *Browse...*), interval without times, start again at the end; enabled in replay mode |
| Profile and clock | Name, simulation step (10 to 10000 ms), fixed start time in UTC or the wall clock, random seed |
| Initial vessel values | Latitude, longitude, altitude, heading, speed over ground, magnetic variation and deviation, depth, transducer offset, water temperature, true wind direction and speed |
| GNSS receiver | Fix, fix quality, satellites in use and in view, HDOP, PDOP, VDOP, geoid separation |
| Drift around the initial values | Amplitude and step per second for heading, speed, depth, water temperature, wind direction and wind speed; an amplitude of 0 freezes the value; enabled in delta mode |
| Steering | Turn rate per degree of rudder, maximum rudder angle |

The fields map one to one onto the `simulation` object of the
[profile file](profile.md#simulation).

### Sentences tab

One row per sentence in the registry with its enabled flag, id, description, group, talker
and period in milliseconds. An empty talker uses the registry default shown as placeholder.
*Enable all*, *Disable all* and *Reset to defaults* act on every row. *Position decimals*
sets the fractional minute digits of latitude and longitude.

Only rows that differ from the registry defaults are written to the profile, so a saved
profile stays small and follows registry changes in later versions.

### Outputs tab

The list on the left holds the outputs in the order they are opened. *Add* offers every
transport type, including *Log (timestamped)* for a filtered recording; *Remove* deletes the selected output. The editor on the right shows the
common fields, *Enabled* and the comma-separated *Sentence filter*, above the fields of the
selected type as listed in the [transport reference](transports.md). Serial ports found on
the machine are offered in the port list, and any other device path can be typed.

*OK* is refused, with the reason shown under the tabs, while the track or replay mode has no
file, a serial output has no port, a file or log output has no path or a TCP client has no
host.

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
| `map/online` | Whether missing tiles are downloaded (default `true`) |
| `map/tile_url` | Tile URL template; empty uses the OpenStreetMap server |
| `map/zoom` | Last map zoom level |

The default folder offered by the profile dialogs is the `profiles` sub-folder of the
application's configuration directory.
