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
## Build & Installation
In order to build `vcml`, you need a working installation of SystemC.
Currently, versions >= 2.3.0 are supported. Furthermore, you need `cmake`,
`libelf` and, optionally, `libvncserver` if you also want graphics support.
This is how to build and install them:

1. *Optional*: download and build SystemC (here `systemc-2.3.2`). Make sure to
   set the environment variables `SYSTEMC_HOME` and `TARGET_ARCH` accordingly:
    ```
    wget http://www.accellera.org/images/downloads/standards/systemc/systemc-2.3.2.tar.gz
    tar -xzf systemc-2.3.2.tar.gz && cd systemc-2.3.2
    export SYSTEMC_HOME=`pwd`
    export TARGET_ARCH=linux64
    mkdir BUILD && cd BUILD
    ../configure --prefix=$SYSTEMC_HOME --enable-optimize --enable-static
    make -j 4 && make install
    ```
    *Note*: if you choose to skip this step, `vcml` will automatically download
    SystemC from `github.com/machineware-gmbh/systemc.git` during configuration
    and set the environment variables accordingly for this build.

    *Note*: you can provide your own SystemC installation by specifing your own
    `SYSTEMC_HOME` and `TARGET_ARCH` variables. Versions starting from `2.3.0`
    are supported.

2. Download and install `cmake`:
   - <https://github.com/Kitware/CMake/releases>

3. Install optional dependencies:
   - `Lua` for scripting support
   - `SDL2` for graphic output
   - `libvnc` for remote graphic output
   - `libslirp` for userspace ethernet emulation

    ```
    # Ubuntu
    sudo apt-get install liblua5.4-dev libsdl2-dev libvncserver-dev libslirp-dev
    # Fedora
    sudo dnf install lua-devel SDL2-devel libvncserver-devel libslirp-devel
    ```
   Optional dependencies are automatically enabled if found by `cmake` on the
   host build system. To disable their use, pass `-DUSE_<LUA|SDL2|VNC|SLIRP>=FALSE`
   to `cmake` during configuration (see step 6).

4. Clone VCML repository and initialize submodules:
    ```
    git clone https://github.com/machineware-gmbh/vcml.git --recursive
    ```
    or, alternatively
    ```
    git clone https://github.com/machineware-gmbh/vcml.git
    git submodule init
    git submodule update
    ```

5. Chose directories for building and deployment:
    ```
    <source-dir>  location of your repo copy,     e.g. /home/jan/vcml
    <build-dir>   location to store object files, e.g. /home/jan/vcml/BUILD
    <install-dir> output directory for binaries,  e.g. /opt/vcml
    ```

6. Configure and build the project using `cmake`. During configuration you must
   state whether or not to build the utility programs and unit tests:
     * `-DVCML_BUILD_UTILS=[ON|OFF]`: build utility programs (default: `ON`)
     * `-DVCML_BUILD_TESTS=[ON|OFF]`: build unit tests (default: `OFF`)

   Release and debug build configurations are controlled via the regular
   parameters:
   ```
   mkdir -p <build-dir>
   cd <build-dir>
   cmake -DCMAKE_INSTALL_PREFIX=<install-dir> -DCMAKE_BUILD_TYPE=RELEASE <source-dir>
   make -j 4
   sudo make install
   sudo make -C mwr install
   sudo make -C systemc install # if you skipped step 1
   ```
   If building with `-DVCML_BUILD_TESTS=ON` you can run all unit tests using
   `make test` within `<build-dir>`.

7. After installation, the following new files should be present:
    ```
    <install-dir>/lib/libvcml.a   # library
    <install-dir>/include/vcml.h  # library header
    <install-dir>/include/vcml/   # header files
    <install-dir>/bin/            # utility programs
    ```

8. Update your environment so that other projects can reference your build:
    ```
    export VCML_HOME=<install-dir>
    ```

----
## Maintaining Multiple Builds
Debug builds (i.e. `-DCMAKE_BUILD_TYPE=DEBUG`) are intended for developers
that use `vcml` to construct a new VP and want to track down bugs.
Note that these builds operate significantly slower than optimized release
builds and should therefore not be used for VPs that are used productively,
e.g. for target software development. To maintain both builds from a single
source repository, try the following:
```
git clone https://github.com/machineware-gmbh/vcml.git --recursive && cd vcml
home=$PWD
for type in "DEBUG" "RELEASE"; do
    install="$home/BUILD/$type"
    build="$home/BUILD/$type/BUILD"
    mkdir -p $build && cd $build
    cmake -DCMAKE_BUILD_TYPE=$type -DCMAKE_INSTALL_PREFIX=$install $home
    make install
done
```
Afterwards, you can use the environment variable `VCML_HOME` to point to the
build you want to use:
* `export VCML_HOME=(...)/vcml/BUILD/DEBUG` for the debug build or
* `export VCML_HOME=(...)/vcml/BUILD/RELEASE` for the release build

----
## Windows Build & Installation
Windows builds are currently supported using
[Microsoft Visual Studio](https://visualstudio.microsoft.com/de/).
There are two ways to build `vcml` on Windows:

1. Using Visual Studio IDE:
  - Launch Visual Studio
  - In the Open Dialog click on `Clone a Repository`
  - Enter `https://github.com/machineware-gmbh/vcml` and click `Clone`
  - Once Visual Studio has cloned the project, double-click on the `vcml` folder
  - Run `Build All` from the build menu.

2. Using the command line:
  - Install [Git for Windows](https://git-scm.com/download/win)
  - Launch `git-bash` (installed by Git for Windows)
  - Run `git clone --recursive https://github.com:machineware-gmbh/vcml`
  - Run `cmake -B BUILD -G "Visual Studio 17" -DCMAKE_BUILD_TYPE=[DEBUG|RELEASE]`
  - Run `cmake --build BUILD`

----
## Documentation
Some basic documentation about this library and its models can be found
[here](doc/main.md).
Another potential useful source for help can be the study of projects that
employ VCML to construct a complete VP, for example
[`or1kmvp`](https://github.com/janweinstock/or1kmvp/).

----
## Contributions
Please note that we currently cannot accept Pull Requests on Github.
Instructions on how to contribute to VCML, along with a curated collection of
existing contributions in the form of individual models or complete Virtual
Platforms can be accessed on our community projects website at:

[https://www.machineware.de/vcml-community](https://www.machineware.de/vcml-community)

----
## License

This project is licensed under the Apache-2.0 license - see the
[LICENSE](LICENSE) file for details.
