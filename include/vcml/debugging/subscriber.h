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

namespace vcml {
namespace debugging {

class target;

class breakpoint;
class watchpoint;

class subscriber
{
public:
    virtual ~subscriber() {}

    virtual void notify_step_complete(target& tgt);

    virtual void notify_breakpoint_hit(const breakpoint& bp);

    virtual void notify_watchpoint_read(const watchpoint& wp,
                                        const range& addr);

    virtual void notify_watchpoint_write(const watchpoint& wp,
                                         const range& addr, u64 newval);
};

class breakpoint
{
private:
    target& m_target;
    u64 m_id;
    u64 m_addr;
    u64 m_count;
    const symbol* m_func;
    vector<subscriber*> m_subscribers;

public:
    target& owner() const { return m_target; }

    u64 id() const { return m_id; }
    u64 address() const { return m_addr; }
    u64 hit_count() const { return m_count; }
    const symbol* function() const { return m_func; }

    bool has_subscribers() const { return !m_subscribers.empty(); }

    breakpoint(target& tgt, u64 addr, const symbol* func);
    virtual ~breakpoint() = default;

    breakpoint(const breakpoint&) = delete;
    breakpoint& operator=(const breakpoint&) = delete;

    void notify();

    bool subscribe(subscriber* s);
    bool unsubscribe(subscriber* s);
};

class watchpoint
{
private:
    target& m_target;
    u64 m_id;
    range m_addr;
    u64 m_count;
    const symbol* m_obj;

    vector<subscriber*> m_subscribers_r;
    vector<subscriber*> m_subscribers_w;

public:
    target& owner() { return m_target; }
    u64 id() const { return m_id; }
    u64 hit_count() const { return m_count; }
    const range& address() const { return m_addr; }
    const symbol* object() const { return m_obj; }

    bool has_read_subscribers() const { return !m_subscribers_r.empty(); }
    bool has_write_subscribers() const { return !m_subscribers_w.empty(); }
    bool has_any_subscribers() const;

    watchpoint(target& tgt, const range& addr, const symbol* obj);
    virtual ~watchpoint() = default;

    watchpoint(const watchpoint&) = delete;
    watchpoint& operator=(const watchpoint&) = delete;

    void notify_read(const range& addr);
    void notify_write(const range& addr, u64 newval);

    bool subscribe(vcml_access prot, subscriber* s);
    bool unsubscribe(vcml_access prot, subscriber* s);
};

inline bool watchpoint::has_any_subscribers() const {
    return has_read_subscribers() || has_write_subscribers();
}

} // namespace debugging
} // namespace vcml

#endif
