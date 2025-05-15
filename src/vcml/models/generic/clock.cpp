/******************************************************************************
 *                                                                            *
 * Copyright (C) 2022 MachineWare GmbH                                        *
 * All Rights Reserved                                                        *
 *                                                                            *
 * This is work is licensed under the terms described in the LICENSE file     *
 * found in the root directory of this source tree.                           *
 *                                                                            *
 ******************************************************************************/

#include "vcml/core/types.h"
#include "vcml/core/systemc.h"
#include "vcml/core/module.h"
#include "vcml/core/model.h"

#include "vcml/logging/logger.h"
#include "vcml/properties/property.h"
#include "vcml/protocols/clk.h"

#include "vcml/models/generic/clock.h"

namespace vcml {
namespace generic {

clock::clock(const sc_module_name& nm, hz_t init_hz):
    module(nm), hz("hz", init_hz), clk("clk") {
    clk = hz;
}

clock::~clock() {
    // nothing to do
}

VCML_EXPORT_MODEL(vcml::generic::clock, name, args) {
    hz_t defclk = 100 * MHz;
    if (!args.empty())
        defclk = from_string<hz_t>(args[0]);
    return new clock(name, defclk);
}

} // namespace generic
} // namespace vcml
