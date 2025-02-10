# Virtual Components Modeling Library (vcml)

The Virtual Components Modeling Library contains a set of SystemC/TLM modeling
primitives and component models that can be used to swiftly assemble system
level simulators for embedded systems, i.e. Virtual Platforms. Its main design
goal is to accelerate VP construction by providing a set of commonly used
features, such as TLM sockets, Interrupt ports, I/O peripherals and registers.
Based on these design primitives, TLM models for frequently deployed components
are also provided, such as memories, memory-mapped buses, UARTs, etc.

[![Build Status](https://github.com/machineware-gmbh/vcml/actions/workflows/cmake.yml/badge.svg?branch=main)](https://github.com/machineware-gmbh/vcml/actions/workflows/cmake.yml)
[![Sanitizer Status](https://github.com/machineware-gmbh/vcml/actions/workflows/asan.yml/badge.svg?branch=main)](https://github.com/machineware-gmbh/vcml/actions/workflows/asan.yml)
[![Lint Status](https://github.com/machineware-gmbh/vcml/actions/workflows/lint.yml/badge.svg?branch=main)](https://github.com/machineware-gmbh/vcml/actions/workflows/lint.yml)
[![Code Style](https://github.com/machineware-gmbh/vcml/actions/workflows/style.yml/badge.svg?branch=main)](https://github.com/machineware-gmbh/vcml/actions/workflows/style.yml)
[![Nightly Status](https://github.com/machineware-gmbh/vcml/actions/workflows/nightly.yml/badge.svg?branch=main)](https://github.com/machineware-gmbh/vcml/actions/workflows/nightly.yml)
[![Coverage Status](https://github.com/machineware-gmbh/vcml/actions/workflows/coverage.yml/badge.svg?branch=main)](https://github.com/machineware-gmbh/vcml/actions/workflows/coverage.yml)
[![Windows Status](https://github.com/machineware-gmbh/vcml/actions/workflows/windows.yml/badge.svg?branch=main)](https://github.com/machineware-gmbh/vcml/actions/workflows/windows.yml)

----
## Documentation
Building instructions can be found [here](doc/build.md).
Some basic documentation about this library and its models can be found
[here](doc/main.md).

----
## Related Projects
| Project                                                        | About                               |
|----------------------------------------------------------------|-------------------------------------|
| [vcml-cci](https://github.com/machineware-gmbh/vcml-cci)       | SystemC CCI integration for VCML    |
| [vcml-silkit](https://github.com/machineware-gmbh/vcml-silkit) | Vector SIL Kit integration for VCML |

### Community
A curated collection of existing contributions in the form of individual models or complete Virtual
Platforms can be found on our community projects website at:

[https://www.machineware.de/vcml-community](https://www.machineware.de/vcml-community)

----
## Contributions
Please note that we currently cannot accept Pull Requests on GitHub.
Contributions to VCML can be submitted as patch files via [email](https://www.machineware.de)
instead.

----
## License

This project is licensed under the Apache-2.0 license - see the
[LICENSE](LICENSE) file for details.
