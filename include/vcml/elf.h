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

#ifndef VCML_ELF_H
#define VCML_ELF_H

#include "vcml/common/includes.h"
#include "vcml/common/types.h"
#include "vcml/common/utils.h"
#include "vcml/common/report.h"

#include "vcml/logging/logger.h"

namespace vcml {

    class elf_symbol;
    class elf_section;
    class elf;

    enum elf_sym_type {
        ELF_SYM_OBJECT = 0,
        ELF_SYM_FUNCTION,
        ELF_SYM_UNKNOWN
    };

    class elf_symbol
    {
        friend class elf;
    private:
        u64 m_virt_addr;
        u64 m_phys_addr;

        string m_name;

        elf_sym_type m_type;

        // disabled
        elf_symbol();
        elf_symbol(const elf_symbol&);

    public:
        u64 get_virt_addr() const { return m_virt_addr; }
        u64 get_phys_addr() const { return m_phys_addr; }

        const string& get_name() const { return m_name; }
        elf_sym_type  get_type() const { return m_type; }

        bool is_function() const { return m_type == ELF_SYM_FUNCTION; }
        bool is_object()   const { return m_type == ELF_SYM_OBJECT; }

        elf_symbol(const char* name, Elf32_Sym* symbol);
        elf_symbol(const char* name, Elf64_Sym* symbol);
        virtual ~elf_symbol();
    };

    class elf_section
    {
    private:
        string m_name;

        u64 m_size;
        u64 m_virt_addr;
        u64 m_phys_addr;

        unsigned char* m_data;

        bool m_flag_alloc;
        bool m_flag_write;
        bool m_flag_exec;

        template <typename EHDR, typename PHDR, typename SHDR>
        void init(Elf* elf, Elf_Scn* scn, EHDR* ehdr, PHDR* phdr, SHDR* shdr);

        // disabled
        elf_section();
        elf_section(const elf_section&);

    public:
        bool needs_alloc   () const { return m_flag_alloc; }
        bool is_writeable  () const { return m_flag_write; }
        bool is_executable () const { return m_flag_exec;  }

        const string& get_name() const { return m_name; }
        void*         get_data() const { return m_data; }
        u64           get_size() const { return m_size; }

        u64 get_virt_addr() const { return m_virt_addr; }
        u64 get_phys_addr() const { return m_phys_addr; }

        bool contains(u64 addr) const;
        u64 offset(u64 addr) const;
        u64 to_phys(u64 addr) const;

        elf_section(Elf* elf, Elf_Scn* section);
        virtual ~elf_section();
    };

    inline bool elf_section::contains(u64 addr) const {
        return (addr >= m_virt_addr) && (addr < (m_virt_addr + m_size));
    }

    inline u64 elf_section::offset(u64 addr) const {
        return addr - m_virt_addr;
    }

    inline u64 elf_section::to_phys(u64 addr) const {
        return offset(addr) + m_phys_addr;
    }

    class elf
    {
    private:
        string      m_filename;
        vcml_endian m_endianess;
        u64         m_entry;
        bool        m_64bit;

        vector<elf_section*> m_sections;
        vector<elf_symbol*>  m_symbols;

        template <typename EHDR, typename SHDR, typename SYM>
        void init(Elf* elf, EHDR* ehdr, SHDR* (*getshdr)(Elf_Scn*));

        // disabled
        elf();
        elf(const elf&);

    public:
        const string& get_filename() const { return m_filename; }

        vcml_endian get_endianess()   const { return m_endianess; }
        u64         get_entry_point() const { return m_entry; }

        bool is_64bit() const { return m_64bit; }

        u64 get_num_sections() const { return m_sections.size(); }
        u64 get_num_symbols()  const { return m_symbols.size(); }

        const vector<elf_section*>& get_sections() const { return m_sections; }
        const vector<elf_symbol*>&  get_symbols() const { return m_symbols; }

        elf(const string& filename);
        virtual ~elf();

        u64 to_phys(u64 virt_addr) const;

        void dump();

        elf_section* get_section(unsigned int  section_idx)  const;
        elf_section* get_section(const string& section_name) const;

        elf_symbol* get_symbol(unsigned int  symbol_idx)  const;
        elf_symbol* get_symbol(const string& symbol_name) const;

        elf_symbol* find_function(u64 addr) const;
    };

}

#endif
