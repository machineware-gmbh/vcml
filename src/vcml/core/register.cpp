/******************************************************************************
 *                                                                            *
 * Copyright (C) 2022 MachineWare GmbH                                        *
 * All Rights Reserved                                                        *
 *                                                                            *
 * This is work is licensed under the terms described in the LICENSE file     *
 * found in the root directory of this source tree.                           *
 *                                                                            *
 ******************************************************************************/

#include "vcml/core/register.h"
#include "vcml/core/peripheral.h"

namespace vcml {

void reg_base::set_access_size(u64 min, u64 max) {
    VCML_ERROR_ON(min > max, "invalid access size specification");
    VCML_ERROR_ON(max > m_cell_size, "maximum access size exceeded");
    m_minsize = min;
    m_maxsize = max;
}

endianess reg_base::get_endian() const {
    return m_host->endian;
}

int reg_base::current_cpu() const {
    return m_host->current_cpu();
}

reg_base::reg_base(address_space space, const string& regname, u64 addr,
                   u64 cell_size, u64 cell_count, u64 cell_stride):
    sc_object(regname.c_str()),
    m_addr(addr),
    m_cell_size(cell_size),
    m_cell_count(cell_count),
    m_cell_stride(cell_stride),
    m_access(VCML_ACCESS_READ_WRITE),
    m_rsync(false),
    m_wsync(false),
    m_wback(true),
    m_aligned(false),
    m_secure(false),
    m_privilege(0),
    m_minsize(0),
    m_maxsize(-1),
    m_host(hierarchy_search<peripheral>()),
    as(space),
    tag(),
    log(m_host ? m_host->log : vcml::log) {
    VCML_ERROR_ON(m_cell_size == 0, "register cell size cannot be 0");
    VCML_ERROR_ON(m_cell_count == 0, "register cell count cannot be 0");
    VCML_ERROR_ON(m_cell_stride < m_cell_size, "cell stride less than size");
    VCML_ERROR_ON(!m_host, "register '%s' outside peripheral", name());
    m_host->add_register(this);
}

reg_base::~reg_base() {
    if (m_host)
        m_host->remove_register(this);
}

tlm_response_status reg_base::check_access(const tlm_generic_payload& tx,
                                           const tlm_sbi& info) const {
    if (info.is_debug)
        return TLM_OK_RESPONSE;

    if (tx.is_read() && !is_readable())
        return TLM_COMMAND_ERROR_RESPONSE;

    if (tx.is_write() && !is_writeable())
        return TLM_COMMAND_ERROR_RESPONSE;

    if (tx.get_data_length() < m_minsize || tx.get_data_length() > m_maxsize)
        return TLM_COMMAND_ERROR_RESPONSE;

    if (m_aligned && (m_minsize > 0) && (tx.get_address() % m_minsize))
        return TLM_COMMAND_ERROR_RESPONSE;

    if (m_privilege > info.privilege)
        return TLM_COMMAND_ERROR_RESPONSE;

    if (m_secure && !info.is_secure)
        return TLM_COMMAND_ERROR_RESPONSE;

    return TLM_OK_RESPONSE;
}

unsigned int reg_base::do_receive(tlm_generic_payload& tx,
                                  const tlm_sbi& info) {
    unsigned int nbytes = 0;
    u64 addr = tx.get_address();
    u64 end = addr + tx.get_data_length();

    while (addr < end) {
        size_t idx = addr / m_cell_stride;
        u64 offset = addr % m_cell_stride;

        if (offset < m_cell_size) {
            u8* data = tx.get_data_ptr() + addr - tx.get_address();
            u64 len = min(m_cell_size - offset, end - addr);
            if (tx.is_read())
                do_read(idx, offset, len, data, info.is_debug);
            if (tx.is_write())
                do_write(idx, offset, len, data, info.is_debug);
            nbytes += len;
        }

        addr = (idx + 1) * m_cell_stride;
    }

    if (nbytes > 0)
        tx.set_response_status(TLM_OK_RESPONSE);

    return nbytes;
}

bool reg_base::overlaps(const range& r) const {
    u64 limit = get_limit();
    if (r.end < m_addr || r.start > limit)
        return false;

    if (r.length() > m_cell_stride)
        return true;

    u64 start = (r.start < m_addr) ? m_addr : r.start;
    u64 end = (r.end > limit) ? limit : r.end;

    u64 off_start = start - m_addr;
    u64 off_end = end - m_addr;

    u64 idx_start = off_start / m_cell_stride;
    u64 idx_end = off_end / m_cell_stride;

    return (idx_start != idx_end) ||
           ((off_start % m_cell_stride) < m_cell_size);
}

bool reg_base::overlaps(const reg_base& r) const {
    u64 limit = get_limit();
    u64 r_limit = r.get_limit();

    if (r_limit < m_addr || r.m_addr > limit)
        return false;

    if (r.m_cell_size == r.m_cell_stride)
        return overlaps(range(r.m_addr, r_limit));

    for (u64 i = 0; i < r.m_cell_count; i++) {
        u64 start = r.m_addr + i * r.m_cell_stride;
        if (overlaps(range(start, start + r.m_cell_size - 1)))
            return true;
    }

    return false;
}

unsigned int reg_base::receive(tlm_generic_payload& tx, const tlm_sbi& info) {
    unsigned int nbytes = 0;

    u64 addr = tx.get_address();
    u64 size = tx.get_data_length();
    u64 strw = tx.get_streaming_width();
    u8* data = tx.get_data_ptr();

    VCML_ERROR_ON(strw != size, "invalid transaction streaming setup");
    VCML_ERROR_ON(!overlaps(tx), "invalid register access");

    tlm_response_status rs = check_access(tx, info);

    u64 start = max(addr, m_addr);
    u64 limit = min(addr + size, get_limit() + 1);

    u64 offset = start - m_addr;
    u64 length = limit - start;

    if (!info.is_debug) {
        if (tx.is_read() && m_rsync)
            m_host->sync();
        if (tx.is_write() && m_wsync)
            m_host->sync();
    }

    tx.set_address(offset);
    tx.set_data_ptr(data + start - addr);
    tx.set_streaming_width(length);
    tx.set_data_length(length);

    m_host->trace_fw(*this, tx, m_host->local_time());

    if (failed(rs))
        tx.set_response_status(rs);
    else
        nbytes += do_receive(tx, info);

    m_host->trace_bw(*this, tx, m_host->local_time());

    tx.set_address(addr);
    tx.set_data_length(size);
    tx.set_streaming_width(strw);
    tx.set_data_ptr(data);

    return nbytes;
}

string reg_base::str() {
    stringstream ss;
    if (m_cell_size > sizeof(u64))
        return "unknown";

    for (u64 i = 0; i < m_cell_count; i++) {
        u64 val = 0;
        do_read(i, 0, m_cell_size, (u8*)&val, true);
        ss << mkstr("0x%0*llx", (int)m_cell_size * 2, val);
        if (i < m_cell_count - 1)
            ss << " ";
    }

    return ss.str();
}

void reg_base::str(const string& s) {
    if (m_cell_size > sizeof(u64))
        return;

    vector<string> args = split(s);
    u64 size = args.size();

    if (size < m_cell_count) {
        log_warn("register %s has not enough initializers", name());
    } else if (size > m_cell_count) {
        log_warn("register %s has too many initializers", name());
    }

    for (u64 i = 0; i < min(m_cell_count, size); i++) {
        u64 val = from_string<u64>(trim(args[i]));
        do_write(i, 0, m_cell_size, (u8*)&val, true);
    }
}

} // namespace vcml
