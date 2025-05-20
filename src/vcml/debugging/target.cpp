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
    if (len < total_size() || !is_readable())
        return false;
    return host->read_cpureg_dbg(*this, buf, total_size());
}

bool cpureg::write(const void* buf, size_t len) const {
    VCML_ERROR_ON(!host, "cpureg %s has no target", name.c_str());
    if (len < total_size() || !is_writeable())
        return false;
    if (!host->write_cpureg_dbg(*this, buf, total_size()))
        return false;
    if (prop != nullptr)
        memcpy(prop->raw_ptr(), buf, total_size());
    return true;
}

unordered_map<string, target*> target::s_targets;

bool target::cmd_cpustatus(const vector<string>& args, ostream& os) {
    os << "Registers:" << mkstr("\n  PC 0x%016llx", program_counter())
       << mkstr("\n  LR 0x%016llx", link_register())
       << mkstr("\n  SP 0x%016llx", stack_pointer())
       << mkstr("\n  FP 0x%016llx", frame_pointer());

    return true;
}

bool target::cmd_loadsyms(const vector<string>& args, ostream& os) {
    if (!mwr::file_exists(args[0])) {
        os << "File not found: " << args[0];
        return false;
    }

    try {
        u64 n = load_symbols_from_elf(args[0]);
        os << "Found " << n << " symbols in file '" << args[0] << "'";
        return true;
    } catch (std::exception& e) {
        os << e.what();
        return false;
    }
}

bool target::cmd_lsym(const vector<string>& args, ostream& os) {
    const debugging::symtab& syms = target::symbols();
    if (syms.empty()) {
        os << "No symbols loaded";
        return true;
    }

    os << "Listing symbols:";
    for (const auto& obj : syms.objects())
        os << mkstr("\nO 0x%016llx %s", obj.virt_addr(), obj.name());
    for (const auto& func : syms.functions())
        os << mkstr("\nF 0x%016llx %s", func.virt_addr(), func.name());

    return true;
}

bool target::cmd_disas(const vector<string>& args, ostream& os) {
    u64 vstart = program_counter();
    if (args.size() > 0)
        vstart = strtoull(args[0].c_str(), NULL, 0);

    u64 vend = vstart + 40; // disassemble 5/10 instructions by default
    if (args.size() > 1)
        vend = strtoull(args[1].c_str(), NULL, 0);

    if (vstart > vend) {
        os << "Invalid range specified";
        return false;
    }

    vector<debugging::disassembly> disas;
    if (!disassemble({ vstart, vend }, disas)) {
        os << "Disassembler reported error";
        return false;
    }

    os << "Disassembly of ";
    os << mkstr(vstart > ~0u ? "0x%016llx.." : "0x%08llx..", vstart);
    os << mkstr(vend > ~0u ? "0x%016llx" : "0x%08llx", vend);

    u64 pgsz;
    bool virt = page_size(pgsz);
    if (virt)
        os << " (virtual)";

    u64 maxsz = 0;
    for (const auto& insn : disas)
        maxsz = max(maxsz, insn.size);

    for (const auto& insn : disas) {
        os << "\n" << (insn.addr == program_counter() ? " > " : "   ");
        if (insn.sym != nullptr) {
            u64 offset = insn.addr - insn.sym->virt_addr();
            if (offset <= insn.sym->size())
                os << mkstr("[%s+0x%04llx] ", insn.sym->name(), offset);
        }

        os << mkstr(insn.addr > ~0u ? "%016llx" : "%08llx", insn.addr);

        u64 phys = insn.addr;
        if (virt) {
            if (virt_to_phys(insn.addr, phys))
                os << mkstr(phys > ~0u ? " %016llx" : " %08llx", phys);
            else
                os << "????????????????";
        }

        os << ": [";
        for (u64 i = 0; i < insn.size; i++) {
            os << mkstr("%02hhx", insn.insn[i]);
            if (i < insn.size - 1)
                os << " ";
        }
        os << "]";

        for (u64 i = insn.size; i < maxsz; i++)
            os << "   ";

        os << " " << escape(insn.code, ",");
    }

    return true;
}

