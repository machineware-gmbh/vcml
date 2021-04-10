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

#ifndef VCML_NET_ADAPTER_H
#define VCML_NET_ADAPTER_H

#include "vcml/common/types.h"
#include "vcml/common/report.h"
#include "vcml/common/strings.h"

namespace vcml { namespace net {

    class client;

    class adapter
    {
    private:
        string m_name;
        vector<client*> m_clients;

        static unordered_map<string, adapter*> s_adapters;

    public:
        const char* adapter_name() const { return m_name.c_str(); }

        adapter(const string& name);
        virtual ~adapter();

        void attach(client* backend);
        void detach(client* backend);

        bool recv_packet(vector<u8>& packet);
        void send_packet(const vector<u8>& packet);

        static adapter* find(const string& name);
        static vector<adapter*> all();
    };

}}

#endif
