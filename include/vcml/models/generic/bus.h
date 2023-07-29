/******************************************************************************
 *                                                                            *
 * Copyright (C) 2022 MachineWare GmbH                                        *
 * All Rights Reserved                                                        *
 *                                                                            *
 * This is work is licensed under the terms described in the LICENSE file     *
 * found in the root directory of this source tree.                           *
 *                                                                            *
 ******************************************************************************/

#ifndef VCML_GENERIC_BUS
#define VCML_GENERIC_BUS

#include "vcml/core/types.h"
#include "vcml/core/range.h"
#include "vcml/core/systemc.h"
#include "vcml/core/component.h"
#include "vcml/core/model.h"

#include "vcml/protocols/tlm.h"

namespace vcml {
namespace generic {

class bus : public component
{
private:
    enum : size_t {
        TARGET_NONE = SIZE_MAX,
        SOURCE_ANY = SIZE_MAX,
    };

    struct mapping {
        size_t target;
        size_t source;
        range addr;
        u64 offset;

        bool operator<(const mapping& m) const;
    };

    std::map<size_t, sc_object*> m_target_peers;
    std::map<size_t, sc_object*> m_source_peers;

    size_t find_target_port(sc_object& peer) const;
    size_t find_source_port(sc_object& peer) const;

    const char* target_peer_name(size_t port) const;
    const char* source_peer_name(size_t port) const;

    set<mapping> m_mappings;
    mapping m_default;

    const mapping& lookup(tlm_target_socket& src, const range& addr) const;
    void handle_bus_error(tlm_generic_payload& tx) const;

    bool cmd_mmap(const vector<string>& args, ostream& os);

protected:
    virtual void b_transport(tlm_target_socket& origin,
                             tlm_generic_payload& tx, sc_time& dt) override;

    virtual unsigned int transport_dbg(tlm_target_socket& origin,
                                       tlm_generic_payload& tx) override;

    virtual bool get_direct_mem_ptr(tlm_target_socket& origin,
                                    tlm_generic_payload& tx,
                                    tlm_dmi& dmi) override;

    virtual void invalidate_direct_mem_ptr(tlm_initiator_socket& origin,
                                           u64 start, u64 end) override;

public:
    using initiator_t = tlm::tlm_base_initiator_socket<>;
    using target_t = tlm::tlm_base_target_socket<>;

    property<bool> lenient;

    tlm_target_array in;
    tlm_initiator_array out;

    void map(size_t target, const range& addr);
    void map(size_t target, const range& addr, u64 offset);
    void map(size_t target, const range& addr, u64 offset, size_t source);

    void map(size_t target, u64 lo, u64 hi);
    void map(size_t target, u64 lo, u64 hi, u64 offset);
    void map(size_t target, u64 lo, u64 hi, u64 offset, size_t source);

    void map_default(size_t target, u64 offset = 0);

    void stub(const range& addr, tlm_response_status rs = TLM_OK_RESPONSE);
    void stub(u64 lo, u64 hi, tlm_response_status rs = TLM_OK_RESPONSE);

    template <typename SOURCE>
    void stub(SOURCE& s, const range& addr,
              tlm_response_status rs = TLM_OK_RESPONSE);
    template <typename SOURCE>
    void stub(SOURCE& s, u64 lo, u64 hi,
              tlm_response_status rs = TLM_OK_RESPONSE);

    template <typename SOURCE>
    size_t bind(SOURCE& s);

    template <typename TARGET>
    size_t bind(TARGET& s, const range& addr, u64 offset = 0);
    template <typename TARGET>
    size_t bind(TARGET& s, u64 lo, u64 hi, u64 offset = 0);
    template <typename SOURCE, typename TARGET>
    size_t bind(SOURCE& s, TARGET& d, const range& addr, u64 offset = 0);
    template <typename SOURCE, typename TARGET>
    size_t bind(SOURCE& source, TARGET& d, u64 lo, u64 hi, u64 offset = 0);

    template <typename SOCKET>
    size_t bind_default(SOCKET& s, u64 offset = 0);

