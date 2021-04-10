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

#include "vcml/nic.h"

namespace vcml {

    nic::nic(const sc_module_name& nm, endianess e,
             unsigned int read_latency, unsigned int write_latency):
        peripheral(nm, e, read_latency, write_latency),
        net::adapter(name()),
        m_clients(),
        clients("clients", "") {
        vector<string> types = split(clients, ' ');
        for  (auto type : types) {
            auto cl = net::client::create(name(), type);
            if (cl != nullptr)
                m_clients.push_back(cl);
            else
                log_warn("failed to create network client '%s'", type.c_str());
        }
    }

    nic::~nic() {
        for (auto cl : m_clients)
            delete cl;
    }

}
