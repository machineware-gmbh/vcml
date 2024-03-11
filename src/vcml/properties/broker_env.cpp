/******************************************************************************
 *                                                                            *
 * Copyright (C) 2022 MachineWare GmbH                                        *
 * All Rights Reserved                                                        *
 *                                                                            *
 * This is work is licensed under the terms described in the LICENSE file     *
 * found in the root directory of this source tree.                           *
 *                                                                            *
 ******************************************************************************/

#include "vcml/properties/broker_env.h"

namespace vcml {

broker_env::broker_env(): broker("environment") {
    // nothing to do
}

broker_env::~broker_env() {
    // nothing to do
}

bool broker_env::defines(const string& name) const {
    string nm = name;
    std::replace(nm.begin(), nm.end(), SC_HIERARCHY_CHAR, '_');
    return mwr::getenv(name).has_value();
}

bool broker_env::lookup(const string& name, string& val) {
    string nm = name;
    std::replace(nm.begin(), nm.end(), SC_HIERARCHY_CHAR, '_');

    auto env = mwr::getenv(nm);
    if (!env)
        return false;

    val = mwr::escape(env.value(), "");
    return true;
}

} // namespace vcml
