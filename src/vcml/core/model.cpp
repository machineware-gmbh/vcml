/******************************************************************************
 *                                                                            *
 * Copyright (C) 2023 MachineWare GmbH                                        *
 * All Rights Reserved                                                        *
 *                                                                            *
 * This is work is licensed under the terms described in the LICENSE file     *
 * found in the root directory of this source tree.                           *
 *                                                                            *
 ******************************************************************************/

#include "vcml/core/model.h"

namespace vcml {

model::model(const sc_module_name& name, const string& kind):
    m_impl(create(kind, name)) {
    // nothing to do
}

module* model::create(const string& type, const sc_module_name& name) {
    vector<string> args = split(type);
    string kind = args[0];
    args.erase(args.begin());

    auto it = modeldb().find(kind);
    if (it != modeldb().end()) {
        module* mod(it->second(name, args));
        VCML_ERROR_ON(!mod, "failed to create instance of %s", kind.c_str());
        if (kind != mod->kind()) {
            mod->log.warn("module kind mismatch, expected %s, is %s",
                          kind.c_str(), mod->kind());
        }

        return mod;
    }

    std::cout << "known models:" << std::endl;
    for (auto [name, proc] : modeldb())
        std::cout << "  " << name << std::endl;
    VCML_ERROR("model not found: %s", kind.c_str());
}

bool model::define(const string& kind, create_fn create) {
    auto it = modeldb().find(kind);
    if (it != modeldb().end())
        return false;

    modeldb()[kind] = create;
    return true;
}

void model::list_models(ostream& os) {
    for (auto [name, func] : modeldb())
        os << name << std::endl;
}

std::map<string, model::create_fn>& model::modeldb() {
    static std::map<string, create_fn> db;
    return db;
}

} // namespace vcml
