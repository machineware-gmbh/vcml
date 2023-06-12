/******************************************************************************
 *                                                                            *
 * Copyright (C) 2022 MachineWare GmbH                                        *
 * All Rights Reserved                                                        *
 *                                                                            *
 * This is work is licensed under the terms described in the LICENSE file     *
 * found in the root directory of this source tree.                           *
 *                                                                            *
 ******************************************************************************/

#ifndef VCML_SYSTEM_H
#define VCML_SYSTEM_H

#include "vcml/core/types.h"
#include "vcml/core/module.h"
#include "vcml/core/register.h"

#include "vcml/debugging/vspserver.h"

namespace vcml {

class system : public module
{
private:
    void timeout();

public:
    property<string> name;
    property<string> desc;
    property<string> config;

    property<bool> backtrace;

    property<int> session;
    property<bool> session_debug;

    property<sc_time> quantum;
    property<sc_time> duration;

    system() = delete;
    system(const system&) = delete;
    explicit system(const sc_module_name& name);
    virtual ~system();
    VCML_KIND(system);

    virtual int run();
};

} // namespace vcml

#endif
