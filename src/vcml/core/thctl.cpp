/******************************************************************************
 *                                                                            *
 * Copyright (C) 2022 MachineWare GmbH                                        *
 * All Rights Reserved                                                        *
 *                                                                            *
 * This is work is licensed under the terms described in the LICENSE file     *
 * found in the root directory of this source tree.                           *
 *                                                                            *
 ******************************************************************************/

#include "vcml/core/thctl.h"
#include "vcml/core/systemc.h"

namespace vcml {

struct thctl {
    thread::id sysc_thread;
    atomic<thread::id> curr_owner;

    mutex sysc_mutex;
    atomic<int> nwaiting;
    condition_variable_any cvar;

    thctl();
    ~thctl();

    bool is_sysc_thread() const;
    bool is_in_critical() const;

    void notify();
    void block();

    void enter_critical();
    void exit_critical();

    void suspend();
    void flush();

    void set_sysc_thread(thread::id id);

    static thctl& instance();
};

thctl::thctl():
    sysc_thread(std::this_thread::get_id()),
    curr_owner(sysc_thread),
    sysc_mutex(),
    nwaiting(0),
    cvar() {
    sysc_mutex.lock();
}

thctl::~thctl() {
    sysc_mutex.unlock();
    notify();
}

bool thctl::is_sysc_thread() const {
    return std::this_thread::get_id() == sysc_thread;
}

bool thctl::is_in_critical() const {
    return std::this_thread::get_id() == curr_owner;
}

void thctl::notify() {
    cvar.notify_all();
}

void thctl::block() {
    VCML_ERROR_ON(is_sysc_thread(), "cannot block SystemC thread");
    lock_guard<mutex> l(sysc_mutex);
}

void thctl::enter_critical() {
    if (is_sysc_thread())
        VCML_ERROR("SystemC thread must not enter critical sections");
    if (is_in_critical())
        VCML_ERROR("thread already in critical section");
    if (!sim_running())
        return;

    int prev = nwaiting++;
    if (!sysc_mutex.try_lock()) {
        if (prev == 0)
            on_next_update([]() -> void { thctl_suspend(); });
        sysc_mutex.lock();
    }

    curr_owner = std::this_thread::get_id();
}

void thctl::exit_critical() {
    if (curr_owner != std::this_thread::get_id())
        VCML_ERROR("thread not in critical section");
    if (nwaiting <= 0)
        VCML_ERROR("no thread in critical section");

    if (!sim_running())
        return;

    curr_owner = thread::id();
    sysc_mutex.unlock();

    if (--nwaiting == 0)
        notify();
}

void thctl::suspend() {
    VCML_ERROR_ON(!is_sysc_thread(), "this is not the SystemC thread");
    VCML_ERROR_ON(!is_in_critical(), "thread not in critical section");

    do {
        cvar.wait(sysc_mutex);
    } while (nwaiting > 0);

    curr_owner = sysc_thread;
}

void thctl::flush() {
    if (nwaiting > 0)
        suspend();
}

void thctl::set_sysc_thread(thread::id id) {
    sysc_thread = id;
}

thctl& thctl::instance() {
    static thctl singleton;
    return singleton;
}

// need to make sure thctl gets created on the main (aka SystemC) thread
thctl& g_thctl = thctl::instance();

bool thctl_is_sysc_thread() {
    return thctl::instance().is_sysc_thread();
}

bool thctl_is_in_critical() {
    return thctl::instance().is_in_critical();
}

void thctl_notify() {
    thctl::instance().notify();
}

void thctl_block() {
    thctl::instance().block();
}

void thctl_enter_critical() {
    thctl::instance().enter_critical();
}

void thctl_exit_critical() {
    thctl::instance().exit_critical();
}

void thctl_suspend() {
    thctl::instance().suspend();
}

void thctl_flush() {
    thctl::instance().flush();
}

void thctl_set_sysc_thread(thread::id id) {
    thctl::instance().set_sysc_thread(id);
}

} // namespace vcml
