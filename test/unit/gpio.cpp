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

TEST(gpio, to_string) {
    gpio_payload tx;
    tx.vector = 42;
    tx.state = true;

    // no formatting checks, just make sure it compiles
    std::cout << tx << std::endl;
}

MATCHER_P(gpio_s, name, "Matches a gpio socket by name") {
    return strcmp(arg.basename(), name) == 0;
}

class gpio_test_harness : public test_base
{
public:
    gpio_initiator_socket out;
    gpio_initiator_socket out2;
    gpio_target_array<256> in;

    // for testing hierarchical binding
    gpio_base_initiator_socket h_out;
    gpio_base_target_socket h_in;

    // for adapter testing
    gpio_initiator_socket a_out;
    sc_signal<bool> signal;
    gpio_target_socket a_in;

    gpio_initiator_array<256> arr_out;
    sc_signal<bool> signal2;
    gpio_target_array<256> arr_in;

    // for array offset binding test
    gpio_initiator_array<4> ab_out0;
    gpio_initiator_array<4> ab_out1;
    gpio_base_initiator_array<8> ab_out_base;
    gpio_target_array<6> ab_in0;
    gpio_target_array<2> ab_in1;
    gpio_base_target_array<8> ab_in_base;

    gpio_test_harness(const sc_module_name& nm):
        test_base(nm),
        out("out"),
        out2("out2"),
        in("in"),
        h_out("h_out"),
        h_in("h_in"),
        a_out("a_out"),
        signal("signal"),
        a_in("a_in"),
        arr_out("arr_out"),
        signal2("signal2"),
        arr_in("arr_in"),
        ab_out0("ab_out0"),
        ab_out1("ab_out1"),
        ab_out_base("ab_out_base"),
        ab_in0("ab_in0"),
        ab_in1("ab_in1"),
        ab_in_base("ab_in_base") {
        // check array dimensions
        EXPECT_EQ(in.limit(), 256);
        EXPECT_EQ(arr_out.limit(), 256);
        EXPECT_EQ(arr_in.limit(), 256);

        // check simple binding
        gpio_bind(*this, "out", *this, "in", 0);

        // check hierarchical binding: out -> h_out -> h_in -> in[1]
        gpio_bind(*this, "out", *this, "h_out");
        gpio_bind(*this, "h_in", *this, "in", 1);
        gpio_bind(*this, "h_out", *this, "h_in");

        // check stubbing
        gpio_stub(*this, "out2");
        gpio_stub(*this, "in", 2);
        EXPECT_TRUE(out2.is_stubbed());
        EXPECT_TRUE(in[2].is_stubbed());

        // check adapters
        gpio_bind(*this, "a_out", signal);
        gpio_bind(*this, "a_in", signal);
        gpio_bind(*this, "arr_out", 14, signal2);
        gpio_bind(*this, "arr_in", 102, signal2);

        // did the port get created?
        EXPECT_TRUE(find_object("gpio.out2"));
        EXPECT_TRUE(find_object("gpio.in[2]"));

        // did the stubs get created?
        EXPECT_TRUE(find_object("gpio.out2_stub"));
        EXPECT_TRUE(find_object("gpio.in[2]_stub"));

        // did the adapters get created?
        EXPECT_TRUE(find_object("gpio.a_out_adapter"));
        EXPECT_TRUE(find_object("gpio.a_in_adapter"));
        EXPECT_TRUE(find_object("gpio.arr_out[14]_adapter"));
        EXPECT_TRUE(find_object("gpio.arr_in[102]_adapter"));

        // test array binding with offsets
        ab_out0.bind(ab_out_base, 0);
        ab_out1.bind(ab_out_base, 4);
        ab_in_base.bind(ab_in0, 0);

        EXPECT_DEATH({ ab_in_base.bind(ab_in1, 5); }, "cannot bind");
        ab_in_base.bind(ab_in1, 6);

        for (size_t i = 0; i < ab_out_base.limit(); i++)
            ab_out_base[i].bind(ab_in_base[i]);
    }

    MOCK_METHOD(void, gpio_notify,
                (const gpio_target_socket&, bool, gpio_vector));

