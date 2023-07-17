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

unordered_map<string, model_create_fn>& modeldb::all() {
    static unordered_map<string, model_create_fn> instance;
    return instance;
}

model modeldb::create(const string& type, const sc_module_name& name) {
    vector<string> args = split(type);
    string kind = args[0];
    args.erase(args.begin());

    auto it = all().find(kind);
    if (it != all().end()) {
        model mod(it->second(name, args));
        if (kind != mod->kind()) {
            mod->log.warn("module kind mismatch, expected %s, actual %s",
                          kind.c_str(), mod->kind());
        }

        return mod;
    }

    std::cout << "known models:" << std::endl;
    for (auto [name, proc] : all())
        std::cout << "  " << name << std::endl;
    VCML_ERROR("model not found: %s", type.c_str());
}

void modeldb::register_model(const string& kind, model_create_fn create) {
    auto it = all().find(kind);
    if (it != all().end())
        VCML_ERROR("model %s already defined", kind.c_str());
    all()[kind] = create;
}

} // namespace vcml
