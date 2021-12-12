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

#ifndef VCML_NET_BACKEND_TAP_H
#define VCML_NET_BACKEND_TAP_H

#include "vcml/common/types.h"
#include "vcml/common/report.h"
#include "vcml/logging/logger.h"
#include "vcml/net/backend.h"

namespace vcml { namespace net {

    class backend_tap: public backend
    {
    private:
        int m_fd;

        enum : size_t {
            ETH_MAX_FRAME_SIZE = 1522,
        };

    public:
        backend_tap(const string& adapter, int devno);
        virtual ~backend_tap();

        virtual bool recv_packet(vector<u8>& packet) override;
        virtual void send_packet(const vector<u8>& packet) override;

        static backend* create(const string& name, const string& type);
    };

}}

#endif
