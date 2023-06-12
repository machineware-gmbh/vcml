/******************************************************************************
 *                                                                            *
 * Copyright (C) 2022 MachineWare GmbH                                        *
 * All Rights Reserved                                                        *
 *                                                                            *
 * This is work is licensed under the terms described in the LICENSE file     *
 * found in the root directory of this source tree.                           *
 *                                                                            *
 ******************************************************************************/

#include <testing.h>
using namespace ::vcml::debugging;

TEST(symbol, construct) {
    symbol empty;
    EXPECT_FALSE(empty.is_function());
    EXPECT_FALSE(empty.is_object());
    EXPECT_EQ(empty.endian(), ENDIAN_UNKNOWN);

    symbol sym("sym", SYMKIND_OBJECT, ENDIAN_LITTLE, 4, 0x100, 0x200);
    EXPECT_FALSE(sym.is_function());
    EXPECT_TRUE(sym.is_object());
    EXPECT_EQ(sym.endian(), ENDIAN_LITTLE);
    EXPECT_EQ(sym.memory(), range(0x100, 0x103));
    EXPECT_EQ(sym.phys_addr(), 0x200);
}

TEST(symtab, inserting) {
    symbol func_a("func_a", SYMKIND_FUNCTION, ENDIAN_LITTLE, 40, 0xc00, 0x100);
    symbol func_b("func_b", SYMKIND_FUNCTION, ENDIAN_LITTLE, 40, 0xd00, 0x200);
    symbol var_a("var_a", SYMKIND_OBJECT, ENDIAN_LITTLE, 4, 0xe00, 0x300);
    symbol var_b("var_b", SYMKIND_OBJECT, ENDIAN_LITTLE, 8, 0xe04, 0x304);

    symtab syms;
    syms.insert(func_a);
    syms.insert(func_b);
    syms.insert(var_a);
    syms.insert(var_b);

    const auto& funcs = syms.functions();
    ASSERT_EQ(funcs.size(), 2);
    EXPECT_STREQ(funcs.begin()->name(), "func_a");
    EXPECT_STREQ(std::next(funcs.begin())->name(), "func_b");
}

TEST(symtab, finding_by_address) {
    symbol func_a("func_a", SYMKIND_FUNCTION, ENDIAN_LITTLE, 40, 0xc00, 0x100);
    symbol func_b("func_b", SYMKIND_FUNCTION, ENDIAN_LITTLE, 40, 0xd00, 0x200);
    symbol var_a("var_a", SYMKIND_OBJECT, ENDIAN_LITTLE, 4, 0xe00, 0x300);
    symbol var_b("var_b", SYMKIND_OBJECT, ENDIAN_LITTLE, 8, 0xe04, 0x304);

    symtab syms;
    syms.insert(func_a);
    syms.insert(func_b);
    syms.insert(var_a);
    syms.insert(var_b);

    const symbol* sym;

    sym = syms.find_function(0xc00);
    ASSERT_TRUE(sym != nullptr);
    EXPECT_TRUE(sym->is_function());
    EXPECT_TRUE(sym->memory().includes(0xc00));
    EXPECT_STREQ(sym->name(), "func_a");

    sym = syms.find_function(0xd10);
    ASSERT_TRUE(sym != nullptr);
    EXPECT_TRUE(sym->is_function());
    EXPECT_TRUE(sym->memory().includes(0xd10));
    EXPECT_STREQ(sym->name(), "func_b");

    sym = syms.find_function(0x100);
    EXPECT_TRUE(sym == nullptr);
    sym = syms.find_function(0x1000);
    EXPECT_TRUE(sym == nullptr);

    sym = syms.find_object(0xe00);
    ASSERT_TRUE(sym != nullptr);
    EXPECT_TRUE(sym->is_object());
    EXPECT_TRUE(sym->memory().includes(0xe00));
    EXPECT_STREQ(sym->name(), "var_a");

    sym = syms.find_object(0xe07);
    ASSERT_TRUE(sym != nullptr);
    EXPECT_TRUE(sym->is_object());
    EXPECT_TRUE(sym->memory().includes(0xe07));
    EXPECT_STREQ(sym->name(), "var_b");

    sym = syms.find_object(0x100);
    EXPECT_TRUE(sym == nullptr);
    sym = syms.find_object(0x1000);
    EXPECT_TRUE(sym == nullptr);
}

