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
    return std::getenv(nm.c_str()) != nullptr;
}

bool broker_env::lookup(const string& name, string& val) {
    string nm = name;
    std::replace(nm.begin(), nm.end(), SC_HIERARCHY_CHAR, '_');

    const char* env = std::getenv(nm.c_str());
    if (env == nullptr)
        return false;

    const size_t max_size = string().max_size() - 1;
    if (strlen(env) > max_size)
        return false;

    val = string(env);
    return true;
}

} // namespace vcml
