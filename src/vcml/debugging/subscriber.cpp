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

    void subscriber::notify_step_complete(target& tgt) {
        // to be overloaded
    }

    void subscriber::notify_breakpoint_hit(const breakpoint& bp) {
        // to be overloaded
    }

    void subscriber::notify_watchpoint_read(const watchpoint& wp,
                                            const range& addr) {
        // to be overloaded
    }

    void subscriber::notify_watchpoint_write(const watchpoint& wp,
                                             const range& addr, u64 newval) {
        // to be overloaded
    }

    static u64 g_next_id = 0;

    breakpoint::breakpoint(target& tgt, u64 addr, const symbol* func):
        m_target(tgt),
        m_id(g_next_id++),
        m_addr(addr),
        m_count(0),
        m_func(func),
        m_subscribers() {
    }

    void breakpoint::notify() {
        m_count++;

        for (subscriber* s : m_subscribers)
            s->notify_breakpoint_hit(*this);
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

    watchpoint::watchpoint(target& tgt, const range& addr, const symbol* obj):
        m_target(tgt),
        m_id(g_next_id++),
        m_addr(addr),
        m_count(0),
        m_obj(obj),
        m_subscribers_r(),
        m_subscribers_w() {
    }

    void watchpoint::notify_read(const range& addr) {
        m_count++;

        for (subscriber* s : m_subscribers_r)
            s->notify_watchpoint_read(*this, addr);
    }

    void watchpoint::notify_write(const range& addr, u64 newval) {
        m_count++;

        for (subscriber* s : m_subscribers_w)
            s->notify_watchpoint_write(*this, addr, newval);
    }

    bool watchpoint::subscribe(vcml_access prot, subscriber* s) {
        size_t subscriptions = 0;

        if (is_read_allowed(prot) && !stl_contains(m_subscribers_r, s)) {
            m_subscribers_r.push_back(s);
            subscriptions++;
        }

        if (is_write_allowed(prot) && !stl_contains(m_subscribers_w, s)) {
            m_subscribers_w.push_back(s);
            subscriptions++;
        }

        return subscriptions > 0;
    }

    bool watchpoint::unsubscribe(vcml_access prot, subscriber* s) {
        size_t subscriptions = 0;

        if (is_read_allowed(prot) && stl_contains(m_subscribers_r, s)) {
            stl_remove_erase(m_subscribers_r, s);
            subscriptions--;
        }

        if (is_write_allowed(prot) && stl_contains(m_subscribers_w, s)) {
            stl_remove_erase(m_subscribers_w, s);
            subscriptions--;
        }


        return subscriptions > 0;
    }

}}
