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
        std::thread::id sysc_thread;
        std::thread::id curr_owner;

        std::mutex mutex;
        std::atomic<size_t> nwaiting;
        std::condition_variable_any notify;

        thctl();
        ~thctl() = default;

        bool is_sysc_thread() const;
        bool is_in_critical() const;

        void enter_critical();
        void exit_critical();

        void suspend();
    };

    thctl::thctl():
        sysc_thread(std::this_thread::get_id()),
        curr_owner(sysc_thread),
        mutex(),
        nwaiting(0),
        notify() {
        mutex.lock();
        on_each_delta_cycle(std::bind(&thctl::suspend, this));
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

        nwaiting++;
        mutex.lock();
        curr_owner = std::this_thread::get_id();
    }

    inline void thctl::exit_critical() {
        if (curr_owner != std::this_thread::get_id())
            VCML_ERROR("thread not in critical section");

        curr_owner = std::thread::id();
        mutex.unlock();

        if (--nwaiting == 0)
            notify.notify_all();
    }

    void thctl::suspend() {
        VCML_ERROR_ON(!is_sysc_thread(), "this is not the SystemC thread");
        VCML_ERROR_ON(!is_in_critical(), "thread not in critical section");

        if (nwaiting == 0)
            return;

        notify.wait(mutex, [&]() -> bool {
            return nwaiting == 0;
        });

        curr_owner = sysc_thread;
    }

    thctl g_thctl;

    bool thctl_is_sysc_thread() {
        return g_thctl.is_sysc_thread();
    }

    bool thctl_is_in_critical() {
        return g_thctl.is_in_critical();
    }

    void thctl_enter_critical() {
        if (sc_is_running())
            g_thctl.enter_critical();
    }

    void thctl_exit_critical() {
        if (sc_is_running())
            g_thctl.exit_critical();
    }

    void thctl_suspend() {
        g_thctl.suspend();
    }

}
