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

using model = shared_ptr<module>;
typedef module* (*model_create_fn)(const sc_module_name&,
                                   const vector<string>&);

class modeldb
{
private:
    static unordered_map<string, model_create_fn>& all();

public:
    static model create(const string& kind, const sc_module_name& name);
    static void register_model(const string& kind, model_create_fn fn);
};

} // namespace vcml

#define VCML_EXPORT_MODEL(kind, name, args)                                 \
    static vcml::module* MWR_CAT(create_model_, __LINE__)(                  \
        const sc_core::sc_module_name& name,                                \
        const std::vector<std::string>& args);                              \
    static MWR_DECL_CONSTRUCTOR void MWR_CAT(register_model_, __LINE__)() { \
        modeldb::register_model(#kind, MWR_CAT(create_model_, __LINE__));   \
    }                                                                       \
    static vcml::module* MWR_CAT(create_model_, __LINE__)(                  \
        const sc_core::sc_module_name& name,                                \
        const std::vector<std::string>& args)

#endif
