/******************************************************************************
 *                                                                            *
 * Copyright (C) 2022 MachineWare GmbH                                        *
 * All Rights Reserved                                                        *
 *                                                                            *
 * This is work is licensed under the terms described in the LICENSE file     *
 * found in the root directory of this source tree.                           *
 *                                                                            *
 ******************************************************************************/

#ifndef VCML_REGISTER_H
#define VCML_REGISTER_H

#include "vcml/core/types.h"
#include "vcml/core/systemc.h"
#include "vcml/core/range.h"

#include "vcml/logging/logger.h"
#include "vcml/properties/property.h"
#include "vcml/protocols/tlm.h"

namespace vcml {

class peripheral;

class reg_base : public sc_object
{
private:
    u64 m_cell_size;
    u64 m_cell_count;
    range m_range;
    vcml_access m_access;
    bool m_rsync;
    bool m_wsync;
    bool m_wback;
    bool m_aligned;
    bool m_secure;
    u64 m_privilege;
    u64 m_minsize;
    u64 m_maxsize;
    peripheral* m_host;

    tlm_response_status check_access(const tlm_generic_payload& tx,
                                     const tlm_sbi& info) const;
    void do_receive(tlm_generic_payload& tx, const tlm_sbi& info);

public:
    const address_space as;

    size_t tag;

    u64 get_address() const { return m_range.start; }
    u64 get_cell_size() const { return m_cell_size; }
    u64 get_cell_count() const { return m_cell_count; }
    u64 get_size() const { return m_range.length(); }
    const range& get_range() const { return m_range; }

    bool is_array() const { return m_cell_count > 1; }

    vcml_access get_access() const { return m_access; }
    void set_access(vcml_access rw) { m_access = rw; }

    bool is_read_only() const;
    bool is_write_only() const;

    bool is_readable() const { return is_read_allowed(m_access); }
    bool is_writeable() const { return is_write_allowed(m_access); }

    void deny_access() { m_access = VCML_ACCESS_NONE; }
    void allow_read_only() { m_access = VCML_ACCESS_READ; }
    void allow_write_only() { m_access = VCML_ACCESS_WRITE; }
    void allow_read_write() { m_access = VCML_ACCESS_READ_WRITE; }

    bool is_aligned_accesses_only() const { return m_aligned; }
    void aligned_accesses_only(bool only = true) { m_aligned = only; }

    void sync_on_read(bool sync = true) { m_rsync = sync; }
    void sync_on_write(bool sync = true) { m_wsync = sync; }

    void sync_always() { m_rsync = m_wsync = true; }
    void sync_never() { m_rsync = m_wsync = false; }

    bool is_writeback() const { return m_wback; }
    void writeback(bool wb = true) { m_wback = wb; }
    void no_writeback() { m_wback = false; }

    bool is_natural_accesses_only() const;
    void natural_accesses_only(bool only = true);

    bool is_secure() const { return m_secure; }
    void set_secure(bool set = true) { m_secure = set; }

    u64 get_privilege() const { return m_privilege; }
    void set_privilege(u64 lvl) { m_privilege = lvl; }

    u64 get_min_access_size() const { return m_minsize; }
    u64 get_max_access_size() const { return m_maxsize; }
    void set_access_size(u64 min, u64 max);

    peripheral* get_host() const { return m_host; }
    int current_cpu() const;

    reg_base(address_space as, const string& nm, u64 addr, u64 size, u64 n);
    virtual ~reg_base();

    VCML_KIND(reg_base);

    virtual void reset() = 0;

    unsigned int receive(tlm_generic_payload& tx, const tlm_sbi& info);

