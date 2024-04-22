/******************************************************************************
 *                                                                            *
 * Copyright (C) 2022 MachineWare GmbH                                        *
 * All Rights Reserved                                                        *
 *                                                                            *
 * This is work is licensed under the terms described in the LICENSE file     *
 * found in the root directory of this source tree.                           *
 *                                                                            *
 ******************************************************************************/

#include "vcml/debugging/subscriber.h"

namespace vcml {
namespace debugging {

void subscriber::notify_step_complete(target& tgt) {
    // to be overloaded
}

void subscriber::notify_basic_block(target& tgt, u64 pc, size_t blksz,
                                    size_t icount) {
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

    stl_remove(m_subscribers, s);
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
        stl_remove(m_subscribers_r, s);
        subscriptions--;
    }

    if (is_write_allowed(prot) && stl_contains(m_subscribers_w, s)) {
        stl_remove(m_subscribers_w, s);
        subscriptions--;
    }

    return subscriptions > 0;
}

} // namespace debugging
} // namespace vcml
