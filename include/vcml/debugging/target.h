/******************************************************************************
 *                                                                            *
 * Copyright 2018 Jan Henrik Weinstock                                        *
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

#ifndef VCML_DEBUGGING_TARGET_H
#define VCML_DEBUGGING_TARGET_H

#include "vcml/common/types.h"
#include "vcml/common/thctl.h"
#include "vcml/common/bitops.h"

#include "vcml/logging/logger.h"

#include "vcml/range.h"

#include "vcml/debugging/symtab.h"

namespace vcml { namespace debugging {

    class target;

    struct cpureg {
        u64     regno;
        string  name;
        u64     size;
        int     prot;
        target* host;

        cpureg(const cpureg&) = default;

        cpureg():
            regno(~0ul), name(), size(), prot(), host() {
        }

        cpureg(u64 no, const string& nm, u64 sz, int p):
            regno(no), name(nm), size(sz), prot(p), host() {
        }

        u64 width() const { return size * 8; }

        bool is_readable()   const { return is_read_allowed(prot); }
        bool is_writeable()  const { return is_write_allowed(prot); }
        bool is_read_only()  const { return prot == VCML_ACCESS_READ; }
        bool is_write_only() const { return prot == VCML_ACCESS_WRITE; }
        bool is_read_write() const { return prot == VCML_ACCESS_READ_WRITE; }

        u64  read() const;
        void write(u64 val) const;
    };

    struct disassembly {
        string code;
        u64 insn;
        u64 addr;
        u64 size;
        const symbol* sym;
    };

    class target
    {
    private:
        endianess m_endian;
        unordered_map<u64, cpureg> m_cpuregs;
        symtab m_symbols;

        static unordered_map<string, target*> s_targets;

    protected:
        virtual void define_cpuregs(const vector<cpureg>& regs);

    public:
        void set_little_endian();
        void set_big_endian();

        bool is_little_endian() const;
        bool is_big_endian() const;
        bool is_host_endian() const;

        const symtab& symbols() const;
        u64 load_symbols_from_elf(const string& file);

        target(const char* name);
        virtual ~target();

        vector<cpureg> cpuregs() const;

        const cpureg* find_cpureg(u64 regno) const;
        const cpureg* find_cpureg(const string& name) const;

        virtual bool read_cpureg_dbg(const cpureg& reg, u64& val);
        virtual bool write_cpureg_dbg(const cpureg& reg, u64 val);

        virtual u64 read_pmem_dbg(u64 addr, void* buffer, u64 size);
        virtual u64 write_pmem_dbg(u64 addr, const void* buffer, u64 size);

        virtual u64 read_vmem_dbg(u64 addr, void* buffer, u64 size);
        virtual u64 write_vmem_dbg(u64 addr, const void* buffer, u64 size);

        virtual bool page_size(u64& size);
        virtual bool virt_to_phys(u64 vaddr, u64& paddr);

        virtual const char* arch();

        virtual u64 core_id();
        virtual u64 program_counter();
        virtual u64 link_register();
        virtual u64 stack_pointer();

        virtual bool disassemble(void* ibuf, u64& addr, string& code);
        virtual bool disassemble(u64 addr, u64 count, vector<disassembly>& s);
        virtual bool disassemble(const range& addr, vector<disassembly>& s);

        virtual bool insert_breakpoint(u64 addr);
        virtual bool remove_breakpoint(u64 addr);

        virtual bool insert_watchpoint(const range& addr, vcml_access a);
        virtual bool remove_watchpoint(const range& addr, vcml_access a);

        virtual void gdb_collect_regs(vector<string>& gdbregs);
        virtual bool gdb_command(const string& command, string& response);

        virtual void gdb_simulate(unsigned int cycles) = 0;
        virtual void gdb_notify(int signal) = 0;

        static vector<target*> targets();
        static target* find(const char* name);
    };

    inline void target::set_little_endian() {
        m_endian = ENDIAN_LITTLE;
    }

    inline void target::set_big_endian() {
        m_endian = ENDIAN_BIG;
    }

    inline bool target::is_little_endian() const {
        return m_endian == ENDIAN_LITTLE;
    }

    inline bool target::is_big_endian() const {
       return m_endian == ENDIAN_BIG;
    }

    inline bool target::is_host_endian() const {
        return m_endian == host_endian();
    }

    inline const symtab& target::symbols() const {
        return m_symbols;
    }

    inline u64 target::load_symbols_from_elf(const string& file) {
        return m_symbols.load_elf(file);
    }

}}

#endif
