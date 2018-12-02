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

#include "vcml/debugging/vncserver.h"

namespace vcml { namespace debugging {

    std::map<u16, shared_ptr<vncserver>> vncserver::m_servers;

    vncserver::vncserver(u16 port):
        m_port(port) {
        // TODO
    }

    vncserver::~vncserver() {
        // TODO
    }

    shared_ptr<vncserver> vncserver::lookup(u16 port) {
        shared_ptr<vncserver>& ptr = m_servers[port];
        if (ptr.get() == NULL)
            ptr.reset(new vncserver(port));
        return ptr;
    }

}}
