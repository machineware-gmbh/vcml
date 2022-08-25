/******************************************************************************
 *                                                                            *
 * Copyright 2018 Jan Henrik Weinstock                                        *
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

class test_harness : public test_base
{
public:
    spi::flash flash;
    spi_initiator_socket spi_out;

    test_harness(const sc_module_name& nm):
        test_base(nm), flash("flash"), spi_out("spi_out") {
        spi_out.bind(flash.spi_in);
        rst.bind(flash.rst);
        clk.bind(flash.clk);
    }

    void spi_send(u8 data) {
        spi_payload tx(data);
        spi_out.transport(tx);
        EXPECT_EQ(tx.mosi, data);
    }

    u8 spi_recv() {
        spi_payload tx(0xff);
        spi_out.transport(tx);
        return tx.miso;
    }

    virtual void run_test() override {
        flash.reset();

        spi_send(0x9f); // READ_IDENT
        u32 jedec_id = spi_recv() << 16 | spi_recv() << 8 | spi_recv();
        u32 jedec_ex = spi_recv() << 8 | spi_recv();
        EXPECT_EQ(jedec_id, 0x202014);
        EXPECT_EQ(jedec_ex, 0);

        spi_send(0x04); // WRITE_DISABLE
        spi_send(0x05); // READ_STATUS
        u8 status = spi_recv();
        EXPECT_EQ(status, bit(7));

        spi_send(0x06); // WRITE_ENABLE
        spi_send(0x05); // READ_STATUS
        status = spi_recv();
        EXPECT_EQ(status, 0);
    }
};

TEST(generic_memory, access) {
    test_harness test("spi");
    sc_core::sc_start();
}
