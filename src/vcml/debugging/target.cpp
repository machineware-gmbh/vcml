/******************************************************************************
 *                                                                            *
 * Copyright (C) 2022 MachineWare GmbH                                        *
 * All Rights Reserved                                                        *
 *                                                                            *
 * This is work is licensed under the terms described in the LICENSE file     *
 * found in the root directory of this source tree.                           *
 *                                                                            *
 ******************************************************************************/

#include "vcml/core/module.h"
#include "vcml/debugging/target.h"

namespace vcml {
namespace debugging {

bool cpureg::read(void* buf, size_t len) const {
    VCML_ERROR_ON(!host, "cpureg %s has no target", name.c_str());
    if (len < size || !is_readable())
        return false;
    return host->read_cpureg_dbg(*this, buf, size);
}

bool cpureg::write(const void* buf, size_t len) const {
    VCML_ERROR_ON(!host, "cpureg %s has no target", name.c_str());
    if (len < size || !is_writeable())
        return false;
    return host->write_cpureg_dbg(*this, buf, size);
}

unordered_map<string, target*> target::s_targets;

void target::define_cpureg(size_t regno, const string& name, size_t size,
                           int prot) {
    define_cpureg(regno, name, size, 1, prot);
}

void target::define_cpureg(size_t regno, const string& name, size_t size,
                           size_t count, int prot) {
    const cpureg* other = find_cpureg(regno);
    if (other != nullptr) {
        if (other->name == name)
            VCML_ERROR("cpureg %s already defined", name.c_str());
        VCML_ERROR("regno %zu already used by %s", regno, other->name.c_str());
    }

    cpureg& newreg = m_cpuregs[regno];
    newreg.regno = regno;
    newreg.size = size;
    newreg.count = count;
    newreg.prot = prot;
    newreg.name = name;
    newreg.host = this;
}

target::target():
    m_mtx(),
    m_name(),
    m_suspendable(true),
    m_running(true),
    m_endian(ENDIAN_UNKNOWN),
    m_cpuregs(),
    m_symbols(),
    m_steppers(),
    m_breakpoints(),
    m_watchpoints() {
    module* host = hierarchy_search<module>();
    VCML_ERROR_ON(!host, "debug target declared outside module");
    m_name = host->name();

    if (stl_contains(s_targets, m_name))
        VCML_ERROR("debug target '%s' already exists", m_name.c_str());
    s_targets[m_name] = this;
}

target::~target() {
    for (auto bp : m_breakpoints)
        delete bp;
    for (auto wp : m_watchpoints)
        delete wp;
    s_targets.erase(m_name);
}

vector<cpureg> target::cpuregs() const {
    vector<cpureg> regs;
    regs.reserve(m_cpuregs.size());
    for (auto& reg : m_cpuregs)
        regs.push_back(reg.second);
    return regs;
}

const cpureg* target::find_cpureg(size_t regno) const {
    auto it = m_cpuregs.find(regno);
    return it != m_cpuregs.end() ? &(it->second) : nullptr;
}

const cpureg* target::find_cpureg(const string& name) const {
    if (name.empty())
        return nullptr;

    string lname = to_lower(name); // lookup is case-insensitive
    bool grouped = name.rfind('/') != string::npos;

    for (auto& it : m_cpuregs) {
        string regname = to_lower(it.second.name);
        if (!grouped) {
            size_t l = regname.rfind('/');
            if (l != string::npos && l < regname.length() - 1)
                regname = regname.substr(l + 1);
        }

        if (regname == lname)
            return &(it.second);
    }

    return nullptr;
}

bool target::read_cpureg_dbg(const cpureg& reg, void* buf, size_t len) {
    return false; // to be overloaded
}

bool target::write_cpureg_dbg(const cpureg& reg, const void* buf, size_t len) {
    return false; // to be overloaded
}

u64 target::read_pmem_dbg(u64 addr, void* buffer, u64 size) {
    return 0; // to be overloaded
}

u64 target::write_pmem_dbg(u64 addr, const void* buffer, u64 size) {
    return 0; // to be overloaded
}

u64 target::read_vmem_dbg(u64 addr, void* buffer, u64 size) {
    u64 pgsz = 0;
    if (!page_size(pgsz))
        return read_pmem_dbg(addr, buffer, size);

    u64 count = 0;
    u64 end = addr + size;
    u8* dest = (u8*)buffer;

    while (addr < end) {
        u64 pa = 0;
        u64 todo = min(end - addr, pgsz - (addr % pgsz));
        if (virt_to_phys(addr, pa))
            count += read_pmem_dbg(pa, dest, todo);
        else
            memset(buffer, 0xee, todo);

        addr += todo;
        dest += todo;
    }

    return count;
}

u64 target::write_vmem_dbg(u64 addr, const void* buffer, u64 size) {
    u64 pgsz = 0;
    if (!page_size(pgsz))
        return write_pmem_dbg(addr, buffer, size);

    u64 count = 0;
    u64 end = addr + size;
    const u8* dest = (const u8*)buffer;

    while (addr < end) {
        u64 pa = 0;
        u64 todo = min(end - addr, pgsz - (addr % pgsz));
        if (virt_to_phys(addr, pa))
            count += write_pmem_dbg(pa, dest, todo);

        addr += todo;
        dest += todo;
    }

    return count;
}

bool target::page_size(u64& size) {
    size = 0;
    return false; // to be overloaded
}

bool target::virt_to_phys(u64 vaddr, u64& paddr) {
    paddr = vaddr;
    return false; // to be overloaded
}

void target::write_gdb_xml_feature(ostream& os) {
    // to be overloaded
}

const char* target::arch() {
    return nullptr; // to be overloaded
}

u64 target::core_id() {
    return 0; // to be overloaded
}

u64 target::program_counter() {
    return 0; // to be overloaded
}

u64 target::link_register() {
    return 0; // to be overloaded
}

u64 target::stack_pointer() {
    return 0; // to be overloaded
}

u64 target::frame_pointer() {
    return 0; // to be overloaded
}

void target::stacktrace(vector<stackframe>& trace, size_t limit) {
    stackframe frame;
    frame.program_counter = program_counter();
    frame.frame_pointer = frame_pointer();
    frame.sym = m_symbols.find_function(frame.program_counter);

    trace.clear();
    trace.push_back(frame);
}

bool target::disassemble(u8* ibuf, u64& addr, string& code) {
    return false; // to be overloaded
}

bool target::disassemble(u64 addr, u64 count, vector<disassembly>& s) {
    while (s.size() < count) {
        disassembly disas = {};
        disas.addr = addr;

        const u64 size = sizeof(disas.insn);
        if (read_vmem_dbg(addr, disas.insn, size) != size)
            break;

        if (!disassemble(disas.insn, addr, disas.code))
            break;

        disas.size = addr - disas.addr;
        VCML_ERROR_ON(disas.size == 0, "disassembler address stuck");
        VCML_ERROR_ON(disas.size > size, "instruction size exceeds limit");

        if (disas.size < size)
            memset(disas.insn + disas.size, 0, size - disas.size);

        disas.sym = m_symbols.find_function(disas.addr);
        s.push_back(disas);
    }

    return s.size() == count;
}

bool target::disassemble(const range& addr, vector<disassembly>& s) {
    u64 size = (addr.length() + 7) & ~7ul; // want at least 8 bytes
    vector<u8> mem(size);

    size = read_vmem_dbg(addr.start, mem.data(), mem.size());
    if (size == 0)
        return false;

    u64 pos = addr.start;
    u8* ptr = mem.data();

    while (pos < addr.start + size) {
        disassembly disas = {};
        disas.addr = pos;

        if (!disassemble(ptr, pos, disas.code))
            break;

        disas.size = pos - disas.addr;
        const u64 lim = sizeof(disas.insn);
        VCML_ERROR_ON(disas.size == 0, "disassembler address stuck");
        VCML_ERROR_ON(disas.size > lim, "insn size exceeds limit");

        memcpy(disas.insn, ptr, disas.size);

        disas.sym = m_symbols.find_function(disas.addr);
        s.push_back(disas);
        ptr += disas.size;
    }

    return ptr > mem.data();
}

bool target::insert_breakpoint(u64 addr) {
    return false; // to be overloaded
}

bool target::remove_breakpoint(u64 addr) {
    return false; // to be overloaded
}

bool target::insert_watchpoint(const range& addr, vcml_access a) {
    return false; // to be overloaded
}

bool target::remove_watchpoint(const range& addr, vcml_access a) {
    return false; // to be overloaded
}

void target::notify_breakpoint_hit(u64 pc) {
    for (auto& bp : m_breakpoints)
        if (bp->address() == pc)
            bp->notify();
}

void target::notify_watchpoint_read(const range& addr) {
    for (auto& wp : m_watchpoints)
        if (wp->address().overlaps(addr))
            wp->notify_read(addr);
}

void target::notify_watchpoint_write(const range& addr, u64 newval) {
    for (auto& wp : m_watchpoints)
        if (wp->address().overlaps(addr))
            wp->notify_write(addr, newval);
}

const breakpoint* target::lookup_breakpoint(u64 addr) {
    for (auto& bp : m_breakpoints)
        if (bp->address() == addr)
            return bp;
    return nullptr;
}

const breakpoint* target::insert_breakpoint(u64 addr, subscriber* subscr) {
    for (auto& bp : m_breakpoints)
        if (bp->address() == addr) {
            bp->subscribe(subscr);
            return bp;
        }

    if (!insert_breakpoint(addr))
        return nullptr;

    const symbol* func = m_symbols.find_function(addr);
    breakpoint* newbp = new breakpoint(*this, addr, func);
    newbp->subscribe(subscr);
    m_breakpoints.push_back(newbp);
    return newbp;
}

bool target::remove_breakpoint(const breakpoint* bp, subscriber* subscr) {
    if (bp == nullptr)
        return false;

    auto it = std::find(m_breakpoints.begin(), m_breakpoints.end(), bp);
    if (it == m_breakpoints.end())
        return false;

    if (!(*it)->unsubscribe(subscr))
        return false;

    if ((*it)->has_subscribers())
        return true;

    if (!remove_breakpoint((*it)->address()))
        return false;

    delete *it;
    m_breakpoints.erase(it);
    return true;
}

bool target::remove_breakpoint(u64 addr, subscriber* subscr) {
    auto it = std::find_if(m_breakpoints.begin(), m_breakpoints.end(),
                           [addr](const breakpoint* bp) -> bool {
                               return bp->address() == addr;
                           });

    if (it == m_breakpoints.end())
        return false;

    if (!(*it)->unsubscribe(subscr))
        return false;

    if ((*it)->has_subscribers())
        return true;

    delete *it;
    m_breakpoints.erase(it);

    return remove_breakpoint(addr);
}

bool target::insert_watchpoint(const range& addr, vcml_access prot,
                               subscriber* subscr) {
    auto wp = std::find_if(m_watchpoints.begin(), m_watchpoints.end(),
                           [addr](const watchpoint* wp) -> bool {
                               return wp->address() == addr;
                           });

    if (wp == m_watchpoints.end()) {
        const symbol* obj = m_symbols.find_object(addr.start);
        watchpoint* newwp = new watchpoint(*this, addr, obj);
        m_watchpoints.push_back(newwp);
        wp = m_watchpoints.end() - 1;
    }

    if (is_read_allowed(prot)) {
        if (!(*wp)->has_read_subscribers())
            if (!insert_watchpoint(addr, VCML_ACCESS_READ))
                return false;
        (*wp)->subscribe(VCML_ACCESS_READ, subscr);
    }

    if (is_write_allowed(prot)) {
        if (!(*wp)->has_write_subscribers())
            if (!insert_watchpoint(addr, VCML_ACCESS_WRITE))
                return false;
        (*wp)->subscribe(VCML_ACCESS_WRITE, subscr);
    }

    return true;
}

bool target::remove_watchpoint(const range& addr, vcml_access prot,
                               subscriber* subscr) {
    auto wp = std::find_if(m_watchpoints.begin(), m_watchpoints.end(),
                           [addr](const watchpoint* wp) -> bool {
                               return wp->address() == addr;
                           });

    if (wp == m_watchpoints.end())
        return false;

    if (is_read_allowed(prot)) {
        (*wp)->unsubscribe(VCML_ACCESS_READ, subscr);
        if (!(*wp)->has_read_subscribers())
            if (!remove_watchpoint(addr, VCML_ACCESS_READ))
                return false;
    }

    if (is_write_allowed(prot)) {
        (*wp)->unsubscribe(VCML_ACCESS_WRITE, subscr);
        if (!(*wp)->has_write_subscribers())
            if (!remove_watchpoint(addr, VCML_ACCESS_WRITE))
                return false;
    }

    if (!(*wp)->has_any_subscribers()) {
        delete *wp;
        m_watchpoints.erase(wp);
    }

    return true;
}

void target::notify_singlestep() {
    vector<subscriber*> prev_steppers;

    m_mtx.lock();
    prev_steppers.swap(m_steppers);
    m_mtx.unlock();

    for (auto s : prev_steppers)
        s->notify_step_complete(*this);
}

vector<target*> target::all() {
    vector<target*> res;
    res.reserve(s_targets.size());
    for (const auto& it : s_targets)
        res.push_back(it.second);
    return res;
}

target* target::find(const string& name) {
    auto it = s_targets.find(name);
    return it != s_targets.end() ? it->second : nullptr;
}

} // namespace debugging
} // namespace vcml
