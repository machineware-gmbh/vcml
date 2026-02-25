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

u64 reg_base::get_address(address_space as) const {
    return m_host ? m_host->offset_of(*this, as) : 0;
}

void reg_base::set_access_size(u64 min, u64 max) {
    VCML_ERROR_ON(min > max, "invalid access size specification");
    VCML_ERROR_ON(max > m_cell_size, "maximum access size exceeded");
    m_minsize = min;
    m_maxsize = max;
}

endianess reg_base::get_endian() const {
    return m_host ? m_host->endian : host_endian();
}

int reg_base::current_cpu() const {
    return m_host ? m_host->current_cpu() : SBI_CPUID_DEFAULT;
}

static logger& find_logger() {
    if (auto* parent = hierarchy_search<module>())
        return parent->log;
    return vcml::log;
}

reg_base::reg_base(const string& regname, u64 cell_size, u64 cell_count,
                   u64 cell_stride):
    sc_object(regname.c_str()),
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
    m_maxsize(U64_MAX),
    m_host(hierarchy_search<peripheral>()),
    tag(),
    log(find_logger()) {
    VCML_ERROR_ON(m_cell_size == 0, "register cell size cannot be 0");
    VCML_ERROR_ON(m_cell_count == 0, "register cell count cannot be 0");
    VCML_ERROR_ON(m_cell_stride < m_cell_size, "cell stride less than size");
}

reg_base::reg_base(address_space space, const string& regname, u64 addr,
                   u64 cell_size, u64 cell_count, u64 cell_stride):
    reg_base(regname, cell_size, cell_count, cell_stride) {
    VCML_ERROR_ON(!m_host, "register '%s' outside peripheral", name());
    m_host->add_register(this, addr, space);
}

