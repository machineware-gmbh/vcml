/******************************************************************************
 *                                                                            *
 * Copyright 2021 Jan Henrik Weinstock                                        *
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

#ifndef VCML_NET_ADAPTER_H
#define VCML_NET_ADAPTER_H

#include "vcml/common/types.h"
#include "vcml/common/bitops.h"
#include "vcml/common/report.h"
#include "vcml/common/strings.h"

#include "vcml/properties/property.h"

#include "vcml/net/backend.h"

namespace vcml {
namespace net {

struct mac_addr {
    array<u8, 6> bytes;

    mac_addr()                = default;
    mac_addr(mac_addr&&)      = default;
    mac_addr(const mac_addr&) = default;
    mac_addr& operator=(const mac_addr&) = default;

    mac_addr(u8, u8, u8, u8, u8, u8);
    mac_addr(const vector<u8>& pkt, size_t off = 0);

    u8& operator[](size_t i) { return bytes.at(i); }
    u8 operator[](size_t i) const { return bytes.at(i); }

    bool operator==(const mac_addr&) const;
    bool operator!=(const mac_addr&) const;

    bool is_multicast() const { return bytes[0] & 1; }
    bool is_broadcast() const;

    u32 hash_crc32() const { return crc32(bytes.data(), 6); }
};

inline mac_addr::mac_addr(u8 a, u8 b, u8 c, u8 d, u8 e, u8 f):
    bytes({ a, b, c, d, e, f }) {
}

inline mac_addr::mac_addr(const vector<u8>& pkt, size_t off): bytes() {
    std::copy(pkt.begin() + off, pkt.begin() + off + 6, bytes.begin());
}

inline bool mac_addr::operator==(const mac_addr& other) const {
    return bytes == other.bytes;
}

inline bool mac_addr::operator!=(const mac_addr& other) const {
    return !(bytes == other.bytes);
}

inline bool mac_addr::is_broadcast() const {
    for (const u8& val : bytes)
        if (val != 0xff)
            return false;
    return true;
}

ostream& operator<<(ostream& os, const mac_addr& addr);

class backend;

class adapter
{
private:
    string m_name;
    size_t m_next_id;
    std::map<size_t, backend*> m_clients;
    vector<backend*> m_listener;
    bool m_link_up;

    bool cmd_create_client(const vector<string>& args, ostream& os);
    bool cmd_destroy_client(const vector<string>& args, ostream& os);
    bool cmd_list_clients(const vector<string>& args, ostream& os);
    bool cmd_link_up(const vector<string>& args, ostream& os);
    bool cmd_link_down(const vector<string>& args, ostream& os);
    bool cmd_link_status(const vector<string>& args, ostream& os);

    static unordered_map<string, adapter*> s_adapters;

public:
    property<string> backends;

    const char* adapter_name() const { return m_name.c_str(); }

    adapter();
    virtual ~adapter();

    void attach(backend* client);
    void detach(backend* client);

    int create_client(const string& type);
    bool destroy_client(int id);

    bool recv_packet(vector<u8>& packet);
    void send_packet(const vector<u8>& packet);

    static adapter* find(const string& name);
    static vector<adapter*> all();

protected:
    virtual void on_link_up();
    virtual void on_link_down();
};

} // namespace net
} // namespace vcml

#endif
