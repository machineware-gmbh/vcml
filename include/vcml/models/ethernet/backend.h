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

#ifndef VCML_ETHERNET_BACKEND_H
#define VCML_ETHERNET_BACKEND_H

#include "vcml/core/types.h"
#include "vcml/core/report.h"
#include "vcml/core/strings.h"

#include "vcml/protocols/eth.h"

namespace vcml {
namespace ethernet {

class bridge;

class backend
{
protected:
    bridge* m_parent;
    string m_type;

public:
    bridge* parent() { return m_parent; }
    const char* type() const { return m_type.c_str(); }

    backend(bridge* gw);
    virtual ~backend();

    backend() = delete;
    backend(const backend&) = delete;
    backend(backend&&) = default;

    virtual void send_to_host(const eth_frame& frame) = 0;
    virtual void send_to_guest(eth_frame frame);

    static backend* create(bridge* br, const string& type);
};

} // namespace ethernet
} // namespace vcml

#endif
