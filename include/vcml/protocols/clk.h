/******************************************************************************
 *                                                                            *
 * Copyright (C) 2022 MachineWare GmbH                                        *
 * All Rights Reserved                                                        *
 *                                                                            *
 * This is work is licensed under the terms described in the LICENSE file     *
 * found in the root directory of this source tree.                           *
 *                                                                            *
 ******************************************************************************/

#ifndef VCML_PROTOCOLS_CLK_H
#define VCML_PROTOCOLS_CLK_H

#include "vcml/core/types.h"
#include "vcml/core/systemc.h"

#include "vcml/protocols/base.h"

namespace vcml {

struct clk_payload {
    hz_t oldhz;
    hz_t newhz;
};

constexpr bool success(const clk_payload& tx) {
    return true;
}

constexpr bool failed(const clk_payload& tx) {
    return false;
}

ostream& operator<<(ostream& os, const clk_payload& clk);

class clk_fw_transport_if : public sc_core::sc_interface
{
public:
    typedef clk_payload protocol_types;
    virtual void clk_transport(const clk_payload& tx) = 0;
};

class clk_bw_transport_if : public sc_core::sc_interface
{
public:
    typedef clk_payload protocol_types;
    virtual hz_t clk_get_hz() = 0;
};

class clk_base_initiator_socket;
class clk_base_target_socket;
class clk_initiator_socket;
class clk_target_socket;
class clk_initiator_stub;
class clk_target_stub;

class clk_host
{
public:
    clk_host() = default;
    virtual ~clk_host() = default;
    virtual void clk_notify(const clk_target_socket&, const clk_payload&) = 0;
};

typedef multi_initiator_socket<clk_fw_transport_if, clk_bw_transport_if>
    clk_base_initiator_socket_b;

typedef multi_target_socket<clk_fw_transport_if, clk_bw_transport_if>
    clk_base_target_socket_b;

class clk_base_initiator_socket : public clk_base_initiator_socket_b
{
private:
    clk_target_stub* m_stub;

public:
    clk_base_initiator_socket(const char*, address_space = VCML_AS_DEFAULT);
    virtual ~clk_base_initiator_socket();
    VCML_KIND(clk_base_initiator_socket);

    using clk_base_initiator_socket_b::bind;
    virtual void bind(clk_base_target_socket& socket);

    bool is_stubbed() const { return m_stub != nullptr; }
    void stub();
};

class clk_base_target_socket : public clk_base_target_socket_b
{
private:
    clk_initiator_stub* m_stub;

public:
    clk_base_target_socket(const char*, address_space = VCML_AS_DEFAULT);
    virtual ~clk_base_target_socket();
    VCML_KIND(clk_base_target_socket);

    using clk_base_target_socket_b::bind;
    virtual void bind(clk_base_initiator_socket& other);
    virtual void complete_binding(clk_base_initiator_socket& socket) {}

    bool is_stubbed() const { return m_stub != nullptr; }
    void stub(hz_t hz = 100 * MHz);
};

using clk_base_initiator_array = socket_array<clk_base_initiator_socket>;
using clk_base_target_array = socket_array<clk_base_target_socket>;

class clk_initiator_socket : public clk_base_initiator_socket
{
public:
    clk_initiator_socket(const char* nm, address_space as = VCML_AS_DEFAULT);
    virtual ~clk_initiator_socket() = default;
    VCML_KIND(clk_initiator_socket);

    hz_t get() const { return m_hz; }
    void set(hz_t hz);

    operator hz_t() const { return get(); }
    clk_initiator_socket& operator=(hz_t hz);

    sc_time cycle() const;
    sc_time cycles(size_t n) const { return cycle() * n; }

private:
    clk_host* m_host;
    hz_t m_hz;

    struct clk_bw_transport : public clk_bw_transport_if {
        clk_initiator_socket* socket;
        clk_bw_transport(clk_initiator_socket* s):
            clk_bw_transport_if(), socket(s) {}
        hz_t clk_get_hz() override { return socket->get(); }
    } m_transport;

    void clk_transport(const clk_payload& tx);
};

inline sc_time clk_initiator_socket::cycle() const {
    if (m_hz == 0)
        return SC_ZERO_TIME;
    return sc_time(1.0 / m_hz, SC_SEC);
}

class clk_target_socket : public clk_base_target_socket
{
public:
    clk_target_socket(const char* nm, address_space as = VCML_AS_DEFAULT);
    virtual ~clk_target_socket() = default;
    VCML_KIND(clk_target_socket);

    using clk_base_target_socket::bind;
    virtual void bind(base_type& other) override;
    virtual void complete_binding(clk_base_initiator_socket& socket) override;

    hz_t read() const;
    operator hz_t() const { return read(); }

    sc_time cycle() const;
    sc_time cycles(size_t n) const { return cycle() * n; }

private:
    clk_host* m_host;

    struct clk_fw_transport : public clk_fw_transport_if {
        clk_target_socket* socket;
        clk_fw_transport(clk_target_socket* s):
            clk_fw_transport_if(), socket(s) {}

        virtual void clk_transport(const clk_payload& tx) override {
            socket->clk_transport_internal(tx);
        }
    } m_transport;

    clk_base_initiator_socket* m_initiator;
    vector<clk_base_target_socket*> m_targets;

    void clk_transport_internal(const clk_payload& tx);

protected:
    virtual void clk_transport(const clk_payload& tx);
};

using clk_initiator_array = socket_array<clk_initiator_socket>;
using clk_target_array = socket_array<clk_target_socket>;

class clk_initiator_stub : private clk_bw_transport_if
{
private:
    hz_t m_hz;

    virtual hz_t clk_get_hz() override { return m_hz; }

public:
    clk_base_initiator_socket clk_out;
    clk_initiator_stub(const char* nm, hz_t hz);
    virtual ~clk_initiator_stub() = default;
};

class clk_target_stub : private clk_fw_transport_if
{
private:
    virtual void clk_transport(const clk_payload& tx) override;

public:
    clk_base_target_socket clk_in;
    clk_target_stub(const char* nm);
    virtual ~clk_target_stub() = default;
};

clk_base_initiator_socket& clk_initiator(const sc_object& parent,
                                         const string& port);
clk_base_initiator_socket& clk_initiator(const sc_object& parent,
                                         const string& port, size_t idx);

clk_base_target_socket& clk_target(const sc_object& parent,
                                   const string& port);
clk_base_target_socket& clk_target(const sc_object& parent, const string& port,
                                   size_t idx);

void clk_stub(const sc_object& obj, const string& port, hz_t hz = 100 * MHz);
void clk_stub(const sc_object& obj, const string& port, size_t idx, hz_t hz);

void clk_bind(const sc_object& obj1, const string& port1,
              const sc_object& obj2, const string& port2);
void clk_bind(const sc_object& obj1, const string& port1,
              const sc_object& obj2, const string& port2, size_t idx2);
void clk_bind(const sc_object& obj1, const string& port1, size_t idx1,
              const sc_object& obj2, const string& port2);
void clk_bind(const sc_object& obj1, const string& port1, size_t idx1,
              const sc_object& obj2, const string& port2, size_t idx2);

} // namespace vcml

#endif
