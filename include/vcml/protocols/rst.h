/******************************************************************************
 *                                                                            *
 * Copyright 2022 Jan Henrik Weinstock                                        *
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

#ifndef VCML_PROTOCOLS_RST_H
#define VCML_PROTOCOLS_RST_H

#include "vcml/common/types.h"
#include "vcml/common/report.h"
#include "vcml/common/systemc.h"

#include "vcml/ports.h"
#include "vcml/module.h"

#include "vcml/protocols/base.h"

namespace vcml {

enum rst_signal {
    RST_PULSE,
    RST_LEVEL,
};

struct rst_payload {
    bool reset;
    rst_signal signal;
};

constexpr bool success(const rst_payload& tx) {
    return true;
}

constexpr bool failed(const rst_payload& tx) {
    return false;
}

const char* rst_signal_str(rst_signal sig);

ostream& operator<<(ostream& os, const rst_payload& rst);

class rst_fw_transport_if : public sc_core::sc_interface
{
public:
    typedef rst_payload protocol_types;
    virtual void rst_transport(const rst_payload& tx) = 0;
};

class rst_bw_transport_if : public sc_core::sc_interface
{
public:
    typedef rst_payload protocol_types;
};

class rst_base_initiator_socket;
class rst_base_target_socket;
class rst_initiator_socket;
class rst_target_socket;
class rst_initiator_stub;
class rst_target_stub;

class rst_host
{
public:
    friend class rst_initiator_socket;
    friend class rst_target_socket;

    typedef vector<rst_initiator_socket*> rst_initiator_sockets;
    typedef vector<rst_target_socket*> rst_target_sockets;

    const rst_initiator_sockets& all_rst_initiator_sockets() const;
    const rst_target_sockets& all_rst_target_sockets() const;

    rst_target_sockets all_rst_target_sockets(address_space as) const;

    rst_host() = default;

    virtual ~rst_host() = default;

    virtual void rst_notify(const rst_target_socket&, const rst_payload&) = 0;

private:
    rst_initiator_sockets m_initiator_sockets;
    rst_target_sockets m_target_sockets;
};

inline const rst_host::rst_initiator_sockets&
rst_host::all_rst_initiator_sockets() const {
    return m_initiator_sockets;
}

inline const rst_host::rst_target_sockets& rst_host::all_rst_target_sockets()
    const {
    return m_target_sockets;
}

typedef multi_initiator_socket<rst_fw_transport_if, rst_bw_transport_if>
    rst_base_initiator_socket_b;

typedef multi_target_socket<rst_fw_transport_if, rst_bw_transport_if>
    rst_base_target_socket_b;

class rst_base_initiator_socket : public rst_base_initiator_socket_b
{
private:
    rst_target_stub* m_stub;

public:
    rst_base_initiator_socket(const char*, address_space = VCML_AS_DEFAULT);
    virtual ~rst_base_initiator_socket();
    VCML_KIND(rst_base_initiator_socket);

    using rst_base_initiator_socket_b::bind;
    virtual void bind(rst_base_target_socket& socket);

    bool is_stubbed() const { return m_stub != nullptr; }
    void stub();
};

class rst_base_target_socket : public rst_base_target_socket_b
{
private:
    rst_initiator_stub* m_stub;

public:
    rst_base_target_socket(const char*, address_space = VCML_AS_DEFAULT);
    virtual ~rst_base_target_socket();
    VCML_KIND(rst_base_target_socket);

    using rst_base_target_socket_b::bind;
    virtual void bind(rst_base_initiator_socket& other);
    virtual void complete_binding(rst_base_initiator_socket& socket) {}

    bool is_stubbed() const { return m_stub != nullptr; }
    void stub();
};

template <const size_t MAX_PORTS = SIZE_MAX>
using rst_base_initiator_socket_array = socket_array<rst_base_initiator_socket,
                                                     MAX_PORTS>;

template <const size_t MAX_PORTS = SIZE_MAX>
using rst_base_target_socket_array = socket_array<rst_base_target_socket,
                                                  MAX_PORTS>;

class rst_initiator_socket : public rst_base_initiator_socket
{
public:
    rst_initiator_socket(const char* nm, address_space as = VCML_AS_DEFAULT);
    virtual ~rst_initiator_socket();
    VCML_KIND(rst_initiator_socket);

    const sc_event& default_event();

    void reset() { reset(true, RST_PULSE); }
    void reset(bool state, rst_signal sig);

    bool read() const { return m_state; }
    operator bool() const { return read(); }

    rst_initiator_socket& operator=(bool set);

private:
    rst_host* m_host;
    sc_event* m_event;

    bool m_state;

    struct rst_bw_transport : public rst_bw_transport_if {
        rst_initiator_socket* socket;
        rst_bw_transport(rst_initiator_socket* s):
            rst_bw_transport_if(), socket(s) {}
    } m_transport;

    void rst_transport(const rst_payload& tx);
};

class rst_target_socket : public rst_base_target_socket
{
    friend class rst_initiator_socket;

public:
    rst_target_socket(const char* nm, address_space as = VCML_AS_DEFAULT);
    virtual ~rst_target_socket();
    VCML_KIND(rst_target_socket);

    const sc_event& default_event();

    bool read() const { return m_state; }
    operator bool() const { return read(); }

    using rst_base_target_socket::bind;
    virtual void bind(rst_target_socket& other);
    virtual void complete_binding(rst_base_initiator_socket& socket) override;

private:
    rst_host* m_target;
    sc_event* m_event;

    bool m_state;

    struct rst_fw_transport : public rst_fw_transport_if {
        rst_target_socket* socket;
        rst_fw_transport(rst_target_socket* s):
            rst_fw_transport_if(), socket(s) {}

        virtual void rst_transport(const rst_payload& tx) override {
            socket->rst_transport(tx);
        }
    } m_transport;

    rst_base_initiator_socket* m_initiator;
    vector<rst_target_socket*> m_targets;

    void rst_transport(const rst_payload& tx);
};

template <const size_t MAX_PORTS = SIZE_MAX>
using rst_initiator_socket_array = socket_array<rst_initiator_socket,
                                                MAX_PORTS>;

template <const size_t MAX_PORTS = SIZE_MAX>
using rst_target_socket_array = socket_array<rst_target_socket, MAX_PORTS>;

class rst_initiator_stub : private rst_bw_transport_if
{
public:
    rst_base_initiator_socket rst_out;
    rst_initiator_stub(const char* nm);
    virtual ~rst_initiator_stub() = default;
};

class rst_target_stub : private rst_fw_transport_if
{
private:
    virtual void rst_transport(const rst_payload& tx) override;

public:
    rst_base_target_socket rst_in;
    rst_target_stub(const char* nm);
    virtual ~rst_target_stub() = default;
};

class rst_initiator_adapter : public module
{
public:
    in_port<bool> rst_in;
    rst_initiator_socket rst_out;

    rst_initiator_adapter(const sc_module_name& nm);
    virtual ~rst_initiator_adapter() = default;

private:
    void update();
};

class rst_target_adapter : public module, public rst_host
{
public:
    rst_target_socket rst_in;
    out_port<bool> rst_out;

    rst_target_adapter(const sc_module_name& nm);
    virtual ~rst_target_adapter() = default;

private:
    virtual void rst_notify(const rst_target_socket& socket,
                            const rst_payload& tx) override;
};

} // namespace vcml

#endif
