# Recording and replaying a log

In this tutorial you record a session to a log file, look inside it, replay it with pause,
step and seek, and replay a plain NMEA log captured by another program. It takes about ten
minutes and needs only the desktop application.

## 1. Record a session

Start *NMEA Simulator X* with any profile and press **Start**. Choose *File → Record log...*
(++ctrl+r++) and accept the suggested file name, something like `nmeasim-20260923-101500.log`
in your documents folder. The status bar reports *Recording to ...* and the action stays
ticked. Steer around with the arrow keys for half a minute, change the depth override, then
press **Stop**.

Open the file in a text editor. It starts with a short header and then holds one line per
sentence, each prefixed with the exact UTC time it was sent:

```text
# NMEA Simulator X log 1
# recorded: 2026-09-23T10:15:00.000Z
# profile: Default
2026-09-23T10:15:00.104Z $GPRMC,101500.10,A,3759.0281,N,02343.6502,E,6.5,45.0,230926,4.6,E,A*0D
```

Untick *File → Record log...* to close the recording. Leaving it ticked would continue the
same file at the next *Start*.

## 2. Replay it

Choose *File → Open log for replay...* (++ctrl+l++) and pick the file you just recorded. The
profile switches to replay mode, the status bar reads *(replay ...)* and the seek slider shows
the length of the recording. Press **Start**: the sentences are sent again, byte for byte, on
the cadence they were recorded, and the dashboard and the map follow them because every
replayed sentence is decoded into the vessel state. The moves you made with the arrow keys
happen again at the same moments.

## 3. Pause, step and seek

Press **Pause** (++f6++). The stream stops where it is; the outputs stay open. Press **Step**
(++f7++) a few times: each press sends exactly one recorded sentence and the console shows
it. Drag the seek slider back to the start: the instruments show the values the log had at
that point, without any sentence being sent, and untick *Pause* to continue from there.

When the log runs out the run ends by itself and the status bar reports *End of the track or
log reached*. To loop instead, open *File → Settings...* and tick *Start again at the end* in
the *Log replay* group.

## 4. Replay a log from another program

Any plain NMEA log works too: a file saved by a plotter, a serial logger or a `nc` session,
with or without timestamps. Open it with *File → Open log for replay...* as before. Lines
without a timestamp are timed from the UTC time fields of the RMC, GGA, GLL and ZDA
sentences they contain, so a log with one fix per second replays at one fix per second. A
log without any time information, for example a depth-only capture, plays at the *Interval
without times* set in the settings dialog, 100 ms by default.

The [log file reference](../reference/log-format.md) lists every line shape the reader
accepts and which sentences update the dashboard.

## 5. Do the same headless

Both halves work from the command line:

```bash
nmeasim run --record monday.log --tcp-server 10110 --duration 600
nmeasim run --replay monday.log --tcp-server 10110
```

## Where next

- [Log files](../reference/log-format.md): the format written and the formats read.
- [Run the simulator headless](../how-to/run-headless.md): recording and replay from scripts.
- [Following a GPX track](following-a-gpx-track.md): drive the vessel from a recorded trip
  instead of a recorded stream.
