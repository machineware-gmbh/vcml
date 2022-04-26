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

#include "vcml/common/systemc.h"
#include "vcml/common/thctl.h"
#include "vcml/common/report.h"
#include "vcml/module.h"

#include "vcml/debugging/suspender.h"

namespace vcml {
namespace debugging {

struct suspend_manager {
    atomic<bool> is_quitting;
    atomic<bool> is_suspended;

    mutable mutex suspender_lock;
    vector<suspender*> suspenders;

    void request_pause(suspender* s);
    void request_resume(suspender* s);

    bool is_suspending(const suspender* s) const;

    size_t count() const;

    suspender* current() const;

    void quit();

    void notify_suspend(sc_object* obj = nullptr);
    void notify_resume(sc_object* obj = nullptr);

    void handle_requests();

    suspend_manager();

    static suspend_manager& instance();
};

suspend_manager& g_manager = suspend_manager::instance();

void suspend_manager::request_pause(suspender* s) {
    if (is_quitting)
        return;
    if (!sim_running())
        VCML_ERROR("cannot suspend, simulation not running");
    lock_guard<mutex> guard(suspender_lock);
    if (suspenders.empty())
        on_next_update([&]() -> void { handle_requests(); });
    stl_add_unique(suspenders, s);
}

void suspend_manager::request_resume(suspender* s) {
    lock_guard<mutex> guard(suspender_lock);
    stl_remove_erase(suspenders, s);
    if (suspenders.empty())
        thctl_notify();
}

bool suspend_manager::is_suspending(const suspender* s) const {
    lock_guard<mutex> guard(suspender_lock);
    return stl_contains(suspenders, s);
}

size_t suspend_manager::count() const {
    lock_guard<mutex> guard(suspender_lock);
    return suspenders.size();
}

suspender* suspend_manager::current() const {
    lock_guard<mutex> guard(suspender_lock);
    if (suspenders.empty())
        return nullptr;
    return suspenders.front();
}

void suspend_manager::quit() {
    lock_guard<mutex> guard(suspender_lock);
    if (!is_quitting)
        on_next_update([]() -> void { sc_stop(); });

    is_quitting = true;
    suspenders.clear();
    thctl_notify();
}

void suspend_manager::notify_suspend(sc_object* obj) {
    const auto& children = obj ? obj->get_child_objects()
                               : sc_core::sc_get_top_level_objects();
    for (auto child : children)
        notify_suspend(child);

    if (obj == nullptr)
        return;

    module* mod = dynamic_cast<module*>(obj);
    if (mod != nullptr)
        mod->session_suspend();
}

void suspend_manager::notify_resume(sc_object* obj) {
    const auto& children = obj ? obj->get_child_objects()
                               : sc_core::sc_get_top_level_objects();
    for (auto child : children)
        notify_resume(child);

    if (obj == nullptr)
        return;

    module* mod = dynamic_cast<module*>(obj);
    if (mod != nullptr)
        mod->session_resume();
}

void suspend_manager::handle_requests() {
    if (is_quitting || count() == 0)
        return;

    is_suspended = true;
    notify_suspend();

    thctl_yield();

    notify_resume();
    is_suspended = false;
}

suspend_manager::suspend_manager():
    is_quitting(false), is_suspended(false), suspender_lock(), suspenders() {
}

suspend_manager& suspend_manager::instance() {
    static suspend_manager singleton;
    return singleton;
}

suspender::suspender(const string& name):
    m_pcount(), m_name(name), m_owner(hierarchy_top()) {
    if (m_owner != nullptr)
        m_name = mkstr("%s%c", m_owner->name(), SC_HIERARCHY_CHAR) + name;
}

suspender::~suspender() {
    if (is_suspending())
        resume();
}

bool suspender::is_suspending() const {
    return suspend_manager::instance().is_suspending(this);
}

void suspender::suspend(bool wait) {
    suspend_manager& manager = suspend_manager::instance();

    if (m_pcount++ == 0)
        manager.request_pause(this);

    if (wait && !thctl_is_sysc_thread())
        thctl_block();
}

void suspender::resume() {
    if (--m_pcount == 0)
        suspend_manager::instance().request_resume(this);
    VCML_ERROR_ON(m_pcount < 0, "unmatched resume");
}

suspender* suspender::current() {
    return suspend_manager::instance().current();
}

void suspender::quit() {
    suspend_manager::instance().quit();
}

bool suspender::simulation_suspended() {
    return suspend_manager::instance().is_suspended;
}

void suspender::handle_requests() {
    suspend_manager::instance().handle_requests();
}

} // namespace debugging
} // namespace vcml
