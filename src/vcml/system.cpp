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

#include "vcml/system.h"

namespace vcml {

    SC_HAS_PROCESS(system);

    void system::timeout() {
        VCML_ERROR_ON(duration == SC_ZERO_TIME, "timeout with zero duration");
        while (true) {
            wait(duration);
            sc_stop();
        }
    }

    system::system(const sc_module_name& nm):
        module(nm),
        name("name", progname()),
        desc("desc", progname()),
        config("config", ""),
        backtrace("backtrace", true),
        session("session", 0),
        session_debug("session_debug", false),
        quantum("quantum", sc_time(1, SC_US)),
        duration("duration", SC_ZERO_TIME) {

        if (backtrace)
            report::report_segfaults();

        if (duration > SC_ZERO_TIME)
            SC_THREAD(timeout);

        if (config.get().empty())
            log_warn("no configuration specified, use -f <config>");
    }

    system::~system() {
        // nothing to do
    }

    int system::run() {
        tlm::tlm_global_quantum::instance().set(quantum);
        if (session > 0) {
            vcml::debugging::vspserver vspsession(session);
            vspsession.echo(session_debug);
            vspsession.start();
        } else if (duration != sc_core::SC_ZERO_TIME) {
            log_info("starting simulation until %s using %s quantum",
                     duration.get().to_string().c_str(),
                     quantum.get().to_string().c_str());
            sc_core::sc_start();
            log_info("simulation stopped");
        } else {
            log_info("starting infinite simulation using %s quantum",
                     quantum.get().to_string().c_str());
            sc_core::sc_start();
            log_info("simulation stopped");
        }

        return EXIT_SUCCESS;
    }

}
