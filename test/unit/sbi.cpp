/******************************************************************************
 *                                                                            *
 * Copyright (C) 2023 MachineWare GmbH                                        *
 * All Rights Reserved                                                        *
 *                                                                            *
 * This is work is licensed under the terms described in the LICENSE file     *
 * found in the root directory of this source tree.                           *
 *                                                                            *
 ******************************************************************************/

#include "testing.h"

TEST(sbi, init) {
    auto sbi = SBI_INSN | sbi_cpuid(3);
    EXPECT_TRUE(sbi.is_insn);
    EXPECT_EQ(sbi.cpuid, 3);
    EXPECT_EQ(sbi.atype, SBI_ATYPE_UX);
    EXPECT_EQ(sbi.privilege, SBI_PRIVILEGE_NONE);
    EXPECT_EQ(sbi.asid, SBI_ASID_GLOBAL);

    auto excl = SBI_EXCL | sbi_asid(3);
    EXPECT_TRUE(excl.is_excl);
    EXPECT_EQ(excl.asid, 3);
    EXPECT_FALSE(excl.is_secure);

    auto debug = SBI_DEBUG | SBI_NODMI;
    EXPECT_TRUE(debug.is_debug);
    EXPECT_TRUE(debug.is_nodmi);
    EXPECT_FALSE(debug.is_insn);

    auto secure = SBI_SECURE | SBI_SYNC;
    EXPECT_TRUE(secure.is_secure);
    EXPECT_TRUE(secure.is_sync);
    EXPECT_FALSE(secure.is_debug);
}

TEST(sbi, error) {
    EXPECT_DEATH({ sbi_cpuid(3) | sbi_cpuid(4); }, "sbi.cpuid");
    EXPECT_DEATH({ sbi_asid(12) | sbi_asid(13); }, "sbi.asid");
    sbi_asid(12) | sbi_asid(12); // should be ok
    sbi_cpuid(7) | sbi_cpuid(7); // should be ok
}