    bus() = delete;
    bus(const sc_module_name& nm);
    virtual ~bus();
    VCML_KIND(bus);
};

inline void bus::map(size_t target, const range& addr) {
    map(target, addr, 0);
}

inline void bus::map(size_t target, const range& addr, u64 offset) {
    map(target, addr, offset, SOURCE_ANY);
}

inline void bus::map(size_t target, u64 lo, u64 hi) {
    map(target, range(lo, hi));
}

inline void bus::map(size_t target, u64 lo, u64 hi, u64 offset) {
    map(target, range(lo, hi), offset);
}

inline void bus::map(size_t target, u64 lo, u64 hi, u64 offset, size_t src) {
    map(target, range(lo, hi), offset, src);
}

inline void bus::stub(const range& addr, tlm_response_status rs) {
    size_t target_port = out.next_index();
    out[target_port].stub(rs);
    map(target_port, addr);
}

inline void bus::stub(u64 lo, u64 hi, tlm_response_status rs) {
    stub(range(lo, hi), rs);
}

template <typename SOURCE>
inline void bus::stub(SOURCE& s, const range& addr, tlm_response_status rs) {
    size_t source_port = bind(s);
    size_t target_port = out.next_index();
    out[target_port].stub(rs);
    map(target_port, addr, 0, source_port);
}

template <typename SOURCE>
inline void bus::stub(SOURCE& s, u64 lo, u64 hi, tlm_response_status rs) {
    stub(s, range(lo, hi), rs);
}

template <typename SOURCE>
size_t bus::bind(SOURCE& source) {
    size_t port = find_source_port(source);
    if (port == TARGET_NONE) {
        port = in.next_index();
        in[port].bind(source);
        m_source_peers[port] = &source;
    }

    return port;
}

template <typename TARGET>
size_t bus::bind(TARGET& target, const range& addr, u64 offset) {
    size_t port = find_target_port(target);
    if (port == TARGET_NONE) {
        port = out.next_index();
        out[port].bind(target);
        m_target_peers[port] = &target;
    }

    map(port, addr, offset);
    return port;
}

template <typename TARGET>
size_t bus::bind(TARGET& target, u64 lo, u64 hi, u64 offset) {
    return bind(target, range(lo, hi), offset);
}

template <typename SOURCE, typename TARGET>
size_t bus::bind(SOURCE& source, TARGET& target, const range& addr, u64 off) {
    size_t source_port = bind(source);
    size_t target_port = find_target_port(target);
    if (target_port == TARGET_NONE) {
        target_port = out.next_index();
        out[target_port].bind(target);
        m_target_peers[target_port] = &target;
    }

    map(target_port, addr, off, source_port);
    return target_port;
}

template <typename SOURCE, typename TARGET>
size_t bus::bind(SOURCE& source, TARGET& target, u64 lo, u64 hi, u64 offset) {
    return bind(source, target, range(lo, hi), offset);
}

template <typename TARGET>
size_t bus::bind_default(TARGET& target, u64 offset) {
    size_t port = out.next_index();
    map_default(port, offset);
    out[port].bind(target);
    m_target_peers[port] = &target;
    return port;
}

void tlm_stub(sc_object& bus, const sc_object& host, const string& port,
              const range& addr, tlm_response_status rs = TLM_OK_RESPONSE);
void tlm_stub(sc_object& bus, const sc_object& host, const string& port,
              size_t idx, const range& addr,
              tlm_response_status rs = TLM_OK_RESPONSE);
void tlm_stub(sc_object& bus, const sc_object& host, const string& port,
              u64 lo, u64 hi, tlm_response_status rs = TLM_OK_RESPONSE);
void tlm_stub(sc_object& bus, const sc_object& host, const string& port,
              size_t idx, u64 lo, u64 hi,
              tlm_response_status rs = TLM_OK_RESPONSE);

void tlm_bind(sc_object& bus, const sc_object& host, const string& port);
void tlm_bind(sc_object& bus, const sc_object& host, const string& port,
              size_t idx);
void tlm_bind(sc_object& bus, const sc_object& host, const string& port,
              const range& addr, u64 offset = 0);
void tlm_bind(sc_object& bus, const sc_object& host, const string& port,
              size_t idx, const range& addr, u64 offset);
void tlm_bind(sc_object& bus, const sc_object& host, const string& port,
              u64 lo, u64 hi, u64 offset = 0);
void tlm_bind(sc_object& bus, const sc_object& host, const string& port,
              size_t idx, u64 lo, u64 hi, u64 offset);
void tlm_bind_default(sc_object& bus, const sc_object& host,
                      const string& port, u64 offset = 0);
void tlm_bind_default(sc_object& bus, const sc_object& host,
                      const string& port, size_t idx, u64 offset);

} // namespace generic
} // namespace vcml

#endif