    virtual void run_test() override {
        // this also forces construction of IN[0]'s default event so that it
        // it can be triggered later on
        EXPECT_TRUE(in[0].default_event().name());

        enum test_vectors : gpio_vector {
            TEST_VECTOR = 0x42,
        };

        // out is bound to in[0] and in[1], so we have to expect each callback
        // to be invoked twice.

        // test callbacks
        EXPECT_CALL(*this, gpio_notify(gpio_s("in[0]"), true, GPIO_NO_VECTOR));
        EXPECT_CALL(*this, gpio_notify(gpio_s("in[1]"), true, GPIO_NO_VECTOR));
        out = true;
        out = true; // should not trigger a second time
        EXPECT_TRUE(in[0]);
        EXPECT_TRUE(in[1]);

        EXPECT_CALL(*this,
                    gpio_notify(gpio_s("in[0]"), false, GPIO_NO_VECTOR));
        EXPECT_CALL(*this,
                    gpio_notify(gpio_s("in[1]"), false, GPIO_NO_VECTOR));
        out = false;
        out = false; // should not trigger a second time
        EXPECT_FALSE(in[0]);
        EXPECT_FALSE(in[1]);

        // test vectors
        EXPECT_CALL(*this, gpio_notify(gpio_s("in[0]"), true, TEST_VECTOR));
        EXPECT_CALL(*this, gpio_notify(gpio_s("in[1]"), true, TEST_VECTOR));
        out.raise(TEST_VECTOR);
        EXPECT_TRUE(in[0][TEST_VECTOR]);
        EXPECT_TRUE(in[1][TEST_VECTOR]);

        EXPECT_CALL(*this, gpio_notify(gpio_s("in[0]"), false, TEST_VECTOR));
        EXPECT_CALL(*this, gpio_notify(gpio_s("in[1]"), false, TEST_VECTOR));
        out.lower(TEST_VECTOR);
        EXPECT_FALSE(in[0].read(TEST_VECTOR));
        EXPECT_FALSE(in[1].read(TEST_VECTOR));

        // test default events
        EXPECT_CALL(*this, gpio_notify(gpio_s("in[0]"), true, GPIO_NO_VECTOR));
        EXPECT_CALL(*this, gpio_notify(gpio_s("in[1]"), true, GPIO_NO_VECTOR));
        EXPECT_CALL(*this,
                    gpio_notify(gpio_s("in[0]"), false, GPIO_NO_VECTOR));
        EXPECT_CALL(*this,
                    gpio_notify(gpio_s("in[1]"), false, GPIO_NO_VECTOR));
        out.pulse();
        wait(in[0].default_event());
        EXPECT_FALSE(in[0]);
        EXPECT_FALSE(in[1]);

        // test adapters
        EXPECT_CALL(*this, gpio_notify(gpio_s("a_in"), true, GPIO_NO_VECTOR));
        a_out.raise();
        wait(signal.default_event());
        EXPECT_TRUE(a_in);

        // test array adapters
        EXPECT_CALL(*this,
                    gpio_notify(gpio_s("arr_in[102]"), true, GPIO_NO_VECTOR));
        arr_out[14].raise();
        wait(signal2.default_event());
        EXPECT_TRUE(arr_in[102]);

        // test array offset bindings
        EXPECT_CALL(*this,
                    gpio_notify(gpio_s("ab_in0[0]"), true, GPIO_NO_VECTOR));
        ab_out0[0].raise();
        EXPECT_CALL(*this,
                    gpio_notify(gpio_s("ab_in0[1]"), true, GPIO_NO_VECTOR));
        ab_out0[1].raise();
        EXPECT_CALL(*this,
                    gpio_notify(gpio_s("ab_in0[2]"), true, GPIO_NO_VECTOR));
        ab_out0[2].raise();
        EXPECT_CALL(*this,
                    gpio_notify(gpio_s("ab_in0[3]"), true, GPIO_NO_VECTOR));
        ab_out0[3].raise();
        EXPECT_CALL(*this,
                    gpio_notify(gpio_s("ab_in0[4]"), true, GPIO_NO_VECTOR));
        ab_out1[0].raise();
        EXPECT_CALL(*this,
                    gpio_notify(gpio_s("ab_in0[5]"), true, GPIO_NO_VECTOR));
        ab_out1[1].raise();
        EXPECT_CALL(*this,
                    gpio_notify(gpio_s("ab_in1[0]"), true, GPIO_NO_VECTOR));
        ab_out1[2].raise();
        EXPECT_CALL(*this,
                    gpio_notify(gpio_s("ab_in1[1]"), true, GPIO_NO_VECTOR));
        ab_out1[3].raise();
    }
};

TEST(gpio, sockets) {
    gpio_test_harness test("gpio");
    sc_core::sc_start();
}
