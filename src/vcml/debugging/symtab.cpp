/******************************************************************************
 *                                                                            *
 * Copyright (C) 2022 MachineWare GmbH                                        *
 * All Rights Reserved                                                        *
 *                                                                            *
 * This is work is licensed under the terms described in the LICENSE file     *
 * found in the root directory of this source tree.                           *
 *                                                                            *
 ******************************************************************************/

#include "vcml/debugging/symtab.h"

namespace vcml {
namespace debugging {

symbol::symbol():
    m_name(),
    m_kind(SYMKIND_UNKNOWN),
    m_endian(ENDIAN_UNKNOWN),
    m_size(0),
    m_virt(~0ull),
    m_phys(~0ull) {
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

    // find first that is not greater than addr
    const symbol& sym = *--it;
    return sym.memory().includes(addr) ? &sym : nullptr;
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

    // find first that is not greater than addr
    const symbol& sym = *--it;
    return sym.memory().includes(addr) ? &sym : nullptr;
}

void symtab::merge(const symtab& other) {
    for (const auto& func : other.m_functions)
        insert_function(func);
    for (const auto& obj : other.m_objects)
        insert_object(obj);
}

u64 symtab::load_elf(const string& filename) {
    if (!mwr::file_exists(filename))
        return 0;

    mwr::elf reader(filename);
    endianess endian = reader.is_big_endian() ? ENDIAN_BIG : ENDIAN_LITTLE;

    for (const auto& symbol : reader.symbols()) {
        switch (symbol.kind) {
        case mwr::elf::KIND_OBJECT:
        case mwr::elf::KIND_COMMON:
        case mwr::elf::KIND_TLS:
            insert({ symbol.name, SYMKIND_OBJECT, endian, symbol.size,
                     symbol.virt, symbol.phys });
            break;

        case mwr::elf::KIND_FUNC:
            insert({ symbol.name, SYMKIND_FUNCTION, endian, symbol.size,
                     symbol.virt, symbol.phys });
            break;

        case mwr::elf::KIND_UNKNOWN:
        case mwr::elf::KIND_NONE:
        default:
            break;
        }
    }

    return reader.symbols().size();
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

} // namespace debugging
} // namespace vcml
