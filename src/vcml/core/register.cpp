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
    m_natural(false),
    m_secure(0),
    m_privilege(0),
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

void reg_base::do_receive(tlm_generic_payload& tx, const tlm_sbi& info) {
    if (tx.is_read() && !is_readable() && !info.is_debug) {
        tx.set_response_status(TLM_COMMAND_ERROR_RESPONSE);
        return;
    }

    if (tx.is_write() && !is_writeable() && !info.is_debug) {
        tx.set_response_status(TLM_COMMAND_ERROR_RESPONSE);
        return;
    }

    if (!info.is_debug) {
        if (m_privilege > info.privilege) {
            tx.set_response_status(TLM_COMMAND_ERROR_RESPONSE);
            return;
        }

        if (m_secure && !info.is_secure) {
            tx.set_response_status(TLM_COMMAND_ERROR_RESPONSE);
            return;
        }
    }

    unsigned char* ptr = tx.get_data_ptr();
    if (m_host->endian != host_endian()) // i.e. if big endian
        memswap(ptr, tx.get_data_length());

    if (tx.is_read())
        do_read(tx, ptr);
    if (tx.is_write())
        do_write(tx, ptr);

    if (m_host->endian != host_endian()) // i.e. swap back
        memswap(ptr, tx.get_data_length());

    tx.set_response_status(TLM_OK_RESPONSE);
}

unsigned int reg_base::receive(tlm_generic_payload& tx, const tlm_sbi& info) {
    u64 addr = tx.get_address();
    u64 size = tx.get_data_length();
    u64 strw = tx.get_streaming_width();
    u8* data = tx.get_data_ptr();

    VCML_ERROR_ON(strw != size, "invalid transaction streaming setup");
    VCML_ERROR_ON(!m_range.overlaps(tx), "invalid register access");
    VCML_ERROR_ON(m_cell_size == 0, "cell size cannot be zero");

    if (m_natural && (size != m_cell_size || addr % m_cell_size)) {
        tx.set_response_status(TLM_COMMAND_ERROR_RESPONSE);
        return 0;
    }

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
    do_receive(tx, info);
    m_host->trace_bw(*this, tx, m_host->local_time());

    tx.set_address(addr);
    tx.set_data_length(size);
    tx.set_streaming_width(strw);
    tx.set_data_ptr(data);

    return tx.is_response_ok() ? span.length() : 0;
}

} // namespace vcml
