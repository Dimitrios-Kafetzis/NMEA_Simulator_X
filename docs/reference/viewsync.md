# ViewSync output

An output with the `viewsync` encoding sends the UDP packets that Google Earth and
[Liquid Galaxy](https://github.com/LiquidGalaxy/liquid-galaxy/wiki/GoogleEarth_ViewSync)
use to synchronise their camera, so that Google Earth follows the simulated vessel. Use a
UDP output pointed at the machine running Google Earth, with ViewSync enabled there in
*Tools → Options → Navigation* or through its `drivers.ini`.

## Packet layout

One packet per output period, a single line of comma-separated values:

```text
7,37.9838000,23.7275000,512.30,45.00,60.00,0.00,63925677296,63925677296,
```

| # | Field | Value sent |
| --- | --- | --- |
| 1 | Counter | Increases by one per packet on this output, starting at 0 when the output opens |
| 2 | Latitude | Vessel latitude, decimal degrees, seven decimals |
| 3 | Longitude | Vessel longitude, decimal degrees |
| 4 | Altitude | Camera altitude, metres: the vessel altitude plus the configured camera height (500 m by default) |
| 5 | Heading | Vessel true heading, degrees |
| 6 | Tilt | Configured camera tilt, degrees from straight down (60 by default) |
| 7 | Roll | Configured camera roll, degrees (0 by default) |
| 8 | Start time | Simulated clock, seconds since 0001-01-01T00:00:00Z |
| 9 | End time | Same as the start time |
| 10 | Planet | Empty for Earth; `sky`, `mars` or `moon` when configured |

The times are the Unix time plus 62 135 596 800 seconds, the offset Google Earth uses.
Packets are sent with a trailing line terminator like every other line; Google Earth ignores
it.
