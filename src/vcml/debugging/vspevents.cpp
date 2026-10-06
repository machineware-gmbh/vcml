/******************************************************************************
 *                                                                            *
 * Copyright (C) 2026 MachineWare GmbH                                        *
 * All Rights Reserved                                                        *
 *                                                                            *
 * This work is licensed under the terms described in the LICENSE file found  *
 * in the root directory of this source tree.                                 *
 *                                                                            *
 ******************************************************************************/

#include "vcml/debugging/vspevents.h"

namespace vcml {
namespace debugging {

vsppublisher::vsppublisher(): m_mtx(), m_subscribed(false), m_subscriptions() {
    // nothing to do
}

bool vsppublisher::publishes(const string& event) const {
    return stl_contains(published_events(), event);
}

bool vsppublisher::is_subscribed(const string& event,
                                 const vspsubscriber* s) const {
    lock_guard<mutex> guard(m_mtx);
    for (const auto& [ev, sub] : m_subscriptions) {
        if (ev == event && sub == s)
            return true;
    }

    return false;
}

vector<pair<string, vspsubscriber*>> vsppublisher::subscriptions() const {
    lock_guard<mutex> guard(m_mtx);
    return m_subscriptions;
}

bool vsppublisher::subscribe_event(const string& event, vspsubscriber* s) {
    VCML_ERROR_ON(!s, "attempt to subscribe invalid subscriber");
    if (!publishes(event))
        return false;

    lock_guard<mutex> guard(m_mtx);
    auto sub = std::make_pair(event, s);
    if (!stl_contains(m_subscriptions, sub)) {
        m_subscriptions.push_back(sub);
        m_subscribed = true;
        on_subscribe(event, s);
    }

    return true;
}

void vsppublisher::unsubscribe_event(const string& event, vspsubscriber* s) {
    lock_guard<mutex> guard(m_mtx);
    stl_remove(m_subscriptions, std::make_pair(event, s));
    m_subscribed = !m_subscriptions.empty();
}

void vsppublisher::publish_event(const sc_object& sender, const string& event,
                                 const sc_time& t,
                                 const string& payload) const {
    // holding the lock guarantees that subscribers are not destroyed while
    // we notify them, see unsubscribe_event
    lock_guard<mutex> guard(m_mtx);
    for (const auto& [ev, sub] : m_subscriptions) {
        if (ev == event)
            sub->on_event(sender, event, t, payload);
    }
}

} // namespace debugging
} // namespace vcml
