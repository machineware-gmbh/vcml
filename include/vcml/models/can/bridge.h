/******************************************************************************
 *                                                                            *
 * Copyright (C) 2022 MachineWare GmbH                                        *
 * All Rights Reserved                                                        *
 *                                                                            *
 * This is work is licensed under the terms described in the LICENSE file     *
 * found in the root directory of this source tree.                           *
 *                                                                            *
 ******************************************************************************/

#ifndef VCML_CAN_BRIDGE_H
#define VCML_CAN_BRIDGE_H

#include "vcml/core/types.h"
#include "vcml/core/systemc.h"
#include "vcml/core/module.h"
#include "vcml/core/model.h"

#include "vcml/properties/property.h"
#include "vcml/protocols/can.h"
#include "vcml/models/can/backend.h"

namespace vcml {
namespace can {

class backend;

class bridge : public module, public can_host
{
private:
    id_t m_next_id;
    unordered_map<id_t, backend*> m_dynamic_backends;
    vector<backend*> m_backends;

    mutable mutex m_mtx;
    queue<can_frame> m_rx;
    sc_event m_ev;

    bool cmd_create_backend(const vector<string>& args, ostream& os);
    bool cmd_destroy_backend(const vector<string>& args, ostream& os);
    bool cmd_list_backends(const vector<string>& args, ostream& os);

    virtual void can_receive(can_frame& frame) override;

    void can_transmit();

    static unordered_map<string, bridge*>& bridges();

public:
    property<string> backends;

    can_initiator_socket can_tx;
    can_target_socket can_rx;

    bridge(const sc_module_name& nm);
    virtual ~bridge();
    VCML_KIND(can::bridge);

    void send_to_host(const can_frame& frame);
    void send_to_guest(can_frame frame);

    void attach(backend* b);
    void detach(backend* b);

    id_t create_backend(const string& type);
    bool destroy_backend(id_t id);

    static bridge* find(const string& name);
    static vector<bridge*> all();

    template <typename T>
    void connect(T& device) {
        can_tx.bind(device.can_rx);
        device.can_tx.bind(can_rx);
    }
};

} // namespace can
} // namespace vcml

#endif
