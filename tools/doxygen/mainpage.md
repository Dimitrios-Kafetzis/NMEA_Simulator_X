# NMEA Simulator X C++ API {#mainpage}

This is the reference of the two libraries that the desktop application and the `nmeasim`
command-line tool are built on. The user documentation, including the architecture overview,
lives at <https://dimitrios-kafetzis.github.io/NMEA_Simulator_X/>.

| Library | Namespace | Depends on | Contents |
| --- | --- | --- | --- |
| `nmeasim::core` | `nmeasim::core` | C++20 standard library, GeographicLib, pugixml | Vessel model, simulation sources, NMEA 0183, AIS, Signal K and ViewSync encoders, tracks and log files |
| `nmeasim::io` | `nmeasim::io` | `core`, Qt Core, Network, SerialPort, WebSockets | Profiles, transports and the simulation runner that connects them |

Good starting points:

- nmeasim::core::simulation::Simulation advances a source and schedules the sentences.
- nmeasim::core::nmea0183::SentenceRegistry lists every sentence the simulator can emit.
- nmeasim::io::Profile is the persistent configuration and nmeasim::io::SimulationRunner runs it.

The API is not stable across minor versions before it is declared so in the changelog; the
libraries are linked statically into the applications.
