 ##############################################################################
 #                                                                            #
 # Copyright (C) 2025 MachineWare GmbH                                        #
 # All Rights Reserved                                                        #
 #                                                                            #
 # This is work is licensed under the terms described in the LICENSE file     #
 # found in the root directory of this source tree.                           #
 #                                                                            #
 ##############################################################################

$ErrorActionPreference = 'Stop'
 
$VCML_HOME = (Get-Item $PSScriptRoot).parent.FullName
$BINDIR = "$VCML_HOME\BUILD"
$BUILD = "$BINDIR\.BUILD"
$NPROC = (Get-WmiObject -Class Win32_Processor).NumberOfCores

if ($args.Length) {
    $BUILDS = $args
}
else {
    $BUILDS = "DEBUG", "RELEASE"
}

cmake -B "$BUILD" -G "Visual Studio 17" `
    -DCMAKE_EXPORT_COMPILE_COMMANDS="ON" `
    -DVCML_BUILD_TESTS="ON" `
    -DVCML_BUILD_UTILS="ON" `
    "$VCML_HOME"

foreach ($TYPE in $BUILDS) {
    cmake --build $BUILD --config $TYPE -j $NPROC
    cmake --install $BUILD --config $TYPE --prefix $BINDIR\$TYPE
}
