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

#ifndef VCML_ETHERNET_BRIDGE_H
#define VCML_ETHERNET_BRIDGE_H

#include "vcml/common/types.h"
#include "vcml/common/bitops.h"
#include "vcml/common/report.h"
#include "vcml/common/strings.h"
#include "vcml/common/systemc.h"

#include "vcml/properties/property.h"
#include "vcml/ethernet/backend.h"
#include "vcml/protocols/eth.h"

#include "vcml/module.h"

namespace vcml {
namespace ethernet {

class backend;

class bridge : public module, public eth_host
{
private:
    id_t m_next_id;
    unordered_map<id_t, backend*> m_dynamic_backends;
    vector<backend*> m_backends;

    mutable mutex m_mtx;
    queue<eth_frame> m_rx;
    sc_event m_ev;

    bool cmd_create_backend(const vector<string>& args, ostream& os);
    bool cmd_destroy_backend(const vector<string>& args, ostream& os);
    bool cmd_list_backends(const vector<string>& args, ostream& os);

    virtual void eth_receive(eth_frame& frame) override;

    void eth_transmit();

    static unordered_map<string, bridge*>& bridges();

public:
    property<string> backends;

    eth_initiator_socket eth_tx;
    eth_target_socket eth_rx;

    bridge(const sc_module_name& nm);
    virtual ~bridge();
    VCML_KIND(ethernet::bridge);

    void send_to_host(const eth_frame& frame);
    void send_to_guest(eth_frame frame);

    void attach(backend* b);
    void detach(backend* b);

    id_t create_backend(const string& type);
    bool destroy_backend(id_t id);

    static bridge* find(const string& name);
    static vector<bridge*> all();

    template <typename T>
    void connect(T& device) {
        eth_tx.bind(device.eth_rx);
        device.eth_tx.bind(eth_rx);
    }
};

} // namespace ethernet
} // namespace vcml

#endif
