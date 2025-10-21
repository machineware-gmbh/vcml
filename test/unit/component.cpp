/******************************************************************************
 *                                                                            *
 * Copyright (C) 2022 MachineWare GmbH                                        *
 * All Rights Reserved                                                        *
 *                                                                            *
 * This is work is licensed under the terms described in the LICENSE file     *
 * found in the root directory of this source tree.                           *
 *                                                                            *
 ******************************************************************************/

#include <gtest/gtest.h>
#include <gmock/gmock.h>

#include <systemc>
#include "vcml.h"

#define ASSERT_OK(tlmcall) ASSERT_EQ(tlmcall, tlm::TLM_OK_RESPONSE)
#define ASSERT_AE(tlmcall) ASSERT_EQ(tlmcall, tlm::TLM_ADDRESS_ERROR_RESPONSE)
#define ASSERT_CE(tlmcall) ASSERT_EQ(tlmcall, tlm::TLM_COMMAND_ERROR_RESPONSE)

using namespace ::testing;
using namespace sc_core;
using namespace vcml;

class test_component : public component
{
public:
    tlm_target_socket in;
    tlm_initiator_socket out;

    generic::clock clock_gen;
    generic::reset reset_gen;

    MOCK_METHOD(void, handle_clock_update, (hz_t, hz_t));
    MOCK_METHOD(void, reset, ());

    test_component(const sc_module_name& nm):
        component(nm),
        in("in"),
        out("out"),
        clock_gen("clock_gen", 100 * MHz),
        reset_gen("reset_gen") {
        out.bind(in);

        clock_gen.clk.bind(clk);
        reset_gen.rst.bind(rst);

        SC_HAS_PROCESS(test_component);
        SC_THREAD(run_test);

        EXPECT_CALL(*this, reset()); // initial reset
    }

    virtual unsigned int transport(tlm_generic_payload& tx, const tlm_sbi& sbi,
                                   address_space as) override {
        EXPECT_EQ(as, VCML_AS_DEFAULT);
        EXPECT_EQ(tx.get_address(), 0x0);
        EXPECT_EQ(tx.get_data_length(), 4);
        EXPECT_NE(tx.get_data_ptr(), nullptr);
        tx.set_response_status(TLM_OK_RESPONSE);
        return tx.get_data_length();
    }

    void run_test() {
        wait(SC_ZERO_TIME);

        u32 data = 0xf3f3f3f3;
        unsigned char* dmi_ptr = (unsigned char*)&data;
        map_dmi(dmi_ptr, 0, 3, VCML_ACCESS_READ);

        ASSERT_OK(out.readw<u32>(0, data))
            << "component did respond to read command";

        tlm_dmi dmi; // previous read should have provided DMI access
        ASSERT_TRUE(out.dmi_cache().lookup(0, 4, TLM_READ_COMMAND, dmi))
            << "component did not provide DMI mapping";
        EXPECT_TRUE(dmi.is_read_allowed())
            << "component denied previously granted DMI read access";
        EXPECT_FALSE(dmi.is_write_allowed())
            << "component granted previously denied DMI write access";
        EXPECT_FALSE(dmi.is_read_write_allowed())
            << "component grants both read- and write access";
        EXPECT_EQ(dmi.get_dmi_ptr(), dmi_ptr)
            << "component returned invalid DMI pointer";

        ASSERT_OK(out.writew<u32>(0, data))
            << "component did not respond to write command";

        EXPECT_EQ(clk.get_hz(), 100 * MHz)
            << "component reported invalid clock rate";
        EXPECT_CALL(*this, handle_clock_update(100 * MHz, 10 * GHz));
        clock_gen.clk = 10 * GHz;
        EXPECT_EQ(clk.get_hz(), 10 * GHz)
            << "component reported invalid new clock rate";

        EXPECT_CALL(*this, reset());
        reset_gen.rst.pulse();

        sc_stop();
    }
};

TEST(component, sockets) {
    test_component test("test");

    sc_start();

    ASSERT_EQ(sc_get_status(), SC_STOPPED);
}
