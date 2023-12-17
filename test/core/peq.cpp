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

class peq_test : public test_base
{
public:
    peq_test(const sc_module_name& nm): test_base(nm) {}
    virtual ~peq_test() = default;

    virtual void run_test() override {
        int val;
        peq<int> queue("peq");

        EXPECT_STREQ(queue.name(), "test.run.peq");
        EXPECT_STREQ(queue.kind(), "vcml::peq");

        queue.notify(2, 2.0, SC_SEC);
        queue.notify(1, 1.0, SC_SEC);
        queue.notify(3, 3.0, SC_SEC);
        queue.notify(3, 3.0, SC_SEC);

        queue.wait(val);
        EXPECT_EQ(val, 1);
        EXPECT_EQ(sc_time_stamp(), sc_time(1.0, SC_SEC));

        queue.notify(4, 2.0, SC_SEC);

        queue.wait(val);
        EXPECT_EQ(val, 2);
        EXPECT_EQ(sc_time_stamp(), sc_time(2.0, SC_SEC));

        queue.wait(val);
        EXPECT_EQ(val, 3);
        EXPECT_EQ(sc_time_stamp(), sc_time(3.0, SC_SEC));

        queue.wait(val);
        EXPECT_EQ(val, 4);
        EXPECT_EQ(sc_time_stamp(), sc_time(3.0, SC_SEC));

        queue.notify(5, 1.0, SC_SEC);
        queue.notify(6, 2.0, SC_SEC);
        queue.cancel(5);
        queue.cancel(99);

        queue.wait(val);
        EXPECT_EQ(val, 6);
        EXPECT_EQ(sc_time_stamp(), sc_time(5.0, SC_SEC));
    }
};

TEST(peq, test) {
    peq_test test("test");
    sc_core::sc_start();
}