    virtual void do_read(const range& addr, void* ptr, bool debug) = 0;
    virtual void do_write(const range& addr, const void* ptr, bool debug) = 0;
};

inline bool reg_base::is_read_only() const {
    return m_access == VCML_ACCESS_READ;
}

inline bool reg_base::is_write_only() const {
    return m_access == VCML_ACCESS_WRITE;
}

inline bool reg_base::is_natural_accesses_only() const {
    return m_aligned && m_minsize == m_cell_size && m_maxsize == m_cell_size;
}

inline void reg_base::natural_accesses_only(bool only) {
    m_aligned = only;
    m_minsize = only ? m_cell_size : 0;
    m_maxsize = only ? m_cell_size : -1;
}

template <typename DATA, size_t N = 1>
class reg : public reg_base, public property<DATA, N>
{
public:
    typedef function<DATA(void)> readfn;
    typedef function<void(DATA)> writefn;
    typedef function<DATA(size_t)> readfn_tagged;
    typedef function<void(DATA, size_t)> writefn_tagged;

    void on_read(const readfn& rd);
    void on_read(const readfn_tagged& rd);

    template <typename HOST>
    void on_read(DATA (HOST::*rd)(void), HOST* host = nullptr);

    template <typename HOST>
    void on_read(DATA (HOST::*rd)(size_t), HOST* host = nullptr);

    void on_write(const writefn& wr);
    void on_write(const writefn_tagged& wr);

    template <typename HOST>
    void on_write(void (HOST::*wr)(DATA), HOST* host = nullptr);

    template <typename HOST>
    void on_write(void (HOST::*wr)(DATA, size_t), HOST* h = nullptr);

    void on_write_mask(DATA mask);
    void on_write_mask(const array<DATA, N>& mask);

    void read_zero();
    void ignore_write();

    bool is_banked() const { return m_banked; }
    void set_banked(bool set = true) { m_banked = set; }

    const DATA& bank(int bank) const;
    DATA& bank(int bank);

    const DATA& bank(int bank, size_t idx) const;
    DATA& bank(int bank, size_t idx);

    const DATA& current_bank(size_t idx = 0) const;
    DATA& current_bank(size_t idx = 0);

    const char* name() const { return sc_core::sc_object::name(); }

    reg(const string& name, u64 addr, DATA init = DATA());
    reg(address_space as, const string& name, u64 addr, DATA init = DATA());
    reg(const tlm_target_socket& socket, const string& name, u64 addr,
        DATA data = DATA());
    virtual ~reg();
    reg() = delete;

    VCML_KIND(reg);

    virtual void reset() override;

    virtual void do_read(const range& addr, void* ptr, bool dbg) override;
    virtual void do_write(const range& addr, const void*, bool dbg) override;

    operator DATA() const;
    operator DATA&();

    const DATA& operator[](size_t idx) const;
    DATA& operator[](size_t idx);

    DATA operator++(int);
    DATA operator--(int);

    reg<DATA, N>& operator++();
    reg<DATA, N>& operator--();

    reg<DATA, N>& operator=(const reg<DATA, N>& other);
    template <typename T>
    reg<DATA, N>& operator=(const reg<T, N>& r);

    template <typename T>
    reg<DATA, N>& operator=(const T& value);
    template <typename T>
    reg<DATA, N>& operator|=(const T& value);
    template <typename T>
    reg<DATA, N>& operator&=(const T& value);
    template <typename T>
    reg<DATA, N>& operator^=(const T& value);
    template <typename T>
    reg<DATA, N>& operator+=(const T& value);
    template <typename T>
    reg<DATA, N>& operator-=(const T& value);
    template <typename T>
    reg<DATA, N>& operator*=(const T& value);
    template <typename T>
    reg<DATA, N>& operator/=(const T& value);

    template <typename T>
    bool operator==(const T& other) const;
    template <typename T>
    bool operator!=(const T& other) const;
    template <typename T>
    bool operator<=(const T& other) const;
    template <typename T>
    bool operator>=(const T& other) const;
    template <typename T>
    bool operator<(const T& other) const;
    template <typename T>
    bool operator>(const T& other) const;

