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

enum test_config : size_t {
    base_spi = 46,
    num_spi = 127,
};

class gicv2m_stim : public test_base
{
public:
    MOCK_METHOD(void, gpio_notify,
                (const gpio_target_socket& socket, bool state,
                 gpio_vector vector),
                (override));
    tlm_initiator_socket out;

    gpio_target_array in;

    gicv2m_stim(const sc_module_name& nm):
        test_base(nm), out("out"), in("in") {}

    virtual void run_test() override {
        enum addresses : size_t {
            TYPER_ADDR = 0x008,
            SETSPI_ADDR = 0x040,
            IIDR_ADDR = 0xfcc,
        };

        u32 val = ~0;
        EXPECT_OK(out.readw(TYPER_ADDR, val)) << "failed to read TYPER reg";
        EXPECT_EQ(val, base_spi << 16 | num_spi);

        Sequence s;
        for (size_t i = base_spi; i < base_spi + num_spi; i++) {
            EXPECT_CALL(*this, gpio_notify(Ref(in[i]), true, GPIO_NO_VECTOR))
                .InSequence(s);
            EXPECT_CALL(*this, gpio_notify(Ref(in[i]), false, GPIO_NO_VECTOR))
                .InSequence(s);
            EXPECT_OK(out.writew(SETSPI_ADDR, i))
                << "failed to write SETSPI reg";
        }

        EXPECT_CALL(*this, gpio_notify(_, _, _)).Times(0).InSequence(s);
        EXPECT_OK(out.writew(SETSPI_ADDR, base_spi + num_spi))
            << "failed to write SETSPI reg";

        val = ~0;
        EXPECT_OK(out.readw(IIDR_ADDR, val)) << "failed to read IIDR reg";
        EXPECT_EQ(val, 'M' << 20);
    }
};

TEST(gicv2m, gicv2m) {
    vcml::broker broker("test");
    broker.define("gicv2m.base_spi", base_spi);
    broker.define("gicv2m.num_spi", num_spi);

    gicv2m_stim stim("gicv2m_stim");
    arm::gicv2m gicv2m("gicv2m");

    stim.clk.bind(gicv2m.clk);
    stim.rst.bind(gicv2m.rst);

    stim.out.bind(gicv2m.in);

    for (size_t i = base_spi; i < base_spi + num_spi; i++)
        gicv2m.out[i].bind(stim.in[i]);

    sc_core::sc_start();
}
