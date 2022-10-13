/******************************************************************************
 *                                                                            *
 * Copyright 2020 Jan Henrik Weinstock                                        *
 *                                                                            *
 * Licensed under the Apache License, Version 2.0 (the "License");            *
 * you may not use this file except in compliance with the License.           *
 * You may obtain a copy of the License at                                    *
 *                                                                            *
 *     http://www.apache.org/licenses/LICENSE-2.0                             *
 *                                                                            *
 * Unless required by applicable law or agreed to in writing, software        *
 * distributed under the License is distributed on an "AS IS" BASIS,          *
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.   *
 * See the License for the specific language governing permissions and        *
 * limitations under the License.                                             *
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
