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

#include "vcml/common/types.h"
#include "vcml/common/strings.h"

namespace vcml { namespace debugging {

    class suspender
    {
    private:
        string m_name;
        atomic<bool> m_suspending;
        sc_object* m_owner;

        void handle_suspend_request();

        static vector<suspender*> suspenders;

        static void notify_suspend(sc_object* obj = nullptr);
        static void notify_resume(sc_object* obj = nullptr);

    public:
        const char* name()   const { return m_name.c_str(); }
        bool is_suspending() const { return m_suspending; }
        sc_object* owner()   const { return m_owner; }

        suspender() = delete;
        explicit suspender(const string& nm);
        virtual ~suspender();

        virtual bool is_suspend_requested() const = 0;

        void wait_for_suspend();
        void resume();

        static void handle_requests();
        static size_t count_requests();
        static bool simulation_suspended();
    };

}}

#endif