bool target::cmd_v2p(const vector<string>& args, ostream& os) {
    u64 phys = -1;
    u64 virt = strtoull(args[0].c_str(), NULL, 0);
    bool ret = virt_to_phys(virt, phys);
    if (!ret) {
        os << mkstr("cannot translate virtual address 0x%llx", virt);
        return false;
    } else {
        os << mkstr("0x%llx -> 0x%llx", virt, phys);
        return true;
    }
}

bool target::cmd_stack(const vector<string>& args, ostream& os) {
    stream_guard guard(os);
    vector<debugging::stackframe> frames;
    stacktrace(frames);

    for (const auto& frame : frames) {
        os << mkstr("[0x%016llx]", frame.program_counter);
        if (frame.sym != nullptr) {
            os << mkstr(" %s +0x%llx", frame.sym->name(),
                        frame.program_counter - frame.sym->virt_addr());
            if (frame.sym->size() > 0)
                os << mkstr("/0x%llx", frame.sym->size());
        }

        os << "\n";
    }

    return true;
}

bool target::cmd_vread(const vector<string>& args, ostream& os) {
    u64 addr = strtoull(args[0].c_str(), NULL, 0);
    u64 size = 1;
    if (args.size() > 1)
        size = strtoull(args[1].c_str(), NULL, 0);

    vector<u8> buffer(size);

    if (!read_vmem_dbg(addr, buffer.data(), size)) {
        os << mkstr("error reading virtual address 0x%llx", addr);
        return false;
    }

    u64 offset = 0;
    while (offset < buffer.size()) {
        os << mkstr("\n0x%llx:", addr + offset);
        for (int i = 0; i < 8; i++, offset++) {
            if (offset < buffer.size())
                os << mkstr(" %02hhx", buffer[offset]);
        }
    }

    return true;
}

static void append_bytes(vector<u8>& buffer, const string& str) {
    if (str.empty())
        return;

    buffer.reserve(str.length() / 2 + 1);
    for (int i = str.length() - 1; i >= 0; i -= 2) {
        u8 data = from_hex_ascii(str[i]);
        if (i > 0)
            data |= from_hex_ascii(str[i - 1]) << 4;
        buffer.push_back(data);
    }
}

bool target::cmd_vwrite(const vector<string>& args, ostream& os) {
    u64 addr = strtoull(args[0].c_str(), NULL, 0);
    vector<u8> buffer;
    for (size_t i = 1; i < args.size(); i++)
        append_bytes(buffer, args[i]);

    if (!write_vmem_dbg(addr, buffer.data(), buffer.size())) {
        os << mkstr("error writing virtual address 0x%llx", addr);
        return false;
    }

    os << mkstr("successfully wrote %zu bytes to 0x%llx", buffer.size(), addr);
    return true;
}

void target::define_cpureg(size_t regno, const string& name, size_t size,
                           size_t count, int prot) {
    if (const cpureg* other = find_cpureg(regno)) {
        if (other->name == name)
            VCML_ERROR("cpureg %s already defined", name.c_str());
        VCML_ERROR("regno %zu already used by %s", regno, other->name.c_str());
    }

    vector<u8> buffer;
    u8* def = nullptr;
    if (is_read_allowed(prot)) {
        buffer.resize(size * count);
        if (!read_reg_dbg(regno, buffer.data(), buffer.size()))
            VCML_ERROR("failed to initialize cpureg %s", name.c_str());
        def = buffer.data();
    }

    cpureg& newreg = m_cpuregs[regno];
    newreg.regno = regno;
    newreg.size = size;
    newreg.count = count;
    newreg.prot = prot;
    newreg.name = name;
    newreg.host = this;
    newreg.prop = std::make_shared<property<void>>(&m_host, name.c_str(), size,
                                                   count, def);
}

void target::define_cpureg_r(size_t regno, const string& name, size_t size,
                             size_t count) {
    define_cpureg(regno, name, size, count, VCML_ACCESS_READ);
}

void target::define_cpureg_w(size_t regno, const string& name, size_t size,
                             size_t count) {
    define_cpureg(regno, name, size, count, VCML_ACCESS_WRITE);
}

void target::define_cpureg_rw(size_t regno, const string& name, size_t size,
                              size_t count) {
    define_cpureg(regno, name, size, count, VCML_ACCESS_READ_WRITE);
}

