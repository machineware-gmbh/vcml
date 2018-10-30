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

#include <gtest/gtest.h>
#include "vcml.h"

extern "C" int sc_main(int argc, char** argv) {
    ::testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}

TEST(elf, symbols) {
    EXPECT_TRUE(sc_core::sc_argc() >= 2);
    EXPECT_TRUE(sc_core::sc_argv()[1] != nullptr);

    std::string path = std::string(sc_core::sc_argv()[1]) + "/test_elf.elf";
    EXPECT_TRUE(vcml::file_exists(path));

    vcml::elf elf(path);
    EXPECT_EQ(elf.get_filename(), path);
    EXPECT_EQ(elf.get_entry_point(), 0x24e0);
    EXPECT_EQ(elf.get_endianess(), vcml::VCML_ENDIAN_BIG);

    EXPECT_FALSE(elf.is_64bit());
    EXPECT_FALSE(elf.get_symbols().empty());
    EXPECT_FALSE(elf.get_sections().empty());

    EXPECT_EQ(elf.get_num_sections(), 30);
    EXPECT_EQ(elf.get_num_symbols(), 71);

    EXPECT_NE(elf.get_section(".ctors"), nullptr);
    EXPECT_NE(elf.get_section(".text"), nullptr);
    EXPECT_NE(elf.get_section(".data"), nullptr);
    EXPECT_NE(elf.get_section(".bss"), nullptr);
    EXPECT_NE(elf.get_section(".init"), nullptr);
    EXPECT_NE(elf.get_section(".symtab"), nullptr);
    EXPECT_NE(elf.get_section(".strtab"), nullptr);

    vcml::elf_symbol* main = elf.get_symbol("main");
    EXPECT_NE(main, nullptr);
    EXPECT_EQ(main->get_name(), "main");
    EXPECT_EQ(main->get_type(), vcml::ELF_SYM_FUNCTION);
    EXPECT_EQ(main->get_virt_addr(), 0x233c);

    vcml::elf_symbol* ctors = elf.get_symbol("__CTOR_LIST__");
    EXPECT_NE(ctors, nullptr);
    EXPECT_EQ(ctors->get_name(), "__CTOR_LIST__");
    EXPECT_EQ(ctors->get_type(), vcml::ELF_SYM_OBJECT);
    EXPECT_EQ(ctors->get_virt_addr(), 0x4860);
}

TEST(elf, sections) {
    vcml::elf elf(sc_core::sc_argv()[0]);
    EXPECT_TRUE(elf.is_64bit());

    EXPECT_FALSE(elf.get_symbols().empty());
    EXPECT_FALSE(elf.get_sections().empty());

    vcml::elf_section* sec = elf.get_section(".text");
    EXPECT_NE(sec, nullptr);
    EXPECT_EQ(sec->get_name(), ".text");
    EXPECT_TRUE(sec->contains((vcml::u64)sc_main));
}
