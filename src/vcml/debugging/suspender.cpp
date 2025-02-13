/******************************************************************************
 *                                                                            *
 * Copyright (C) 2022 MachineWare GmbH                                        *
 * All Rights Reserved                                                        *
 *                                                                            *
 * This is work is licensed under the terms described in the LICENSE file     *
 * found in the root directory of this source tree.                           *
 *                                                                            *
 ******************************************************************************/

#include "vcml/core/systemc.h"
#include "vcml/core/thctl.h"
#include "vcml/core/module.h"

#include "vcml/debugging/suspender.h"

namespace vcml {
namespace debugging {

struct suspend_manager {
    atomic<bool> is_quitting;
    atomic<bool> is_suspended;

    mutable mutex suspender_lock;
    std::condition_variable_any cv;

    vector<suspender*> waiting_suspenders;
    vector<suspender*> active_suspenders;

    void request_pause(suspender* s);
    void request_resume(suspender* s);

    bool is_suspending(const suspender* s) const;

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

    {
        std::unique_lock<mutex> lock(suspender_lock);
        if (waiting_suspenders.empty())
            on_next_update([&]() { handle_requests(); });
        stl_add_unique(waiting_suspenders, s);

        if (thctl_is_sysc_thread())
            return;

        while (!stl_contains(active_suspenders, s))
            cv.wait(lock);
    }

    thctl_block();
}

void suspend_manager::request_resume(suspender* s) {
    lock_guard<mutex> guard(suspender_lock);
    stl_remove(waiting_suspenders, s);
    if (!stl_contains(active_suspenders, s))
        return;

    stl_remove(active_suspenders, s);
    if (active_suspenders.empty())
        thctl_notify();
}

bool suspend_manager::is_suspending(const suspender* s) const {
    lock_guard<mutex> guard(suspender_lock);
    return stl_contains(active_suspenders, s);
}

suspender* suspend_manager::current() const {
    lock_guard<mutex> guard(suspender_lock);
    if (active_suspenders.empty())
        return nullptr;
    return active_suspenders.front();
}

void suspend_manager::quit() {
    lock_guard<mutex> guard(suspender_lock);
    if (!is_quitting)
        on_next_update(request_stop);

    is_quitting = true;
    waiting_suspenders.clear();
    active_suspenders.clear();
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
    if (is_quitting)
        return;

    suspender_lock.lock();

    vector<suspender*> suspenders;
    std::swap(suspenders, waiting_suspenders);
    for (suspender* s : suspenders) {
        if (s->check_suspension_point())
            active_suspenders.push_back(s);
        else
            waiting_suspenders.push_back(s);
    }

    if (!active_suspenders.empty()) {
        is_suspended = true;
        suspender_lock.unlock();
        notify_suspend();
        suspender_lock.lock();

        cv.notify_all();

        while (!active_suspenders.empty()) {
            suspender_lock.unlock();
            thctl_suspend();
            suspender_lock.lock();
        }

        suspender_lock.unlock();
        notify_resume();
        suspender_lock.lock();
        is_suspended = false;
    }

    suspender_lock.unlock();
}

suspend_manager::suspend_manager():
    is_quitting(false),
    is_suspended(false),
    suspender_lock(),
    waiting_suspenders(),
    active_suspenders() {
}

suspend_manager& suspend_manager::instance() {
    static suspend_manager singleton;
    return singleton;
}

suspender::suspender(const string& name):
    m_name(name), m_owner(hierarchy_top()) {
    if (m_owner != nullptr)
        m_name = mkstr("%s%c", m_owner->name(), SC_HIERARCHY_CHAR) + name;
}

suspender::~suspender() {
    if (is_suspending())
        resume();
}

bool suspender::check_suspension_point() {
    return true;
}

bool suspender::is_suspending() const {
    return suspend_manager::instance().is_suspending(this);
}

void suspender::suspend() {
    suspend_manager::instance().request_pause(this);
}

void suspender::resume() {
    suspend_manager::instance().request_resume(this);
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
