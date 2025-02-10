# Build & Installation

1. Download and install CMake:

   VCML uses [CMake](https://cmake.org/) as its building system. Please install it from the repository of your
   distribution or download ready-to-use executables from CMake's [download page](https://cmake.org/download/).

2. Clone VCML repository and initialize submodules:

    ```sh
    git clone https://github.com/machineware-gmbh/vcml.git --recursive
    ```
    or, alternatively
    ```sh
    git clone https://github.com/machineware-gmbh/vcml.git
    git submodule update --init
    ```

3. *Optional*: Download and build SystemC.

    *Note:*
    If you choose to skip this step, VCML will automatically download
    SystemC from [`github.com/machineware-gmbh/systemc`](https://github.com/machineware-gmbh/systemc) during configuration
    and set up the build environment accordingly.

    VCML provides a helper script that downloads and builds SystemC from [Accellera](https://accellera.org/).
    Choose a build directory and version of SystemC and run the following script
    from the root directory of VCML:
    ```sh
    ./utils/setup-systemc --prefix /opt/sysc --version 2.3.3 --optimize
    ```
    Here, the script will build SystemC 2.3.3 and install it into `/opt/sysc`.
    You can also build a debug version of SystemC, by appending `--debug` to the command.

    After running the script, the `SYSTEMC_HOME` environment variable must be set, so that
    the build system of VCML can find SystemC. The helper script will print the correct value
    at the end of the execution.
    This allows you to run the following command:
    ```sh
    export SYSTEMC_HOME=<PATH_TO_SYSTEMC> # e.g. /opt/sysc/2.3.3
    ```

    *Note:*
    You can provide your own SystemC installation by specifying your own
    `SYSTEMC_HOME` and `TARGET_ARCH` variables. Versions starting from `2.3.0`
    are supported.

4. *Optional*: Download LibMWR:

    *Note:*
    If you choose to skip this step, VCML will automatically download
    LibMWR from [`github.com/machineware-gmbh/mwr`](https://github.com/machineware-gmbh/mwr)
    during configuration and set up the build environment accordingly.

    Download [LibMWR](https://github.com/machineware-gmbh/mwr) and extract it. Then set
    the environment variable `MWR_HOME` so that the build system of VCML can find the
    library. The environment variable must point to the top level source path of LibMWR,
    where the `CMakeLists.txt` is located.

    *Note:*
    You can provide your own LibMWR implementation by specifying your own
    `MWR_HOME` variable.


4. Choose directories for building and deployment of VCML:

    ```
    <source-dir>  location of your repo copy,     e.g. /home/jan/vcml
    <build-dir>   location to store object files, e.g. /home/jan/vcml/BUILD
    <install-dir> output directory for binaries,  e.g. /opt/vcml
    ```

5. Configure and build the project using `cmake`.

   During configuration, you must state whether to build the utility programs and
   unit tests:
     * `-DVCML_BUILD_UTILS=[ON|OFF]`: build utility programs (default: `ON`)
     * `-DVCML_BUILD_TESTS=[ON|OFF]`: build unit tests (default: `OFF`)

   Optional dependencies are automatically enabled if found by `cmake` on the
   host build system. To disable their use, pass `-DUSE_<DEPENDENCY_NAME>=FALSE`
   to `cmake` during configuration. Check out the [following section](#dependencies)
   for more information about the dependencies of VCML.

   Release and debug build configurations are controlled via the regular
   CMake parameters:
   ```sh
   cmake -B <build-dir> -DCMAKE_INSTALL_PREFIX=<install-dir> -DCMAKE_BUILD_TYPE=RELEASE <source-dir>
   cmake --build <build-dir>
   ```
   If building with `-DVCML_BUILD_TESTS=ON`, you can run all unit tests with
   `ctest --test-dir <build-dir>`.

   After building, VCML can then be installed into `<install-dir>` with the
   following command:

   ```sh
   cmake --install <build-dir>
   ```
   Depending on the path, you may need elevated rights to write to the installation directory (e.g. with `sudo`).

   Alternately, VCML also provides helper scripts that automatically setup, build and install VCML
   using GCC or Clang.
   ```sh
   <source-dir>/utils/setup-gcc [DEBUG|RELEASE|...] # for GCC builds
   <source-dir>/utils/setup-clang [DEBUG|RELEASE|...] # for Clang builds
   ```
   After running the helper script, the installed library can be found in `<source-dir>/BUILD/<build-type>`.


After installation, the following new files should be present:
```
<install-dir>/lib/libvcml.a   # library (on release builds)
<install-dir>/lib/libvcmld.a  # library (on debug builds)
<install-dir>/include/vcml.h  # library header
<install-dir>/include/vcml/   # header files
<install-dir>/bin/            # utility programs/scripts
```
Update your environment so that other projects can reference your build:
```sh
export VCML_HOME=<install-dir>
```

## Windows Build & Installation

Windows builds are currently supported using
[Microsoft Visual Studio](https://visualstudio.microsoft.com/).
There are two ways to build `vcml` on Windows:

1. Using Visual Studio IDE:
   - Launch Visual Studio
   - In the Open Dialog click on `Clone a Repository`
   - Enter `https://github.com/machineware-gmbh/vcml` and click `Clone`
   - Once Visual Studio has cloned the project, double-click on the `vcml` folder
   - Run `Build All` from the build menu.

   CMake parameters such as `SYSTEMC_HOME` can be configured by selecting `CMake Settings
   for vcml` from the project menu. There, in the section `CMake variables and cache`, the
   desired parameters can be added or changed.

2. Using the command line:
   - Install [Git for Windows](https://git-scm.com/download/win)
   - Launch `git-bash` (installed by Git for Windows)
   - Run `git clone --recursive https://github.com:machineware-gmbh/vcml`
   - Run `cmake -B BUILD -G "Visual Studio 17" -DCMAKE_BUILD_TYPE=[DEBUG|RELEASE]`
   - Run `cmake --build BUILD`

## Maintaining Multiple Builds

Debug builds (i.e. `-DCMAKE_BUILD_TYPE=DEBUG`) are intended for developers
that use `vcml` to construct a new VP and want to track down bugs.
Note that these builds operate significantly slower than optimized release
builds and should therefore not be used for VPs that are used productively,
e.g. for target software development. To maintain both builds from a single
source repository, try the following:
```sh
git clone https://github.com/machineware-gmbh/vcml.git --recursive && cd vcml
home=$PWD
for type in "DEBUG" "RELEASE"; do
    install="$home/BUILD/$type"
    build="$home/BUILD/$type/BUILD"
    mkdir -p $build && cd $build
    cmake -DCMAKE_BUILD_TYPE=$type -DCMAKE_INSTALL_PREFIX=$install $home
    cmake --build .
    cmake --install .
done
```
Afterwards, you can use the environment variable `VCML_HOME` to point to the
build you want to use:
* `export VCML_HOME=(...)/vcml/BUILD/DEBUG` for the debug build or
* `export VCML_HOME=(...)/vcml/BUILD/RELEASE` for the release build

# Dependencies

VCML requires a SystemC build. Currently, versions >= 2.3.0 are supported.

Besides, it also uses [LibMWR](https://github.com/machineware-gmbh/mwr), which will be
automatically downloaded and installed during build. If the environment variable `MWR_HOME`
is set to a local path, VCML will use the local copy instead.

## Optional dependencies

VCML also offers extra optional features that require the separate installation of
additional libraries.

  - Host SocketCAN support:

    If the host Linux kernel is built with SocketCAN support, VCML can use SocketCAN
    devices as a backend for exchanging CAN frames between the host and the virtual
    environment. SocketCAN devices are **not** supported on Windows!

  - Host TAP support:

    If the host Linux kernel is built with TAP support, TAP devices can be used as
    an Ethernet backend. This is significantly faster than SLiRP, but requires
    elevated privileges on the host for creating the TAP devices. TAP devices are
    **not** supported on Windows!

  - libslirp:

    SLiRP can be used for forwarding Ethernet frames to/from the virtualized
    environment without the need for elevated rights on the host.

  - libusb:

    VCML can expose host USB devices to the virtual environment with the help of
    libusb.

  - libvnc:

    VCML uses libvnc for providing a VNC server that gives access to graphical
    output. A separate VNC client is needed, for example [Remmina](https://remmina.org/)
    or [TightVNC](https://www.tightvnc.com/).

  - Lua:

    VCML-based VPs can be configured using Lua scripts. See [here](lua.md) under
    section `Configuration via LUA Scripting` for further information.

  - SDL2:

    With SDL2, VCML can create a window that displays graphical output of
    a Virtual Platform.
