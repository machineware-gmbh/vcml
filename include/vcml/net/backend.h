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

#ifndef VCML_NET_BACKEND_H
#define VCML_NET_BACKEND_H

#include "vcml/common/types.h"
#include "vcml/common/report.h"
#include "vcml/common/strings.h"

namespace vcml { namespace net {

    class backend
    {
    private:
        string m_adapter;

    protected:
        string m_type;

    public:
        const char* adapter_name() const { return m_adapter.c_str(); }
        const char* type() const { return m_type.c_str(); }

        backend(const string& adapter);
        virtual ~backend();

        virtual bool recv_packet(vector<u8>& packet) = 0;
        virtual void send_packet(const vector<u8>& packet) = 0;

        static backend* create(const string& adapter, const string& type);
    };

}}

#endif
