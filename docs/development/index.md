# Development

Everything a contributor needs beyond the [contributing guide](https://github.com/Dimitrios-Kafetzis/NMEA_Simulator_X/blob/main/CONTRIBUTING.md).

- [Coding standards](coding-standards.md): naming, layout, ownership and Qt usage rules.
- [Release process](release-process.md): how a change becomes a downloadable package.
- [Roadmap](roadmap.md): milestones from scaffold to 1.0 and beyond.

## Repository layout

```text
NMEA_Simulator_X/
├── .github/        CI workflows, issue and pull request templates, CODEOWNERS
├── cmake/          Warning and sanitizer helper modules
├── docs/           This documentation site (MkDocs) and the ADRs
├── packaging/      Installer, image and package-manager recipes
├── samples/        GPX, KML and log files used by tutorials and tests
├── src/
│   ├── core/       nmeasim::core, the Qt-free engine
│   ├── io/         nmeasim::io, transports and persistence on Qt Core/Network/SerialPort
│   ├── cli/        nmeasim command-line tool
│   └── app/        NMEASimulatorX desktop application (Qt Widgets)
├── tests/          Catch2 test suites mirroring src/
├── CMakeLists.txt  Top-level build
├── CMakePresets.json
└── vcpkg.json      Third-party C++ dependencies
```
