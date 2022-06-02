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

#ifndef VCML_ETHERNET_BACKEND_TAP_H
#define VCML_ETHERNET_BACKEND_TAP_H

#include "vcml/common/types.h"
#include "vcml/common/report.h"
#include "vcml/common/aio.h"
#include "vcml/logging/logger.h"

#include "vcml/models/ethernet/backend.h"
#include "vcml/models/ethernet/bridge.h"

namespace vcml {
namespace ethernet {

class backend_tap : public backend
{
private:
    int m_fd;

    void close_tap();

public:
    backend_tap(bridge* br, int devno);
    virtual ~backend_tap();

    virtual void send_to_host(const eth_frame& frame) override;

    static backend* create(bridge* br, const string& type);
};

} // namespace ethernet
} // namespace vcml

#endif
