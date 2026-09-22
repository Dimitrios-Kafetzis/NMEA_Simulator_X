# Transports

A transport is an output channel. Several can run at the same time, each with its own
sentence filter (milestone M1, runner) and encoding. Every transport reports a state
(`closed`, `opening`, `open`, `failed`), the last error, the number of attached clients and
the number of bytes written.

Lines are written exactly as produced by the encoder plus `<CR><LF>`; transports never alter
the payload.

| Transport | Direction | Settings | Notes |
| --- | --- | --- | --- |
| TCP server | listens | bind address, port | Any number of clients; each receives every line. Port `0` picks a free port. Clients that disconnect or error are removed immediately. Data received from clients is discarded. |
| TCP client | connects | host, port, reconnect interval | Reconnects automatically after the peer drops the connection; lines are dropped while disconnected. |
| UDP | sends | mode (unicast, broadcast, multicast), address, port, interface, multicast TTL | One datagram per line. In broadcast mode an empty address resolves to the subnet broadcast of the chosen interface, or `255.255.255.255` when no interface is chosen; an explicit address such as `255.255.255.255` can always be given. The interface, when set, is the source of the datagrams and the multicast egress. |
| WebSocket server | listens | bind address, port, greeting | Each line is one text frame. The greeting, when set, is sent to every client right after it connects; Signal K uses this for its `hello` message. |
| Serial port | writes | port, baud rate, data bits, parity, stop bits, flow control | Any positive baud rate is accepted. The port is opened write-only; an unplugged device moves the transport to `failed`. |
| File | writes | path, append or truncate | Flushed after every line so the file can be tailed while the simulator runs. |

## Network interfaces

The simulator lists every IPv4 address of every interface that is up, including loopback,
with the subnet broadcast address. The list is used to choose the UDP source interface and to
show the addresses clients can connect to.

## Ports used by convention

| Port | Use |
| --- | --- |
| 10110 | NMEA 0183 over TCP or UDP, the de-facto convention used by OpenCPN and most gateways |
| 3000 | Signal K WebSocket, matching the Signal K server default |