    template <typename F>
    DATA get_field() const;
    template <typename F>
    void set_field();
    template <typename F>
    void set_field(DATA val);

    template <const DATA BIT>
    void set_bit(bool set);

private:
    bool m_banked;
    DATA m_init[N];
    std::map<int, DATA*> m_banks;

    readfn m_read;
    writefn m_write;
    readfn_tagged m_read_tagged;
    writefn_tagged m_write_tagged;

    void init_bank(int bank);
};
template <typename DATA, size_t N>
void reg<DATA, N>::on_read(const readfn& rd) {
    VCML_ERROR_ON(m_read, "read callback already defined");
    VCML_ERROR_ON(m_read_tagged, "tagged read callback already defined");
    m_read = rd;
}

template <typename DATA, size_t N>
void reg<DATA, N>::on_read(const readfn_tagged& rd) {
    VCML_ERROR_ON(m_read, "read callback already defined");
    VCML_ERROR_ON(m_read_tagged, "tagged read callback already defined");
    m_read_tagged = rd;
}

template <typename DATA, size_t N>
template <typename HOST>
void reg<DATA, N>::on_read(DATA (HOST::*rd)(void), HOST* host) {
    if (host == nullptr)
        host = dynamic_cast<HOST*>(get_host());
    if (host == nullptr)
        host = hierarchy_search<HOST>();
    VCML_ERROR_ON(!host, "read callback has no host");
    readfn fn = std::bind(rd, host);
    on_read(fn);
}

template <typename DATA, size_t N>
template <typename HOST>
void reg<DATA, N>::on_read(DATA (HOST::*rd)(size_t), HOST* host) {
    if (host == nullptr)
        host = dynamic_cast<HOST*>(get_host());
    if (host == nullptr)
        host = hierarchy_search<HOST>();
    VCML_ERROR_ON(!host, "tagged read callback has no host");
    readfn_tagged fn = std::bind(rd, host, std::placeholders::_1);
    on_read(fn);
}

template <typename DATA, size_t N>
void reg<DATA, N>::on_write(const writefn& wr) {
    VCML_ERROR_ON(m_write, "write callback already defined");
    VCML_ERROR_ON(m_write_tagged, "tagged write callback already defined");
    m_write = wr;
}

template <typename DATA, size_t N>
void reg<DATA, N>::on_write(const writefn_tagged& wr) {
    VCML_ERROR_ON(m_write, "write callback already defined");
    VCML_ERROR_ON(m_write_tagged, "tagged write callback already defined");
    m_write_tagged = wr;
}

template <typename DATA, size_t N>
template <typename HOST>
void reg<DATA, N>::on_write(void (HOST::*wr)(DATA), HOST* host) {
    if (host == nullptr)
        host = dynamic_cast<HOST*>(get_host());
    if (host == nullptr)
        host = hierarchy_search<HOST>();
    VCML_ERROR_ON(!host, "write callback has no host");
    writefn fn = std::bind(wr, host, std::placeholders::_1);
    on_write(fn);
}

template <typename DATA, size_t N>
template <typename HOST>
void reg<DATA, N>::on_write(void (HOST::*wr)(DATA, size_t), HOST* host) {
    if (host == nullptr)
        host = dynamic_cast<HOST*>(get_host());
    if (host == nullptr)
        host = hierarchy_search<HOST>();
    VCML_ERROR_ON(!host, "tagged write callback has no host");
    writefn_tagged fn = std::bind(wr, host, std::placeholders::_1,
                                  std::placeholders::_2);
    on_write(fn);
}

template <typename DATA, size_t N>
void reg<DATA, N>::on_write_mask(DATA mask) {
    on_write([this, mask](DATA val) -> void {
        *this = (*this & ~mask) | (val & mask);
    });
}

template <typename DATA, size_t N>
void reg<DATA, N>::on_write_mask(const array<DATA, N>& mask) {
    on_write([this, mask](DATA val, size_t i) -> void {
        current_bank(i) = (current_bank(i) & ~mask[i]) | (val & mask[i]);
    });
}

template <typename DATA, size_t N>
void reg<DATA, N>::read_zero() {
    on_read([]() -> DATA { return 0; });
}

template <typename DATA, size_t N>
void reg<DATA, N>::ignore_write() {
    on_write([](DATA val) -> void { (void)val; });
}

template <typename DATA, size_t N>
const DATA& reg<DATA, N>::bank(int bk) const {
    return bank(bk, 0);
}

template <typename DATA, size_t N>
DATA& reg<DATA, N>::bank(int bk) {
    return bank(bk, 0);
}

template <typename DATA, size_t N>
const DATA& reg<DATA, N>::bank(int bk, size_t idx) const {
    VCML_ERROR_ON(idx >= N, "index %zu out of bounds", idx);
    if (bk == 0 || !m_banked)
        return property<DATA, N>::get(idx);
    if (!stl_contains(m_banks, bk))
        return property<DATA, N>::get_default();
    return m_banks.at(bk)[idx];
}

template <typename DATA, size_t N>
DATA& reg<DATA, N>::bank(int bk, size_t idx) {
    VCML_ERROR_ON(idx >= N, "index %zu out of bounds", idx);
    if (bk == 0 || !m_banked)
        return property<DATA, N>::get(idx);
    if (!stl_contains(m_banks, bk))
        init_bank(bk);
    return m_banks[bk][idx];
}

template <typename DATA, size_t N>
const DATA& reg<DATA, N>::current_bank(size_t idx) const {
    return bank(current_cpu(), idx);
}

template <typename DATA, size_t N>
DATA& reg<DATA, N>::current_bank(size_t idx) {
    return bank(current_cpu(), idx);
}

template <typename DATA, size_t N>
reg<DATA, N>::reg(const string& nm, u64 addr, DATA def):
    reg(VCML_AS_DEFAULT, nm, addr, def) {
}

template <typename DATA, size_t N>
reg<DATA, N>::reg(address_space a, const string& nm, u64 addr, DATA d):
    reg_base(a, nm, addr, sizeof(DATA), N),
    property<DATA, N>(nm.c_str(), d),
    m_banked(false),
    m_init(),
    m_banks(),
    m_read(),
    m_write(),
    m_read_tagged(),
    m_write_tagged() {
    for (size_t i = 0; i < N; i++)
        m_init[i] = property<DATA, N>::get(i);
}

template <typename DATA, size_t N>
reg<DATA, N>::reg(const tlm_target_socket& socket, const string& name,
                  u64 addr, DATA data):
    reg<DATA, N>(socket.as, name, addr, data) {
}

template <typename DATA, size_t N>
reg<DATA, N>::~reg() {
    for (auto bank : m_banks)
        delete[] bank.second;
}

template <typename DATA, size_t N>
void reg<DATA, N>::reset() {
    for (size_t i = 0; i < N; i++)
        property<DATA, N>::set(m_init[i], i);

    for (auto bank : m_banks) {
        for (unsigned int i = 0; i < N; i++)
            m_banks[bank.first][i] = m_init[i];
    }
}

template <typename DATA, size_t N>
void reg<DATA, N>::do_read(const range& txaddr, void* ptr, bool debug) {
    range addr(txaddr);
    unsigned char* dest = (unsigned char*)ptr;

    while (addr.start <= addr.end) {
        u64 idx = addr.start / sizeof(DATA);
        u64 off = addr.start % sizeof(DATA);
        u64 size = min(addr.length(), (u64)sizeof(DATA));

        DATA val;

        if (m_read_tagged)
            val = m_read_tagged(N > 1 ? idx : tag);
        else if (m_read)
            val = m_read();
        else
            val = current_bank(idx);

        if (!debug && is_writeback())
            current_bank(idx) = val;

        unsigned char* ptr = (unsigned char*)&val + off;
        memcpy(dest, ptr, size);

        addr.start += size;
        dest += size;
    }
}

template <typename DATA, size_t N>
void reg<DATA, N>::do_write(const range& txaddr, const void* data,
                            bool debug) {
    range addr(txaddr);
    const unsigned char* src = (const unsigned char*)data;

    while (addr.start <= addr.end) {
        u64 idx = addr.start / sizeof(DATA);
        u64 off = addr.start % sizeof(DATA);
        u64 size = min(addr.length(), (u64)sizeof(DATA));

        DATA val = current_bank(idx);

        unsigned char* ptr = (unsigned char*)&val + off;
        memcpy(ptr, src, size);

        if (m_write_tagged)
            m_write_tagged(val, N > 1 ? idx : tag);
        else if (m_write)
            m_write(val);
        else
            current_bank(idx) = val;

        addr.start += size;
        src += size;
    }
}

template <typename DATA, size_t N>
reg<DATA, N>::operator DATA() const {
    return current_bank();
}

template <typename DATA, size_t N>
reg<DATA, N>::operator DATA&() {
    return current_bank();
}

template <typename DATA, size_t N>
const DATA& reg<DATA, N>::operator[](size_t idx) const {
    return current_bank(idx);
}

template <typename DATA, size_t N>
DATA& reg<DATA, N>::operator[](size_t idx) {
    return current_bank(idx);
}

template <typename DATA, size_t N>
DATA reg<DATA, N>::operator++(int unused) {
    (void)unused;
    DATA result = current_bank();

    for (size_t i = 0; i < N; i++)
        ++current_bank(i);

    return result;
}

template <typename DATA, size_t N>
DATA reg<DATA, N>::operator--(int unused) {
    (void)unused;
    DATA result = current_bank();

    for (size_t i = 0; i < N; i++)
        --current_bank(i);

    return result;
}

template <typename DATA, size_t N>
reg<DATA, N>& reg<DATA, N>::operator++() {
    for (size_t i = 0; i < N; i++)
        current_bank(i)++;
    return *this;
}

template <typename DATA, size_t N>
reg<DATA, N>& reg<DATA, N>::operator--() {
    for (size_t i = 0; i < N; i++)
        current_bank(i)--;
    return *this;
}

template <typename DATA, size_t N>
inline reg<DATA, N>& reg<DATA, N>::operator=(const reg<DATA, N>& r) {
    for (size_t i = 0; i < N; i++)
        current_bank(i) = r.current_bank(i);
    return *this;
}

template <typename DATA, size_t N>
template <typename T>
inline reg<DATA, N>& reg<DATA, N>::operator=(const reg<T, N>& r) {
    for (size_t i = 0; i < N; i++)
        current_bank(i) = r.current_bank(i);
    return *this;
}

template <typename DATA, size_t N>
template <typename T>
reg<DATA, N>& reg<DATA, N>::operator=(const T& value) {
    for (size_t i = 0; i < N; i++)
        current_bank(i) = value;
    return *this;
}

template <typename DATA, size_t N>
template <typename T>
reg<DATA, N>& reg<DATA, N>::operator|=(const T& value) {
    for (size_t i = 0; i < N; i++)
        current_bank(i) |= value;
    return *this;
}

template <typename DATA, size_t N>
template <typename T>
reg<DATA, N>& reg<DATA, N>::operator&=(const T& value) {
    for (size_t i = 0; i < N; i++)
        current_bank(i) &= value;
    return *this;
}

template <typename DATA, size_t N>
template <typename T>
reg<DATA, N>& reg<DATA, N>::operator^=(const T& value) {
    for (size_t i = 0; i < N; i++)
        current_bank(i) ^= value;
    return *this;
}

template <typename DATA, size_t N>
template <typename T>
reg<DATA, N>& reg<DATA, N>::operator+=(const T& value) {
    for (size_t i = 0; i < N; i++)
        current_bank(i) += value;
    return *this;
}

template <typename DATA, size_t N>
template <typename T>
reg<DATA, N>& reg<DATA, N>::operator-=(const T& value) {
    for (size_t i = 0; i < N; i++)
        current_bank(i) -= value;
    return *this;
}

template <typename DATA, size_t N>
template <typename T>
reg<DATA, N>& reg<DATA, N>::operator*=(const T& value) {
    for (size_t i = 0; i < N; i++)
        current_bank(i) *= value;
    return *this;
}

template <typename DATA, size_t N>
template <typename T>
reg<DATA, N>& reg<DATA, N>::operator/=(const T& value) {
    for (size_t i = 0; i < N; i++)
        current_bank(i) /= value;
    return *this;
}

template <typename DATA, size_t N>
template <typename T>
inline bool reg<DATA, N>::operator==(const T& other) const {
    for (size_t i = 0; i < N; i++)
        if (current_bank(i) != other)
            return false;
    return true;
}

template <typename DATA, size_t N>
template <typename T>
inline bool reg<DATA, N>::operator<(const T& other) const {
    for (size_t i = 0; i < N; i++)
        if (current_bank(i) >= other)
            return false;
    return true;
}

template <typename DATA, size_t N>
template <typename T>
inline bool reg<DATA, N>::operator>(const T& other) const {
    for (size_t i = 0; i < N; i++)
        if (current_bank(i) <= other)
            return false;
    return true;
}

template <typename DATA, size_t N>
template <typename T>
inline bool reg<DATA, N>::operator!=(const T& other) const {
    return !operator==(other);
}

template <typename DATA, size_t N>
template <typename T>
inline bool reg<DATA, N>::operator<=(const T& other) const {
    return !operator>(other);
}

template <typename DATA, size_t N>
template <typename T>
inline bool reg<DATA, N>::operator>=(const T& other) const {
    return !operator<(other);
}

template <typename DATA, size_t N>
template <typename F>
inline DATA reg<DATA, N>::get_field() const {
    return vcml::get_field<F>(current_bank());
}

template <typename DATA, size_t N>
template <typename F>
inline void reg<DATA, N>::set_field() {
    for (size_t i = 0; i < N; i++)
        vcml::set_field<F>(current_bank(i));
}

template <typename DATA, size_t N>
template <typename F>
inline void reg<DATA, N>::set_field(DATA val) {
    for (size_t i = 0; i < N; i++)
        vcml::set_field<F>(current_bank(i), val);
}

template <typename DATA, size_t N>
template <const DATA BIT>
inline void reg<DATA, N>::set_bit(bool set) {
    for (size_t i = 0; i < N; i++)
        vcml::set_bit<BIT>(current_bank(i), set);
}

template <typename DATA, size_t N>
void reg<DATA, N>::init_bank(int bank) {
    VCML_ERROR_ON(!m_banked, "cannot create banks in register %s", name());

    if (bank == 0 || stl_contains(m_banks, bank))
        return;

    m_banks[bank] = new DATA[N];
    for (size_t i = 0; i < N; i++)
        m_banks[bank][i] = m_init[i];
}

} // namespace vcml

#define VCML_LOG_REG_BIT_CHANGE(bit, reg, val)                       \
    do {                                                             \
        if ((reg & bit) != (val & bit))                              \
            log_debug(#bit " bit %s", val& bit ? "set" : "cleared"); \
    } while (0)

#define VCML_LOG_REG_FIELD_CHANGE(field, reg, val)                        \
    do {                                                                  \
        vcml::u64 from = vcml::get_field<field>(reg);                     \
        vcml::u64 to = vcml::get_field<field>(val);                       \
        if (from != to)                                                   \
            log_debug(#field " changed from 0x%llx to 0x%llx", from, to); \
    } while (0)

#endif
