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

#ifndef VCML_PROTOCOLS_TLM_MEMORY_H
#define VCML_PROTOCOLS_TLM_MEMORY_H

#include "vcml/common/types.h"
#include "vcml/common/report.h"
#include "vcml/common/range.h"
#include "vcml/common/systemc.h"

#include "vcml/protocols/tlm_sbi.h"

namespace vcml {

class tlm_memory : public tlm_dmi
{
private:
    void* m_base;
    size_t m_size;
    bool m_discard;

public:
    u8* data() const { return get_dmi_ptr(); }
    size_t size() const;

    void allow_read_only() { allow_read(); }
    void allow_write_only() { allow_write(); }

    void discard_writes(bool discard = true) { m_discard = discard; }

    tlm_memory();
    tlm_memory(size_t size, alignment al = VCML_ALIGN_NONE);
    tlm_memory(const tlm_memory&) = delete;
    tlm_memory(tlm_memory&& other) noexcept;
    virtual ~tlm_memory();

    void init(size_t size, alignment al);
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

inline size_t tlm_memory::size() const {
    return get_end_address() - get_start_address() + 1;
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
