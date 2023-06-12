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

class mock_sdcard : public component, public sd_host
{
public:
    sd_target_socket sd_in;

    mock_sdcard(const sc_module_name& nm): component(nm), sd_in("sd_in") {}

    MOCK_METHOD(sd_status, test_transport, (sd_command&));
    MOCK_METHOD(sd_status_tx, test_data_read, (u8&));
    MOCK_METHOD(sd_status_rx, test_data_write, (u8));

    virtual void sd_transport(const sd_target_socket& s, sd_command& tx) {
        tx.status = test_transport(tx);
    }

    virtual void sd_transport(const sd_target_socket& s, sd_data& tx) {
        if (tx.mode == SD_READ)
            tx.status.read = test_data_read(tx.data);
        if (tx.mode == SD_WRITE)
            tx.status.write = test_data_write(tx.data);
    }
};

class sdhci_harness : public test_base
{
public:
    sd::sdhci sdhci;
    generic::memory mem;
    mock_sdcard sdcard;

    tlm_initiator_socket out;

    sdhci_harness(const sc_module_name& nm):
        test_base(nm),
        sdhci("sdhci"),
        mem("mem", 1024),
        sdcard("mock_sd"),
        out("out") {
        rst.bind(sdhci.rst);
        clk.bind(sdhci.clk);

        rst.bind(mem.rst);
        clk.bind(mem.clk);

        rst.bind(sdcard.rst);
        clk.bind(sdcard.clk);

        // I/O Mapping
        out.bind(sdhci.in);
        sdhci.sd_out.bind(sdcard.sd_in);
        sdhci.out.bind(mem.in);

        // IRQ Mapping
        sdhci.irq.stub();
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
        cmd.status = SD_INCOMPLETE;

        EXPECT_CALL(sdcard, test_transport(_))
            .WillOnce(DoAll(SetArgReferee<0>(cmd), Return(SD_OK)));

        ASSERT_OK(out.writew<u32>(0x08, 0x00000000))
            << "write zero to ARG register";
        ASSERT_OK(out.writew<u16>(0x0e, 0x0000))
            << "write zero to CMD register (go_idle_state)";

        u32 value_of_response;
        ASSERT_OK(out.readw(0x10, value_of_response))
            << "read the RESPONSE register";
        EXPECT_EQ(0x01020304, value_of_response);

        EXPECT_TRUE(sdhci.irq.read())
            << "check whether an interrupt has been triggered";

        u32 value_of_normal_int_stat;
        ASSERT_OK(out.readw(0x30, value_of_normal_int_stat))
            << "check if it was the right interrupt (Command Complete)";
        EXPECT_EQ(0x0001, value_of_normal_int_stat);

        out.writew(0x30, 0x0001); // clear the interrupt

        ASSERT_OK(out.readw(0x30, value_of_normal_int_stat))
            << "check if the interrupt was cleared";
        EXPECT_EQ(0x0000, value_of_normal_int_stat);

        u32 value_of_error_int_stat;
        ASSERT_OK(out.readw(0x32, value_of_error_int_stat))
            << "error interrupt has been triggered additionally";
        EXPECT_EQ(0x0000, value_of_error_int_stat);

        /**********************************************************************
         *                                                                    *
         *             test read_multiple_block (without DMA)                 *
         *                                                                    *
         **********************************************************************/

        ASSERT_OK(out.writew(0x2f, 0x01)) << "reset the SDHCI";
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
        cmd.status = SD_INCOMPLETE;

        EXPECT_CALL(sdcard, test_transport(_))
            .WillOnce(DoAll(SetArgReferee<0>(cmd), Return(SD_OK_TX_RDY)));

        EXPECT_CALL(sdcard, test_data_read(_))
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

        ASSERT_OK(out.writew<u16>(0x04, 0x0008))
            << "define block size to eight bytes";
        ASSERT_OK(out.writew<u16>(0x06, 0x0002))
            << "write two to BLOCK_COUNT_16BIT register";
        ASSERT_OK(out.writew<u32>(0x08, 0x00000000))
            << "write zero to ARG register";
        ASSERT_OK(out.writew<u16>(0x0e, 0x123a))
            << "write CMD18 (READ_MULTIPLE_BLOCK) to CMD register";

        ASSERT_OK(out.readw(0x10, value_of_response))
            << "read the RESPONSE register";
        EXPECT_EQ(0x01020304, value_of_response);

        EXPECT_TRUE(sdhci.irq.read())
            << "check whether an interrupt has been triggered";

        ASSERT_OK(out.readw(0x30, value_of_normal_int_stat))
            << "check if it was the right interrupt (Command Complete = "
               "0x0001, Buffer Read Ready = 0x0020)";
        EXPECT_EQ(0x0021, value_of_normal_int_stat);
        ASSERT_OK(out.writew(0x30, 0x0021)) << "clear the interrupt";

        ASSERT_OK(out.readw(0x32, value_of_error_int_stat))
            << "error interrupt has been triggered additionally";
        EXPECT_EQ(0x0000, value_of_error_int_stat);

        // read 2 blocks with block size 8
        u32 value_of_buffer_data_port;
        ASSERT_OK(out.readw(0x20, value_of_buffer_data_port))
            << "read BUFFER_DATA_PORT register";
        EXPECT_EQ(0x04030201, value_of_buffer_data_port);
        ASSERT_OK(out.readw(0x20, value_of_buffer_data_port))
            << "read BUFFER_DATA_PORT register";
        EXPECT_EQ(0x08070605, value_of_buffer_data_port);
        ASSERT_OK(out.readw(0x20, value_of_buffer_data_port))
            << "read BUFFER_DATA_PORT register";
        EXPECT_EQ(0x0C0B0A09, value_of_buffer_data_port);
        ASSERT_OK(out.readw(0x20, value_of_buffer_data_port))
            << "read BUFFER_DATA_PORT register";
        EXPECT_EQ(0x100f0e0d, value_of_buffer_data_port);

        EXPECT_TRUE(sdhci.irq.read())
            << "check whether an interrupt has been triggered";

        ASSERT_OK(out.readw(0x30, value_of_normal_int_stat))
            << "check if it was the right interrupt (Transfer Complete)";
        EXPECT_EQ(0x0002, value_of_normal_int_stat);

        ASSERT_OK(out.writew(0x30, 0x0002)) << "clear the interrupt";

        ASSERT_OK(out.readw(0x30, value_of_normal_int_stat))
            << "check whether all the interrupts have been cleared";
        EXPECT_EQ(0x0000, value_of_normal_int_stat);

        ASSERT_OK(out.readw(0x32, value_of_error_int_stat))
            << "error interrupt has been triggered additionally";
        EXPECT_EQ(0x0000, value_of_error_int_stat);

        /**********************************************************************
         *                                                                    *
         *             test write_multiple_block (without DMA)                *
         *                                                                    *
         **********************************************************************/

        ASSERT_OK(out.writew(0x2f, 0x01)) << "reset the SDHCI";
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
        cmd.status = SD_INCOMPLETE;

        EXPECT_CALL(sdcard, test_transport(_))
            .WillOnce(DoAll(SetArgReferee<0>(cmd), Return(SD_OK_RX_RDY)));

        u8 test_sd_mem[16];
        memset(test_sd_mem, 0, sizeof(test_sd_mem));

        EXPECT_CALL(sdcard, test_data_write(_))
            .WillOnce(DoAll(SaveArg<0>(&test_sd_mem[0]), Return(SDRX_OK)))
            .WillOnce(DoAll(SaveArg<0>(&test_sd_mem[1]), Return(SDRX_OK)))
            .WillOnce(DoAll(SaveArg<0>(&test_sd_mem[2]), Return(SDRX_OK)))
            .WillOnce(DoAll(SaveArg<0>(&test_sd_mem[3]), Return(SDRX_OK)))
            .WillOnce(DoAll(SaveArg<0>(&test_sd_mem[4]), Return(SDRX_OK)))
            .WillOnce(DoAll(SaveArg<0>(&test_sd_mem[5]), Return(SDRX_OK)))
            .WillOnce(DoAll(SaveArg<0>(&test_sd_mem[6]), Return(SDRX_OK)))
            .WillOnce(
                DoAll(SaveArg<0>(&test_sd_mem[7]), Return(SDRX_OK_BLK_DONE)))
            .WillOnce(DoAll(SaveArg<0>(&test_sd_mem[8]), Return(SDRX_OK)))
            .WillOnce(DoAll(SaveArg<0>(&test_sd_mem[9]), Return(SDRX_OK)))
            .WillOnce(DoAll(SaveArg<0>(&test_sd_mem[10]), Return(SDRX_OK)))
            .WillOnce(DoAll(SaveArg<0>(&test_sd_mem[11]), Return(SDRX_OK)))
            .WillOnce(DoAll(SaveArg<0>(&test_sd_mem[12]), Return(SDRX_OK)))
            .WillOnce(DoAll(SaveArg<0>(&test_sd_mem[13]), Return(SDRX_OK)))
            .WillOnce(DoAll(SaveArg<0>(&test_sd_mem[14]), Return(SDRX_OK)))
            .WillOnce(
                DoAll(SaveArg<0>(&test_sd_mem[15]), Return(SDRX_OK_BLK_DONE)));

        ASSERT_OK(out.writew<u16>(0x04, 0x0008))
            << "define block size to eight byte";
        ASSERT_OK(out.writew<u16>(0x06, 0x0002))
            << "write two to BLOCK_COUNT_16BIT register";
        ASSERT_OK(out.writew<u32>(0x08, 0x00000000))
            << "write zero to ARG register";
        ASSERT_OK(out.writew<u16>(0x0e, 0x193a))
            << "write CMD25 (WRITE_MULTIPLE_BLOCK) to CMD register";

        ASSERT_OK(out.readw(0x10, value_of_response))
            << "read the RESPONSE register";
        EXPECT_EQ(0x01020304, value_of_response);

        EXPECT_TRUE(sdhci.irq.read())
            << "check whether an interrupt has been triggered";

        ASSERT_OK(out.readw(0x30, value_of_normal_int_stat))
            << "check if it was the right interrupt (Command Complete = "
               "0x0001, Buffer Write Ready = 0x0010)";
        EXPECT_EQ(0x0011, value_of_normal_int_stat);
        ASSERT_OK(out.writew<u16>(0x30, 0x0011)) << "clear the interrupt";

        ASSERT_OK(out.readw(0x32, value_of_error_int_stat))
            << "error interrupt has been triggered additionally";
        EXPECT_EQ(0x0000, value_of_error_int_stat);

        // write 2 blocks with block size 8
        ASSERT_OK(out.writew<u32>(0x20, 0x04030201))
            << "write BUFFER_DATA_PORT register";
        ASSERT_OK(out.writew<u32>(0x20, 0x08070605))
            << "write BUFFER_DATA_PORT register";
        ASSERT_OK(out.writew<u32>(0x20, 0x0c0b0a09))
            << "write BUFFER_DATA_PORT register";
        ASSERT_OK(out.writew<u32>(0x20, 0x100f0e0d))
            << "write BUFFER_DATA_PORT register";

        for (int i = 1; i < 17; i++) {
            EXPECT_EQ((u8)i, test_sd_mem[i - 1])
                << "check whether the write process was successful";
        }

        EXPECT_TRUE(sdhci.irq.read())
            << "check whether an interrupt has been triggered";

        ASSERT_OK(out.readw(0x30, value_of_normal_int_stat))
            << "check if it was the right interrupt (Transfer Complete)";
        EXPECT_EQ(0x0002, value_of_normal_int_stat);

        ASSERT_OK(out.writew<u16>(0x30, 0x0002)) << "clear the interrupt";

        ASSERT_OK(out.readw(0x30, value_of_normal_int_stat))
            << "check whether all the interrupts have been cleared";
        EXPECT_EQ(0x0000, value_of_normal_int_stat);

        ASSERT_OK(out.readw(0x32, value_of_error_int_stat))
            << "error interrupt has been triggered additionally";
        EXPECT_EQ(0x0000, value_of_error_int_stat);

        /**********************************************************************
         *                                                                    *
         *             test read_multiple_block (with DMA)                    *
         *                                                                    *
         **********************************************************************/

        ASSERT_OK(out.writew<u8>(0x2F, 0x01)) << "reset the SDHCI";
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
        cmd.status = SD_INCOMPLETE;

        EXPECT_CALL(sdcard, test_transport(_))
            .WillOnce(DoAll(SetArgReferee<0>(cmd), Return(SD_OK_TX_RDY)));

        EXPECT_CALL(sdcard, test_data_read(_))
            .WillOnce(DoAll(SetArgReferee<0>(0x01), Return(SDTX_OK)))
            .WillOnce(DoAll(SetArgReferee<0>(0X02), Return(SDTX_OK)))
            .WillOnce(DoAll(SetArgReferee<0>(0x03), Return(SDTX_OK)))
            .WillOnce(DoAll(SetArgReferee<0>(0x04), Return(SDTX_OK)))
            .WillOnce(DoAll(SetArgReferee<0>(0x05), Return(SDTX_OK)))
            .WillOnce(DoAll(SetArgReferee<0>(0x06), Return(SDTX_OK)))
            .WillOnce(DoAll(SetArgReferee<0>(0x07), Return(SDTX_OK)))
            .WillOnce(DoAll(SetArgReferee<0>(0x08), Return(SDTX_OK_BLK_DONE)))
            .WillOnce(DoAll(SetArgReferee<0>(0x09), Return(SDTX_OK)))
            .WillOnce(DoAll(SetArgReferee<0>(0x0A), Return(SDTX_OK)))
            .WillOnce(DoAll(SetArgReferee<0>(0X0B), Return(SDTX_OK)))
            .WillOnce(DoAll(SetArgReferee<0>(0x0C), Return(SDTX_OK)))
            .WillOnce(DoAll(SetArgReferee<0>(0x0D), Return(SDTX_OK)))
            .WillOnce(DoAll(SetArgReferee<0>(0x0E), Return(SDTX_OK)))
            .WillOnce(DoAll(SetArgReferee<0>(0x0F), Return(SDTX_OK)))
            .WillOnce(DoAll(SetArgReferee<0>(0x10), Return(SDTX_OK_BLK_DONE)));

        ASSERT_OK(out.writew<u32>(0x00, 0x00000010)) << "set the SDMA address";
        ASSERT_OK(out.writew<u16>(0x04, 0x0008))
            << "define block size to eight byte";
        ASSERT_OK(out.writew<u16>(0x06, 0x0002))
            << "write two to BLOCK_COUNT_16BIT register";
        ASSERT_OK(out.writew<u32>(0x08, 0x00000000))
            << "write zero to ARG register";
        ASSERT_OK(out.writew<u16>(0x0e, 0x123a))
            << "write CMD18 (READ_MULTIPLE_BLOCK) to CMD register";

        ASSERT_OK(out.readw(0x10, value_of_response))
            << "read the RESPONSE register";
        EXPECT_EQ(0x01020304, value_of_response);

        wait(1, SC_US); // allow the DMI transfer to complete
        EXPECT_TRUE(sdhci.irq.read())
            << "check whether an interrupt has been triggered";

        ASSERT_OK(out.readw(0x30, value_of_normal_int_stat))
            << "check if it was the right interrupt (cmd complete = 0x0001)";
        EXPECT_EQ(0x0003, value_of_normal_int_stat);
        ASSERT_OK(out.writew<u16>(0x30, 0x0003)) << "clear the interrupt";

        ASSERT_OK(out.readw(0x32, value_of_error_int_stat))
            << "error interrupt has been triggered additionally";
        EXPECT_EQ(0x0000, value_of_error_int_stat);

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

        ASSERT_OK(out.writew(0x2F, 0x01)) << "reset the SDHCI controller";
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
        cmd.status = SD_INCOMPLETE;

        EXPECT_CALL(sdcard, test_transport(_))
            .WillOnce(DoAll(SetArgReferee<0>(cmd), Return(SD_OK_RX_RDY)));

        memset(test_sd_mem, 0, sizeof(test_sd_mem)); // reset buffer

        EXPECT_CALL(sdcard, test_data_write(_))
            .WillOnce(DoAll(SaveArg<0>(&test_sd_mem[0]), Return(SDRX_OK)))
            .WillOnce(DoAll(SaveArg<0>(&test_sd_mem[1]), Return(SDRX_OK)))
            .WillOnce(DoAll(SaveArg<0>(&test_sd_mem[2]), Return(SDRX_OK)))
            .WillOnce(DoAll(SaveArg<0>(&test_sd_mem[3]), Return(SDRX_OK)))
            .WillOnce(DoAll(SaveArg<0>(&test_sd_mem[4]), Return(SDRX_OK)))
            .WillOnce(DoAll(SaveArg<0>(&test_sd_mem[5]), Return(SDRX_OK)))
            .WillOnce(DoAll(SaveArg<0>(&test_sd_mem[6]), Return(SDRX_OK)))
            .WillOnce(
                DoAll(SaveArg<0>(&test_sd_mem[7]), Return(SDRX_OK_BLK_DONE)))
            .WillOnce(DoAll(SaveArg<0>(&test_sd_mem[8]), Return(SDRX_OK)))
            .WillOnce(DoAll(SaveArg<0>(&test_sd_mem[9]), Return(SDRX_OK)))
            .WillOnce(DoAll(SaveArg<0>(&test_sd_mem[10]), Return(SDRX_OK)))
            .WillOnce(DoAll(SaveArg<0>(&test_sd_mem[11]), Return(SDRX_OK)))
            .WillOnce(DoAll(SaveArg<0>(&test_sd_mem[12]), Return(SDRX_OK)))
            .WillOnce(DoAll(SaveArg<0>(&test_sd_mem[13]), Return(SDRX_OK)))
            .WillOnce(DoAll(SaveArg<0>(&test_sd_mem[14]), Return(SDRX_OK)))
            .WillOnce(
                DoAll(SaveArg<0>(&test_sd_mem[15]), Return(SDRX_OK_BLK_DONE)));

        mem0 = 0x0807060504030201;
        mem1 = 0x100f0e0d0c0b0a09;
        ASSERT_OK(mem.write(range(0x40, 0x47), &mem0, SBI_NONE));
        ASSERT_OK(mem.write(range(0x48, 0x4f), &mem1, SBI_NONE));

        ASSERT_OK(out.writew<u32>(0x00, 0x00000040)) << "set the SDMA address";
        ASSERT_OK(out.writew<u16>(0x04, 0x0008))
            << "define block size to eight byte";
        ASSERT_OK(out.writew<u16>(0x06, 0x0002))
            << "write two to BLOCK_COUNT_16BIT register";
        ASSERT_OK(out.writew<u32>(0x08, 0x00000000))
            << "write zero to ARG register";
        ASSERT_OK(out.writew<u16>(0x0e, 0x193a))
            << "write CMD25 (WRITE_MULTIPLE_BLOCK) to CMD register";

        ASSERT_OK(out.readw(0x10, value_of_response))
            << "read the RESPONSE register";
        EXPECT_EQ(0x01020304, value_of_response);

        wait(1, SC_US); // allow the DMI transfer to complete
        EXPECT_TRUE(sdhci.irq.read())
            << "check whether an interrupt has been triggered";

        ASSERT_OK(out.readw(0x30, value_of_normal_int_stat))
            << "check if it was the right interrupt (cmd complete = 0x0001)";
        EXPECT_EQ(0x0003, value_of_normal_int_stat);
        ASSERT_OK(out.writew<u16>(0x30, 0x0003)) << "clear the interrupt";

        ASSERT_OK(out.readw(0x32, value_of_error_int_stat))
            << "error interrupt has been triggered additionally";
        EXPECT_EQ(0x0000, value_of_error_int_stat);

        for (int i = 1; i < 17; i++) {
            EXPECT_EQ((u8)i, test_sd_mem[i - 1])
                << "check host DMA transfer was successful";
        }

        ASSERT_OK(out.readw(0x30, value_of_normal_int_stat))
            << "check whether all the interrupts have been cleared";
        EXPECT_EQ(0x0000, value_of_normal_int_stat);

        ASSERT_OK(out.readw(0x32, value_of_error_int_stat))
            << "error interrupt has been triggered additionally";
        EXPECT_EQ(0x0000, value_of_error_int_stat);
    }
};

TEST(sdhci, sdhci) {
    sdhci_harness test("TEST");
    sc_core::sc_start();
}
