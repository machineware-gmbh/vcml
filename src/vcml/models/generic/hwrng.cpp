/******************************************************************************
 *                                                                            *
 * Copyright (C) 2022 MachineWare GmbH                                        *
 * All Rights Reserved                                                        *
 *                                                                            *
 * This is work is licensed under the terms described in the LICENSE file     *
 * found in the root directory of this source tree.                           *
 *                                                                            *
 ******************************************************************************/

#include "vcml/models/generic/hwrng.h"

namespace vcml {
namespace generic {

u32 hwrng::read_rng() {
    if (pseudo)
        return (u32)rand();

    u32 data = 0;
    if (!mwr::fill_random(&data, sizeof(data)))
        log_warn("failed to get random data");

    return data;
}

hwrng::hwrng(const sc_module_name& nm):
    peripheral(nm),
    rng("rng", 0x0),
    in("in"),
    pseudo("pseudo", false),
    seed("seed", 0) {
    rng.allow_read_only();
    rng.sync_never();
    rng.on_read(&hwrng::read_rng);
}

hwrng::~hwrng() {
    // nothing to do
}

void hwrng::reset() {
    if (pseudo)
        srand(seed);
}

VCML_EXPORT_MODEL(vcml::generic::hwrng, name, args) {
    return new hwrng(name);
}

} // namespace generic
} // namespace vcml
