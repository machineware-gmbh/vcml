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

#ifndef VCML_DEBUGGING_ELF_READER_H
#define VCML_DEBUGGING_ELF_READER_H

#include "vcml/core/types.h"
#include "vcml/debugging/symtab.h"

namespace vcml {
namespace debugging {

struct elf_segment {
    const u64 virt;
    const u64 phys;
    const u64 size;
    const u64 filesz;
    const u64 offset;
    const bool r, w, x;
};

class elf_reader
{
private:
    string m_filename;
    symtab m_symtab;
    int m_fd;
    u64 m_entry;
    u64 m_machine;
    endianess m_endian;

    vector<elf_segment> m_segments;

    template <typename TRAITS, typename ELF>
    void read_sections(ELF* elf);

    u64 to_phys(u64 virt) const;

public:
    u64 entry() const { return m_entry; }
    u64 machine() const { return m_machine; }

    endianess endian() const { return m_endian; }
    bool is_little_endian() const { return m_endian == ENDIAN_LITTLE; }
    bool is_big_endian() const { return m_endian == ENDIAN_BIG; }

    const char* filename() const { return m_filename.c_str(); }

    const vector<elf_segment>& segments() const { return m_segments; }

    elf_reader(const string& filename);
    ~elf_reader();

    elf_reader(const elf_reader&) = delete;

    u64 read_symbols(symtab& tab);
    u64 read_segment(const elf_segment& segment, u8* dest);
};

} // namespace debugging
} // namespace vcml

#endif
