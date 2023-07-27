/******************************************************************************
 *                                                                            *
 * Copyright (C) 2022 MachineWare GmbH                                        *
 * All Rights Reserved                                                        *
 *                                                                            *
 * This is work is licensed under the terms described in the LICENSE file     *
 * found in the root directory of this source tree.                           *
 *                                                                            *
 ******************************************************************************/

#ifndef VCML_PROTOCOLS_SERIAL_H
#define VCML_PROTOCOLS_SERIAL_H

#include "vcml/core/types.h"
#include "vcml/core/systemc.h"

#include "vcml/protocols/base.h"

namespace vcml {

enum serial_bits : size_t {
    SERIAL_5_BITS = 5,
    SERIAL_6_BITS = 6,
    SERIAL_7_BITS = 7,
    SERIAL_8_BITS = 8,
};

enum serial_stop : size_t {
    SERIAL_STOP_1 = 1,
    SERIAL_STOP_2 = 2,
    SERIAL_STOP_1_5 = 3,
};

constexpr u32 serial_mask(serial_bits size) {
    return bitmask(size, 0);
}

enum serial_parity {
    SERIAL_PARITY_NONE,
    SERIAL_PARITY_ODD,
    SERIAL_PARITY_EVEN,
    SERIAL_PARITY_MARK,
    SERIAL_PARITY_SPACE
};

typedef hz_t baud_t;

enum : baud_t {
    SERIAL_1200BD = 1200,
    SERIAL_2400BD = 2400,
    SERIAL_4800BD = 4800,
    SERIAL_9600BD = 9600,
    SERIAL_14400BD = 14400,
    SERIAL_38400BD = 38400,
    SERIAL_57600BD = 57600,
    SERIAL_115200BD = 115200,
    SERIAL_128000BD = 128000,
    SERIAL_256000BD = 256000,
};

struct serial_payload {
    u32 data;
    u32 mask;

    baud_t baud;

    serial_bits width;
    serial_parity parity;
    serial_stop stop;
};

const char* serial_parity_str(serial_parity par);
const char* serial_stop_str(serial_stop stop);

ostream& operator<<(ostream& os, serial_parity par);
ostream& operator<<(ostream& os, serial_stop stop);
ostream& operator<<(ostream& os, const serial_payload& tx);

bool serial_calc_parity(u8 data, serial_parity mode);
bool serial_test_parity(const serial_payload& tx);

inline bool success(const serial_payload& tx) {
    return serial_test_parity(tx);
}

inline bool failed(const serial_payload& tx) {
    return !serial_test_parity(tx);
}

class serial_initiator_socket;
class serial_target_socket;
class serial_initiator_stub;
class serial_target_stub;

class serial_host
{
public:
    serial_host() = default;
    virtual ~serial_host() = default;
    virtual void serial_receive(const serial_target_socket&, serial_payload&);
    virtual void serial_receive(const serial_target_socket&, u8 data);
    virtual void serial_receive(u8 data);
};

class serial_fw_transport_if : public sc_core::sc_interface
{
public:
    typedef serial_payload protocol_types;

    virtual void serial_transport(serial_payload& tx) = 0;
};

class serial_bw_transport_if : public sc_core::sc_interface
{
public:
    typedef serial_payload protocol_types;
};

typedef base_initiator_socket<serial_fw_transport_if, serial_bw_transport_if>
    serial_base_initiator_socket_b;

typedef base_target_socket<serial_fw_transport_if, serial_bw_transport_if>
    serial_base_target_socket_b;

class serial_base_initiator_socket : public serial_base_initiator_socket_b
{
private:
    serial_target_stub* m_stub;

public:
    serial_base_initiator_socket(const char*, address_space = VCML_AS_DEFAULT);
    virtual ~serial_base_initiator_socket();
    VCML_KIND(serial_base_initiator_socket);

