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

MATCHER_P3(match_trace, dir, addr, data, "matches trace entry") {
    return arg.kind == PROTO_TLM && arg.dir == dir && !arg.error &&
           arg.payload.get_address() == addr &&
           arg.payload.get_data_length() == 4 &&
           memcmp(arg.payload.get_data_ptr(), &data, 4) == 0;
}

MATCHER_P(match_trace_error, err, "matches if trace has error set") {
    return arg.error == err && is_backward_trace(arg.dir);
}

class mock_tracer : public vcml::tracer
{
public:
    mock_tracer(): tracer() {}
    MOCK_METHOD(void, trace, (const tracer::activity<tlm_generic_payload>&),
                (override));

    virtual void trace(const activity<gpio_payload>&) override {}
    virtual void trace(const activity<clk_payload>&) override {}
    virtual void trace(const activity<pci_payload>&) override {}
    virtual void trace(const activity<i2c_payload>&) override {}
    virtual void trace(const activity<spi_payload>&) override {}
    virtual void trace(const activity<sd_command>&) override {}
    virtual void trace(const activity<sd_data>&) override {}
    virtual void trace(const activity<vq_message>&) override {}
    virtual void trace(const activity<serial_payload>&) override {}
    virtual void trace(const activity<eth_frame>&) override {}
    virtual void trace(const activity<can_frame>&) override {}
    virtual void trace(const activity<usb_packet>&) override {}
};

class test_harness : public test_base
{
public:
    tracer_term term;
    mock_tracer mock;

    u64 addr;
    u32 data;

    tlm_initiator_socket out;
    tlm_target_socket in;

    test_harness(const sc_module_name& nm):
        test_base(nm), term(), mock(), addr(), data(), out("out"), in("in") {
        out.bind(in);
    }

    virtual unsigned int transport(tlm_generic_payload& tx,
                                   const tlm_sbi& info,
                                   address_space as) override {
        if (tx.get_address() == 0) {
            tx.set_response_status(TLM_ADDRESS_ERROR_RESPONSE);
            return 0;
        }

        EXPECT_EQ(tx.get_address(), addr) << "received wrong address";
        EXPECT_EQ(tx.get_data_length(), sizeof(data)) << "received wrong size";
        EXPECT_EQ(*reinterpret_cast<u32*>(tx.get_data_ptr()), data)
            << "received wrong data";
        EXPECT_FALSE(info.is_debug) << "received debug request";

        tx.set_response_status(TLM_OK_RESPONSE);
        return tx.get_data_length();
    }

    virtual void run_test() override {
        addr = 0x420;
        data = 0x1234;

        out.trace_all = true;
        out.trace_errors = false;

        EXPECT_CALL(mock, trace(match_trace(TRACE_FW, addr, data)));
        EXPECT_CALL(mock, trace(match_trace(TRACE_BW, addr, data)));
        EXPECT_OK(out.writew(addr, data)) << "failed to send transaction";

        out.trace_all = false;
        out.trace_errors = false;

        EXPECT_CALL(mock, trace(_)).Times(0);
        EXPECT_OK(out.writew(addr, data)) << "failed to send transaction";

        out.trace_all = false;
        out.trace_errors = true;

        EXPECT_CALL(mock, trace(match_trace_error(true))).Times(1);
        EXPECT_AE(out.writew(0, data)) << "did not get an address error";
    }
};

TEST(tracing, basic) {
    for (int i = 0; i < NUM_PROTOCOLS; i++) {
        EXPECT_STRNE(protocol_name((protocol_kind)i), "unknown protocol")
            << "name undefined for protocol " << i;
        EXPECT_NE(tracer_term::colors[i], nullptr)
            << "color undefined for protocol "
            << protocol_name((protocol_kind)i);
    }

    test_harness test("harness");
    sc_core::sc_start();
}
