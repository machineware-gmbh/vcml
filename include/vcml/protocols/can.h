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

#ifndef VCML_PROTOCOLS_CAN_H
#define VCML_PROTOCOLS_CAN_H

#include "vcml/core/types.h"
#include "vcml/core/range.h"
#include "vcml/core/systemc.h"

#include "vcml/protocols/base.h"

namespace vcml {

u8 len2dlc(size_t len);
size_t dlc2len(u8 dlc);

enum can_msgid_flags : u32 {
    CAN_SID = bitmask(11),
    CAN_EID = bitmask(29),
    CAN_EFF = bit(31), // extended frame format (29 bit id)
    CAN_RTR = bit(30), // remote transmission request
    CAN_ERR = bit(29), // error message frame
};

enum can_flags : u8 {
    CANFD_BRS = bit(0),
    CANFD_ESI = bit(1),
    CANFD_FDF = bit(2),
};

struct can_frame {
    u32 msgid;
    u8 dlc;
    u8 flags;
    u8 data[64] MWR_DECL_ALIGN(8);

    bool is_eff() const { return msgid & CAN_EFF; }
    bool is_rtr() const { return msgid & CAN_RTR; }
    bool is_err() const { return msgid & CAN_ERR; }

    bool is_brs() const { return flags & CANFD_BRS; }
    bool is_esi() const { return flags & CANFD_ESI; }
    bool is_fdf() const { return flags & CANFD_FDF; }

    u32 id() const { return msgid & (is_eff() ? CAN_EID : CAN_SID); }
    size_t length() const { return dlc2len(dlc); }

    bool operator==(const can_frame& other);
    bool operator!=(const can_frame& other);
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
    friend class can_initiator_socket;
    friend class can_target_socket;

    typedef set<can_initiator_socket*> can_initiator_sockets;
    typedef set<can_target_socket*> can_target_sockets;

    const can_initiator_sockets& all_can_initiator_sockets() const {
        return m_initiator_sockets;
    }

    const can_target_sockets& all_can_target_sockets() const {
        return m_target_sockets;
    }

    can_initiator_socket* find_can_initiator(const string& name) const;
    can_target_socket* find_can_target(const string& name) const;

    can_host() = default;
    virtual ~can_host() = default;
    can_host(can_host&&) = delete;
    can_host(const can_host&) = delete;

protected:
    virtual void can_receive(const can_target_socket& sock, can_frame& frame);
    virtual void can_receive(can_frame& frame);
    virtual bool can_rx_pop(can_frame& frame);

private:
    can_initiator_sockets m_initiator_sockets;
    can_target_sockets m_target_sockets;
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
};

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
    virtual ~can_initiator_socket();
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
    virtual ~can_target_socket();
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

template <const size_t MAX_SOCKETS = SIZE_MAX>
using can_base_initiator_socket_array = socket_array<can_base_initiator_socket,
                                                     MAX_SOCKETS>;
template <const size_t MAX_SOCKETS = SIZE_MAX>
using can_base_target_socket_array = socket_array<can_base_target_socket,
                                                  MAX_SOCKETS>;
template <const size_t MAX_SOCKETS = SIZE_MAX>
using can_initiator_socket_array = socket_array<can_initiator_socket,
                                                MAX_SOCKETS>;
template <const size_t MAX_SOCKETS = SIZE_MAX>
using can_target_socket_array = socket_array<can_target_socket, MAX_SOCKETS>;

} // namespace vcml

#endif
