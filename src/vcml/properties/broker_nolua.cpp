/******************************************************************************
 *                                                                            *
 * Copyright (C) 2022 MachineWare GmbH                                        *
 * All Rights Reserved                                                        *
 *                                                                            *
 * This is work is licensed under the terms described in the LICENSE file     *
 * found in the root directory of this source tree.                           *
 *                                                                            *
 ******************************************************************************/

#include "vcml/properties/broker_lua.h"

namespace vcml {

broker_lua::broker_lua(const string& filename): broker("lua") {
    VCML_REPORT("lua scripting support not available");
}

broker_lua::~broker_lua() {
    // nothing to do
}

} // namespace vcml
