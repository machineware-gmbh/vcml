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

#include <unistd.h>
#include <fcntl.h>
#include <libelf.h>

#include "vcml/debugging/elf_reader.h"

namespace vcml {
namespace debugging {

struct elf32_traits {
    typedef Elf32_Ehdr Elf_Ehdr;
    typedef Elf32_Phdr Elf_Phdr;
    typedef Elf32_Shdr Elf_Shdr;
    typedef Elf32_Sym Elf_Sym;

    static Elf_Ehdr* elf_getehdr(Elf* elf) { return elf32_getehdr(elf); }
    static Elf_Phdr* elf_getphdr(Elf* elf) { return elf32_getphdr(elf); }
    static Elf_Shdr* elf_getshdr(Elf_Scn* scn) { return elf32_getshdr(scn); }
};

struct elf64_traits {
    typedef Elf64_Ehdr Elf_Ehdr;
    typedef Elf64_Phdr Elf_Phdr;
    typedef Elf64_Shdr Elf_Shdr;
    typedef Elf64_Sym Elf_Sym;

    static Elf_Ehdr* elf_getehdr(Elf* elf) { return elf64_getehdr(elf); }
    static Elf_Phdr* elf_getphdr(Elf* elf) { return elf64_getphdr(elf); }
    static Elf_Shdr* elf_getshdr(Elf_Scn* scn) { return elf64_getshdr(scn); }
};

static symkind elf_symkind(u32 info) {
    switch (ELF32_ST_TYPE(info)) {
    case STT_OBJECT:
        return SYMKIND_OBJECT;
    case STT_FUNC:
    case STT_NOTYPE:
        return SYMKIND_FUNCTION;
    default:
        return SYMKIND_UNKNOWN;
    }
}

static endianess elf_endianess(Elf* elf) {
    const char* ident = elf_getident(elf, nullptr);
    if (ident == nullptr)
        return ENDIAN_UNKNOWN;

    switch (ident[EI_DATA]) {
    case ELFDATA2LSB:
        return ENDIAN_LITTLE;
    case ELFDATA2MSB:
        return ENDIAN_BIG;
    default:
        return ENDIAN_UNKNOWN;
    }
}

template <typename T>
static vector<elf_segment> elf_segments(Elf* elf) {
    size_t count = 0;

    int err = elf_getphdrnum(elf, &count);
    if (err)
        VCML_ERROR("elf_begin failed: %s", elf_errmsg(err));

    vector<elf_segment> segments;
    typename T::Elf_Phdr* hdr = T::elf_getphdr(elf);
    for (u64 i = 0; i < count; i++, hdr++) {
        if (hdr->p_type == PT_LOAD) {
            bool r = hdr->p_flags & PF_R;
            bool w = hdr->p_flags & PF_W;
            bool x = hdr->p_flags & PF_X;
            segments.push_back({ hdr->p_vaddr, hdr->p_paddr, hdr->p_memsz,
                                 hdr->p_filesz, hdr->p_offset, r, w, x });
        }
    }

    return segments;
}

template <typename T, typename ELF>
void elf_reader::read_sections(ELF* elf) {
    Elf_Scn* scn = nullptr;
    while ((scn = elf_nextscn(elf, scn)) != nullptr) {
        typename T::Elf_Shdr* shdr = T::elf_getshdr(scn);
        if (shdr->sh_type != SHT_SYMTAB)
            continue;

        Elf_Data* data = elf_getdata(scn, nullptr);
        size_t numsyms = shdr->sh_size / shdr->sh_entsize;

        typename T::Elf_Sym* syms = (typename T::Elf_Sym*)(data->d_buf);
        for (size_t i = 0; i < numsyms; i++) {
            if (syms[i].st_size == 0)
                continue;

            symkind kind = elf_symkind(syms[i].st_info);
            if (kind == SYMKIND_UNKNOWN)
                continue;

            char* name = elf_strptr(elf, shdr->sh_link, syms[i].st_name);
            if (name == nullptr || strlen(name) == 0)
                continue;

            u64 size = syms[i].st_size;
            u64 virt = syms[i].st_value;
            u64 phys = to_phys(virt);

            m_symtab.insert({ name, kind, m_endian, size, virt, phys });
        }
    }
}

u64 elf_reader::to_phys(u64 virt) const {
    for (auto& seg : m_segments) {
        if ((virt >= seg.virt) && virt < (seg.virt + seg.size))
            return seg.phys + virt - seg.virt;
    }

    return virt;
}

elf_reader::elf_reader(const string& path):
    m_filename(path),
    m_fd(-1),
    m_entry(0),
    m_machine(0),
    m_endian(ENDIAN_UNKNOWN) {
    if (elf_version(EV_CURRENT) == EV_NONE)
        VCML_ERROR("failed to read libelf version");

    m_fd = open(filename(), O_RDONLY, 0);
    if (m_fd < 0)
        VCML_ERROR("cannot open elf file '%s'", filename());

    Elf* elf = elf_begin(m_fd, ELF_C_READ, nullptr);
    if (elf == nullptr)
        VCML_ERROR("error reading '%s' (%s)", filename(), elf_errmsg(-1));

    if (elf_kind(elf) != ELF_K_ELF)
        VCML_ERROR("ELF version error in %s", filename());

    Elf32_Ehdr* ehdr32 = elf32_getehdr(elf);
    Elf64_Ehdr* ehdr64 = elf64_getehdr(elf);

    if (ehdr32) {
        m_entry = ehdr32->e_entry;
        m_machine = ehdr32->e_machine;
        m_endian = elf_endianess(elf);
        m_segments = elf_segments<elf32_traits>(elf);
        read_sections<elf32_traits>(elf);
    }

    if (ehdr64) {
        m_entry = ehdr64->e_entry;
        m_machine = ehdr64->e_machine;
        m_endian = elf_endianess(elf);
        m_segments = elf_segments<elf64_traits>(elf);
        read_sections<elf64_traits>(elf);
    }

    elf_end(elf);
}

elf_reader::~elf_reader() {
    if (m_fd >= 0)
        close(m_fd);
}

u64 elf_reader::read_symbols(symtab& tab) {
    tab.merge(m_symtab);
    return m_symtab.count();
}

u64 elf_reader::read_segment(const elf_segment& segment, u8* dest) {
    VCML_ERROR_ON(m_fd < 0, "ELF file '%s' not open", filename());

    if (lseek(m_fd, segment.offset, SEEK_SET) != (ssize_t)segment.offset)
        VCML_ERROR("cannot seek within ELF file '%s'", filename());

    if (fd_read(m_fd, dest, segment.filesz) != segment.filesz)
        VCML_ERROR("cannot read ELF file '%s'", filename());

    if (lseek(m_fd, 0, SEEK_SET) != 0)
        VCML_ERROR("cannot seek within ELF file '%s'", filename());

    if (segment.filesz < segment.size)
        memset(dest + segment.filesz, 0, segment.size - segment.filesz);

    return segment.size;
}

} // namespace debugging
} // namespace vcml
