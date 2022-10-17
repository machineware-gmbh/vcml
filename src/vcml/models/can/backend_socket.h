/******************************************************************************
 *                                                                            *
 * Copyright 2022 Jan Henrik Weinstock                                        *
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

#ifndef VCML_CAN_BACKEND_SOCKET_H
#define VCML_CAN_BACKEND_SOCKET_H

#include "vcml/core/types.h"
#include "vcml/core/systemc.h"
#include "vcml/core/aio.h"

#include "vcml/logging/logger.h"

#include "vcml/models/can/backend.h"
#include "vcml/models/can/bridge.h"

namespace vcml {
namespace can {

class backend_socket : public backend
{
private:
    string m_name;
    int m_socket;

public:
    backend_socket(bridge* br, const string& ifname);
    virtual ~backend_socket();

    virtual void send_to_host(const can_frame& frame) override;

    static backend* create(bridge* br, const string& type);
};

} // namespace can
} // namespace vcml

#endif
