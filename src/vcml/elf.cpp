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

#include <fcntl.h> // for open

#include "vcml/elf.h"

namespace vcml {

    elf_symbol::elf_symbol(const char* name, Elf32_Sym* symbol):
        m_virt_addr(symbol->st_value),
        m_phys_addr(symbol->st_value),
        m_name(name),
        m_type() {
        switch (ELF32_ST_TYPE(symbol->st_info)) {
        case STT_OBJECT : m_type = ELF_SYM_OBJECT;   break;
        case STT_FUNC   : m_type = ELF_SYM_FUNCTION; break;
        default         : m_type = ELF_SYM_UNKNOWN;  break;
        }
    }

    elf_symbol::elf_symbol(const char* name, Elf64_Sym* symbol):
        m_virt_addr(symbol->st_value),
        m_phys_addr(symbol->st_value),
        m_name(name),
        m_type() {
        switch (ELF64_ST_TYPE(symbol->st_info)) {
        case STT_OBJECT : m_type = ELF_SYM_OBJECT;   break;
        case STT_FUNC   : m_type = ELF_SYM_FUNCTION; break;
        default         : m_type = ELF_SYM_UNKNOWN;  break;
        }
    }

    elf_symbol::~elf_symbol() {
        // nothing to do
    }

    template <typename EHDR, typename PHDR, typename SHDR>
    void elf_section::init(Elf* elf, Elf_Scn* scn, EHDR* ehdr, PHDR* phdr,
                           SHDR* shdr) {
        size_t shstrndx = 0;
        if (elf_getshdrstrndx (elf, &shstrndx) != 0)
            VCML_ERROR("Call to elf_getshdrstrndx failed");

        char* name = elf_strptr(elf, shstrndx, shdr->sh_name);
        if (name == nullptr)
            VCML_ERROR("Call to elfstrptr failed");

        m_name = name;
        m_size = shdr->sh_size;
        m_data = new unsigned char[m_size];

        m_virt_addr = shdr->sh_addr;
        m_phys_addr = shdr->sh_addr;

        m_flag_alloc = shdr->sh_flags & SHF_ALLOC;
        m_flag_write = shdr->sh_flags & SHF_WRITE;
        m_flag_exec  = shdr->sh_flags & SHF_EXECINSTR;

        // Check program headers for physical address
        for (unsigned int i = 0; i < ehdr->e_phnum; i++) {
            u64 start = phdr[i].p_offset;
            u64 end   = phdr[i].p_offset + phdr[i].p_memsz;
            if ((start != 0) && (shdr->sh_offset >= start) &&
                (shdr->sh_offset < end))
                m_phys_addr = phdr[i].p_paddr + shdr->sh_addr
                              - phdr[i].p_vaddr;
        }

        Elf_Data* data = nullptr;
        unsigned int copied = 0;
        while ((copied < shdr->sh_size) && (data = elf_getdata(scn, data))) {
            if (data->d_buf) {
                memcpy(m_data + copied, data->d_buf, data->d_size);
            }
        }
    }

    elf_section::elf_section(Elf* elf, Elf_Scn* scn):
        m_name(""),
        m_size(0),
        m_virt_addr(0),
        m_phys_addr(0),
        m_data(nullptr),
        m_flag_alloc(false),
        m_flag_write(false),
        m_flag_exec(false) {
        Elf32_Ehdr* ehdr32 = elf32_getehdr(elf);
        Elf32_Phdr* phdr32 = elf32_getphdr(elf);
        Elf32_Shdr* shdr32 = elf32_getshdr(scn);

        Elf64_Ehdr* ehdr64 = elf64_getehdr(elf);
        Elf64_Phdr* phdr64 = elf64_getphdr(elf);
        Elf64_Shdr* shdr64 = elf64_getshdr(scn);

        if (ehdr64 && phdr64 && shdr64)
            init(elf, scn, ehdr64, phdr64, shdr64);
        else if (ehdr32 && phdr32 && shdr32)
            init(elf, scn, ehdr32, phdr32, shdr32);
        else
            VCML_ERROR("unknown ELF class");
    }

    elf_section::~elf_section() {
        if (m_data != nullptr)
            delete [] m_data;
    }

    template <typename EHDR, typename SHDR, typename SYM>
    void elf::init(Elf* e, EHDR* ehdr, SHDR* (*getshdr)(Elf_Scn*)) {
        switch (ehdr->e_ident[EI_DATA]) {
        case ELFDATA2LSB: m_endianess = ENDIAN_LITTLE; break;
        case ELFDATA2MSB: m_endianess = ENDIAN_BIG;    break;
        default:
            VCML_ERROR("invalid endianess specified in ELF header");
            break;
        }

        // Traverse all sections
        Elf_Scn* scn = nullptr;
        while ((scn = elf_nextscn(e, scn)) != 0) {
            SHDR* shdr = getshdr(scn);
            if (shdr->sh_type == SHT_SYMTAB) {
                Elf_Data* data = elf_getdata(scn, nullptr);
                unsigned int num_symbols = shdr->sh_size / shdr->sh_entsize;
                SYM* symbols = reinterpret_cast<SYM*>(data->d_buf);
                for (unsigned int i = 0; i < num_symbols; i++) {
                    char* name = elf_strptr(e, shdr->sh_link,
                                            symbols[i].st_name);
                    if (name == nullptr)
                        VCML_ERROR("elf_strptr failed: %s", elf_errmsg(-1));

                    elf_symbol* sym = new elf_symbol(name, symbols + i);
                    m_symbols.push_back(sym);
                }
            }

            // Add to our section list
            elf_section* sec = new elf_section(e, scn);
            m_sections.push_back(sec);
        }

        // Update physical addresses of symbols once all sections are loaded.
        for (auto symbol : m_symbols)
            symbol->m_phys_addr = to_phys(symbol->get_virt_addr());

        std::sort(m_symbols.begin(), m_symbols.end(),
            [] (const elf_symbol* a, const elf_symbol* b) -> bool {
                return a->get_virt_addr() < b->get_virt_addr();
        });

        m_entry = to_phys(ehdr->e_entry);
    }

