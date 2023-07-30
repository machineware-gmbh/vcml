/******************************************************************************
 *                                                                            *
 * Copyright (C) 2022 MachineWare GmbH                                        *
 * All Rights Reserved                                                        *
 *                                                                            *
 * This is work is licensed under the terms described in the LICENSE file     *
 * found in the root directory of this source tree.                           *
 *                                                                            *
 ******************************************************************************/

#ifndef VCML_ETHERNET_BRIDGE_H
#define VCML_ETHERNET_BRIDGE_H

#include "vcml/core/types.h"
#include "vcml/core/systemc.h"
#include "vcml/core/module.h"
#include "vcml/core/model.h"

#include "vcml/properties/property.h"
#include "vcml/protocols/eth.h"
#include "vcml/models/ethernet/backend.h"

namespace vcml {
namespace ethernet {

class backend;

class bridge : public module, public eth_host
{
private:
    size_t m_next_id;
    unordered_map<size_t, backend*> m_dynamic_backends;
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

    size_t create_backend(const string& type);
    bool destroy_backend(size_t id);

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
