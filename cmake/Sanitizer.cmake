 ##############################################################################
 #                                                                            #
 # Copyright 2022 Jan Henrik Weinstock                                        #
 #                                                                            #
 # Licensed under the Apache License, Version 2.0 (the "License");            #
 # you may not use this file except in compliance with the License.           #
 # You may obtain a copy of the License at                                    #
 #                                                                            #
 #     http://www.apache.org/licenses/LICENSE-2.0                             #
 #                                                                            #
 # Unless required by applicable law or agreed to in writing, software        #
 # distributed under the License is distributed on an "AS IS" BASIS,          #
 # WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.   #
 # See the License for the specific language governing permissions and        #
 # limitations under the License.                                             #
 #                                                                            #
 ##############################################################################

# Address Sanitizer
set(CMAKE_C_FLAGS_ASAN
    "-fsanitize=address -fno-omit-frame-pointer -O1 -g" CACHE STRING
    "Flags used by the C++ compiler during ASAN builds." FORCE)
set(CMAKE_CXX_FLAGS_ASAN
    "-fsanitize=address -fno-omit-frame-pointer -O1 -g" CACHE STRING
    "Flags used by the C compiler during ASAN builds." FORCE)
set(CMAKE_EXE_LINKER_FLAGS_ASAN
    "-fsanitize=address -fno-omit-frame-pointer" CACHE STRING
    "Flags used for linking binaries during ASAN builds." FORCE)
set(CMAKE_SHARED_LINKER_FLAGS_ASAN
    "-fsanitize=address -fno-omit-frame-pointer" CACHE STRING
    "Flags used by the shared libraries linker during ASAN builds." FORCE)

mark_as_advanced(CMAKE_C_FLAGS_ASAN CMAKE_CXX_FLAGS_ASAN
                 CMAKE_EXE_LINKER_FLAGS_ASAN CMAKE_SHARED_LINKER_FLAGS_ASAN)


# Thread Sanitizer
set(CMAKE_C_FLAGS_TSAN
    "-fsanitize=thread -fno-omit-frame-pointer -O1 -g" CACHE STRING
    "Flags used by the C++ compiler during TSAN builds." FORCE)
set(CMAKE_CXX_FLAGS_TSAN
    "-fsanitize=thread -fno-omit-frame-pointer -O1 -g" CACHE STRING
    "Flags used by the C compiler during TSAN builds." FORCE)
set(CMAKE_EXE_LINKER_FLAGS_TSAN
    "-fsanitize=thread -fno-omit-frame-pointer" CACHE STRING
    "Flags used for linking binaries during TSAN builds." FORCE)
set(CMAKE_SHARED_LINKER_FLAGS_TSAN
    "-fsanitize=thread -fno-omit-frame-pointer" CACHE STRING
    "Flags used by the shared libraries linker during TSAN builds." FORCE)

mark_as_advanced(CMAKE_C_FLAGS_TSAN CMAKE_CXX_FLAGS_TSAN
                 CMAKE_EXE_LINKER_FLAGS_TSAN CMAKE_SHARED_LINKER_FLAGS_TSAN)


# Undefined Behavior Sanitizer
set(CMAKE_C_FLAGS_UBSAN
    "-fsanitize=undefined -fno-omit-frame-pointer -O1 -g" CACHE STRING
    "Flags used by the C++ compiler during UBSAN builds." FORCE)
set(CMAKE_CXX_FLAGS_UBSAN
    "-fsanitize=undefined -fno-omit-frame-pointer -O1 -g" CACHE STRING
    "Flags used by the C compiler during UBSAN builds." FORCE)
set(CMAKE_EXE_LINKER_FLAGS_UBSAN
    "-fsanitize=undefined -fno-omit-frame-pointer" CACHE STRING
    "Flags used for linking binaries during UBSAN builds." FORCE)
set(CMAKE_SHARED_LINKER_FLAGS_UBSAN
    "-fsanitize=undefined -fno-omit-frame-pointer" CACHE STRING
    "Flags used by the shared libraries linker during UBSAN builds." FORCE)

mark_as_advanced(CMAKE_C_FLAGS_UBSAN CMAKE_CXX_FLAGS_UBSAN
                 CMAKE_EXE_LINKER_FLAGS_UBSAN CMAKE_SHARED_LINKER_FLAGS_UBSAN)


# Update the documentation string of CMAKE_BUILD_TYPE for GUIs
set(CMAKE_BUILD_TYPE "${CMAKE_BUILD_TYPE}" CACHE STRING
    "Choose the type of build, options are: DEBUG RELEASE RELWITHDEBINFO MINSIZEREL ASAN TSAN UBSAN."
    FORCE)
