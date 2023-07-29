/******************************************************************************
 *                                                                            *
 * Copyright (C) 2022 MachineWare GmbH                                        *
 * All Rights Reserved                                                        *
 *                                                                            *
 * This is work is licensed under the terms described in the LICENSE file     *
 * found in the root directory of this source tree.                           *
 *                                                                            *
 ******************************************************************************/

#include "vcml/models/generic/reset.h"

namespace vcml {
namespace generic {

reset::reset(const sc_module_name& nm, bool init_state):
    module(nm), state("state", init_state), rst("rst") {
}

reset::~reset() {
    // nothing to do
}

void reset::end_of_elaboration() {
    rst.pulse();
    rst = state;
}

VCML_EXPORT_MODEL(vcml::generic::reset, name, args) {
    return new reset(name);
}

} // namespace generic
} // namespace vcml
