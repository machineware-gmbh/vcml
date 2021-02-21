/******************************************************************************
 *                                                                            *
 * Copyright 2021 Jan Henrik Weinstock                                        *
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

#ifndef VCML_DEBUGGING_SUBSCRIBER_H
#define VCML_DEBUGGING_SUBSCRIBER_H

#include "vcml/common/types.h"
#include "vcml/common/report.h"
#include "vcml/common/range.h"

#include "vcml/debugging/symtab.h"

namespace vcml { namespace debugging {

    class breakpoint;

    class subscriber
    {
    public:
        virtual ~subscriber() {}

        virtual void notify(const breakpoint& bp) = 0;
    };

    class breakpoint
    {
    private:
        u64 m_addr;
        u64 m_count;
        const symbol* m_func;
        vector<subscriber*> m_subscribers;

    public:
        u64 address() const { return m_addr; }
        u64 hit_count() const { return m_count; }

        const symbol* function() const { return m_func; }

        bool has_subscriber() const { return !m_subscribers.empty(); }

        breakpoint(u64 addr, const symbol* func);
        breakpoint(const breakpoint&) = default;
        virtual ~breakpoint() = default;

        void notify();

        bool subscribe(subscriber* s);
        bool unsubscribe(subscriber* s);
    };

}}

#endif
