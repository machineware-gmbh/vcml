/******************************************************************************
 *                                                                            *
 * Copyright (C) 2026 MachineWare GmbH                                        *
 * All Rights Reserved                                                        *
 *                                                                            *
 * This work is licensed under the terms described in the LICENSE file found  *
 * in the root directory of this source tree.                                 *
 *                                                                            *
 ******************************************************************************/

#include "testing.h"

class gate_test : public test_base
{
public:
    enum : size_t { GATES = 6 };

    gpio::gate gate_not;
    gpio::gate gate_and;
    gpio::gate gate_or;
    gpio::gate gate_nand;
    gpio::gate gate_nor;
    gpio::gate gate_xor;

    gpio_initiator_array<GATES> in0;
    gpio_initiator_array<GATES> in1;
    gpio_target_array<GATES> out;

    gpio::gate* gates[GATES];

    gate_test(const sc_module_name& nm):
        test_base(nm),
        gate_not("gate_not", gpio::gate::LOGIC_NOT),
        gate_and("gate_and", gpio::gate::LOGIC_AND),
        gate_or("gate_or", gpio::gate::LOGIC_OR),
        gate_nand("gate_nand", gpio::gate::LOGIC_NAND),
        gate_nor("gate_nor", gpio::gate::LOGIC_NOR),
        gate_xor("gate_xor", gpio::gate::LOGIC_XOR),
        in0("in0"),
        in1("in1"),
        out("out"),
        gates{
            &gate_not, &gate_and, &gate_or, &gate_nand, &gate_nor, &gate_xor
        } {
        for (size_t i = 0; i < GATES; i++) {
            in0[i].bind(gates[i]->in[0]);
            if (gates[i] != &gate_not)
                in1[i].bind(gates[i]->in[1]);
            gates[i]->out.bind(out[i]);
        }

        in1[0].stub();

        add_test("strings", &gate_test::test_strings);
        add_test("logic", &gate_test::test_logic);
    }

    void test_strings() {
        EXPECT_STREQ(gate_not.kind(), "vcml::gate_not");
        EXPECT_STREQ(gate_and.kind(), "vcml::gate_and");
        EXPECT_STREQ(gate_or.kind(), "vcml::gate_or");
        EXPECT_STREQ(gate_nand.kind(), "vcml::gate_nand");
        EXPECT_STREQ(gate_nor.kind(), "vcml::gate_nor");
        EXPECT_STREQ(gate_xor.kind(), "vcml::gate_xor");
        for (auto* gate : gates)
            EXPECT_STREQ(gate->version(), VCML_VERSION_STRING);
    }

    bool eval(size_t gate, bool a, bool b) {
        in0[gate] = !a;
        in1[gate] = !b;
        in1[gate] = b;
        in0[gate] = a;
        return out[gate];
    }

    void test_logic() {
        for (bool a : { false, true }) {
            EXPECT_EQ(eval(0, a, false), !a) << "not " << a;
            for (bool b : { false, true }) {
                EXPECT_EQ(eval(1, a, b), a && b) << "and " << a << b;
                EXPECT_EQ(eval(2, a, b), a || b) << "or " << a << b;
                EXPECT_EQ(eval(3, a, b), !(a && b)) << "nand " << a << b;
                EXPECT_EQ(eval(4, a, b), !(a || b)) << "nor " << a << b;
                EXPECT_EQ(eval(5, a, b), a != b) << "xor " << a << b;
            }
        }

        for (size_t i = 0; i < GATES; i++)
            eval(i, false, false);
    }
};

TEST(gpio, gate) {
    gate_test test("test");
    sc_core::sc_start();
}
