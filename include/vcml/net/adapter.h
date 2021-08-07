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

#include "vcml/properties/property.h"

#include "vcml/net/client.h"

namespace vcml { namespace net {

    class client;

    class adapter
    {
    private:
        string m_name;
        size_t m_next_id;
        std::map<size_t, client*> m_clients;
        vector<client*> m_listener;

        bool cmd_create_client(const vector<string>& args, ostream& os);
        bool cmd_destroy_client(const vector<string>& args, ostream& os);
        bool cmd_list_clients(const vector<string>& args, ostream& os);

        static unordered_map<string, adapter*> s_adapters;

    public:
        property<string> clients;

        const char* adapter_name() const { return m_name.c_str(); }

        adapter();
        virtual ~adapter();

        void attach(client* client);
        void detach(client* client);

        int  create_client(const string& type);
        bool destroy_client(int id);

        bool recv_packet(vector<u8>& packet);
        void send_packet(const vector<u8>& packet);

        static adapter* find(const string& name);
        static vector<adapter*> all();
    };

}}

#endif
