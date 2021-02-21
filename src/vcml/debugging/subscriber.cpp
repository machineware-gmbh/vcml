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

#include "vcml/debugging/subscriber.h"

namespace vcml { namespace debugging {

    breakpoint::breakpoint(u64 addr, const symbol* func):
        m_addr(addr),
        m_count(0),
        m_func(func),
        m_subscribers() {
    }

    void breakpoint::notify() {
        m_count++;

        for (subscriber* s : m_subscribers)
            s->notify(*this);
    }

    bool breakpoint::subscribe(subscriber* s) {
        if (stl_contains(m_subscribers, s))
            return false;

        m_subscribers.push_back(s);
        return true;
    }

    bool breakpoint::unsubscribe(subscriber* s) {
        if (!stl_contains(m_subscribers, s))
            return false;

        stl_remove_erase(m_subscribers, s);
        return true;
    }

}}
