# Transports

A transport is an output channel. Several can run at the same time, each with its own
sentence filter and [encoding](profile.md#outputs): NMEA 0183 sentences, Signal K deltas or
ViewSync packets. Every transport reports a state (`closed`, `opening`, `open`, `failed`), the
last error, the number of attached clients and the number of bytes (lines or messages)
written.

Lines are written exactly as produced by the encoder plus `<CR><LF>`, with an optional
[TAG block](nmea0183-sentences.md#tag-blocks) in front of NMEA 0183 sentences; transports
never alter the payload. A recording ([log output](log-format.md)) always stores the plain
sentences, without TAG block.

| Transport | Direction | Settings | Notes |
| --- | --- | --- | --- |
| TCP server | listens | bind address, port | Any number of clients; each receives every line. Port `0` picks a free port. Clients that disconnect or error are removed immediately. Data received from clients is discarded. |
| TCP client | connects | host, port, reconnect interval | Reconnects automatically after the peer drops the connection; lines are dropped while disconnected. |
| UDP | sends | mode (unicast, broadcast, multicast), address, port, interface, multicast TTL | One datagram per line. In broadcast mode an empty address resolves to the subnet broadcast of the chosen interface, or `255.255.255.255` when no interface is chosen; an explicit address such as `255.255.255.255` can always be given. The interface, when set, is the source of the datagrams and the multicast egress. |
| WebSocket server | listens | bind address, port | Each line is one text frame. With the `signalk` encoding every client receives the Signal K [hello message](signalk.md#hello-message) right after it connects. |
| Serial port | writes | port, baud rate, data bits, parity, stop bits, flow control | Any positive baud rate is accepted. The port is opened write-only; an unplugged device moves the transport to `failed`. |
| File | writes | path, append or truncate | Flushed after every line so the file can be tailed while the simulator runs. |
| Log | writes | path, append or truncate | A recording: a `#` header when the file is new, then every line prefixed with the wall-clock UTC time, see the [log file reference](log-format.md). Flushed after every line. |

## Network interfaces

The simulator lists every IPv4 address of every interface that is up and running, including
loopback, with the subnet broadcast address. The list is used to choose the UDP source interface and to
show the addresses clients can connect to.

The interface of a UDP output is given by its system name (`eth0`, `en0`) or its descriptive
name (`Ethernet`); it must be up and running and have an IPv4 address, otherwise the output
fails. It is both the source of the datagrams and, in multicast mode, the multicast egress.
UDP destinations are IPv4 addresses: an IPv6 address or a host name makes the output fail.

## Ports used by convention

| Port | Use |
| --- | --- |
| 10110 | NMEA 0183 over TCP or UDP, the de-facto convention used by OpenCPN and most gateways |
| 3000 | Signal K WebSocket, matching the Signal K server default |
| 42000 | ViewSync UDP, the port Google Earth listens on by default |
