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

#ifndef VCML_NET_CLIENT_SLIRP_H
#define VCML_NET_CLIENT_SLIRP_H

#include "vcml/common/types.h"
#include "vcml/common/report.h"
#include "vcml/logging/logger.h"
#include "vcml/net/client.h"

#include <libslirp.h>
#include <libslirp-version.h>

namespace vcml { namespace net {

    class client_slirp;

    class slirp_network
    {
    private:
        SlirpConfig m_config;
        Slirp* m_slirp;

        set<client_slirp*> m_clients;

        atomic<bool> m_running;
        thread m_thread;

        void slirp_thread();

    public:
        slirp_network(unsigned int id);
        virtual ~slirp_network();

        void send_packet(const u8* ptr, size_t len);
        void recv_packet(const u8* ptr, size_t len);

        void register_client(client_slirp* client);
        void unregister_client(client_slirp* client);
    };

    class client_slirp: public client
    {
    private:
        shared_ptr<slirp_network> m_network;

        mutable mutex m_packets_mtx;
        queue<shared_ptr<vector<u8>>> m_packets;

    public:
        client_slirp(const string& ada, const shared_ptr<slirp_network>& net);
        virtual ~client_slirp();

        void disconnect() { m_network = nullptr; }

        void queue_packet(shared_ptr<vector<u8>> packet);

        virtual bool recv_packet(vector<u8>& packet) override;
        virtual void send_packet(const vector<u8>& packet) override;

        static client* create(const string& adapter, const string& type);
    };

}}

#endif
