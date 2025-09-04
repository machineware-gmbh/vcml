/******************************************************************************
 *                                                                            *
 * Copyright (C) 2025 MachineWare GmbH                                        *
 * All Rights Reserved                                                        *
 *                                                                            *
 * This is work is licensed under the terms described in the LICENSE file     *
 * found in the root directory of this source tree.                           *
 *                                                                            *
 ******************************************************************************/

#ifndef VCML_PROTOCOLS_LIN_H
#define VCML_PROTOCOLS_LIN_H

#include "vcml/core/types.h"
#include "vcml/core/systemc.h"

#include "vcml/protocols/base.h"

namespace vcml {

enum lin_status : int {
    LIN_SUCCESS = 1,
    LIN_INCOMPLETE = 0,
    LIN_CHECKSUM_ERROR = -1,
    LIN_SYNC_ERROR = -2,
    LIN_TIMEOUT_ERROR = -3,
    LIN_PARITY_ERROR = -4,
    LIN_FRAMING_ERROR = -5,
    LIN_BIT_ERROR = -6,
};

constexpr size_t lin_payload_size(u8 linid) {
    if (linid < 32)
        return 2;
    if (linid < 48)
        return 4;
    return 8;
}

struct lin_payload {
    u8 linid;
    u8 data[8];
    lin_status status;
    constexpr size_t size() const { return lin_payload_size(linid); }
};

constexpr bool success(lin_status sts) {
    return sts > 0;
}

constexpr bool failed(lin_status sts) {
    return sts < 0;
}

constexpr bool success(const lin_payload& tx) {
    return success(tx.status);
}

constexpr bool failed(const lin_payload& tx) {
    return failed(tx.status);
}

const char* lin_status_str(lin_status sts);

ostream& operator<<(ostream& os, lin_status resp);
ostream& operator<<(ostream& os, const lin_payload& tx);

class lin_initiator_socket;
class lin_target_socket;
class lin_initiator_stub;
class lin_target_stub;

class lin_host
{
public:
    lin_host() = default;
    virtual ~lin_host() = default;
    lin_host(lin_host&&) = delete;
    lin_host(const lin_host&) = delete;

    virtual void lin_receive(const lin_target_socket& s, lin_payload& tx) = 0;
};

class lin_fw_transport_if : public sc_core::sc_interface
{
public:
    typedef lin_payload protocol_types;

    virtual void lin_transport(lin_payload& tx) = 0;
};

class lin_bw_transport_if : public sc_core::sc_interface
{
public:
    typedef lin_payload protocol_types;
};

typedef multi_initiator_socket<lin_fw_transport_if, lin_bw_transport_if>
    lin_base_initiator_socket_b;

typedef base_target_socket<lin_fw_transport_if, lin_bw_transport_if>
    lin_base_target_socket_b;

class lin_base_initiator_socket : public lin_base_initiator_socket_b
{
private:
    lin_target_stub* m_stub;

public:
    lin_base_initiator_socket(const char*, address_space = VCML_AS_DEFAULT);
    virtual ~lin_base_initiator_socket();
    VCML_KIND(lin_base_initiator_socket);

    bool is_stubbed() const { return m_stub != nullptr; }
    void stub();

    virtual void bind_socket(sc_object& obj) override;
    virtual void stub_socket(void* data) override;
};

class lin_base_target_socket : public lin_base_target_socket_b
{
private:
    lin_initiator_stub* m_stub;

public:
    lin_base_target_socket(const char*, address_space = VCML_AS_DEFAULT);
    virtual ~lin_base_target_socket();
    VCML_KIND(lin_base_target_socket);

    bool is_stubbed() const { return m_stub != nullptr; }
    void stub();

    virtual void bind_socket(sc_object& obj) override;
    virtual void stub_socket(void* data) override;
};

template <size_t N = SIZE_MAX>
using lin_base_initiator_array = socket_array<lin_base_initiator_socket, N>;

template <size_t N = SIZE_MAX>
using lin_base_target_array = socket_array<lin_base_target_socket, N>;

class lin_initiator_socket : public lin_base_initiator_socket
{
private:
    lin_host* m_host;

    struct lin_bw_transport : lin_bw_transport_if {
        lin_initiator_socket* socket;
        lin_bw_transport(lin_initiator_socket* s):
            lin_bw_transport_if(), socket(s) {}
        virtual ~lin_bw_transport() = default;
    } m_transport;

public:
    lin_initiator_socket(const char* name, address_space as = VCML_AS_DEFAULT);
    virtual ~lin_initiator_socket() = default;
    VCML_KIND(lin_initiator_socket);

    lin_status send(u8 linid, u8 data[]);

    void transport(lin_payload& tx);
};

class lin_target_socket : public lin_base_target_socket
{
private:
    lin_host* m_host;

    struct lin_fw_transport : lin_fw_transport_if {
        lin_target_socket* socket;
        lin_fw_transport(lin_target_socket* t):
            lin_fw_transport_if(), socket(t) {}
        virtual ~lin_fw_transport() = default;
        virtual void lin_transport(lin_payload& tx) override {
            socket->lin_transport(tx);
        }
    } m_transport;

    void lin_transport(lin_payload& tx);

public:
    lin_target_socket(const char* name, address_space as = VCML_AS_DEFAULT);
    virtual ~lin_target_socket() = default;
    VCML_KIND(lin_target_socket);
};

class lin_initiator_stub : private lin_bw_transport_if
{
public:
    lin_base_initiator_socket lin_out;
    lin_initiator_stub(const char* nm);
    virtual ~lin_initiator_stub() = default;
};

class lin_target_stub : private lin_fw_transport_if
{
private:
    virtual void lin_transport(lin_payload& tx) override;

public:
    lin_base_target_socket lin_in;
    lin_target_stub(const char* nm);
    virtual ~lin_target_stub() = default;
};

template <size_t N = SIZE_MAX>
using lin_initiator_array = socket_array<lin_initiator_socket, N>;

template <size_t N = SIZE_MAX>
using lin_target_array = socket_array<lin_target_socket, N>;

void lin_stub(const sc_object& obj, const string& port);
void lin_stub(const sc_object& obj, const string& port, size_t idx);

void lin_bind(const sc_object& obj1, const string& port1,
              const sc_object& obj2, const string& port2);
void lin_bind(const sc_object& obj1, const string& port1,
              const sc_object& obj2, const string& port2, size_t idx2);
void lin_bind(const sc_object& obj1, const string& port1, size_t idx1,
              const sc_object& obj2, const string& port2);
void lin_bind(const sc_object& obj1, const string& port1, size_t idx1,
              const sc_object& obj2, const string& port2, size_t idx2);

} // namespace vcml

#endif
