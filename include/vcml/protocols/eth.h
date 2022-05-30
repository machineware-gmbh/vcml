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

#ifndef VCML_PROTOCOLS_ETH_H
#define VCML_PROTOCOLS_ETH_H

#include "vcml/common/types.h"
#include "vcml/common/range.h"
#include "vcml/common/bitops.h"
#include "vcml/common/systemc.h"
#include "vcml/common/utils.h"
#include "vcml/module.h"

#include "vcml/protocols/base.h"

namespace vcml {

struct mac_addr {
    array<u8, 6> bytes;

    mac_addr() = default;
    mac_addr(mac_addr&&) = default;
    mac_addr(const mac_addr&) = default;
    mac_addr& operator=(const mac_addr&) = default;

    mac_addr(u8 a0, u8 a1, u8 a2, u8 a3, u8 a4, u8 a5):
        bytes({ a0, a1, a2, a3, a4, a5 }) {}
    mac_addr(const vector<u8>& pkt, size_t offset = 0): bytes() {
        VCML_ERROR_ON(bytes.size() + offset > pkt.size(), "packet too small");
        std::copy(pkt.begin() + offset, pkt.begin() + offset + bytes.size(),
                  bytes.begin());
    }

    mac_addr(const char* str);
    mac_addr(const string& str): mac_addr(str.c_str()) {}

    u8& operator[](size_t i) { return bytes.at(i); }
    u8 operator[](size_t i) const { return bytes.at(i); }

    bool operator==(const mac_addr& o) const { return bytes == o.bytes; }
    bool operator!=(const mac_addr& o) const { return !(bytes == o.bytes); }

    bool is_multicast() const { return bytes[0] & 1; }
    bool is_broadcast() const {
        for (const u8& val : bytes)
            if (val != 0xff)
                return false;
        return true;
    }

    u32 hash_crc32() const { return crc32(bytes.data(), 6); }

    string to_string() const;

    static const char* format;
};

struct eth_frame {
    enum : size_t {
        FRAME_MIN_SIZE = 64,
        FRAME_MAX_SIZE = 1522,
    };

    vector<u8> raw;

    eth_frame() = default;
    eth_frame(eth_frame&&) = default;
    eth_frame(const eth_frame&) = default;
    eth_frame(const vector<u8>& frame): raw(frame) {}
    eth_frame(vector<u8>&& frame): raw(std::move(frame)) {}
    eth_frame(const mac_addr& dest, const mac_addr& src,
              const vector<u8>& payload);

    template <typename T>
    T read(size_t offset) const {
        T val = T();
        VCML_ERROR_ON(sizeof(T) + offset > size(), "reading beyond frame");
        memcpy(&val, raw.data() + offset, sizeof(T));
        return val;
    }

    u32 read_crc() const { return read<u32>(size() - sizeof(u32)); }
    u32 calc_crc() const { return crc32(raw.data(), size() - sizeof(u32)); }
    void refresh_crc();

    bool has_tag() const { return read<u16>(12) == 0x8100; }

    size_t size() const { return raw.size(); }
    size_t header_size() const { return has_tag() ? 18 : 14; }
    size_t payload_size() const { return size() - header_size() - 4; }

    u8* payload() { return raw.data() + header_size(); }
    const u8* payload() const { return raw.data() + header_size(); }

    u8& payload(size_t i) { return raw.at(header_size() + i); }
    u8 payload(size_t i) const { return raw.at(header_size() + i); }

    mac_addr destination() const { return mac_addr(raw, 0); }
    mac_addr source() const { return mac_addr(raw, 6); }

    bool is_multicast() const { return destination().is_multicast(); }
    bool is_broadcast() const { return destination().is_broadcast(); }

    bool is_valid() const;

    u8& operator[](size_t i) { return raw.at(i); }
    u8 operator[](size_t i) const { return raw.at(i); }

    bool operator==(const eth_frame& f) const { return raw == f.raw; }
    bool operator!=(const eth_frame& f) const { return !(raw == f.raw); }
};

ostream& operator<<(ostream& os, const mac_addr& addr);
ostream& operator<<(ostream& os, const eth_frame& frame);

inline bool success(const eth_frame& frame) {
    return frame.is_valid();
}

inline bool failed(const eth_frame& frame) {
    return !frame.is_valid();
}

class eth_initiator_socket;
class eth_target_socket;
class eth_initiator_stub;
class eth_target_stub;

class eth_host
{
public:
    friend class eth_initiator_socket;
    friend class eth_target_socket;

    typedef set<eth_initiator_socket*> eth_initiator_sockets;
    typedef set<eth_target_socket*> eth_target_sockets;

    const eth_initiator_sockets& all_eth_initiator_sockets() const {
        return m_initiator_sockets;
    }

    const eth_target_sockets& all_eth_target_sockets() const {
        return m_target_sockets;
    }

    eth_initiator_socket* eth_find_initiator(const string& name) const;
    eth_target_socket* eth_find_target(const string& name) const;

