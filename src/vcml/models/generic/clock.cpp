/******************************************************************************
 *                                                                            *
 * Copyright (C) 2022 MachineWare GmbH                                        *
 * All Rights Reserved                                                        *
 *                                                                            *
 * This is work is licensed under the terms described in the LICENSE file     *
 * found in the root directory of this source tree.                           *
 *                                                                            *
 ******************************************************************************/

#include "vcml/models/generic/clock.h"

namespace vcml {
namespace generic {

clock::clock(const sc_module_name& nm, clock_t init_hz):
    module(nm), hz("hz", init_hz), clk("clk") {
    clk = hz;
}

clock::~clock() {
    // nothing to do
}

} // namespace generic
} // namespace vcml