reg_base::~reg_base() {
    if (m_host)
        m_host->remove_register(this);
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

unsigned int reg_base::receive(tlm_generic_payload& tx, const tlm_sbi& info) {
    u64 addr = tx.get_address();
    u64 size = tx.get_data_length();
    u64 strw = tx.get_streaming_width();

    VCML_ERROR_ON(strw != size, "invalid transaction streaming setup");

    if (addr >= get_size())
        return 0;

    u64 limit = min(addr + size, get_size());
    u64 length = limit - addr;

    if (!info.is_debug) {
        if (tx.is_read() && m_rsync && m_host)
            m_host->sync();
        if (tx.is_write() && m_wsync && m_host)
            m_host->sync();
    }

    tx.set_streaming_width(length);
    tx.set_data_length(length);

    if (m_host)
        m_host->trace_fw(*this, tx, m_host->local_time());

    unsigned int nbytes = do_receive(tx, info);

    if (m_host)
        m_host->trace_bw(*this, tx, m_host->local_time());

    tx.set_data_length(size);
    tx.set_streaming_width(strw);

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

reg_bank::reg_bank(const char* name):
    sc_object(name),
    m_aligned_only(),
    m_natural_only(),
    m_access_size(),
    m_regs() {
    // nothing to do
}

reg_bank::~reg_bank() {
    // nothing to do
}

bool reg_bank::contains(const reg_base& reg) const {
    return offset_of(reg).has_value();
}

bool reg_bank::contains(const string& regnm) const {
    return offset_of(regnm).has_value();
}

optional<u64> reg_bank::offset_of(const reg_base& reg) const {
    auto it = std::find_if(
        m_regs.begin(), m_regs.end(),
        [rr = &reg](const auto& entry) { return entry.second == rr; });
    if (it != m_regs.end())
        return it->first;
    return std::nullopt;
}

optional<u64> reg_bank::offset_of(const string& regnm) const {
    auto it = std::find_if(
        m_regs.begin(), m_regs.end(),
        [&regnm](const auto& entry) { return regnm == entry.second->name(); });
    if (it != m_regs.end())
        return it->first;
    return std::nullopt;
}

void reg_bank::aligned_accesses_only(bool set) {
    m_aligned_only = set;
    for (auto [_, reg] : m_regs)
        reg->aligned_accesses_only(set);
}

void reg_bank::natural_accesses_only(bool set) {
    m_natural_only = set;
    for (auto [_, reg] : m_regs)
        reg->natural_accesses_only(set);
}

void reg_bank::set_access_size(u64 min, u64 max) {
    m_access_size = pair(min, max);
    for (auto [_, reg] : m_regs)
        reg->set_access_size(min, max);
}

void reg_bank::collect(vector<reg_base*>& regs) {
    for (const auto& [addr, reg] : m_regs)
        regs.push_back(reg);
}

void reg_bank::reset() {
    for (const auto& [addr, reg] : m_regs)
        reg->reset();
}

void reg_bank::insert(reg_base* reg, u64 offset) {
    for (const auto& [addr, other] : m_regs) {
        if (overlaps(*reg, offset, *other, addr)) {
            VCML_ERROR(
                "register %s at 0x%llx overlaps with register %s at "
                "0x%llx",
                reg->name(), offset, other->name(), addr);
        }
    }

    m_regs[offset] = reg;

    if (m_aligned_only)
        reg->aligned_accesses_only(*m_aligned_only);
    if (m_natural_only)
        reg->natural_accesses_only(*m_natural_only);
    if (m_access_size)
        reg->set_access_size(m_access_size->first, m_access_size->second);
}

void reg_bank::remove(reg_base* reg) {
    auto it = std::find_if(m_regs.begin(), m_regs.end(),
                           [reg](const auto& it) { return it.second == reg; });
    if (it != m_regs.end())
        m_regs.erase(it);
}

unsigned int reg_bank::receive(tlm_generic_payload& tx, const tlm_sbi& sbi) {
    if (m_regs.empty())
        return 0;

    unsigned int num_bytes = 0;

    u64 addr = tx.get_address();
    u64 size = tx.get_data_length();
    u64 strw = tx.get_streaming_width();
    u8* data = tx.get_data_ptr();

    u64 limit = addr + size;
    auto last = m_regs.lower_bound(limit);

    for (auto it = last; it != m_regs.begin();) {
        --it;

        u64 base = it->first;
        reg_base* reg = it->second;

        if (reg && check_access(*reg, base, tx, sbi)) {
            u64 start = max(addr, base);
            u64 end = min(limit, base + reg->get_size());

            tx.set_address(start - base);
            tx.set_data_length(end - start);
            tx.set_streaming_width(end - start);
            tx.set_data_ptr(data + start - addr);

            num_bytes += reg->receive(tx, sbi);

            tx.set_address(addr);
            tx.set_data_length(size);
            tx.set_streaming_width(strw);
            tx.set_data_ptr(data);

            if (success(tx) && reg->is_natural_accesses_only())
                break;
        }

        if (num_bytes >= size)
            break;
    }

    return num_bytes;
}

bool reg_bank::check_access(const reg_base& reg, u64 off,
                            tlm_generic_payload& tx,
                            const tlm_sbi& sbi) const {
    if (!overlaps(reg, off, tx))
        return false;

    auto rs = reg.check_access(tx, sbi);
    if (failed(rs))
        tx.set_response_status(rs);

    if (failed(tx))
        return false;

    return true;
}

bool reg_bank::overlaps(const reg_base& reg, u64 off, const range& r) const {
    if (r.end < off || r.start >= off + reg.get_size())
        return false;

    if (reg.get_cell_size() == reg.get_cell_stride())
        return true;

    for (u64 i = 0; i < reg.get_cell_count(); i++) {
        range cell;
        cell.start = off + i * reg.get_cell_stride();
        cell.end = cell.start + reg.get_cell_size() - 1;
        if (r.overlaps(cell))
            return true;
    }

    return false;
}

bool reg_bank::overlaps(const reg_base& reg0, u64 off0, const reg_base& reg1,
                        u64 off1) const {
    if (reg0.get_cell_size() == reg0.get_cell_stride())
        return overlaps(reg1, off1, range(off0, off0 + reg0.get_size() - 1));

    if (reg1.get_cell_size() == reg1.get_cell_stride())
        return overlaps(reg0, off0, range(off1, off1 + reg1.get_size() - 1));

    for (u64 i = 0; i < reg0.get_cell_count(); i++) {
        range cell;
        cell.start = off0 + i * reg0.get_cell_stride();
        cell.end = cell.start + reg0.get_cell_size() - 1;
        if (overlaps(reg1, off1, cell))
            return true;
    }

    return false;
}

} // namespace vcml
