/******************************************************************************
 *                                                                            *
 * Copyright (C) 2026 MachineWare GmbH                                        *
 * All Rights Reserved                                                        *
 *                                                                            *
 * This work is licensed under the terms described in the LICENSE file found  *
 * in the root directory of this source tree.                                 *
 *                                                                            *
 ******************************************************************************/

#ifndef VCML_META_GDBSERVER_H
#define VCML_META_GDBSERVER_H

#include "vcml/core/types.h"
#include "vcml/core/systemc.h"
#include "vcml/core/module.h"
#include "vcml/core/model.h"

#include "vcml/logging/logger.h"
#include "vcml/properties/property.h"
#include "vcml/debugging/gdbserver.h"

namespace vcml {
namespace meta {

class gdbserver : public module
{
private:
    vector<vcml::debugging::target*> m_targets;
    vcml::debugging::gdbserver* m_gdb;

public:
    property<int> port;
    property<string> host;
    property<bool> wait;
    property<bool> echo;

    const vector<vcml::debugging::target*>& targets() const {
        return m_targets;
    }

    gdbserver(const sc_module_name& name, vcml::debugging::target& target);
    gdbserver(const sc_module_name& name,
              const vector<vcml::debugging::target*>& targets =
                  vcml::debugging::target::all());
    virtual ~gdbserver();
    VCML_KIND(meta::gdbserver);

protected:
    virtual void end_of_elaboration() override;
};

inline gdbserver::gdbserver(const sc_module_name& nm,
                            vcml::debugging::target& target):
    gdbserver(nm, { &target }) {
}

} // namespace meta
} // namespace vcml

#endif
