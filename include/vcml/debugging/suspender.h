/******************************************************************************
 *                                                                            *
 * Copyright (C) 2022 MachineWare GmbH                                        *
 * All Rights Reserved                                                        *
 *                                                                            *
 * This is work is licensed under the terms described in the LICENSE file     *
 * found in the root directory of this source tree.                           *
 *                                                                            *
 ******************************************************************************/

#ifndef VCML_DEBUGGING_SUSPENDER_H
#define VCML_DEBUGGING_SUSPENDER_H

#include "vcml/core/types.h"
#include "vcml/core/systemc.h"
#include "vcml/core/thctl.h"

namespace vcml {
namespace debugging {

class suspender
{
private:
    atomic<int> m_pcount;
    string m_name;
    sc_object* m_owner;

public:
    const char* name() const { return m_name.c_str(); }
    sc_object* owner() const { return m_owner; }

    suspender() = delete;
    explicit suspender(const string& nm);
    virtual ~suspender();

    virtual bool check_suspension_point();

    bool is_suspending() const;

    void suspend(bool wait = true);
    void resume();

    static suspender* current();
    static void quit();
    static bool simulation_suspended();
    static void handle_requests();
};

} // namespace debugging
} // namespace vcml

#endif
