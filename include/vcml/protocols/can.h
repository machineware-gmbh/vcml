/******************************************************************************
 *                                                                            *
 * Copyright (C) 2022 MachineWare GmbH                                        *
 * All Rights Reserved                                                        *
 *                                                                            *
 * This work is licensed under the terms described in the LICENSE file found  *
 * in the root directory of this source tree.                                 *
 *                                                                            *
 ******************************************************************************/

#ifndef VCML_PROTOCOLS_CAN_H
#define VCML_PROTOCOLS_CAN_H

#include "vcml/core/types.h"
#include "vcml/core/range.h"
#include "vcml/core/systemc.h"

#include "vcml/protocols/base.h"

namespace vcml {

u16 len2dlc(size_t len);
size_t dlc2len(u16 dlc);

enum can_msgid_flags : u32 {
    CAN_SID = bitmask(11),
    CAN_EID = bitmask(29),
    CAN_EFF = bit(31), // extended frame format (29 bit id)
    CAN_RTR = bit(30), // remote transmission request
    CAN_ERR = bit(29), // error message frame
};

enum can_flags : u8 {
    CAN_FD_BRS = bit(0),
    CAN_FD_ESI = bit(1),
    CAN_FD_FDF = bit(2),
    CAN_XL_SEC = bit(0),
    CAN_XL_RRS = bit(1),
    CAN_XL_XLF = bit(7),
};

using CAN_XL_MSGID_VCID = field<8, 16, u32>;

struct can_frame {
    u32 msgid;
    u8 flags;
    u8 sdt; // CAN XL Service Data Unit Type f or CANsec
    u32 af; // CAN XL Acceptance Field
    vector<u8> data;

    bool is_eff() const { return !is_canxl() && (msgid & CAN_EFF); }
    bool is_rtr() const { return !is_canxl() && (msgid & CAN_RTR); }
    bool is_err() const { return !is_canxl() && (msgid & CAN_ERR); }

    bool is_cancc() const { return !is_canxl() && !is_canfd(); };
    bool is_canfd() const {
        return (flags & CAN_FD_FDF) && !(flags & CAN_XL_XLF);
    };
    bool is_canxl() const { return flags & CAN_XL_XLF; }

    bool is_brs() const {
        return (is_canfd() && (flags & CAN_FD_BRS)) || is_canxl();
    }
    bool is_esi() const { return is_canfd() && (flags & CAN_FD_ESI); }
    bool is_fdf() const { return !is_cancc(); }

    bool is_xlf() const { return is_canxl(); }
    bool is_sec() const { return is_canxl() && (flags & CAN_XL_SEC); }
    bool is_rrs() const { return is_canxl() && (flags & CAN_XL_RRS); }

    void set_vcid(u32 vcid) { set_field<CAN_XL_MSGID_VCID>(msgid, vcid); }

    u32 id() const { return msgid & (is_eff() ? CAN_EID : CAN_SID); }
    u32 vcid() const {
        return is_canxl() ? get_field<CAN_XL_MSGID_VCID>(msgid) : 0ul;
    }
    size_t length() const { return data.size(); }
    u16 dlc() const { return is_canxl() ? length() : len2dlc(length()); }

    bool operator==(const can_frame& other) const;
    bool operator!=(const can_frame& other) const;
};

ostream& operator<<(ostream& os, const can_frame& frame);

inline bool success(const can_frame& frame) {
    return !frame.is_err();
}

inline bool failed(const can_frame& frame) {
    return frame.is_err();
}

class can_initiator_socket;
class can_target_socket;
class can_initiator_stub;
class can_target_stub;

class can_host
{
public:
    can_host() = default;
    virtual ~can_host() = default;

    virtual void can_receive(const can_target_socket& sock, can_frame& frame);
    virtual void can_receive(can_frame& frame);
    virtual bool can_rx_pop(can_frame& frame);

private:
    queue<can_frame> m_rx_queue;
};

class can_fw_transport_if : public sc_core::sc_interface
{
public:
    typedef can_frame protocol_types;

