/******************************************************************************
 *                                                                            *
 * Copyright (C) 2023 MachineWare GmbH                                        *
 * All Rights Reserved                                                        *
 *                                                                            *
 * This is work is licensed under the terms described in the LICENSE file     *
 * found in the root directory of this source tree.                           *
 *                                                                            *
 ******************************************************************************/

#ifndef VCML_MODEL_H
#define VCML_MODEL_H

#include "vcml/core/types.h"
#include "vcml/core/systemc.h"
#include "vcml/core/module.h"

namespace vcml {

class model
{
public:
    typedef module* (*create_fn)(const sc_module_name&, const vector<string>&);

    model(const sc_module_name& name, const string& kind);
    virtual ~model() = default;

    model(const model& other) = default;
    model(model&& other) = default;

    operator module&() const { return *m_impl; }
    shared_ptr<module> operator->() const { return m_impl; }

    static bool define(const string& kind, create_fn fn);
    static void list_models(ostream& os);

private:
    shared_ptr<module> m_impl;

    static module* create(const string& kind, const sc_module_name& name);
    static std::map<string, create_fn>& modeldb();
};

} // namespace vcml

#define VCML_EXPORT_MODEL(type, name, args)                                 \
    static vcml::module* MWR_CAT(create_model_, __LINE__)(                  \
        const sc_core::sc_module_name& name,                                \
        const std::vector<std::string>& args);                              \
    static MWR_DECL_CONSTRUCTOR void MWR_CAT(register_model_, __LINE__)() { \
        if (!vcml::model::define(#type, MWR_CAT(create_model_, __LINE__)))  \
            VCML_ERROR("model '%s' already defined", #type);                \
    }                                                                       \
    static vcml::module* MWR_CAT(create_model_, __LINE__)(                  \
        const sc_core::sc_module_name& name,                                \
        const std::vector<std::string>& args)

#endif
