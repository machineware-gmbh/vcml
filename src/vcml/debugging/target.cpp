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

#include "vcml/debugging/target.h"

namespace vcml { namespace debugging {

    u64 cpureg::read() const {
        u64 val;

        if (host == nullptr)
            VCML_ERROR("cpureg %s has no target", name.c_str());
        if (!is_readable())
            VCML_ERROR("cannot read cpureg %s", name.c_str());
        if (!host->read_cpureg_dbg(*this, val))
            VCML_ERROR("failed to read cpureg %s", name.c_str());
        if (width() < 64 && val >= 1ul << width())
            VCML_ERROR("value 0x%lx exceeds size of %s", val, name.c_str());

        return val;
    }

    void cpureg::write(u64 val) const {
        if (width() < 64 && val >= 1ul << width())
            VCML_ERROR("value 0x%lx exceeds size of %s", val, name.c_str());
        if (host == nullptr)
            VCML_ERROR("cpureg %s has no target", name.c_str());
        if (!is_writeable())
            VCML_ERROR("cannot write cpureg %s", name.c_str());
        if (!host->write_cpureg_dbg(*this, val))
            VCML_ERROR("failed to read cpureg %s", name.c_str());
    }

    unordered_map<string, target*> target::s_targets;

    void target::define_cpuregs(const vector<cpureg>& regs) {
        for (const auto& reg : regs) {
            const char* regname = reg.name.c_str();
            if (m_cpuregs.find(reg.regno) != m_cpuregs.end())
                VCML_ERROR("cpureg %s already defined", regname);

            cpureg& newreg = m_cpuregs[reg.regno];
            newreg.regno = reg.regno;
            newreg.size = reg.size;
            newreg.prot = reg.prot;
            newreg.name = reg.name;
            newreg.host = this;
        }
    }

    target::target(const char* name):
        m_endian(ENDIAN_UNKNOWN),
        m_cpuregs() {
        auto it = s_targets.find(name);
        if (it != s_targets.end())
            VCML_ERROR("debug target '%s' already exists", name);
        s_targets[name] = this;
    }

    target::~target() {
        // nothing to do
    }

    vector<cpureg> target::cpuregs() const {
        vector<cpureg> regs;
        for (auto& reg : m_cpuregs)
            regs.push_back(reg.second);
        return std::move(regs);
    }

    const cpureg* target::find_cpureg(u64 regno) const {
        auto it = m_cpuregs.find(regno);
        return it != m_cpuregs.end() ? &it->second : nullptr;
    }

    const cpureg* target::find_cpureg(const string& name) const {
        for (auto& it : m_cpuregs)
            if (it.second.name.compare(name) == 0)
                return &it.second;
        return nullptr;
    }

    bool target::read_cpureg_dbg(const cpureg& reg, u64& val) {
        return false; // to be overloaded
    }

    bool target::write_cpureg_dbg(const cpureg& reg, u64 val) {
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

    bool target::disassemble(void* ibuf, u64& addr, string& code) {
        return false; // to be overloaded
    }

    bool target::disassemble(u64 addr, u64 count, vector<disassembly>& s) {
        while (s.size() < count) {
            disassembly disas = {};
            disas.addr = addr;

            const u64 size = sizeof(disas.insn);
            if (read_vmem_dbg(addr, &disas.insn, size) != size)
                break;

            if (!disassemble(&disas.insn, addr, disas.code))
                break;

            disas.size = addr - disas.addr;
            VCML_ERROR_ON(disas.size == 0, "disassembler address stuck");
            VCML_ERROR_ON(disas.size > 8, "insn size exceeds 8 byte limit");

            if (disas.size < 8)
                disas.insn &= bitmask(disas.size * 8);

            if (is_big_endian())
                memswap(&disas.insn, disas.size);

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
            VCML_ERROR_ON(disas.size == 0, "disassembler address stuck");
            VCML_ERROR_ON(disas.size > 8, "insn size exceeds 8 byte limit");

            memcpy(&disas.insn, ptr, disas.size);
            if (is_big_endian())
                memswap(&disas.insn, disas.size);

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

    void target::gdb_collect_regs(vector<string>& gdbregs) {
        // to be overloaded
    }

    bool target::gdb_command(const string& command, string& response) {
        return false; // to be overloaded
    }

    vector<target*> target::targets() {
        vector<target*> res(s_targets.size());
        for (auto it : s_targets)
            res.push_back(it.second);
        return std::move(res);
    }

    target* target::find(const char* name) {
        auto it = s_targets.find(name);
        return it != s_targets.end() ? it->second : nullptr;
    }

}}
