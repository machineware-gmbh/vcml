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

namespace vcml { namespace debugging {

    vector<suspender*> suspender::suspenders;

    void suspender::notify_suspend(sc_object* obj) {
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

    void suspender::notify_resume(sc_object* obj) {
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

    void suspender::handle_suspend_request() {
        if (!is_suspend_requested())
            return;

        m_suspending = true;
        notify_suspend();

        do {
            thctl_suspend();
        } while (is_suspend_requested());

        notify_resume();
        m_suspending = false;
    }

    suspender::suspender(const string& name):
        m_name(name),
        m_suspending(false),
        m_owner(sc_get_curr_simcontext()->active_object()) {
        if (m_owner != nullptr)
            m_name = m_owner->name() + SC_HIERARCHY_CHAR + m_name;

        if (suspenders.empty())
            on_each_delta_cycle(&suspender::handle_requests);

        suspenders.push_back(this);
    }

    suspender::~suspender() {
        stl_remove_erase(suspenders, this);
    }

    void suspender::wait_for_suspend() {
        VCML_ERROR_ON(thctl_is_sysc_thread(), "cannot block main thread");
        VCML_ERROR_ON(!is_suspend_requested(), "no suspend requested");

        while (!simulation_suspended());
            // spin
    }

    void suspender::resume() {
        //VCML_ERROR_ON(!is_suspending(), "illegal attempt to resume");
        thctl_resume();
    }

    void suspender::handle_requests() {
        for (suspender* s : suspenders)
            s->handle_suspend_request();
    }

    size_t suspender::count_requests() {
        size_t count = 0;
        for (suspender* s : suspenders)
            if (s->is_suspend_requested())
                count++;
        return count;
    }

    bool suspender::simulation_suspended() {
        for (suspender* s : suspenders)
            if (s->is_suspending())
                return true;
        return false;
    }

}}

