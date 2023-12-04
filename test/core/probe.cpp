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

class test_harness : public test_base
{
public:
    tlm_probe probe;
    tlm_initiator_socket out;
    tlm_target_socket in;

    test_harness(const sc_module_name& nm):
        test_base(nm), probe("probe"), out("out"), in("in") {
        tlm_bind(*this, "out", probe, "in");
        tlm_bind(probe, "out", *this, "in");
        EXPECT_STREQ(probe.kind(), "vcml::tlm_probe");
    }

    virtual ~test_harness() = default;

    MOCK_METHOD(void, receive, (tlm_command, u64));

    virtual unsigned int transport(tlm_target_socket& socket,
                                   tlm_generic_payload& tx,
                                   const tlm_sbi& sideband) override {
        receive(tx.get_command(), tx.get_address());
        tx.set_response_status(TLM_OK_RESPONSE);
        return tx.get_data_length();
    }

    virtual void run_test() override {
        u32 data = -1;
        EXPECT_CALL(*this, receive(TLM_WRITE_COMMAND, 0x1234));
        EXPECT_OK(out.writew(0x1234, data));

        EXPECT_CALL(*this, receive(TLM_READ_COMMAND, 0x5678));
        EXPECT_OK(out.readw(0x5678, data));
    }
};

TEST(tlm, probe) {
    test_harness test("tlm");
    sc_core::sc_start();
}
