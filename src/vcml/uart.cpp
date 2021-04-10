/******************************************************************************
 *                                                                            *
 * Copyright 2021 Jan Henrik Weinstock                                        *
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

#include "vcml/uart.h"

namespace vcml {

    bool uart::cmd_history(const vector<string>& args, ostream& os) {
        vector<u8> history;
        fetch_history(history);
        for (u8 val : history)
            os << (int)val;
        return true;
    }

    uart::uart(const sc_module_name& nm, endianess e,
        unsigned int read_latency, unsigned int write_latency):
        peripheral(nm, e, read_latency, write_latency),
        serial::port(name()),
        m_backends(),
        backends("backends", "") {
        register_command("history", 0, this, &uart::cmd_history,
            "show previously transmitted data from this UART");

        vector<string> types = split(backends, ' ');
        for  (auto type : types) {
            auto be = serial::backend::create(name(), type);
            if (be != nullptr)
                m_backends.push_back(be);
            else
                log_warn("failed to create serial backend '%s'", type.c_str());
        }
    }

    uart::~uart() {
        for (auto backend : m_backends)
            delete backend;
    }

}
