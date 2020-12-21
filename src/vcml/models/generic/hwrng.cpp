/******************************************************************************
 *                                                                            *
 * Copyright 2020 Jan Henrik Weinstock                                        *
 *                                                                            *
 * Licensed under the Apache License, Version 2.0 (the "License");            *
 * you may not use this file except in compliance with the License.           *
 * You may obtain a copy of the License at                                    *
 *                                                                            *
 *     http://www.apache.org/licenses/LICENSE-2.0                             *
 *                                                                            *
 * Unless required by applicable law or agreed to in writing, software        *
 * distributed under the License is distributed on an "AS IS" BASIS,          *
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.   *
 * See the License for the specific language governing permissions and        *
 * limitations under the License.                                             *
 *                                                                            *
 ******************************************************************************/

#include <unistd.h>
#include <fcntl.h>

#include "vcml/models/generic/hwrng.h"

namespace vcml { namespace generic {

    u32 hwrng::read_RNG() {
        if (pseudo)
            return (u32)rand();

        static const char* src = "/dev/urandom";
        int fd = open(src, O_RDONLY);
        VCML_ERROR_ON(fd < 0, "failed to open %s: %s", src, strerror(errno));

        u32 data = 0;
        if (!fd_read(fd, &data, sizeof(data)))
            VCML_ERROR("failed to read %s: %s", src, strerror(errno));

        if (close(fd) < 0)
            VCML_ERROR("failed to close %s: %s", src, strerror(errno));

        return data;
    }

    hwrng::hwrng(const sc_module_name& nm):
        peripheral(nm),
        RNG("RNG", 0x0),
        IN("IN"),
        pseudo("pseudo", false),
        seed("seed", 0) {

        RNG.allow_read_only();
        RNG.sync_never();
        RNG.read = &hwrng::read_RNG;

        reset();
    }

    hwrng::~hwrng() {
        // nothing to do
    }

    void hwrng::reset() {
        if (pseudo)
            srand(seed);
    }

}}