    virtual void can_transport(can_frame& frame) = 0;
};

class can_bw_transport_if : public sc_core::sc_interface
{
public:
    typedef can_frame protocol_types;
};

typedef base_initiator_socket<can_fw_transport_if, can_bw_transport_if>
    can_base_initiator_socket_b;

typedef base_target_socket<can_fw_transport_if, can_bw_transport_if>
    can_base_target_socket_b;

class can_base_initiator_socket : public can_base_initiator_socket_b
{
private:
    can_target_stub* m_stub;

public:
    can_base_initiator_socket(const char*, address_space = VCML_AS_DEFAULT);
    virtual ~can_base_initiator_socket();
    VCML_KIND(can_base_initiator_socket);

    bool is_stubbed() const { return m_stub != nullptr; }
    void stub();

    virtual void bind_socket(sc_object& obj) override;
    virtual void stub_socket(void* data) override;
};

class can_base_target_socket : public can_base_target_socket_b
{
private:
    can_initiator_stub* m_stub;

public:
    can_base_target_socket(const char*, address_space = VCML_AS_DEFAULT);
    virtual ~can_base_target_socket();
    VCML_KIND(can_base_target_socket);

    bool is_stubbed() const { return m_stub != nullptr; }
    void stub();

    virtual void bind_socket(sc_object& obj) override;
    virtual void stub_socket(void* data) override;
};

template <size_t N = SIZE_MAX>
using can_base_initiator_array = socket_array<can_base_initiator_socket, N>;

template <size_t N = SIZE_MAX>
using can_base_target_array = socket_array<can_base_target_socket, N>;

class can_initiator_socket : public can_base_initiator_socket
{
private:
    can_host* m_host;

    struct can_bw_transport : can_bw_transport_if {
        can_initiator_socket* socket;
        can_bw_transport(can_initiator_socket* s):
            can_bw_transport_if(), socket(s) {}
        virtual ~can_bw_transport() = default;
    } m_transport;

public:
    can_initiator_socket(const char* name, address_space = VCML_AS_DEFAULT);
    virtual ~can_initiator_socket() = default;
    VCML_KIND(can_initiator_socket);

    void send(can_frame& frame);
};

class can_target_socket : public can_base_target_socket
{
private:
    can_host* m_host;

    struct can_fw_transport : can_fw_transport_if {
        can_target_socket* socket;
        can_fw_transport(can_target_socket* t):
            can_fw_transport_if(), socket(t) {}
        virtual ~can_fw_transport() = default;
        virtual void can_transport(can_frame& frame) override {
            socket->can_transport(frame);
        }
    } m_transport;

    void can_transport(can_frame& frame);

public:
    can_target_socket(const char* name, address_space as = VCML_AS_DEFAULT);
    virtual ~can_target_socket() = default;
    VCML_KIND(can_target_socket);
};

class can_initiator_stub : private can_bw_transport_if
{
public:
    can_base_initiator_socket can_tx;
    can_initiator_stub(const char* nm);
    virtual ~can_initiator_stub() = default;
};

class can_target_stub : private can_fw_transport_if
{
private:
    virtual void can_transport(can_frame& frame) override;

public:
    can_base_target_socket can_rx;
    can_target_stub(const char* nm);
    virtual ~can_target_stub() = default;
};

template <size_t N = SIZE_MAX>
using can_initiator_array = socket_array<can_initiator_socket, N>;

template <size_t N = SIZE_MAX>
using can_target_array = socket_array<can_target_socket, N>;

void can_stub(const sc_object& obj, const string& port);
void can_stub(const sc_object& obj, const string& port, size_t idx);

void can_bind(const sc_object& obj1, const string& port1,
              const sc_object& obj2, const string& port2);
void can_bind(const sc_object& obj1, const string& port1,
              const sc_object& obj2, const string& port2, size_t idx2);
void can_bind(const sc_object& obj1, const string& port1, size_t idx1,
              const sc_object& obj2, const string& port2);
void can_bind(const sc_object& obj1, const string& port1, size_t idx1,
              const sc_object& obj2, const string& port2, size_t idx2);

} // namespace vcml

#endif
