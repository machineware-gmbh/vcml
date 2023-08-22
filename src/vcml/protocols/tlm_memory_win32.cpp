/******************************************************************************
 *                                                                            *
 * Copyright (C) 2022 MachineWare GmbH                                        *
 * All Rights Reserved                                                        *
 *                                                                            *
 * This is work is licensed under the terms described in the LICENSE file     *
 * found in the root directory of this source tree.                           *
 *                                                                            *
 ******************************************************************************/

#include "vcml/protocols/tlm_memory.h"

#include <Windows.h>

namespace vcml {

int tlm_memory::init_shared(const string& shared, size_t size) {
    VCML_ERROR_ON(is_shared(), "shared memory already initialized");
    m_shared = shared;
    // mkstr("Global\\%s", shared.c_str());

    DWORD szhi = (DWORD)(size >> 32);
    DWORD szlo = (DWORD)(size >> 0);
    m_handle = CreateFileMapping(INVALID_HANDLE_VALUE, NULL, PAGE_READWRITE,
                                 szhi, szlo, m_shared.c_str());
    if (m_handle == NULL)
        VCML_ERROR("failed to allocate shared memory %s", shared.c_str());

    m_base = MapViewOfFile(m_handle, FILE_MAP_ALL_ACCESS, 0, 0, 0);
    VCML_ERROR_ON(!m_base, "MapViewOfFile failed: %u", GetLastError());

    MEMORY_BASIC_INFORMATION info;
    if (VirtualQuery(m_base, &info, sizeof(info)) != sizeof(info))
        VCML_ERROR("failed to get shared memory size");

    if (info.RegionSize != size) {
        VCML_ERROR(
            "shared memory '%s' has unexpected size: expected %zu, actual %zu",
            m_shared.c_str(), m_size, (size_t)info.RegionSize);
    }

    return 0;
}

tlm_memory::tlm_memory():
    tlm_dmi(),
    m_handle(nullptr),
    m_base(nullptr),
    m_size(0),
    m_discard(false),
    m_shared() {
}

tlm_memory::tlm_memory(size_t size): tlm_memory() {
    init(size, VCML_ALIGN_NONE);
}

tlm_memory::tlm_memory(size_t size, alignment align): tlm_memory() {
    init(size, align);
}

tlm_memory::tlm_memory(const string& shared, size_t size): tlm_memory() {
    init(shared, size, VCML_ALIGN_NONE);
}

tlm_memory::tlm_memory(const string& shared, size_t size, alignment align):
    tlm_memory() {
    init(shared, size, align);
}

tlm_memory::tlm_memory(tlm_memory&& other) noexcept:
    tlm_dmi(std::move(other)),
    m_handle(other.m_handle),
    m_base(other.m_base),
    m_size(other.m_size),
    m_discard(other.m_discard) {
    other.m_handle = INVALID_HANDLE_VALUE;
    other.m_base = nullptr;
    other.m_size = 0;
    other.free();
}

tlm_memory::~tlm_memory() {
    free();
}

void tlm_memory::init(const string& shared, size_t size, alignment al) {
    VCML_ERROR_ON(m_size, "memory already initialized");

    // mmap automatically aligns up to 4k, for larger alignments we
    // reserve extra space to include an aligned start address plus size
    u64 extra = (al > VCML_ALIGN_4K) ? (1ull << al) - 1 : 0;
    m_size = size + extra;

    if (shared.empty()) {
        m_base = VirtualAlloc(NULL, m_size, MEM_COMMIT, PAGE_READWRITE);
        VCML_ERROR_ON(!m_base, "VirtualAlloc failed: %u", GetLastError());
    } else {
        init_shared(shared, m_size);
    }

    u8* ptr = (u8*)(((u64)m_base + extra) & ~extra);
    VCML_ERROR_ON(!is_aligned(ptr, al), "memory alignment failed");

    tlm_dmi::init();
    set_dmi_ptr(ptr);
    set_start_address(0);
    set_end_address(size - 1);
    allow_read_write();
}

void tlm_memory::free() {
    if (m_handle) {
        if (m_base)
            UnmapViewOfFile(m_base);
        CloseHandle(m_handle);

        m_handle = INVALID_HANDLE_VALUE;
        m_base = nullptr;
    }

    if (m_base)
        VirtualFree(m_base, 0, MEM_RELEASE);

    m_shared = "";
    m_size = 0;

    tlm_dmi::init();
}

tlm_response_status tlm_memory::fill(u8 data, bool debug) {
    if (!is_write_allowed() && !debug)
        return m_discard ? TLM_OK_RESPONSE : TLM_COMMAND_ERROR_RESPONSE;

    fill(data);
    return TLM_OK_RESPONSE;
}

tlm_response_status tlm_memory::read(const range& addr, void* dest,
                                     bool debug) {
    if (addr.end >= m_size)
        return TLM_ADDRESS_ERROR_RESPONSE;

    if (!debug && !is_read_allowed())
        return TLM_COMMAND_ERROR_RESPONSE;

    memcpy(dest, data() + addr.start, addr.length());
    return TLM_OK_RESPONSE;
}

tlm_response_status tlm_memory::write(const range& addr, const void* src,
                                      bool debug) {
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
    tlm_response_status res = TLM_OK_RESPONSE;
    range addr(tx);

    if (tx.is_read())
        res = read(addr, tx.get_data_ptr(), sbi.is_debug);
    if (tx.is_write())
        res = write(addr, tx.get_data_ptr(), sbi.is_debug);

    tx.set_response_status(res);
}

} // namespace vcml
