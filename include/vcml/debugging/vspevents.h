/******************************************************************************
 *                                                                            *
 * Copyright (C) 2026 MachineWare GmbH                                        *
 * All Rights Reserved                                                        *
 *                                                                            *
 * This work is licensed under the terms described in the LICENSE file found  *
 * in the root directory of this source tree.                                 *
 *                                                                            *
 ******************************************************************************/

#ifndef VCML_DEBUGGING_VSPEVENTS_H
#define VCML_DEBUGGING_VSPEVENTS_H

#include "vcml/core/types.h"
#include "vcml/core/systemc.h"

namespace vcml {
namespace debugging {

constexpr const char VSP_EVENT_TRACE[] = "trace";
constexpr const char VSP_EVENT_LED[] = "led";
constexpr const char VSP_EVENT_UART[] = "uart";

class vspsubscriber
{
public:
    virtual ~vspsubscriber() = default;

    // called on the thread that published the event, so implementations
    // must be quick and must not (un)subscribe from within this callback
    virtual void on_event(const sc_object& sender, const string& event,
                          const sc_time& t, const string& payload) = 0;
};

class vsppublisher
{
private:
    mutable mutex m_mtx;
    atomic<bool> m_subscribed;
    vector<pair<string, vspsubscriber*>> m_subscriptions;

public:
    vsppublisher();
    virtual ~vsppublisher() = default;

    virtual vector<string> published_events() const = 0;
    bool publishes(const string& event) const;

    bool has_subscribers() const { return m_subscribed; }
    bool is_subscribed(const string& event, const vspsubscriber* s) const;
    vector<pair<string, vspsubscriber*>> subscriptions() const;

    bool subscribe_event(const string& event, vspsubscriber* s);
    void unsubscribe_event(const string& event, vspsubscriber* s);

    void publish_event(const sc_object& sender, const string& event,
                       const sc_time& t, const string& payload) const;
};

} // namespace debugging
} // namespace vcml

#endif
