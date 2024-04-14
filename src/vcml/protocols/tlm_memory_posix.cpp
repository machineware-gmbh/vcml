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

#include <sys/mman.h>
#include <sys/stat.h>
#include <unistd.h>
#include <fcntl.h>

namespace vcml {

int tlm_memory::init_shared(const string& shared, size_t size) {
    VCML_ERROR_ON(is_shared(), "shared memory already initialized");
    m_shared = shared;
    int fd = shm_open(m_shared.c_str(), O_RDWR | O_CREAT | O_EXCL, 0600);
    if (fd < 0 && errno != EEXIST) {
        VCML_ERROR("cannot access shared memory '%s': %s", m_shared.c_str(),
                   strerror(errno));
    }

    if (fd >= 0) {
        int res = ftruncate(fd, m_size);
        VCML_ERROR_ON(res, "ftruncate failed: %s", strerror(errno));
        return fd;
    }

    fd = shm_open(m_shared.c_str(), O_RDWR, 0600);
    VCML_ERROR_ON(fd < 0, "cannot access shared memory '%s': %s",
                  m_shared.c_str(), strerror(errno));

    struct stat stat {};
    VCML_ERROR_ON(fstat(fd, &stat), "fstat failed: %s", strerror(errno));
    if ((size_t)stat.st_size != m_size) {
        VCML_ERROR(
            "shared memory '%s' has unexpected size: expected %zu, actual %zu",
            m_shared.c_str(), m_size, (size_t)stat.st_size);
    }

    return fd;
}

tlm_memory::tlm_memory():
    tlm_dmi(), m_handle(), m_base(), m_size(0), m_discard(false), m_shared() {
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
    other.m_handle = nullptr;
    other.m_base = nullptr;
    other.m_size = 0;
    other.free();
}

tlm_memory::~tlm_memory() {
    free();
}

void tlm_memory::init(const string& shared, size_t size, alignment al) {
    VCML_ERROR_ON(m_size, "memory already initialized");

    // mmap automatically aligns up to host page size, for larger alignments
    // we reserve extra space to include an aligned start address plus size
    u64 extra = (al > host_page_alignment()) ? (1ull << al) - 1 : 0;
    m_size = size + extra;

    int fd = -1;
    if (!shared.empty())
        fd = init_shared(shared, m_size);

    int perms = PROT_READ | PROT_WRITE;
    int flags = MAP_NORESERVE;
    if (is_shared())
        flags |= MAP_SHARED;
    else
        flags |= MAP_PRIVATE | MAP_ANON;

    m_base = mmap(0, m_size, perms, flags, fd, 0);
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

    if (!m_shared.empty())
        shm_unlink(m_shared.c_str());

    m_shared = "";
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
