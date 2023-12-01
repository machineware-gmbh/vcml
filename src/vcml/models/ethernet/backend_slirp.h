/******************************************************************************
 *                                                                            *
 * Copyright (C) 2022 MachineWare GmbH                                        *
 * All Rights Reserved                                                        *
 *                                                                            *
 * This is work is licensed under the terms described in the LICENSE file     *
 * found in the root directory of this source tree.                           *
 *                                                                            *
 ******************************************************************************/

#ifndef VCML_ETHERNET_BACKEND_SLIRP_H
#define VCML_ETHERNET_BACKEND_SLIRP_H

#include "vcml/core/types.h"
#include "vcml/logging/logger.h"

#include "vcml/models/ethernet/backend.h"
#include "vcml/models/ethernet/bridge.h"

#include <libslirp.h>
#include <libslirp-version.h>

namespace vcml {
namespace ethernet {

class backend_slirp;

class slirp_network
{
private:
    unsigned int m_id;

    SlirpConfig m_config;
    Slirp* m_slirp;

    set<backend_slirp*> m_clients;

    mutex m_mtx;
    atomic<bool> m_running;
    thread m_thread;

    void slirp_thread();

public:
    slirp_network(unsigned int id);
    virtual ~slirp_network();

    void send_packet(const u8* ptr, size_t len);
    void recv_packet(const u8* ptr, size_t len);

    void register_client(backend_slirp* client);
    void unregister_client(backend_slirp* client);
};

class backend_slirp : public backend
{
private:
    shared_ptr<slirp_network> m_network;

public:
    backend_slirp(bridge* br, const shared_ptr<slirp_network>& net);
    virtual ~backend_slirp();

    void disconnect() { m_network = nullptr; }

    virtual void send_to_host(const eth_frame& frame) override;

    static backend* create(bridge* br, const string& type);
};

} // namespace ethernet
} // namespace vcml

#endif
