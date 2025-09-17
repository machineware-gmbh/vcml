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

class tlm_harness : public test_base
{
public:
    tlm_initiator_socket tlm_ifull;
    tlm_base_initiator_socket tlm_ibase;
    tlm_base_target_socket tlm_tbase;
    tlm_target_socket tlm_tfull;

    tlm_initiator_array<128> tlm_out;
    tlm_base_initiator_array<128> tlm_out_h;
    tlm_base_target_array<128> tlm_in_h;
    tlm_target_array<128> tlm_in;

    tlm_harness(const sc_module_name& nm):
        test_base(nm),
        tlm_ifull("tlm_ifull"),
        tlm_ibase("tlm_ibase"),
        tlm_tbase("tlm_tbase"),
        tlm_tfull("tlm_tfull"),
        tlm_out("tlm_out"),
        tlm_out_h("tlm_out_h"),
        tlm_in_h("tlm_in_h"),
        tlm_in("tlm_in") {
        // test binding checker
        EXPECT_FALSE(tlm_ifull.is_bound());
        EXPECT_FALSE(tlm_ibase.is_bound());
        EXPECT_FALSE(tlm_tbase.is_bound());
        EXPECT_FALSE(tlm_tfull.is_bound());

        // test hierarchy binding
        tlm_bind(*this, "tlm_ifull", *this, "tlm_ibase");
        tlm_bind(*this, "tlm_tbase", *this, "tlm_tfull");
        tlm_bind(*this, "tlm_ibase", *this, "tlm_tbase");
        tlm_bind(*this, "tlm_out", 0, *this, "tlm_out_h", 1);
        tlm_bind(*this, "tlm_in_h", 2, *this, "tlm_in", 3);
        tlm_bind(*this, "tlm_out_h", 1, *this, "tlm_in_h", 2);

        // test binding checker update
        EXPECT_TRUE(tlm_ifull.is_bound());
        EXPECT_TRUE(tlm_ibase.is_bound());
        EXPECT_TRUE(tlm_tbase.is_bound());
        EXPECT_TRUE(tlm_tfull.is_bound());

        // test stubbing
        tlm_bind(*this, "tlm_out", 44, *this, "tlm_out_h", 44);
        tlm_bind(*this, "tlm_in_h", 55, *this, "tlm_in", 55);
        tlm_stub(*this, "tlm_out", 33);
        tlm_stub(*this, "tlm_out_h", 44);
        tlm_stub(*this, "tlm_in_h", 55);
        tlm_stub(*this, "tlm_in", 66);

        EXPECT_TRUE(find_object("tlm.tlm_out[33]_stub"));
        EXPECT_TRUE(find_object("tlm.tlm_out_h[44]_stub"));
        EXPECT_TRUE(find_object("tlm.tlm_in_h[55]_stub"));
        EXPECT_TRUE(find_object("tlm.tlm_in[66]_stub"));

        EXPECT_EQ(tlm_out.limit(), 128);
        EXPECT_EQ(tlm_out_h.limit(), 128);
        EXPECT_EQ(tlm_in_h.limit(), 128);
        EXPECT_EQ(tlm_in.limit(), 128);

        // test generic stubbing
        stub("tlm.tlm_out[77]");
        stub("tlm.tlm_out");
        EXPECT_TRUE(find_object("tlm.tlm_out[77]_stub"));
        EXPECT_TRUE(find_object("tlm.tlm_out[78]_stub"));
    }

    virtual ~tlm_harness() = default;

    MOCK_METHOD(void, receive, (string, u64));

    virtual unsigned int transport(tlm_target_socket& socket,
                                   tlm_generic_payload& tx,
                                   const tlm_sbi& sideband) override {
        receive(socket.basename(), tx.get_address());
        tx.set_response_status(TLM_OK_RESPONSE);
        return tx.get_data_length();
    }

    virtual void run_test() override {
        EXPECT_DEATH(tlm_out[123], "no socket at index 123");

        EXPECT_CALL(*this, receive("tlm_tfull", 0x1234));
        EXPECT_OK(tlm_ifull.writew(0x1234, 0));

        EXPECT_CALL(*this, receive("tlm_in[3]", 0x5678));
        EXPECT_OK(tlm_out[0].writew(0x5678, 0));
    }
};

TEST(tlm, base_sockets) {
    tlm_harness test("tlm");
    sc_core::sc_start();
}
