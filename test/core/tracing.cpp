/******************************************************************************
 *                                                                            *
 * Copyright 2021 Jan Henrik Weinstock                                        *
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

#include "testing.h"

MATCHER_P3(match_trace, dir, addr, data, "matches trace entry") {
    return arg.kind == PROTO_TLM && arg.dir == dir &&
           !arg.error && arg.payload.get_address() == addr &&
           arg.payload.get_data_length() == 4 &&
           memcmp(arg.payload.get_data_ptr(), &data, 4) == 0;
}

MATCHER_P(match_trace_error, err, "matches if trace has error set") {
    return arg.error == err && is_backward_trace(arg.dir);
}

class mock_tracer: public vcml::tracer
{
public:
    mock_tracer(): tracer() {}
    MOCK_METHOD(void, trace, (const tracer::entry<tlm_generic_payload>&), (override));
};

class test_harness: public test_base
{
public:
    tracer_term term;
    mock_tracer mock;

    u64 addr;
    u32 data;

    tlm_initiator_socket OUT;
    tlm_target_socket IN;

    test_harness(const sc_module_name& nm):
        test_base(nm),
        term(),
        mock(),
        addr(),
        data(),
        OUT("OUT"),
        IN("IN") {
        OUT.bind(IN);
    }

    virtual unsigned int transport(tlm_generic_payload& tx,
        const tlm_sbi& info, address_space as) override {
        if (tx.get_address() == 0) {
            tx.set_response_status(TLM_ADDRESS_ERROR_RESPONSE);
            return 0;
        }

        EXPECT_EQ(tx.get_address(), addr)
            << "received wrong address";
        EXPECT_EQ(tx.get_data_length(), sizeof(data))
            << "received wrong size";
        EXPECT_EQ(*reinterpret_cast<u32*>(tx.get_data_ptr()), data)
            << "received wrong data";
        EXPECT_FALSE(info.is_debug)
            << "received debug request";

        tx.set_response_status(TLM_OK_RESPONSE);
        return tx.get_data_length();
    }

    virtual void run_test() override {
        addr = 0x420;
        data = 0x1234;

        OUT.trace = true;
        OUT.trace_errors = false;

        EXPECT_CALL(mock, trace(match_trace(TRACE_FW, addr, data)));
        EXPECT_CALL(mock, trace(match_trace(TRACE_BW, addr, data)));
        EXPECT_OK(OUT.writew(addr, data)) << "failed to send transaction";

        OUT.trace = false;
        OUT.trace_errors = false;

        EXPECT_CALL(mock, trace(_)).Times(0);
        EXPECT_OK(OUT.writew(addr, data)) << "failed to send transaction";

        OUT.trace = false;
        OUT.trace_errors = true;

        EXPECT_CALL(mock, trace(match_trace_error(true))).Times(1);
        EXPECT_AE(OUT.writew(0, data)) << "did not get an address error";
    }
};

TEST(tracing, basic) {
    test_harness test("harness");
    sc_core::sc_start();
}


