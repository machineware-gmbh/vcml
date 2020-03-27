/******************************************************************************
 *                                                                            *
 * Copyright 2020 Lasse Urban, Lukas Juenger, Jan Henrik Weinstock            *
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

class mock_sdcard: public component, public sd_fw_transport_if
{
public:
    sd_target_socket SD_IN;

    mock_sdcard(const sc_module_name& nm):
        component(nm),
        SD_IN("SD_IN") {
        SD_IN.bind(*this);
    }

    MOCK_METHOD1(sd_transport, sd_status(sd_command&));
    MOCK_METHOD1(sd_data_read, sd_tx_status(u8&));
    MOCK_METHOD1(sd_data_write, sd_rx_status(u8));
};

class sdhci_harness: public test_base
{
public:
    generic::sdhci sdhci;
    generic::memory mem;
    mock_sdcard sdcard;

    sc_signal<bool> irq_sig;

    master_socket OUT;

    sdhci_harness(const sc_module_name& nm):
        test_base(nm),
        sdhci("SDHCI"),
        mem("MEM", 1024),
        sdcard("MOCK_SD"),
        OUT("OUT") {

        sdhci.RESET.stub();
        sdhci.CLOCK.stub(100 * MHz);

        mem.RESET.stub();
        mem.CLOCK.stub(100 * MHz);

        sdcard.RESET.stub();
        sdcard.CLOCK.stub(100 * MHz);

        // I/O Mapping
        OUT.bind(sdhci.IN);
        sdhci.SD_OUT.bind(sdcard.SD_IN);
        sdhci.OUT.bind(mem.IN);

        // IRQ Mapping
        sdhci.IRQ.bind(irq_sig);
    }

    virtual ~sdhci_harness() {
        // nothing to do
    }

    virtual void run_test() override {
        /**********************************************************************
         *                                                                    *
         *             test go_idle_state (without DMA)                       *
         *                                                                    *
         **********************************************************************/

        sdhci.dma_enabled = false; // tests without DMA

        vcml::sd_command cmd;
        cmd.spi = false;
        cmd.opcode = 0;
        cmd.argument = 0;
        cmd.crc = 0;
        cmd.resp_len = 6;
        cmd.response[0] = 0;
        cmd.response[1] = 1;
        cmd.response[2] = 2;
        cmd.response[3] = 3;
        cmd.response[4] = 4;
        cmd.response[5] = 0;

        EXPECT_CALL(sdcard, sd_transport(_))
            .WillOnce(DoAll(SetArgReferee<0>(cmd), Return(SD_OK)));


        ASSERT_OK(OUT.writew<u32>(0x08, 0x00000000))
            << "write zero to ARG register";
        ASSERT_OK(OUT.writew<u16>(0x0e, 0x0000))
            << "write zero to CMD register (go_idle_state)";

        u32 value_of_RESPONSE;
        ASSERT_OK(OUT.readw(0x10, value_of_RESPONSE))
            << "read the RESPONSE register";
        EXPECT_EQ(0x01020304, value_of_RESPONSE);

        EXPECT_TRUE(sdhci.IRQ.read())
            << "check whether an interrupt has been triggered";

        u32 value_of_NORMAL_INT_STAT;
        ASSERT_OK(OUT.readw(0x30, value_of_NORMAL_INT_STAT))
            << "check if it was the right interrupt (Command Complete)";
        EXPECT_EQ(0x0001, value_of_NORMAL_INT_STAT);

        OUT.writew(0x30, 0x0001); // clear the interrupt

        ASSERT_OK(OUT.readw(0x30, value_of_NORMAL_INT_STAT))
            << "check if the interrupt was cleared";
        EXPECT_EQ(0x0000, value_of_NORMAL_INT_STAT);

        u32 value_of_ERROR_INT_STAT;
        ASSERT_OK(OUT.readw(0x32, value_of_ERROR_INT_STAT))
            << "error interrupt has been triggered additionally";
        EXPECT_EQ(0x0000, value_of_ERROR_INT_STAT);

        /**********************************************************************
         *                                                                    *
         *             test read_multiple_block (without DMA)                 *
         *                                                                    *
         **********************************************************************/

        ASSERT_OK(OUT.writew(0x2f, 0x01)) << "reset the SDHCI";
        sdhci.dma_enabled = false; // tests without DMA

        cmd.spi = false;
        cmd.opcode = 18;
        cmd.argument = 0;
        cmd.crc = 0;
        cmd.resp_len = 6;
        cmd.response[0] = 0;
        cmd.response[1] = 1;
        cmd.response[2] = 2;
        cmd.response[3] = 3;
        cmd.response[4] = 4;
        cmd.response[5] = 0;

        EXPECT_CALL(sdcard, sd_transport(_))
            .WillOnce(DoAll(SetArgReferee<0>(cmd), Return(SD_OK_TX_RDY)));

        EXPECT_CALL(sdcard, sd_data_read(_))
            .WillOnce(DoAll(SetArgReferee<0>(0x01), Return(SDTX_OK)))
            .WillOnce(DoAll(SetArgReferee<0>(0X02), Return(SDTX_OK)))
            .WillOnce(DoAll(SetArgReferee<0>(0x03), Return(SDTX_OK)))
            .WillOnce(DoAll(SetArgReferee<0>(0x04), Return(SDTX_OK)))
            .WillOnce(DoAll(SetArgReferee<0>(0x05), Return(SDTX_OK)))
            .WillOnce(DoAll(SetArgReferee<0>(0x06), Return(SDTX_OK)))
            .WillOnce(DoAll(SetArgReferee<0>(0x07), Return(SDTX_OK)))
            .WillOnce(DoAll(SetArgReferee<0>(0x08), Return(SDTX_OK_BLK_DONE)))
            .WillOnce(DoAll(SetArgReferee<0>(0x09), Return(SDTX_OK)))
            .WillOnce(DoAll(SetArgReferee<0>(0x0a), Return(SDTX_OK)))
            .WillOnce(DoAll(SetArgReferee<0>(0X0b), Return(SDTX_OK)))
            .WillOnce(DoAll(SetArgReferee<0>(0x0c), Return(SDTX_OK)))
            .WillOnce(DoAll(SetArgReferee<0>(0x0d), Return(SDTX_OK)))
            .WillOnce(DoAll(SetArgReferee<0>(0x0e), Return(SDTX_OK)))
            .WillOnce(DoAll(SetArgReferee<0>(0x0f), Return(SDTX_OK)))
            .WillOnce(DoAll(SetArgReferee<0>(0x10), Return(SDTX_OK_BLK_DONE)));

        ASSERT_OK(OUT.writew<u16>(0x04, 0x0008))
            << "define block size to eight bytes";
        ASSERT_OK(OUT.writew<u16>(0x06, 0x0002))
            << "write two to BLOCK_COUNT_16BIT register";
        ASSERT_OK(OUT.writew<u32>(0x08, 0x00000000))
            << "write zero to ARG register";
        ASSERT_OK(OUT.writew<u16>(0x0e, 0x123a))
            << "write CMD18 (READ_MULTIPLE_BLOCK) to CMD register";

        ASSERT_OK(OUT.readw(0x10, value_of_RESPONSE))
            << "read the RESPONSE register";
        EXPECT_EQ(0x01020304, value_of_RESPONSE);

        EXPECT_TRUE(sdhci.IRQ.read())
            << "check whether an interrupt has been triggered";

        ASSERT_OK(OUT.readw(0x30, value_of_NORMAL_INT_STAT))
            << "check if it was the right interrupt (Command Complete = " \
               "0x0001, Buffer Read Ready = 0x0020)";
        EXPECT_EQ(0x0021, value_of_NORMAL_INT_STAT);
        ASSERT_OK(OUT.writew(0x30, 0x0021)) << "clear the interrupt";

        ASSERT_OK(OUT.readw(0x32, value_of_ERROR_INT_STAT))
            << "error interrupt has been triggered additionally";
        EXPECT_EQ(0x0000, value_of_ERROR_INT_STAT);

        // read 2 blocks with block size 8
        u32 value_of_BUFFER_DATA_PORT;
        ASSERT_OK(OUT.readw(0x20, value_of_BUFFER_DATA_PORT))
            << "read BUFFER_DATA_PORT register";
        EXPECT_EQ(0x04030201, value_of_BUFFER_DATA_PORT);
        ASSERT_OK(OUT.readw(0x20, value_of_BUFFER_DATA_PORT))
            << "read BUFFER_DATA_PORT register";
        EXPECT_EQ(0x08070605, value_of_BUFFER_DATA_PORT);
        ASSERT_OK(OUT.readw(0x20, value_of_BUFFER_DATA_PORT))
            << "read BUFFER_DATA_PORT register";
        EXPECT_EQ(0x0C0B0A09, value_of_BUFFER_DATA_PORT);
        ASSERT_OK(OUT.readw(0x20, value_of_BUFFER_DATA_PORT))
            << "read BUFFER_DATA_PORT register";
        EXPECT_EQ(0x100F0E0D, value_of_BUFFER_DATA_PORT);

        EXPECT_TRUE(sdhci.IRQ.read())
            << "check whether an interrupt has been triggered";

        ASSERT_OK(OUT.readw(0x30, value_of_NORMAL_INT_STAT))
            << "check if it was the right interrupt (Transfer Complete)";
        EXPECT_EQ(0x0002, value_of_NORMAL_INT_STAT);

        ASSERT_OK(OUT.writew(0x30, 0x0002)) << "clear the interrupt";

        ASSERT_OK(OUT.readw(0x30, value_of_NORMAL_INT_STAT))
            << "check whether all the interrupts have been cleared";
        EXPECT_EQ(0x0000, value_of_NORMAL_INT_STAT);

        ASSERT_OK(OUT.readw(0x32, value_of_ERROR_INT_STAT))
            << "error interrupt has been triggered additionally";
        EXPECT_EQ(0x0000, value_of_ERROR_INT_STAT);

        /**********************************************************************
         *                                                                    *
         *             test write_multiple_block (without DMA)                *
         *                                                                    *
         **********************************************************************/

        ASSERT_OK(OUT.writew(0x2f, 0x01)) << "reset the SDHCI";
        sdhci.dma_enabled = false; // tests without DMA

        cmd.spi = false;
        cmd.opcode = 25;
        cmd.argument = 0;
        cmd.crc = 0;
        cmd.resp_len = 6;
        cmd.response[0] = 0;
        cmd.response[1] = 1;
        cmd.response[2] = 2;
        cmd.response[3] = 3;
        cmd.response[4] = 4;
        cmd.response[5] = 0;

        EXPECT_CALL(sdcard, sd_transport(_))
            .WillOnce(DoAll(SetArgReferee<0>(cmd), Return(SD_OK_RX_RDY)));

        u8 test_sd_mem[16];
        memset(test_sd_mem, 0, sizeof(test_sd_mem));

        EXPECT_CALL(sdcard, sd_data_write(_))
            .WillOnce(DoAll(SaveArg<0>(&test_sd_mem[0]), Return(SDRX_OK)))
            .WillOnce(DoAll(SaveArg<0>(&test_sd_mem[1]), Return(SDRX_OK)))
            .WillOnce(DoAll(SaveArg<0>(&test_sd_mem[2]), Return(SDRX_OK)))
            .WillOnce(DoAll(SaveArg<0>(&test_sd_mem[3]), Return(SDRX_OK)))
            .WillOnce(DoAll(SaveArg<0>(&test_sd_mem[4]), Return(SDRX_OK)))
            .WillOnce(DoAll(SaveArg<0>(&test_sd_mem[5]), Return(SDRX_OK)))
            .WillOnce(DoAll(SaveArg<0>(&test_sd_mem[6]), Return(SDRX_OK)))
            .WillOnce(DoAll(SaveArg<0>(&test_sd_mem[7]), Return(SDRX_OK_BLK_DONE)))
            .WillOnce(DoAll(SaveArg<0>(&test_sd_mem[8]), Return(SDRX_OK)))
            .WillOnce(DoAll(SaveArg<0>(&test_sd_mem[9]), Return(SDRX_OK)))
            .WillOnce(DoAll(SaveArg<0>(&test_sd_mem[10]), Return(SDRX_OK)))
            .WillOnce(DoAll(SaveArg<0>(&test_sd_mem[11]), Return(SDRX_OK)))
            .WillOnce(DoAll(SaveArg<0>(&test_sd_mem[12]), Return(SDRX_OK)))
            .WillOnce(DoAll(SaveArg<0>(&test_sd_mem[13]), Return(SDRX_OK)))
            .WillOnce(DoAll(SaveArg<0>(&test_sd_mem[14]), Return(SDRX_OK)))
            .WillOnce(DoAll(SaveArg<0>(&test_sd_mem[15]), Return(SDRX_OK_BLK_DONE)));

        ASSERT_OK(OUT.writew<u16>(0x04, 0x0008))
            << "define block size to eight byte";
        ASSERT_OK(OUT.writew<u16>(0x06, 0x0002))
            << "write two to BLOCK_COUNT_16BIT register";
        ASSERT_OK(OUT.writew<u32>(0x08, 0x00000000))
            << "write zero to ARG register";
        ASSERT_OK(OUT.writew<u16>(0x0e, 0x193a))
            << "write CMD25 (WRITE_MULTIPLE_BLOCK) to CMD register";

        ASSERT_OK(OUT.readw(0x10, value_of_RESPONSE))
            << "read the RESPONSE register";
        EXPECT_EQ(0x01020304, value_of_RESPONSE);

        EXPECT_TRUE(sdhci.IRQ.read())
            << "check whether an interrupt has been triggered";

        ASSERT_OK(OUT.readw(0x30, value_of_NORMAL_INT_STAT))
            << "check if it was the right interrupt (Command Complete = " \
               "0x0001, Buffer Write Ready = 0x0010)";
        EXPECT_EQ(0x0011, value_of_NORMAL_INT_STAT);
        ASSERT_OK(OUT.writew<u16>(0x30, 0x0011)) << "clear the interrupt";

        ASSERT_OK(OUT.readw(0x32, value_of_ERROR_INT_STAT))
            << "error interrupt has been triggered additionally";
        EXPECT_EQ(0x0000, value_of_ERROR_INT_STAT);

        // write 2 blocks with block size 8
        ASSERT_OK(OUT.writew<u32>(0x20, 0x04030201))
            << "write BUFFER_DATA_PORT register";
        ASSERT_OK(OUT.writew<u32>(0x20, 0x08070605))
            << "write BUFFER_DATA_PORT register";
        ASSERT_OK(OUT.writew<u32>(0x20, 0x0c0b0a09))
            << "write BUFFER_DATA_PORT register";
        ASSERT_OK(OUT.writew<u32>(0x20, 0x100f0e0d))
            << "write BUFFER_DATA_PORT register";

        for (int i = 1; i < 17; i++) {
            EXPECT_EQ((u8)i, test_sd_mem[i-1])
                << "check whether the write process was successful";
        }

        EXPECT_TRUE(sdhci.IRQ.read())
            << "check whether an interrupt has been triggered";

        ASSERT_OK(OUT.readw(0x30, value_of_NORMAL_INT_STAT))
            << "check if it was the right interrupt (Transfer Complete)";
        EXPECT_EQ(0x0002, value_of_NORMAL_INT_STAT);

        ASSERT_OK(OUT.writew<u16>(0x30, 0x0002)) << "clear the interrupt";

        ASSERT_OK(OUT.readw(0x30, value_of_NORMAL_INT_STAT))
            << "check whether all the interrupts have been cleared";
        EXPECT_EQ(0x0000, value_of_NORMAL_INT_STAT);

        ASSERT_OK(OUT.readw(0x32, value_of_ERROR_INT_STAT))
           << "error interrupt has been triggered additionally";
        EXPECT_EQ(0x0000, value_of_ERROR_INT_STAT);

        /**********************************************************************
         *                                                                    *
         *             test read_multiple_block (with DMA)                    *
         *                                                                    *
         **********************************************************************/

        ASSERT_OK(OUT.writew<u8>(0x2F, 0x01)) << "reset the SDHCI";
        sdhci.dma_enabled = true; // tests with DMA

        cmd.spi = false;
        cmd.opcode = 18;
        cmd.argument = 0;
        cmd.crc = 0;
        cmd.resp_len = 6;
        cmd.response[0] = 0;
        cmd.response[1] = 1;
        cmd.response[2] = 2;
        cmd.response[3] = 3;
        cmd.response[4] = 4;
        cmd.response[5] = 0;

        EXPECT_CALL(sdcard, sd_transport(_))
            .WillOnce(DoAll(SetArgReferee<0>(cmd), Return(SD_OK_TX_RDY)));

        EXPECT_CALL(sdcard, sd_data_read(_))
            .WillOnce(DoAll(SetArgReferee<0>(0x01),Return(SDTX_OK)))
            .WillOnce(DoAll(SetArgReferee<0>(0X02),Return(SDTX_OK)))
            .WillOnce(DoAll(SetArgReferee<0>(0x03),Return(SDTX_OK)))
            .WillOnce(DoAll(SetArgReferee<0>(0x04),Return(SDTX_OK)))
            .WillOnce(DoAll(SetArgReferee<0>(0x05),Return(SDTX_OK)))
            .WillOnce(DoAll(SetArgReferee<0>(0x06),Return(SDTX_OK)))
            .WillOnce(DoAll(SetArgReferee<0>(0x07),Return(SDTX_OK)))
            .WillOnce(DoAll(SetArgReferee<0>(0x08),Return(SDTX_OK_BLK_DONE)))
            .WillOnce(DoAll(SetArgReferee<0>(0x09),Return(SDTX_OK)))
            .WillOnce(DoAll(SetArgReferee<0>(0x0A),Return(SDTX_OK)))
            .WillOnce(DoAll(SetArgReferee<0>(0X0B),Return(SDTX_OK)))
            .WillOnce(DoAll(SetArgReferee<0>(0x0C),Return(SDTX_OK)))
            .WillOnce(DoAll(SetArgReferee<0>(0x0D),Return(SDTX_OK)))
            .WillOnce(DoAll(SetArgReferee<0>(0x0E),Return(SDTX_OK)))
            .WillOnce(DoAll(SetArgReferee<0>(0x0F),Return(SDTX_OK)))
            .WillOnce(DoAll(SetArgReferee<0>(0x10),Return(SDTX_OK_BLK_DONE)));

        ASSERT_OK(OUT.writew<u32>(0x00, 0x00000010))
            << "set the SDMA address";
        ASSERT_OK(OUT.writew<u16>(0x04, 0x0008))
            << "define block size to eight byte";
        ASSERT_OK(OUT.writew<u16>(0x06, 0x0002))
            << "write two to BLOCK_COUNT_16BIT register";
        ASSERT_OK(OUT.writew<u32>(0x08, 0x00000000))
            << "write zero to ARG register";
        ASSERT_OK(OUT.writew<u16>(0x0e, 0x123a))
            << "write CMD18 (READ_MULTIPLE_BLOCK) to CMD register";

        ASSERT_OK(OUT.readw(0x10, value_of_RESPONSE))
            << "read the RESPONSE register";
        EXPECT_EQ(0x01020304, value_of_RESPONSE);

        wait(1, SC_US); // allow the DMI transfer to complete
        EXPECT_TRUE(sdhci.IRQ.read())
            << "check whether an interrupt has been triggered";

        ASSERT_OK(OUT.readw(0x30, value_of_NORMAL_INT_STAT))
            << "check if it was the right interrupt (cmd complete = 0x0001)";
        EXPECT_EQ(0x0003, value_of_NORMAL_INT_STAT);
        ASSERT_OK(OUT.writew<u16>(0x30, 0x0003)) << "clear the interrupt";

        ASSERT_OK(OUT.readw(0x32, value_of_ERROR_INT_STAT))
            << "error interrupt has been triggered additionally";
        EXPECT_EQ(0x0000, value_of_ERROR_INT_STAT);

        u64 mem0, mem1;
        mem.read(range(0x00000010, 0x00000017), &mem0, SBI_NONE);
        mem.read(range(0x00000018, 0x0000001f), &mem1, SBI_NONE);

        ASSERT_EQ(mem0, 0x0807060504030201)
            << "check host memory (DMA) was successful";
        ASSERT_EQ(mem1, 0x100f0e0d0c0b0a09)
            << "check host memory (DMA) was successful";

        /**********************************************************************
         *                                                                    *
         *             test write_multiple_block (with DMA)                   *
         *                                                                    *
         **********************************************************************/

        ASSERT_OK(OUT.writew(0x2F, 0x01)) << "reset the SDHCI controller";
        sdhci.dma_enabled = true; // tests with DMA

        cmd.spi = false;
        cmd.opcode = 25;
        cmd.argument = 0;
        cmd.crc = 0;
        cmd.resp_len = 6;
        cmd.response[0] = 0;
        cmd.response[1] = 1;
        cmd.response[2] = 2;
        cmd.response[3] = 3;
        cmd.response[4] = 4;
        cmd.response[5] = 0;

        EXPECT_CALL(sdcard, sd_transport(_))
            .WillOnce(DoAll(SetArgReferee<0>(cmd), Return(SD_OK_RX_RDY)));

        memset(test_sd_mem, 0, sizeof(test_sd_mem)); // reset buffer

        EXPECT_CALL(sdcard, sd_data_write(_))
            .WillOnce(DoAll(SaveArg<0>(&test_sd_mem[0]), Return(SDRX_OK)))
            .WillOnce(DoAll(SaveArg<0>(&test_sd_mem[1]), Return(SDRX_OK)))
            .WillOnce(DoAll(SaveArg<0>(&test_sd_mem[2]), Return(SDRX_OK)))
            .WillOnce(DoAll(SaveArg<0>(&test_sd_mem[3]), Return(SDRX_OK)))
            .WillOnce(DoAll(SaveArg<0>(&test_sd_mem[4]), Return(SDRX_OK)))
            .WillOnce(DoAll(SaveArg<0>(&test_sd_mem[5]), Return(SDRX_OK)))
            .WillOnce(DoAll(SaveArg<0>(&test_sd_mem[6]), Return(SDRX_OK)))
            .WillOnce(DoAll(SaveArg<0>(&test_sd_mem[7]), Return(SDRX_OK_BLK_DONE)))
            .WillOnce(DoAll(SaveArg<0>(&test_sd_mem[8]), Return(SDRX_OK)))
            .WillOnce(DoAll(SaveArg<0>(&test_sd_mem[9]), Return(SDRX_OK)))
            .WillOnce(DoAll(SaveArg<0>(&test_sd_mem[10]), Return(SDRX_OK)))
            .WillOnce(DoAll(SaveArg<0>(&test_sd_mem[11]), Return(SDRX_OK)))
            .WillOnce(DoAll(SaveArg<0>(&test_sd_mem[12]), Return(SDRX_OK)))
            .WillOnce(DoAll(SaveArg<0>(&test_sd_mem[13]), Return(SDRX_OK)))
            .WillOnce(DoAll(SaveArg<0>(&test_sd_mem[14]), Return(SDRX_OK)))
            .WillOnce(DoAll(SaveArg<0>(&test_sd_mem[15]), Return(SDRX_OK_BLK_DONE)));


        mem0 = 0x0807060504030201;
        mem1 = 0x100f0e0d0c0b0a09;
        ASSERT_OK(mem.write(range(0x40, 0x47), &mem0, SBI_NONE));
        ASSERT_OK(mem.write(range(0x48, 0x4f), &mem1, SBI_NONE));

        ASSERT_OK(OUT.writew<u32>(0x00, 0x00000040))
            << "set the SDMA address";
        ASSERT_OK(OUT.writew<u16>(0x04, 0x0008))
            << "define block size to eight byte";
        ASSERT_OK(OUT.writew<u16>(0x06, 0x0002))
            << "write two to BLOCK_COUNT_16BIT register";
        ASSERT_OK(OUT.writew<u32>(0x08, 0x00000000))
            << "write zero to ARG register";
        ASSERT_OK(OUT.writew<u16>(0x0e, 0x193a))
            << "write CMD25 (WRITE_MULTIPLE_BLOCK) to CMD register";

        ASSERT_OK(OUT.readw(0x10, value_of_RESPONSE))
            << "read the RESPONSE register";
        EXPECT_EQ(0x01020304, value_of_RESPONSE);

        wait(1, SC_US); // allow the DMI transfer to complete
        EXPECT_TRUE(sdhci.IRQ.read())
            << "check whether an interrupt has been triggered";

        ASSERT_OK(OUT.readw(0x30, value_of_NORMAL_INT_STAT))
            << "check if it was the right interrupt (cmd complete = 0x0001)";
        EXPECT_EQ(0x0003, value_of_NORMAL_INT_STAT);
        ASSERT_OK(OUT.writew<u16>(0x30, 0x0003)) << "clear the interrupt";

        ASSERT_OK(OUT.readw(0x32, value_of_ERROR_INT_STAT))
            << "error interrupt has been triggered additionally";
        EXPECT_EQ(0x0000, value_of_ERROR_INT_STAT);

        for (int i = 1; i < 17; i++) {
            EXPECT_EQ((u8)i, test_sd_mem[i-1])
                << "check host DMA transfer was successful";
        }

        ASSERT_OK(OUT.readw(0x30, value_of_NORMAL_INT_STAT))
            << "check whether all the interrupts have been cleared";
        EXPECT_EQ(0x0000, value_of_NORMAL_INT_STAT);

        ASSERT_OK(OUT.readw(0x32, value_of_ERROR_INT_STAT))
            << "error interrupt has been triggered additionally";
        EXPECT_EQ(0x0000, value_of_ERROR_INT_STAT);
    }

};

TEST(sdhci, sdhci) {
    sdhci_harness test("TEST");
    sc_core::sc_start();
}