    eth_host();
    virtual ~eth_host() = default;
    eth_host(eth_host&&) = delete;
    eth_host(const eth_host&) = delete;

protected:
    virtual void eth_receive(const eth_target_socket& sock, eth_frame& frame);
    virtual void eth_receive(eth_frame& frame);

    virtual void eth_link_up();
    virtual void eth_link_up(const eth_initiator_socket& sock);
    virtual void eth_link_up(const eth_target_socket& sock);

    virtual void eth_link_down();
    virtual void eth_link_down(const eth_initiator_socket& sock);
    virtual void eth_link_down(const eth_target_socket& sock);

private:
    eth_initiator_sockets m_initiator_sockets;
    eth_target_sockets m_target_sockets;
    bool m_link_up;

    vector<string> gather_sockets(const vector<string>& names,
                                  eth_initiator_sockets& initiators,
                                  eth_target_sockets& targets) const;

    bool cmd_link_up(const vector<string>& args, ostream& os);
    bool cmd_link_down(const vector<string>& args, ostream& os);
    bool cmd_link_status(const vector<string>& args, ostream& os);
};

class eth_fw_transport_if : public sc_core::sc_interface
{
public:
    typedef eth_frame protocol_types;

    virtual void eth_transport(eth_frame& frame) = 0;
};

class eth_bw_transport_if : public sc_core::sc_interface
{
public:
    typedef eth_frame protocol_types;
};

typedef base_initiator_socket<eth_fw_transport_if, eth_bw_transport_if>
    eth_base_initiator_socket_b;

typedef base_target_socket<eth_fw_transport_if, eth_bw_transport_if>
    eth_base_target_socket_b;

class eth_base_initiator_socket : public eth_base_initiator_socket_b
{
private:
    eth_target_stub* m_stub;

public:
    eth_base_initiator_socket(const char*, address_space = VCML_AS_DEFAULT);
    virtual ~eth_base_initiator_socket();
    VCML_KIND(eth_base_initiator_socket);

    bool is_stubbed() const { return m_stub != nullptr; }
    void stub();
};

class eth_base_target_socket : public eth_base_target_socket_b
{
private:
    eth_initiator_stub* m_stub;

public:
    eth_base_target_socket(const char*, address_space = VCML_AS_DEFAULT);
    virtual ~eth_base_target_socket();
    VCML_KIND(eth_base_target_socket);

    bool is_stubbed() const { return m_stub != nullptr; }
    void stub();
};

class eth_initiator_socket : public eth_base_initiator_socket
{
private:
    bool m_link_up;
    eth_host* m_host;

    struct eth_bw_transport : eth_bw_transport_if {
        eth_initiator_socket* socket;
        eth_bw_transport(eth_initiator_socket* s):
            eth_bw_transport_if(), socket(s) {}
        virtual ~eth_bw_transport() = default;
    } m_transport;

public:
    bool link_up() const { return m_link_up; }
    void set_link_up(bool up = true);

    eth_initiator_socket(const char* name, address_space = VCML_AS_DEFAULT);
    virtual ~eth_initiator_socket();
    VCML_KIND(eth_initiator_socket);

    void send(eth_frame& frame);
};

class eth_target_socket : public eth_base_target_socket
{
private:
    bool m_link_up;
    eth_host* m_host;

    struct eth_fw_transport : eth_fw_transport_if {
        eth_target_socket* socket;
        eth_fw_transport(eth_target_socket* t):
            eth_fw_transport_if(), socket(t) {}
        virtual ~eth_fw_transport() = default;
        virtual void eth_transport(eth_frame& frame) override {
            socket->eth_transport(frame);
        }
    } m_transport;

    void eth_transport(eth_frame& frame);

public:
    bool link_up() const { return m_link_up; }
    void set_link_up(bool up = true);

    eth_target_socket(const char* name, address_space as = VCML_AS_DEFAULT);
    virtual ~eth_target_socket();
    VCML_KIND(eth_target_socket);
};

class eth_initiator_stub : private eth_bw_transport_if
{
public:
    eth_base_initiator_socket eth_tx;
    eth_initiator_stub(const char* nm);
    virtual ~eth_initiator_stub() = default;
};

class eth_target_stub : private eth_fw_transport_if
{
private:
    virtual void eth_transport(eth_frame& frame) override;

public:
    eth_base_target_socket eth_rx;
    eth_target_stub(const char* nm);
    virtual ~eth_target_stub() = default;
};

template <const size_t MAX_SOCKETS = SIZE_MAX>
using eth_base_initiator_socket_array = socket_array<eth_base_initiator_socket,
                                                     MAX_SOCKETS>;
template <const size_t MAX_SOCKETS = SIZE_MAX>
using eth_base_target_socket_array = socket_array<eth_base_target_socket,
                                                  MAX_SOCKETS>;
template <const size_t MAX_SOCKETS = SIZE_MAX>
using eth_initiator_socket_array = socket_array<eth_initiator_socket,
                                                MAX_SOCKETS>;
template <const size_t MAX_SOCKETS = SIZE_MAX>
using eth_target_socket_array = socket_array<eth_target_socket, MAX_SOCKETS>;

} // namespace vcml

#endif
