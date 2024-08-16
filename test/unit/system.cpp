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

class harness : public vcml::system
{
public:
    mwr::publishers::terminal term;
    mock_publisher pub;

    harness(const sc_module_name& nm): vcml::system(nm), term(), pub() {
        SC_HAS_PROCESS(harness);
        SC_METHOD(test_method);
        pub.expect(LOG_INFO, "starting infinite simulation");
    }

    virtual ~harness() = default;

    void test_method() {
        pub.expect(LOG_ERROR, "wait() is only allowed in SC_THREADs");
        wait(SC_ZERO_TIME);
    }
};

TEST(system, exceptions) {
    // restore default handler, otherwise reports will count as a failure
    auto handler = ::sc_core::sc_report_handler::default_handler;
    ::sc_core::sc_report_handler::set_handler(handler);

    harness test("harness");
    EXPECT_EQ(test.run(), EXIT_FAILURE);
}