    elf::elf(const string& fname):
        m_filename(fname),
        m_endianess(),
        m_entry(),
        m_sections(),
        m_symbols() {
        if (elf_version(EV_CURRENT) == EV_NONE)
            VCML_ERROR("failed to read libelf version");

        int fd = open(m_filename.c_str(), O_RDONLY, 0);
        if (fd < 0)
            VCML_ERROR("cannot open elf file '%s'", m_filename.c_str());

        Elf* e = elf_begin(fd, ELF_C_READ, nullptr);
        if (e == nullptr)
            VCML_ERROR("elf_begin failed: %s", elf_errmsg(-1));

        if (elf_kind(e) != ELF_K_ELF)
            VCML_ERROR("error reading elf file");

        Elf32_Ehdr* ehdr32 = elf32_getehdr(e);
        Elf64_Ehdr* ehdr64 = elf64_getehdr(e);

        m_64bit = ehdr64 != nullptr;

        if (ehdr64)
            init<Elf64_Ehdr, Elf64_Shdr, Elf64_Sym>(e, ehdr64, elf64_getshdr);
        else if (ehdr32)
            init<Elf32_Ehdr, Elf32_Shdr, Elf32_Sym>(e, ehdr32, elf32_getshdr);
        else
            VCML_ERROR("cannot find elf program header");

        elf_end(e);
        close(fd);
    }

    elf::~elf() {
        for (auto symbol : m_symbols)
            delete symbol;
        for (auto section : m_sections)
            delete section;
    }

    u64 elf::to_phys(uint64_t virt_addr) const {
        for (auto section : m_sections)
            if (section->contains(virt_addr))
                return section->to_phys(virt_addr);
        return virt_addr;
    }

    void elf::dump() {
        log_info("%s has %zu sections:\n", m_filename.c_str(),
                 m_sections.size());

        static const char* endstr[] = { "little", "big", "unknown" };

        log_info("name     : %s\n", m_filename.c_str());
        log_info("entry    : 0x%08lx\n", m_entry);
        log_info("endian   : %s\n", endstr[m_endianess]);
        log_info("sections : %zu\n", m_sections.size());
        log_info("symbols  : %zu\n", m_symbols.size());

        log_info("\nsections:\n");
        log_info("[nr] vaddr      paddr      size       name\n");

        for (unsigned int i = 0; i < m_sections.size(); i++) {
            elf_section* sec = m_sections[i];
            log_info("[%2d] ", i);
            log_info("0x%08lx ", sec->get_virt_addr());
            log_info("0x%08lx ", sec->get_phys_addr());
            log_info("0x%08lx ", sec->get_size());
            log_info("%s\n", sec->get_name().c_str());
        }

        log_info("\nsymbols:\n");
        log_info("[   nr] vaddr      paddr      type     name\n");

        static const char* typestr[] = { "OBJECT  ", "FUNCTION", "UNKNOWN " };
        for (unsigned int i = 0; i < m_symbols.size(); i++) {
            elf_symbol* sym = m_symbols[i];
            if (sym->get_type() != ELF_SYM_FUNCTION)
                continue;

            log_info("[%5d] ", i);
            log_info("0x%08lx ", sym->get_virt_addr());
            log_info("0x%08lx ", sym->get_phys_addr());
            log_info("%s ", typestr[sym->get_type()]);
            log_info("%s\n", sym->get_name().c_str());
        }
    }

    elf_section* elf::get_section(unsigned int idx) const {
        if (idx >= m_sections.size())
            return nullptr;
        return m_sections[idx];
    }

    elf_section* elf::get_section(const string& name) const {
        for (auto section : m_sections)
            if (section->get_name() == name)
                return section;
        return nullptr;
    }

    elf_symbol* elf::get_symbol(unsigned int idx) const {
        if (idx >= m_symbols.size())
            return nullptr;
        return m_symbols[idx];
    }

    elf_symbol* elf::get_symbol(const std::string& name) const {
        for (auto symbol : m_symbols)
            if (symbol->get_name() == name)
                return symbol;
        return nullptr;
    }

    elf_symbol* elf::find_function(uint64_t addr) const {
        for (unsigned int i = m_symbols.size(); i > 0; i--) {
            elf_symbol* sym = m_symbols[i - 1];
            if (sym->is_function() && (sym->get_virt_addr() <= addr))
                return sym;
        }

        return nullptr;
    }

}
