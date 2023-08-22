/******************************************************************************
 *                                                                            *
 * Copyright (C) 2022 MachineWare GmbH                                        *
 * All Rights Reserved                                                        *
 *                                                                            *
 * This is work is licensed under the terms described in the LICENSE file     *
 * found in the root directory of this source tree.                           *
 *                                                                            *
 ******************************************************************************/

#ifndef VCML_PROTOCOLS_TLM_MEMORY_H
#define VCML_PROTOCOLS_TLM_MEMORY_H

#include "vcml/core/types.h"
#include "vcml/core/range.h"
#include "vcml/core/systemc.h"

#include "vcml/protocols/tlm_sbi.h"
#include "vcml/protocols/tlm_dmi_cache.h"

namespace vcml {

class tlm_memory : public tlm_dmi
{
private:
    void* m_handle;
    void* m_base;
    size_t m_size;
    bool m_discard;
    string m_shared;

    int init_shared(const string& shared, size_t size);

public:
    u8* data() const { return get_dmi_ptr(); }
    size_t size() const { return dmi_get_size(*this); }

    bool is_shared() const { return !m_shared.empty(); }
    const char* shared_name() const { return m_shared.c_str(); }

    void allow_read_only() { allow_read(); }
    void allow_write_only() { allow_write(); }

    void discard_writes(bool discard = true) { m_discard = discard; }

    tlm_memory();
    tlm_memory(size_t size);
    tlm_memory(size_t size, alignment al);
    tlm_memory(const string& shared, size_t size);
    tlm_memory(const string& shared, size_t size, alignment al);
    tlm_memory(const tlm_memory&) = delete;
    tlm_memory(tlm_memory&& other) noexcept;
    virtual ~tlm_memory();

    void init(size_t size, alignment al);
    void init(const string& shared, size_t size, alignment al);
    void free();
    void fill(u8 data);

    tlm_response_status fill(u8 data, bool debug);

    tlm_response_status read(const range& addr, void* dest,
                             bool debug = false);

    tlm_response_status write(const range& addr, const void* dest,
                              bool debug = false);

    template <typename T>
    tlm_response_status read(u64 addr, T& data, bool debug = false);

    template <typename T>
    tlm_response_status write(u64 addr, const T& data, bool debug = false);

    void transport(tlm_generic_payload& tx, const tlm_sbi& sbi);

    u8 operator[](size_t offset) const;
    u8& operator[](size_t offset);
};

inline void tlm_memory::init(size_t size, alignment al) {
    init("", size, al);
}

inline void tlm_memory::fill(u8 val) {
    memset(data(), val, size());
}

template <typename T>
tlm_response_status tlm_memory::read(u64 addr, T& data, bool dbg) {
    return read({ addr, addr + sizeof(data) - 1 }, &data, dbg);
}

template <typename T>
tlm_response_status tlm_memory::write(u64 addr, const T& data, bool dbg) {
    return write({ addr, addr + sizeof(data) - 1 }, &data, dbg);
}

inline u8 tlm_memory::operator[](size_t offset) const {
    VCML_ERROR_ON(data() == nullptr, "memory not initialized");
    VCML_ERROR_ON(offset >= size(), "offset out of bounds: %zu", offset);
    return data()[offset];
}

inline u8& tlm_memory::operator[](size_t offset) {
    VCML_ERROR_ON(data() == nullptr, "memory not initialized");
    VCML_ERROR_ON(offset >= size(), "offset out of bounds: %zu", offset);
    return data()[offset];
}

} // namespace vcml

#endif
