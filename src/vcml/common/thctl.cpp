/******************************************************************************
 *                                                                            *
 * Copyright 2018 Jan Henrik Weinstock                                        *
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

#include "vcml/common/thctl.h"
#include "vcml/common/report.h"
#include "vcml/common/systemc.h"

namespace vcml {

struct thctl {
    thread::id sysc_thread;
    thread::id curr_owner;

    mutex mtx;
    atomic<size_t> nwaiting;
    condition_variable_any notify;

    thctl();
    ~thctl() = default;

    bool is_sysc_thread() const;
    bool is_in_critical() const;

    void enter_critical();
    void exit_critical();

    void suspend();

    static thctl& instance();
};

thctl::thctl():
    sysc_thread(std::this_thread::get_id()),
    curr_owner(sysc_thread),
    mtx(),
    nwaiting(0),
    notify() {
    mtx.lock();
}

inline bool thctl::is_sysc_thread() const {
    return std::this_thread::get_id() == sysc_thread;
}

inline bool thctl::is_in_critical() const {
    return std::this_thread::get_id() == curr_owner;
}

inline void thctl::enter_critical() {
    if (is_sysc_thread())
        VCML_ERROR("SystemC thread must not enter critical sections");
    if (is_in_critical())
        VCML_ERROR("thread already in critical section");
    if (!sim_running())
        return;

    if (nwaiting++ == 0)
        on_next_update([]() -> void { thctl_suspend(); });

    mtx.lock();
    curr_owner = std::this_thread::get_id();
}

inline void thctl::exit_critical() {
    if (curr_owner != std::this_thread::get_id())
        VCML_ERROR("thread not in critical section");

    if (!sim_running())
        return;

    curr_owner = thread::id();
    mtx.unlock();

    if (--nwaiting == 0)
        notify.notify_all();
}

void thctl::suspend() {
    VCML_ERROR_ON(!is_sysc_thread(), "this is not the SystemC thread");
    VCML_ERROR_ON(!is_in_critical(), "thread not in critical section");

    if (nwaiting == 0)
        return;

    notify.wait(mtx, [&]() -> bool { return nwaiting == 0; });

    curr_owner = sysc_thread;
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

void thctl_enter_critical() {
    thctl::instance().enter_critical();
}

void thctl_exit_critical() {
    thctl::instance().exit_critical();
}

void thctl_suspend() {
    thctl::instance().suspend();
}

} // namespace vcml