    bool is_stubbed() const { return m_stub != nullptr; }
    void stub();
};

class serial_base_target_socket : public serial_base_target_socket_b
{
private:
    serial_initiator_stub* m_stub;

public:
    serial_base_target_socket(const char*, address_space = VCML_AS_DEFAULT);
    virtual ~serial_base_target_socket();
    VCML_KIND(serial_base_target_socket);

    bool is_stubbed() const { return m_stub != nullptr; }
    void stub();
};

using serial_base_initiator_array = socket_array<serial_base_initiator_socket>;
using serial_base_target_array = socket_array<serial_base_target_socket>;

class serial_initiator_socket : public serial_base_initiator_socket
{
private:
    baud_t m_baud;
    serial_bits m_width;
    serial_parity m_parity;
    serial_stop m_stop;
    serial_host* m_host;

    struct serial_bw_transport : serial_bw_transport_if {
        serial_initiator_socket* socket;
        serial_bw_transport(serial_initiator_socket* s):
            serial_bw_transport_if(), socket(s) {}
        virtual ~serial_bw_transport() = default;
    } m_transport;

public:
    baud_t baud() const { return m_baud; }
    void set_baud(baud_t b) { m_baud = b; }

    serial_bits data_width() const { return m_width; }
    void set_data_width(serial_bits w) { m_width = w; }

    serial_parity parity() const { return m_parity; }
    void set_parity(serial_parity p) { m_parity = p; }

    serial_stop stop_bits() const { return m_stop; }
    void set_stop_bits(serial_stop s) { m_stop = s; }

    sc_time cycle() const;

    serial_initiator_socket(const char* name, address_space = VCML_AS_DEFAULT);
    virtual ~serial_initiator_socket() = default;
    VCML_KIND(serial_initiator_socket);

    void send(u8 data);

    void transport(serial_payload& tx);
};

class serial_target_socket : public serial_base_target_socket
{
private:
    serial_host* m_host;

    struct serial_fw_transport : serial_fw_transport_if {
        serial_target_socket* socket;
        serial_fw_transport(serial_target_socket* t):
            serial_fw_transport_if(), socket(t) {}
        virtual ~serial_fw_transport() = default;
        virtual void serial_transport(serial_payload& tx) override {
            socket->serial_transport(tx);
        }
    } m_transport;

    void serial_transport(serial_payload& tx);

public:
    serial_target_socket(const char* name, address_space as = VCML_AS_DEFAULT);
    virtual ~serial_target_socket() = default;
    VCML_KIND(serial_target_socket);
};

class serial_initiator_stub : private serial_bw_transport_if
{
public:
    serial_base_initiator_socket serial_tx;
    serial_initiator_stub(const char* nm);
    virtual ~serial_initiator_stub() = default;
};

class serial_target_stub : private serial_fw_transport_if
{
private:
    virtual void serial_transport(serial_payload& tx) override;

public:
    serial_base_target_socket serial_rx;
    serial_target_stub(const char* nm);
    virtual ~serial_target_stub() = default;
};

using serial_initiator_array = socket_array<serial_initiator_socket>;
using serial_target_array = socket_array<serial_target_socket>;

serial_base_initiator_socket& serial_initiator(const sc_object& parent,
                                               const string& port);
serial_base_initiator_socket& serial_initiator(const sc_object& parent,
                                               const string& port, size_t idx);

serial_base_target_socket& serial_target(const sc_object& parent,
                                         const string& port);
serial_base_target_socket& serial_target(const sc_object& parent,
                                         const string& port, size_t idx);

void serial_stub(const sc_object& obj, const string& port);
void serial_stub(const sc_object& obj, const string& port, size_t idx);

void serial_bind(const sc_object& obj1, const string& port1,
                 const sc_object& obj2, const string& port2);
void serial_bind(const sc_object& obj1, const string& port1,
                 const sc_object& obj2, const string& port2, size_t idx2);
void serial_bind(const sc_object& obj1, const string& port1, size_t idx1,
                 const sc_object& obj2, const string& port2);
void serial_bind(const sc_object& obj1, const string& port1, size_t idx1,
                 const sc_object& obj2, const string& port2, size_t idx2);

} // namespace vcml

#endif
