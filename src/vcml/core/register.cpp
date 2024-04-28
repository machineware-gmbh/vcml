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

int reg_base::current_cpu() const {
    return m_host->current_cpu();
}

reg_base::reg_base(address_space space, const string& regname, u64 addr,
                   u64 cell_size, u64 cell_count):
    sc_object(regname.c_str()),
    m_cell_size(cell_size),
    m_cell_count(cell_count),
    m_range(addr, addr + cell_size * cell_count - 1),
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
    tag() {
    VCML_ERROR_ON(m_cell_size == 0, "register cell size cannot be 0");
    VCML_ERROR_ON(m_cell_count == 0, "register cell count cannot be 0");
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

void reg_base::do_receive(tlm_generic_payload& tx, const tlm_sbi& info) {
    if (tx.get_data_length() > 0) {
        unsigned char* ptr = tx.get_data_ptr();
        if (m_host->endian != host_endian()) // i.e. if big endian
            memswap(ptr, tx.get_data_length());

        if (tx.is_read())
            do_read(tx, ptr, info.is_debug);
        if (tx.is_write())
            do_write(tx, ptr, info.is_debug);

        if (m_host->endian != host_endian()) // i.e. swap back
            memswap(ptr, tx.get_data_length());
    }

    tx.set_response_status(TLM_OK_RESPONSE);
}

unsigned int reg_base::receive(tlm_generic_payload& tx, const tlm_sbi& info) {
    u64 addr = tx.get_address();
    u64 size = tx.get_data_length();
    u64 strw = tx.get_streaming_width();
    u8* data = tx.get_data_ptr();

    VCML_ERROR_ON(strw != size, "invalid transaction streaming setup");
    VCML_ERROR_ON(!m_range.overlaps(tx), "invalid register access");

    tlm_response_status rs = check_access(tx, info);

    range span = m_range.intersect(tx);

    tx.set_address(span.start - m_range.start);
    tx.set_data_ptr(data + span.start - addr);
    tx.set_streaming_width(span.length());
    tx.set_data_length(span.length());

    if (!info.is_debug) {
        if (tx.is_read() && m_rsync)
            m_host->sync();
        if (tx.is_write() && m_wsync)
            m_host->sync();
    }

    m_host->trace_fw(*this, tx, m_host->local_time());

    if (failed(rs))
        tx.set_response_status(rs);
    else
        do_receive(tx, info);

    m_host->trace_bw(*this, tx, m_host->local_time());

    tx.set_address(addr);
    tx.set_data_length(size);
    tx.set_streaming_width(strw);
    tx.set_data_ptr(data);

    return tx.is_response_ok() ? span.length() : 0;
}

} // namespace vcml