TEST(symtab, finding_by_name) {
    symbol func_a("func_a", SYMKIND_FUNCTION, ENDIAN_LITTLE, 40, 0xc00, 0x100);
    symbol func_b("func_b", SYMKIND_FUNCTION, ENDIAN_LITTLE, 40, 0xd00, 0x200);
    symbol var_a("var_a", SYMKIND_OBJECT, ENDIAN_LITTLE, 4, 0xe00, 0x300);
    symbol var_b("var_b", SYMKIND_OBJECT, ENDIAN_LITTLE, 8, 0xe04, 0x304);

    symtab syms;
    syms.insert(func_a);
    syms.insert(func_b);
    syms.insert(var_a);
    syms.insert(var_b);

    const symbol* sym;

    sym = syms.find_function("func_a");
    ASSERT_TRUE(sym != nullptr);
    EXPECT_TRUE(sym->is_function());
    EXPECT_STREQ(sym->name(), "func_a");
    EXPECT_EQ(sym->memory(), range(0xc00, 0xc27));

    sym = syms.find_function("func_b");
    ASSERT_TRUE(sym != nullptr);
    EXPECT_TRUE(sym->is_function());
    EXPECT_STREQ(sym->name(), "func_b");
    EXPECT_EQ(sym->memory(), range(0xd00, 0xd27));

    sym = syms.find_object("var_a");
    ASSERT_TRUE(sym != nullptr);
    EXPECT_TRUE(sym->is_object());
    EXPECT_STREQ(sym->name(), "var_a");
    EXPECT_EQ(sym->memory(), range(0xe00, 0xe03));

    sym = syms.find_object("var_b");
    ASSERT_TRUE(sym != nullptr);
    EXPECT_TRUE(sym->is_object());
    EXPECT_STREQ(sym->name(), "var_b");
    EXPECT_EQ(sym->memory(), range(0xe04, 0xe0b));
}

TEST(symtab, removing) {
    symbol func_a("func_a", SYMKIND_FUNCTION, ENDIAN_LITTLE, 40, 0xc00, 0x100);
    symbol func_b("func_b", SYMKIND_FUNCTION, ENDIAN_LITTLE, 40, 0xd00, 0x200);
    symbol var_a("var_a", SYMKIND_OBJECT, ENDIAN_LITTLE, 4, 0xe00, 0x300);
    symbol var_b("var_b", SYMKIND_OBJECT, ENDIAN_LITTLE, 8, 0xe04, 0x304);

    symtab syms;
    syms.insert(func_a);
    syms.insert(func_b);
    syms.insert(var_a);
    syms.insert(var_b);

    ASSERT_EQ(syms.count_functions(), 2);
    ASSERT_EQ(syms.count_functions(), 2);

    syms.remove(func_a);

    EXPECT_TRUE(syms.find_function(func_a.virt_addr()) == nullptr);
    EXPECT_TRUE(syms.find_function(func_a.name()) == nullptr);
    EXPECT_EQ(syms.count_functions(), 1);

    syms.remove(var_b);

    EXPECT_TRUE(syms.find_object(var_b.virt_addr()) == nullptr);
    EXPECT_TRUE(syms.find_object(var_b.name()) == nullptr);
    EXPECT_EQ(syms.count_objects(), 1);

    syms.clear();
    EXPECT_EQ(syms.count_functions(), 0);
    EXPECT_EQ(syms.count_objects(), 0);
}

TEST(symtab, load_elf) {
    symtab syms;
    syms.load_elf(get_resource_path("elf.elf"));

    const symbol* global_a = syms.find_symbol("global_a");
    ASSERT_TRUE(global_a != nullptr);
    EXPECT_TRUE(global_a->is_object());
    EXPECT_TRUE(global_a->is_little_endian());
    EXPECT_EQ(global_a->size(), 4);

    const symbol* global_b = syms.find_symbol("global_b");
    ASSERT_TRUE(global_b != nullptr);
    EXPECT_TRUE(global_b->is_object());
    EXPECT_TRUE(global_b->is_little_endian());
    EXPECT_EQ(global_b->size(), 8);

    const symbol* func_a = syms.find_symbol("func_c");
    ASSERT_TRUE(func_a != nullptr);
    EXPECT_TRUE(func_a->is_function());
    EXPECT_TRUE(func_a->is_little_endian());
    EXPECT_EQ(func_a->size(), 18);
}

TEST(symtab, find_with_offset) {
    symtab syms;
    syms.load_elf(get_resource_path("elf.elf"));

    ASSERT_FALSE(syms.empty());

    auto funcs = syms.functions();
    for (const symbol& func : funcs) {
        for (u64 offset = 0; offset < func.size(); offset++) {
            const symbol* other = syms.find_symbol(func.virt_addr() + offset);
            ASSERT_FALSE(other == nullptr);
            EXPECT_STREQ(other->name(), func.name());
        }
    }

    auto objs = syms.objects();
    for (const symbol& obj : objs) {
        for (u64 offset = 0; offset < obj.size(); offset++) {
            const symbol* other = syms.find_symbol(obj.virt_addr() + offset);
            ASSERT_FALSE(other == nullptr);
            EXPECT_STREQ(other->name(), obj.name());
        }
    }
}
