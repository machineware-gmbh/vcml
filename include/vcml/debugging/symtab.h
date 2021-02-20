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

#ifndef VCML_DEBUGGING_SYMTAB_H
#define VCML_DEBUGGING_SYMTAB_H

#include "vcml/common/types.h"
#include "vcml/common/bitops.h"
#include "vcml/common/strings.h"
#include "vcml/range.h"

namespace vcml { namespace debugging {

    enum symkind {
        SYMKIND_UNKNOWN,
        SYMKIND_FUNCTION,
        SYMKIND_OBJECT,
    };

    class symbol
    {
    private:
        string    m_name;
        symkind   m_kind;
        endianess m_endian;
        u64       m_size;
        u64       m_virt;
        u64       m_phys;

    public:
        symbol();
        symbol(const string& name, symkind kind, endianess endian, u64 size,
               u64 virt_addr, u64 phys_addr);

        symbol(const symbol&) = default;
        ~symbol() = default;

        const char* name() const { return m_name.c_str(); }

        symkind kind()     const { return m_kind; }
        bool is_function() const { return m_kind == SYMKIND_FUNCTION; }
        bool is_object()   const { return m_kind == SYMKIND_OBJECT; }

        endianess endian()      const { return m_endian; }
        bool is_little_endian() const { return m_endian == ENDIAN_LITTLE; }
        bool is_big_endian()    const { return m_endian == ENDIAN_BIG; }

        u64 size() const { return m_size; }

        u64 phys_addr() const { return m_phys; }
        u64 virt_addr() const { return m_virt; }

        range memory() const { return { m_virt, m_virt + m_size - 1 }; }
    };

    class symtab
    {
    public:
        struct symbol_compare {
            bool operator () (const symbol& a, const symbol& b) const {
                return a.virt_addr() < b.virt_addr();
            }
        };

        typedef std::set<symbol, symbol_compare> symset;

        size_t count_functions()  const { return m_functions.size(); }
        size_t count_objects()    const { return m_objects.size(); }

        const symset& functions() const { return m_functions; }
        const symset& objects()   const { return m_objects; }

        symtab() = default;
        ~symtab() = default;

        void insert(const symbol& sym);
        void remove(const symbol& sym);

        void clear();

        const symbol* find_symbol(const string& name) const;
        const symbol* find_symbol(u64 addr) const;

        const symbol* find_function(const string& name) const;
        const symbol* find_function(u64 addr) const;

        const symbol* find_object(const string& name) const;
        const symbol* find_object(u64 addr) const;

        void merge(const symtab&);

        u64 load_elf(const string& filename);

    private:
        std::set<symbol, symbol_compare> m_functions;
        std::set<symbol, symbol_compare> m_objects;

        unordered_map<string, const symbol*> m_function_names;
        unordered_map<string, const symbol*> m_object_names;

        void insert_function(const symbol& sym);
        void insert_object(const symbol& sym);

        void remove_function(const symbol& sym);
        void remove_object(const symbol& sym);
    };

}}

#endif
