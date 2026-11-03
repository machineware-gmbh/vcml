/******************************************************************************
 *                                                                            *
 * Copyright (C) 2023 MachineWare GmbH                                        *
 * All Rights Reserved                                                        *
 *                                                                            *
 * This work is licensed under the terms described in the LICENSE file found  *
 * in the root directory of this source tree.                                 *
 *                                                                            *
 ******************************************************************************/

#ifndef VCML_MODEL_H
#define VCML_MODEL_H

#include "vcml/core/types.h"
#include "vcml/core/systemc.h"
#include "vcml/core/module.h"

#include "vcml/properties/property.h"

#include "vcml/protocols/base.h"

namespace vcml {

class model
{
public:
    typedef module* (*create_fn)(const sc_module_name&, const vector<string>&);

    model(const sc_module_name& name, const string& kind);
    virtual ~model() = default;

    const char* name() const { return m_impl->name(); }
    std::map<string, string> sockets() const { return list_sockets(*m_impl); }

    model(const model& other) = default;
    model(model&& other) = default;

    operator module&() const { return *m_impl; }
    shared_ptr<module> operator->() const { return m_impl; }

    property_base* find_property(const string& name) const;

    template <typename T, size_t N = 1>
    const property<T, N>* find_property(const string& name) const;

    template <typename T, size_t N = 1>
    T get_property(const string& name, size_t idx = 0) const;

    template <typename T, size_t N = 1>
    T get_property_or_default(const string& name, const T& defval,
                              size_t idx = 0) const;

    static bool define(const string& kind, create_fn fn);
    static void list_models(vector<string>& models);
    static void list_models(ostream& os);

private:
    shared_ptr<module> m_impl;

    static module* create(const string& kind, const sc_module_name& name);
    static std::map<string, create_fn>& modeldb();
};

template <typename T, size_t N>
const property<T, N>* model::find_property(const string& name) const {
    property_base* prop = find_property(name);
    if (prop == nullptr)
        return nullptr;

    auto* typed = dynamic_cast<const property<T, N>*>(prop);
    VCML_REPORT_ON(!typed, "property %s has type %s[%zu], requested %s[%zu]",
                   prop->fullname(), prop->type(), prop->count(),
                   type_name<T>(), N);
    return typed;
}

template <typename T, size_t N>
T model::get_property(const string& name, size_t idx) const {
    const property<T, N>* prop = find_property<T, N>(name);
    VCML_REPORT_ON(!prop, "%s has no property '%s'", this->name(),
                   name.c_str());
    VCML_REPORT_ON(idx >= N, "index %zu out of bounds for property %s[%zu]",
                   idx, prop->fullname(), N);
    if constexpr (N == 1)
        return prop->get();
    else
        return prop->get(idx);
}

template <typename T, size_t N>
T model::get_property_or_default(const string& name, const T& defval,
                                 size_t idx) const {
    if (find_property<T, N>(name) == nullptr)
        return defval;
    return get_property<T, N>(name, idx);
}

} // namespace vcml

#define VCML_EXPORT_MODEL(type, name, args)                                \
    static vcml::module* MWR_CAT(create_model_, __LINE__)(                 \
        const sc_core::sc_module_name& name,                               \
        const std::vector<std::string>& args);                             \
    MWR_CONSTRUCTOR(MWR_CAT(register_model_, __LINE__)) {                  \
        if (!vcml::model::define(#type, MWR_CAT(create_model_, __LINE__))) \
            VCML_ERROR("model '%s' already defined", #type);               \
    }                                                                      \
    static vcml::module* MWR_CAT(create_model_, __LINE__)(                 \
        const sc_core::sc_module_name& name,                               \
        const std::vector<std::string>& args)

#endif