void target::fetch_cpuregs() {
    for (auto& [id, reg] : m_cpuregs) {
        if (!reg.is_readable())
            continue;

        if (!reg.read(reg.prop->raw_ptr(), reg.prop->raw_len()))
            log_warn("failed to fetch cpureg %s", reg.name.c_str());
    }
}

void target::flush_cpuregs() {
    for (auto& [id, reg] : m_cpuregs) {
        if (!reg.is_writeable())
            continue;

        if (!reg.write(reg.prop->raw_ptr(), reg.prop->raw_len()))
            log_warn("failed to flush cpureg %s", reg.name.c_str());
    }
}

void target::reset_cpuregs() {
    for (auto& [id, reg] : m_cpuregs)
        reg.prop->reset();
}

void target::update_single_stepping(bool on) {
    // to be overloaded
}

bool target::start_basic_block_trace() {
    return false; // to be overloaded
}

bool target::stop_basic_block_trace() {
    return false; // to be overloaded
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

void target::notify_singlestep() {
    vector<subscriber*> prev_steppers;

    m_mtx.lock();
    prev_steppers.swap(m_steppers);
    m_mtx.unlock();

    for (auto s : prev_steppers)
        s->notify_step_complete(*this);
}

void target::notify_basic_block(u64 pc, size_t blksz, size_t icount) {
    m_mtx.lock();
    vector<subscriber*> local(m_bbtracer);
    m_mtx.unlock();

    for (auto s : local)
        s->notify_basic_block(*this, pc, blksz, icount);
}

target::target(module& host):
    m_mtx(),
    m_host(host),
    m_name(host.name()),
    m_suspendable(true),
    m_running(true),
    m_endian(ENDIAN_UNKNOWN),
    m_cpuregs(),
    m_symbols(),
    m_steppers(),
    m_bbtracer(),
    m_breakpoints(),
    m_watchpoints() {
    if (stl_contains(s_targets, m_name))
        VCML_ERROR("debug target '%s' already exists", m_name.c_str());
    s_targets[m_name] = this;

    host.register_command("cpustatus", 0, this, &target::cmd_cpustatus,
                          "prints cpu status registers");
    host.register_command("loadsyms", 1, this, &target::cmd_loadsyms,
                          "load a symbol file for use in disassembly");
    host.register_command("lsym", 0, this, &target::cmd_lsym,
                          "show a list of all available symbols");
    host.register_command("disas", 0, this, &target::cmd_disas,
                          "disassemble instructions from memory");
    host.register_command("v2p", 1, this, &target::cmd_v2p,
                          "translate a virtual address to physical");
    host.register_command("stack", 0, this, &target::cmd_stack,
                          "generates a stack trace");
    host.register_command("vread", 1, this, &target::cmd_vread,
                          "read virtual memory: vread <addr> <count>");
    host.register_command("vwrite", 2, this, &target::cmd_vwrite,
                          "write virtual memory: vwrite <addr> <bytes>");
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
    for (auto& [id, reg] : m_cpuregs)
        regs.push_back(reg);
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
    return read_reg_dbg(reg.regno, buf, len);
}

bool target::write_cpureg_dbg(const cpureg& reg, const void* buf, size_t len) {
    return write_reg_dbg(reg.regno, buf, len);
}

bool target::read_reg_dbg(size_t regno, void* buf, size_t len) {
    return false; // to be overloaded
}

bool target::write_reg_dbg(size_t regno, const void* buf, size_t len) {
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

bool target::trace_basic_blocks(subscriber* subscr) {
    lock_guard<mutex> guard(m_mtx);
    if (m_bbtracer.empty() && !start_basic_block_trace())
        return false;

    stl_add_unique(m_bbtracer, subscr);
    return true;
}

bool target::untrace_basic_blocks(subscriber* subscr) {
    lock_guard<mutex> guard(m_mtx);
    if (!mwr::stl_contains(m_bbtracer, subscr))
        return false;

    stl_remove(m_bbtracer, subscr);
    if (m_bbtracer.empty() && !stop_basic_block_trace())
        return false;

    return true;
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
