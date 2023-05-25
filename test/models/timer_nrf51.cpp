/******************************************************************************
 *                                                                            *
 * Copyright 2023 MachineWare GmbH                                            *
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

class nrf51_test : public test_base
{
public:
    timers::nrf51 nrf51;
    tlm_initiator_socket out;
    gpio_target_socket irq;

    enum : u64 {
        NRF51_START = 0x0,
        NRF51_STOP = 0x4,
        NRF51_COUNT = 0x8,
        NRF51_CLEAR = 0xc,
        NRF51_SHUTDOWN = 0x10,
        NRF51_CAPTURE0 = 0x40,
        NRF51_COMPARE0 = 0x140,
        NRF51_SHORTS = 0x200,
        NRF51_INTENSET = 0x304,
        NRF51_INTENCLR = 0x308,
        NRF51_MODE = 0x504,
        NRF51_BITMODE = 0x508,
        NRF51_PRESCALER = 0x510,
        NRF51_CC = 0x540,
    };

    nrf51_test(const sc_module_name& nm):
        test_base(nm), nrf51("nrf51"), out("out"), irq("irq") {
        out.bind(nrf51.in);
        nrf51.irq.bind(irq);
        rst.bind(nrf51.rst);
        clk.bind(nrf51.clk);
        EXPECT_STREQ(nrf51.kind(), "vcml::timers::nrf51");
    }

    virtual void run_test() override {
        // u32 data;

        wait(SC_ZERO_TIME);
        EXPECT_FALSE(irq) << "irq did not reset";
    }
};

TEST(timer, nrf51) {
    nrf51_test test("test");
    sc_core::sc_start();
}
