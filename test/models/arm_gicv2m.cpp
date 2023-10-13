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
    BASE_SPI = 46,
    NUM_SPI = 127,
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
        EXPECT_EQ(val, BASE_SPI << 16 | NUM_SPI);

        Sequence s;
        for (size_t i = BASE_SPI; i < BASE_SPI + NUM_SPI; i++) {
            EXPECT_CALL(*this, gpio_notify(Ref(in[i]), true, GPIO_NO_VECTOR))
                .InSequence(s);
            EXPECT_CALL(*this, gpio_notify(Ref(in[i]), false, GPIO_NO_VECTOR))
                .InSequence(s);
            EXPECT_OK(out.writew(SETSPI_ADDR, i))
                << "failed to write SETSPI reg";
        }

        EXPECT_CALL(*this, gpio_notify(_, _, _)).Times(0).InSequence(s);
        EXPECT_OK(out.writew(SETSPI_ADDR, BASE_SPI + NUM_SPI))
            << "failed to write SETSPI reg";

        val = ~0;
        EXPECT_OK(out.readw(IIDR_ADDR, val)) << "failed to read IIDR reg";
        EXPECT_EQ(val, 'M' << 20);
    }
};

TEST(gicv2m, gicv2m) {
    vcml::broker broker("test");
    broker.define("gicv2m.base_spi", BASE_SPI);
    broker.define("gicv2m.num_spi", NUM_SPI);

    gicv2m_stim stim("gicv2m_stim");
    arm::gicv2m gicv2m("gicv2m");

    stim.clk.bind(gicv2m.clk);
    stim.rst.bind(gicv2m.rst);

    stim.out.bind(gicv2m.in);

    for (size_t i = BASE_SPI; i < BASE_SPI + NUM_SPI; i++)
        gicv2m.out[i].bind(stim.in[i]);

    EXPECT_STREQ(gicv2m.kind(), "vcml::arm::gicv2m");

    sc_core::sc_start();
}
