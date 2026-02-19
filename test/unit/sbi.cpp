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

TEST(sbi, to_string) {
    auto sbi0 = SBI_DEBUG | SBI_NODMI | SBI_SYNC | SBI_INSN | SBI_EXCL |
                SBI_LOCK | SBI_SECURE | SBI_TR_REQ | sbi_cpuid(4) |
                sbi_privilege(2) | sbi_asid(6);
    EXPECT_EQ(tlm_sbi_to_str(sbi0),
              "CPU4 P2 ASID6 +debug +nodmi +sync +insn +excl +lock +secure "
              "+txrq");

    auto sbi1 = SBI_EXCL | SBI_SECURE | SBI_TRANSLATED;
    EXPECT_EQ(tlm_sbi_to_str(sbi1), "CPU0 +excl +secure +translated");

    u32 data = 0x44332211;
    tlm_generic_payload tx;
    tx_setup(tx, TLM_WRITE_COMMAND, 0x1234, &data, sizeof(data));
    tx_set_sbi(tx, SBI_NODMI | SBI_SYNC | SBI_SECURE);
    tx.set_response_status(TLM_OK_RESPONSE);
    EXPECT_EQ(tlm_transaction_to_str(tx),
              "WR 0x00001234 [11 22 33 44] CPU0 +nodmi +sync +secure "
              "(TLM_OK_RESPONSE)");
}

TEST(sbi, error) {
    EXPECT_DEATH({ sbi_cpuid(3) | sbi_cpuid(4); }, "sbi.cpuid");
    EXPECT_DEATH({ sbi_asid(12) | sbi_asid(13); }, "sbi.asid");
    sbi_asid(12) | sbi_asid(12); // should be ok
    sbi_cpuid(7) | sbi_cpuid(7); // should be ok
}
