# Virtual Components Modeling Library (vcml)

The Virtual Components Modeling Library contains a set of SystemC/TLM modeling
primitives and component models that can be used to swiftly assemble system
level simulators for embedded systems, i.e. Virtual Platforms. Its main design
goal is to accelerate VP construction by providing a set of commonly used
features, such as TLM sockets, Interrupt ports, I/O peripherals and registers.
Based on these design primitives, TLM models for frequently deployed components
are also provided, such as memories, memory-mapped buses, UARTs, etc.

[![Build Status](https://travis-ci.org/janweinstock/vcml.svg?branch=master)](https://travis-ci.org/janweinstock/vcml)
[![Coverity Scan Build Status](https://scan.coverity.org/projects/18906/badge.svg)](https://scan.coverity.com/projects/janweinstock-vcml)

----
## Build & Installation
In order to build `vcml`, you need a working installation of SystemC.
Currently, versions >= 2.3.0 are supported. Furthermore, you need `cmake`,
`libelf` and `googletest-1.8.0` if you also want to run the unit tests. This
is how to build and install them:

1. Download and build SystemC (here `systemc-2.3.1a`). Make sure to set the
   environment variables `SYSTEMC_HOME` and `TARGET_ARCH` accordingly:
    ```
    wget http://www.accellera.org/images/downloads/standards/systemc/systemc-2.3.1a.tar.gz
    tar -xzf systemc-2.3.1a.tar.gz && cd systemc-2.3.1a
    export SYSTEMC_HOME=`pwd`
    export TARGET_ARCH=linux64
    mkdir BUILD && cd BUILD
    ../configure --prefix=$SYSTEMC_HOME --enable-optimize --enable-static CXXFLAGS="-std=c++11"
    make -j 4 && make install
    ```

2. Install `libelf` and `cmake`:
    ```
    sudo apt-get install cmake libelf-dev  # Ubuntu
    sudo yum install cmake3 libelf-dev     # Centos
    ```
3. (Optional) Install `googletest-1.8.0` and set the environment variables
    `GTEST_ROOT` and `GMOCK_ROOT`:
    ```
    wget https://github.com/google/googletest/archive/release-1.8.0.tar.gz
    tar -xzf release-1.8.0.tar.gz && cd googletest-release-1.8.0
    mkdir BUILD && cd BUILD
    cmake .. -DCMAKE_INSTALL_PREFIX=INSTALL -DCMAKE_CXX_STANDARD=11
    make -j 4 && make install
    export GTEST_ROOT=`pwd`/INSTALL
    export GMOCK_ROOT=$GTEST_ROOT
    ```
4. Chose directories for building and deployment:
    ```
    <source-dir>  location of your repo copy,     e.g. /home/jan/vcml
    <build-dir>   location to store object files, e.g. /home/jan/vcml/BUILD
    <install-dir> output directory for binaries,  e.g. /opt/vcml
    ```

5. Configure and build the project using `cmake`. During configuration you must
   state whether or not to build the utility programs and unit tests:
     * `-DBUILD_UTILS=[ON|OFF]`: build utility programs (default: `ON`)
     * `-DBUILD_TESTS=[ON|OFF]`: build unit tests (default: `OFF`)
     * `-DBUILD_PIC=[ON|OFF]`: build position independent code (default: `ON`)

   Release and debug build configurations are controlled via the regular
   parameters:
   ```
   mkdir -p <build-dir>
   cd <build-dir>
   cmake -DCMAKE_INSTALL_PREFIX=<install-dir> -DCMAKE_BUILD_TYPE=RELEASE <source-dir>
   make -j 4
   sudo make install
   ```
   If building with `-DBUILD_TESTS=ON` you can run all unit tests using
   `make test` within `<build-dir>`.

6. After installation, the following new files should be present:
    ```
    <install-dir>/lib/libvcml.a   # library
    <install-dir>/include/vcml.h  # library header
    <install-dir>/include/vcml/   # header files
    <install-dir>/bin/            # utility programs
    ```

7. Update your environment so that other projects can reference your build:
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
git clone https://github.com/janweinstock/vcml.git && cd vcml
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
## Documentation
Some basic documentation about this library and its models can be found
[here](doc/main.md).
Another potential useful source for help can be the study of projects that
employ VCML to construct a complete VP, for example
[`or1kmvp`](https://github.com/janweinstock/or1kmvp/).

----
## License

This project is licensed under the Apache-2.0 license - see the
[LICENSE](LICENSE) file for details.
