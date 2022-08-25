/******************************************************************************
 *                                                                            *
 * Copyright 2018 Jan Henrik Weinstock                                        *
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

#ifndef VCML_GENERIC_BUS
#define VCML_GENERIC_BUS

#include "vcml/core/types.h"
#include "vcml/core/range.h"
#include "vcml/core/report.h"
#include "vcml/core/systemc.h"
#include "vcml/core/component.h"

#include "vcml/protocols/tlm.h"

namespace vcml {
namespace generic {

class bus;

template <typename T>
class bus_ports
{
private:
    unsigned int m_next;
    bus* m_parent;
    std::map<unsigned int, T*> m_sockets;

public:
    bus_ports() = delete;
    bus_ports(bus* parent);
    virtual ~bus_ports();

    bool exists(unsigned int idx) const;

    unsigned int next_idx() const { return m_next; }
    T& next() { return operator[](next_idx()); }

    T& operator[](unsigned int idx);
    const T& operator[](unsigned int idx) const;

    typedef typename std::map<unsigned int, T*>::iterator iterator;

    iterator begin() { return m_sockets.begin(); }
    iterator end() { return m_sockets.end(); }
};

template <typename T>
inline bus_ports<T>::bus_ports(bus* parent):
    m_next(0), m_parent(parent), m_sockets() {
    VCML_ERROR_ON(parent == nullptr, "bus_ports parent must not be NULL");
}

template <typename T>
inline bus_ports<T>::~bus_ports() {
    for (auto it : m_sockets)
        delete it.second;
}

template <typename T>
inline bool bus_ports<T>::exists(unsigned int idx) const {
    return m_sockets.find(idx) != m_sockets.end();
}

template <typename T>
inline const T& bus_ports<T>::operator[](unsigned int idx) const {
    VCML_ERROR_ON(!exists(idx), "bus port %d does not exist", idx);
    return *m_sockets.at(idx);
}

class bus : public component
{
public:
    template <unsigned int BUSWIDTH>
    using initiator_socket = tlm::tlm_initiator_socket<BUSWIDTH>;
    template <unsigned int BUSWIDTH>
    using target_socket = tlm::tlm_target_socket<BUSWIDTH>;

private:
    bool cmd_mmap(const vector<string>& args, ostream& os);

    struct mapping {
        int port;
        range addr;
        u64 offset;
        string peer;
    };

    vector<module*> m_adapters;
    vector<mapping> m_mappings;
    mapping m_default;

    target_socket<64>* create_target_socket(unsigned int idx);
    initiator_socket<64>* create_initiator_socket(unsigned int idx);

    template <unsigned int W1, unsigned int W2>
    void bind_internal(initiator_socket<W1>& s1, target_socket<W2>& s2);

    const mapping& lookup(const range& addr) const;

    void cb_b_transport(int port, tlm_generic_payload& tx, sc_time& dt);
    unsigned int cb_transport_dbg(int port, tlm_generic_payload& tx);
    bool cb_get_direct_mem_ptr(int port, tlm_generic_payload& tx,
                               tlm_dmi& dmi);
    void cb_invalidate_direct_mem_ptr(int port, sc_dt::uint64 s,
                                      sc_dt::uint64 e);

    using component::b_transport;
    using component::transport_dbg;
    using component::get_direct_mem_ptr;
    using component::invalidate_direct_mem_ptr;

protected:
    void b_transport(int port, tlm_generic_payload& tx, sc_time& dt);
    unsigned int transport_dbg(int port, tlm_generic_payload& tx);
    bool get_direct_mem_ptr(int port, tlm_generic_payload& tx, tlm_dmi& dmi);
    void invalidate_direct_mem_ptr(int port, sc_dt::uint64 start,
                                   sc_dt::uint64 end);

public:
    bus_ports<target_socket<64>> in;
    bus_ports<initiator_socket<64>> out;

    int find_port(const sc_object& socket) const;

    void map(unsigned int port, const range& addr, u64 offset = 0,
             const string& peer = "");
    void map(unsigned int port, u64 start, u64 end, u64 offset = 0,
             const string& peer = "");
    void map_default(unsigned int port, u64 offset = 0,
                     const string& peer = "");

    template <unsigned int BUSWIDTH>
    unsigned int bind(initiator_socket<BUSWIDTH>& socket);

    template <unsigned int BUSWIDTH>
    unsigned int bind(target_socket<BUSWIDTH>& socket, const range& addr,
                      u64 offset = 0);

    template <unsigned int BUSWIDTH>
    unsigned int bind(target_socket<BUSWIDTH>& socket, u64 start, u64 end,
                      u64 offset = 0);

    template <unsigned int BUSWIDTH>
    unsigned int bind_default(initiator_socket<BUSWIDTH>& s, u64 offset = 0);

    bus() = delete;
    bus(const sc_module_name& nm);
    virtual ~bus();
    VCML_KIND(bus);

    template <typename T>
    T* create_socket(unsigned int idx);
};

template <typename T>
inline T& bus_ports<T>::operator[](unsigned int idx) {
    if (!exists(idx)) {
        m_sockets[idx] = m_parent->create_socket<T>(idx);
        m_next = idx + 1;
    }

    return *m_sockets[idx];
}

template <unsigned int W1, unsigned int W2>
void bus::bind_internal(initiator_socket<W1>& ini, target_socket<W2>& tgt) {
    if (W1 == W2) {
        ini.bind(tgt);
    } else {
        hierarchy_guard guard(this);
        string name = mkstr("bwa_%s_%s", ini.basename(), tgt.basename());
        auto* bwa = new tlm_bus_width_adapter<W1, W2>(name.c_str());
        m_adapters.push_back(bwa);
        ini.bind(bwa->in);
        bwa->out.bind(tgt);
    }
}

template <unsigned int WIDTH>
unsigned int bus::bind(initiator_socket<WIDTH>& socket) {
    unsigned int port = in.next_idx();
    bind_internal(socket, in[port]);
    return port;
}

template <unsigned int WIDTH>
unsigned int bus::bind(target_socket<WIDTH>& socket, const range& addr,
                       u64 offset) {
    int port = find_port(socket);
    if (port < 0) {
        port = out.next_idx();
        bind_internal(out[port], socket);
    }

    map(port, addr, offset, socket.name());
    return port;
}

template <unsigned int WIDTH>
unsigned int bus::bind(target_socket<WIDTH>& s, u64 start, u64 end, u64 off) {
    return bind(s, range(start, end), off);
}

template <unsigned int BUSWIDTH>
unsigned int bus::bind_default(initiator_socket<BUSWIDTH>& s, u64 offset) {
    unsigned int port = out.next_idx();
    map_default(port, offset, s.name());
    bind_internal(out[port], s);
    return port;
}

} // namespace generic
} // namespace vcml

#endif
