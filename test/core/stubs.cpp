/******************************************************************************
 *                                                                            *
 * Copyright (C) 2022 MachineWare GmbH                                        *
 * All Rights Reserved                                                        *
 *                                                                            *
 * This is work is licensed under the terms described in the LICENSE file     *
 * found in the root directory of this source tree.                           *
 *                                                                            *
 ******************************************************************************/

#include "testing.h"

class test_harness : public test_base
{
public:
    tlm_initiator_socket out;
    tlm_target_socket in;

    test_harness(const sc_module_name& nm):
        test_base(nm), out("out"), in("in") {
        out.stub(TLM_ADDRESS_ERROR_RESPONSE);
        tlm_stub(*this, "in");
    }

    virtual void run_test() override {
        u32 data = 0;
        sc_time now = local_time_stamp();
        EXPECT_AE(out.writew(0x1234, data))
            << "stub did not response with programmed status";
        EXPECT_EQ(now, local_time_stamp()) << "stub advanced systemc time";

        in.invalidate_dmi(); // no response, but test for aborts
    }
};

TEST(stubs, transactions) {
    test_harness test("harness");
    sc_core::sc_start();
}
