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
    vcml::elf elf(sc_core::sc_argv()[0]);
    EXPECT_TRUE(elf.is_64bit());

    EXPECT_FALSE(elf.get_symbols().empty());
    EXPECT_FALSE(elf.get_sections().empty());

    vcml::elf_symbol* sym = elf.get_symbol("sc_main");
    EXPECT_NE(sym, nullptr);
    EXPECT_EQ(sym->get_name(), "sc_main");
    EXPECT_EQ(sym->get_type(), vcml::ELF_SYM_FUNCTION);
    EXPECT_EQ(sym->get_virt_addr(), (vcml::u64)sc_main);
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
