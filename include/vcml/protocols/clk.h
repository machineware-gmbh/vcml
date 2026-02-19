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

struct clk_desc {
    sc_time period;
    bool polarity;
    double duty_cycle;
};

extern const clk_desc CLK_OFF;

inline bool operator==(const clk_desc& a, const clk_desc& b) {
    return a.period == b.period && a.polarity == b.polarity &&
           a.duty_cycle == b.duty_cycle;
}

inline bool operator!=(const clk_desc& a, const clk_desc& b) {
    return !(a == b);
}

constexpr bool success(const clk_desc& clk) {
    return true;
}

constexpr bool failed(const clk_desc& clk) {
    return false;
}

inline hz_t clk_get_hz(const clk_desc& clk) {
    if (clk.period == SC_ZERO_TIME)
        return 0;
    return (hz_t)(1.0 / clk.period.to_seconds() + 0.5);
}

inline void clk_set_hz(clk_desc& clk, hz_t hz) {
    if (hz > 0)
        clk.period = sc_time(1.0 / hz, SC_SEC);
    else
        clk.period = SC_ZERO_TIME;
}

inline bool clk_is_off(const clk_desc& clk) {
    return clk.period == SC_ZERO_TIME;
}

inline bool clk_is_on(const clk_desc& clk) {
    return !clk_is_off(clk);
}

inline clk_desc clk_mul(const clk_desc& clk, u64 mul) {
    clk_desc r(clk);
    r.period = mul ? r.period / mul : SC_ZERO_TIME;
    return r;
}

inline clk_desc clk_div(const clk_desc& clk, u64 div) {
    clk_desc r(clk);
    r.period *= div;
    return r;
}

inline clk_desc clk_scale(const clk_desc& clk, u64 mul, u64 div) {
    clk_desc r(clk);
    r.period = mul ? (r.period * div) / mul : SC_ZERO_TIME;
    return r;
}

inline clk_desc clk_fmul(const clk_desc& clk, double mul) {
    clk_desc r(clk);
    r.period /= mul;
    return r;
}

inline clk_desc clk_fdiv(const clk_desc& clk, double div) {
    clk_desc r(clk);
    r.period *= div;
    return r;
}

inline clk_desc clk_fscale(const clk_desc& clk, double mul, double div) {
    clk_desc r(clk);
    r.period = (r.period * div) / mul;
    return r;
}

ostream& operator<<(ostream& os, const clk_desc& clk);

class clk_fw_transport_if : public sc_core::sc_interface
{
public:
    typedef clk_desc protocol_types;
    virtual void clk_transport(const clk_desc& newclk,
                               const clk_desc& oldclk) = 0;
};

class clk_bw_transport_if : public sc_core::sc_interface
{
public:
    typedef clk_desc protocol_types;
    virtual clk_desc clk_query() = 0;
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
    virtual void clk_notify(const clk_target_socket& socket,
                            const clk_desc& newclk,
                            const clk_desc& oldclk) = 0;
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

    virtual void bind_socket(sc_object& obj) override;
    virtual void stub_socket(void* stub) override;

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

    virtual void bind_socket(sc_object& obj) override;
    virtual void stub_socket(void* stub) override;

    bool is_stubbed() const { return m_stub != nullptr; }
    void stub(const clk_desc& clk);
    void stub(hz_t hz = 100 * MHz);
};

template <size_t N = SIZE_MAX>
using clk_base_initiator_array = socket_array<clk_base_initiator_socket, N>;

template <size_t N = SIZE_MAX>
using clk_base_target_array = socket_array<clk_base_target_socket, N>;

class clk_initiator_socket : public clk_base_initiator_socket
{
public:
    clk_initiator_socket(const char* nm, address_space as = VCML_AS_DEFAULT);
    virtual ~clk_initiator_socket() = default;
    VCML_KIND(clk_initiator_socket);

    const clk_desc& get() const { return m_clk; }
    void set(const clk_desc& clk);
    hz_t get_hz() const { return clk_get_hz(m_clk); }
    void set_hz(hz_t hz);

    operator clk_desc() const { return get(); }
    clk_initiator_socket& operator=(const clk_desc& clk);

    operator hz_t() const { return get_hz(); }
    clk_initiator_socket& operator=(hz_t hz);

    clk_initiator_socket& operator=(const clk_target_socket& other);

    sc_time cycle() const { return m_clk.period; }
    sc_time cycles(size_t n) const { return cycle() * n; }

private:
    clk_host* m_host;
    clk_desc m_clk;

    struct clk_bw_transport : public clk_bw_transport_if {
        clk_initiator_socket* socket;
        clk_bw_transport(clk_initiator_socket* s):
            clk_bw_transport_if(), socket(s) {}
        virtual clk_desc clk_query() override { return socket->m_clk; }
    } m_transport;

    void clk_transport(const clk_desc& newclk, const clk_desc& oldclk);
};

inline clk_desc operator*(const clk_initiator_socket& s, double d) {
    return clk_mul(s, d);
}

inline clk_desc operator/(const clk_initiator_socket& s, double d) {
    return clk_div(s, d);
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

    clk_desc get() const;

    hz_t get_hz() const { return clk_get_hz(get()); }
    bool get_polarity() const { return get().polarity; }
    double get_duty_cycle() const { return get().duty_cycle; }

    operator clk_desc() const { return get(); }
    operator hz_t() const { return get_hz(); }

    sc_time cycle() const { return get().period; }
    sc_time cycles(size_t n) const { return cycle() * n; }

private:
    clk_host* m_host;

    struct clk_fw_transport : public clk_fw_transport_if {
        clk_target_socket* socket;
        clk_fw_transport(clk_target_socket* s):
            clk_fw_transport_if(), socket(s) {}

        virtual void clk_transport(const clk_desc& newclk,
                                   const clk_desc& oldclk) override {
            socket->clk_transport_internal(newclk, oldclk);
        }
    } m_transport;

    clk_base_initiator_socket* m_initiator;
    vector<clk_base_target_socket*> m_targets;

    void clk_transport_internal(const clk_desc& newclk,
                                const clk_desc& oldclk);

protected:
    virtual void clk_transport(const clk_desc& newclk, const clk_desc& oldclk);
};

template <size_t N = SIZE_MAX>
using clk_initiator_array = socket_array<clk_initiator_socket, N>;

template <size_t N = SIZE_MAX>
using clk_target_array = socket_array<clk_target_socket, N>;

class clk_initiator_stub : private clk_bw_transport_if
{
private:
    clk_desc m_clk;

    virtual clk_desc clk_query() override { return m_clk; }

public:
    clk_base_initiator_socket clk_out;
    clk_initiator_stub(const char* nm, const clk_desc& clk);
    virtual ~clk_initiator_stub() = default;
};

class clk_target_stub : private clk_fw_transport_if
{
private:
    virtual void clk_transport(const clk_desc& newclk,
                               const clk_desc& oldclk) override;

public:
    clk_base_target_socket clk_in;
    clk_target_stub(const char* nm);
    virtual ~clk_target_stub() = default;
};

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
