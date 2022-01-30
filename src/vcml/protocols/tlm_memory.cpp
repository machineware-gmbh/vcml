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

#include "vcml/protocols/tlm_memory.h"

#include <sys/mman.h>

namespace vcml {

    tlm_memory::tlm_memory():
        tlm_dmi(),
        m_base(nullptr),
        m_size(0),
        m_discard(false) {
    }

    tlm_memory::tlm_memory(size_t size, alignment align):
        tlm_memory() {
        init(size, align);
    }

    tlm_memory::tlm_memory(tlm_memory&& other):
        tlm_dmi(std::move(other)),
        m_base(other.m_base),
        m_size(other.m_size),
        m_discard(other.m_discard) {
        other.m_base = nullptr;
        other.m_size = 0;
        other.free();
    }

    tlm_memory::~tlm_memory() {
        free();
    }

    void tlm_memory::init(size_t size, alignment al) {
        VCML_ERROR_ON(m_size, "memory already initialized");

        // mmap automatically aligns up to 4k, for larger alignments we
        // reserve extra space to include an aligned start address plus size
        u64 extra = (al > VCML_ALIGN_4K) ? (1ull << al) - 1 : 0;
        m_size = size + extra;

        const int perms = PROT_READ | PROT_WRITE;
        const int flags = MAP_PRIVATE | MAP_ANON | MAP_NORESERVE;

        m_base = mmap(0, m_size, perms, flags, -1, 0);
        VCML_ERROR_ON(m_base == MAP_FAILED, "mmap failed: %s", strerror(errno));
        u8* ptr = (u8*)(((u64)m_base + extra) & ~extra);
        VCML_ERROR_ON(!is_aligned(ptr, al), "memory alignment failed");

        tlm_dmi::init();
        set_dmi_ptr(ptr);
        set_start_address(0);
        set_end_address(size - 1);
        allow_read_write();
    }

    void tlm_memory::free() {
        if (m_base != nullptr) {
            int ret = munmap(m_base, m_size);
            VCML_ERROR_ON(ret, "munmap failed: %d", ret);
        }

        m_base = nullptr;
        m_size = 0;

        tlm_dmi::init();
    }

    tlm_response_status tlm_memory::fill(u8 data, bool debug) {
        if (!is_write_allowed() && !debug)
            return m_discard ? TLM_OK_RESPONSE : TLM_COMMAND_ERROR_RESPONSE;

        fill(data);
        return TLM_OK_RESPONSE;
    }

    tlm_response_status
    tlm_memory::read(const range& addr, void* dest, bool debug) {
        if (addr.end >= m_size)
            return TLM_ADDRESS_ERROR_RESPONSE;

        if (!debug && !is_read_allowed())
            return TLM_COMMAND_ERROR_RESPONSE;

        memcpy(dest, data() + addr.start, addr.length());
        return TLM_OK_RESPONSE;
    }

    tlm_response_status
    tlm_memory::write(const range& addr, const void* src, bool debug) {
        if (addr.end >= m_size)
            return TLM_ADDRESS_ERROR_RESPONSE;

        if (!debug) {
            if (m_discard)
                return TLM_OK_RESPONSE;

            if (!is_write_allowed())
                return TLM_COMMAND_ERROR_RESPONSE;
        }

        memcpy(data() + addr.start, src, addr.length());
        return TLM_OK_RESPONSE;
    }

    void tlm_memory::transport(tlm_generic_payload& tx, const tlm_sbi& sbi) {
        tlm_response_status res;
        range addr(tx);

        if (tx.is_read())
            res = read(addr, tx.get_data_ptr(), sbi.is_debug);
        if (tx.is_write())
            res = write(addr, tx.get_data_ptr(), sbi.is_debug);

        tx.set_response_status(res);
    }

}
