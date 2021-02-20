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

#include "vcml/debugging/symtab.h"
#include "vcml/debugging/elf_reader.h"

namespace vcml { namespace debugging {

    symbol::symbol():
       m_name(),
       m_kind(SYMKIND_UNKNOWN),
       m_endian(ENDIAN_UNKNOWN),
       m_size(0),
       m_phys(~0ull),
       m_virt(~0ull) {
    }

    symbol::symbol(const string& name, symkind kind, endianess endian, u64 sz,
       u64 virt_addr, u64 phys_addr):
       m_name(name),
       m_kind(kind),
       m_endian(endian),
       m_size(sz),
       m_virt(virt_addr),
       m_phys(phys_addr) {
    }

    void symtab::insert(const symbol& sym) {
        if (!sym.is_function() && !sym.is_object())
            VCML_ERROR("symbol '%s' has no known type", sym.name());

        if (sym.is_function())
            insert_function(sym);

        if (sym.is_object())
            insert_object(sym);
    }

    void symtab::remove(const symbol& sym) {
        if (!sym.is_function() && !sym.is_object())
            VCML_ERROR("symbol '%s' has no known type", sym.name());

        if (sym.is_function())
            remove_function(sym);

        if (sym.is_object())
            remove_object(sym);
    }

    void symtab::clear() {
        m_functions.clear();
        m_objects.clear();
        m_function_names.clear();
        m_object_names.clear();
    }

    const symbol* symtab::find_symbol(const string& name) const {
        const symbol* result = find_function(name);
        if (result != nullptr)
            return result;
        return find_object(name);
    }

    const symbol* symtab::find_symbol(u64 addr) const {
        const symbol* result = find_function(addr);
        if (result != nullptr)
            return result;
        return find_object(addr);
    }

    const symbol* symtab::find_function(const string& name) const {
        const auto it = m_function_names.find(name);
        return it != m_function_names.end() ? it->second : nullptr;
    }

    const symbol* symtab::find_function(u64 addr) const {
        if (m_functions.empty())
            return nullptr;
        if (addr < m_functions.begin()->virt_addr())
            return nullptr;
        if (addr > m_functions.rbegin()->memory().end)
            return nullptr;

        symbol upper("", SYMKIND_FUNCTION, ENDIAN_LITTLE, 0, addr, addr);
        auto it = m_functions.upper_bound(upper);
        return &(*--it); // find first that is *not* greater than addr
    }

    const symbol* symtab::find_object(const string& name) const {
        const auto it = m_object_names.find(name);
        return it != m_object_names.end() ? it->second : nullptr;
    }

    const symbol* symtab::find_object(u64 addr) const {
        if (m_objects.empty())
            return nullptr;
        if (addr < m_objects.begin()->virt_addr())
            return nullptr;
        if (addr > m_objects.rbegin()->memory().end)
            return nullptr;

        symbol upper("", SYMKIND_OBJECT, ENDIAN_LITTLE, 0, addr, addr);
        auto it = m_objects.upper_bound(upper);
        return &(*--it); // first that is *not* greater than addr
    }

    void symtab::merge(const symtab& other) {
        for (const auto& func : other.m_functions)
            insert_function(func);
        for (const auto& obj : other.m_objects)
            insert_object(obj);
    }

    u64 symtab::load_elf(const string& filename) {
        if (!file_exists(filename))
            return 0;

        elf_reader loader(filename);
        return loader.read_symbols(*this);
    }

    void symtab::insert_function(const symbol& sym) {
        auto insert = m_functions.insert(sym);
        if (insert.second)
            m_function_names[sym.name()] = &(*insert.first);
    }

    void symtab::insert_object(const symbol& sym) {
        auto insert = m_objects.insert(sym);
        if (insert.second)
            m_object_names[sym.name()] = &(*insert.first);
    }

    void symtab::remove_function(const symbol& sym) {
        auto func = m_function_names.find(sym.name());
        if (func != m_function_names.end())
            m_function_names.erase(func);
        m_functions.erase(sym);
    }

    void symtab::remove_object(const symbol& sym) {
        auto obj = m_object_names.find(sym.name());
        if (obj != m_object_names.end())
            m_object_names.erase(obj);
        m_objects.erase(sym);
    }

}}
