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

    VCML_REPORT("model not found: %s", kind.c_str());
}

bool model::define(const string& kind, create_fn create) {
    auto it = modeldb().find(kind);
    if (it != modeldb().end())
        return false;

    modeldb()[kind] = create;
    return true;
}

void model::list_models(vector<string>& models) {
    models.reserve(modeldb().size());
    for (auto& [name, func] : modeldb())
        models.push_back(name);
}

void model::list_models(ostream& os) {
    vector<string> models;
    list_models(models);
    for (const string& name : models)
        os << name << std::endl;
}

std::map<string, model::create_fn>& model::modeldb() {
    static std::map<string, create_fn> db;
    return db;
}

class empty : public module
{
public:
    empty(const sc_module_name& nm): module(nm) {}
    virtual ~empty() = default;
    virtual const char* kind() const override { return "empty"; }
};

VCML_EXPORT_MODEL(empty, name, args) {
    return new empty(name);
}

} // namespace vcml
