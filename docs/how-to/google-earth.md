# Follow the vessel in Google Earth

Google Earth Pro on the desktop and Liquid Galaxy installations accept *ViewSync* UDP
packets that move the camera; the simulator can send them so that the view follows the
vessel. The packet layout is on the [ViewSync reference page](../reference/viewsync.md).

## Enable ViewSync in Google Earth

Google Earth listens for ViewSync packets only when configured to. Close Google Earth and
edit its `drivers.ini`:

| Platform | File |
| --- | --- |
| Windows | `%LOCALAPPDATA%\Google\GoogleEarth\drivers.ini` |
| macOS | `~/Library/Application Support/Google Earth/drivers.ini` |
| Linux | `~/.googleearth/drivers.ini` |

Add or change the `ViewSync` section so that it receives, on the port you will send to:

```ini
[ViewSync]
sendEnabled = false
receiveEnabled = true
port = 42000
hostname =
```

Start Google Earth again. Liquid Galaxy installations already receive on their configured
port; use that port and the address of the master node instead.

## Send packets from the simulator

Add a *UDP* output with the `viewsync` encoding pointed at the machine running Google Earth.
From the command line:

```bash
nmeasim run --udp 192.168.1.20:42000 --encoding viewsync
```

In the desktop application add a *UDP* output in *File → Settings... → Outputs*, set the
address and port, and choose *ViewSync* as its *Encoding*. The *ViewSync* fields set the
camera height above the vessel (500 m by default), its tilt (60 degrees, looking ahead
along the heading) and roll. A profile carries the same settings:

```json
{ "type": "udp", "address": "192.168.1.20", "port": 42000, "encoding": "viewsync",
  "period_ms": 200, "viewsync": { "camera_altitude_m": 800, "tilt_deg": 55 } }
```

Google Earth applies each packet as it arrives, so a short period (100 to 250 ms) gives a
smooth flight. Sail a [track](follow-a-track.md) to fly along a recorded route.

## Check the packets without Google Earth

```bash
nmeasim run --stdout --encoding viewsync --quiet --duration 2 | python3 tools/check_viewsync_stream.py
```
