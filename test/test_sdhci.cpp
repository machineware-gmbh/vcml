// written by Lasse Urban

#include <gtest/gtest.h>
#include <gmock/gmock.h>
#include "vcml.h"

using namespace ::testing;

class initiator: public vcml::component
{
public:
    vcml::master_socket OUT;
    vcml::generic::memory mem;


    initiator(const sc_core::sc_module_name& nm):
        vcml::component(nm),
        OUT("OUT"),
        mem("mem", 1024){
    		vcml::clock_t clk = 100 * vcml::MHz;
            mem.RESET.stub();
            mem.CLOCK.stub(clk);
            RESET.stub();
            CLOCK.stub(clk);
    }
};

class mock_sdcard: public vcml::component, public vcml::sd_fw_transport_if
{
public:
    vcml::sd_target_socket SD_IN;

    mock_sdcard(const sc_core::sc_module_name& nm):
        vcml::component(nm),
        SD_IN("SD_IN") {
    		vcml::clock_t clk = 100 * vcml::MHz;
            SD_IN.bind(*this);
            RESET.stub();
            CLOCK.stub(clk);
    }

    MOCK_METHOD1(sd_transport, vcml::sd_status(vcml::sd_command&));
    MOCK_METHOD1(sd_data_read, vcml::sd_tx_status(vcml::u8&));
    MOCK_METHOD1(sd_data_write, vcml::sd_rx_status(vcml::u8));

};


TEST(sdhci, sdhci) {

    vcml::generic::sdhci sdhci("SDHCI");
    vcml::clock_t clk = 100 * vcml::MHz;
    sdhci.RESET.stub();
    sdhci.CLOCK.stub(clk);

    sc_core::sc_signal<bool> irq_sig;
    initiator host_system("HOST_SYSTEM");
    mock_sdcard sdcard("MOCK_SD");

    // I/O Mapping
    host_system.OUT.bind(sdhci.IN);
    sdhci.SD_OUT.bind(sdcard.SD_IN);
    sdhci.OUT.bind(host_system.mem.IN);

    // IRQ Mapping
    sdhci.IRQ.bind(irq_sig);


/********************************************************************************
 *                                                                              *
 *                  test go_idle_state (without DMA)                            *
 *                                                                              *
 ********************************************************************************/

    sdhci.DMA_enabled = false;                                          // tests without DMA

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

    EXPECT_CALL(sdcard, sd_transport(_)).WillOnce(DoAll(SetArgReferee<0>(cmd), Return(vcml::SD_OK)));

    sc_core::sc_time quantum(10.0, sc_core::SC_SEC);
    sc_core::sc_start(quantum);                                         // start SystemC simulation

    host_system.OUT.writew(0x08, 0x00000000);                           // write zero to ARG register
    host_system.OUT.writew(0x0E, 0x0000);                               // write zero to CMD register (go_idle_state)

    vcml::u32 value_of_RESPONSE;
    vcml::u32(host_system.OUT.readw(0x10, value_of_RESPONSE));          // read the RESPONSE register
    EXPECT_EQ(0x01020304, value_of_RESPONSE);

    EXPECT_TRUE(sdhci.IRQ.read());                                      // check whether an interrupt has been triggered

    vcml::u32 value_of_NORMAL_INT_STAT;
    vcml::u32(host_system.OUT.readw(0x30, value_of_NORMAL_INT_STAT));   // check if it was the right interrupt (Command Complete)
    EXPECT_EQ(0x0001, value_of_NORMAL_INT_STAT);

    host_system.OUT.writew(0x30, 0x0001);                               // clear the interrupt

    vcml::u32(host_system.OUT.readw(0x30, value_of_NORMAL_INT_STAT));   // check if the interrupt was cleared
    EXPECT_EQ(0x0000, value_of_NORMAL_INT_STAT);

    vcml::u32 value_of_ERROR_INT_STAT;
    vcml::u32(host_system.OUT.readw(0x32, value_of_ERROR_INT_STAT));    // ... and that no error interrupt has been triggered additionally
    EXPECT_EQ(0x0000, value_of_ERROR_INT_STAT);


/********************************************************************************
 *                                                                              *
 *                  test read_multiple_block (without DMA)                      *
 *                                                                              *
 ********************************************************************************/

    host_system.OUT.writew(0x2F, 0x01);                                 // reset the SDHCI controller
    sdhci.DMA_enabled = false;                                          // tests without DMA

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

    EXPECT_CALL(sdcard, sd_transport(_)).WillOnce(DoAll(SetArgReferee<0>(cmd), Return(vcml::SD_OK_TX_RDY)));

    EXPECT_CALL(sdcard, sd_data_read(_))
          .WillOnce(DoAll(SetArgReferee<0>(0x01),Return(vcml::SDTX_OK)))
          .WillOnce(DoAll(SetArgReferee<0>(0X02),Return(vcml::SDTX_OK)))
          .WillOnce(DoAll(SetArgReferee<0>(0x03),Return(vcml::SDTX_OK)))
          .WillOnce(DoAll(SetArgReferee<0>(0x04),Return(vcml::SDTX_OK)))
          .WillOnce(DoAll(SetArgReferee<0>(0x05),Return(vcml::SDTX_OK)))
          .WillOnce(DoAll(SetArgReferee<0>(0x06),Return(vcml::SDTX_OK)))
          .WillOnce(DoAll(SetArgReferee<0>(0x07),Return(vcml::SDTX_OK)))
          .WillOnce(DoAll(SetArgReferee<0>(0x08),Return(vcml::SDTX_OK_BLK_DONE)))
          .WillOnce(DoAll(SetArgReferee<0>(0x09),Return(vcml::SDTX_OK)))
          .WillOnce(DoAll(SetArgReferee<0>(0x0A),Return(vcml::SDTX_OK)))
          .WillOnce(DoAll(SetArgReferee<0>(0X0B),Return(vcml::SDTX_OK)))
          .WillOnce(DoAll(SetArgReferee<0>(0x0C),Return(vcml::SDTX_OK)))
          .WillOnce(DoAll(SetArgReferee<0>(0x0D),Return(vcml::SDTX_OK)))
          .WillOnce(DoAll(SetArgReferee<0>(0x0E),Return(vcml::SDTX_OK)))
          .WillOnce(DoAll(SetArgReferee<0>(0x0F),Return(vcml::SDTX_OK)))
          .WillOnce(DoAll(SetArgReferee<0>(0x10),Return(vcml::SDTX_OK_BLK_DONE)));

    host_system.OUT.writew(0x04, 0x0008);                               // define block size to eight byte
    host_system.OUT.writew(0x06, 0x0002);                               // write two to BLOCK_COUNT_16BIT register
    host_system.OUT.writew(0x08, 0x00000000);                           // write zero to ARG register
    host_system.OUT.writew(0x0E, 0x123A);                               // write CMD18 (READ_MULTIPLE_BLOCK) to CMD register

    vcml::u32(host_system.OUT.readw(0x10, value_of_RESPONSE));          // read the RESPONSE register
    EXPECT_EQ(0x01020304, value_of_RESPONSE);

    EXPECT_TRUE(sdhci.IRQ.read());                                      // check whether an interrupt has been triggered

    vcml::u32(host_system.OUT.readw(0x30, value_of_NORMAL_INT_STAT));   // check if it was the right interrupt (Command Complete = 0x0001, Buffer Read Ready = 0x0020)
    EXPECT_EQ(0x0021, value_of_NORMAL_INT_STAT);
    host_system.OUT.writew(0x30, 0x0021);                               // clear the interrupt

    vcml::u32(host_system.OUT.readw(0x32, value_of_ERROR_INT_STAT));    // ... and that no error interrupt has been triggered additionally
    EXPECT_EQ(0x0000, value_of_ERROR_INT_STAT);

    /* read 2 blocks with blocksize 8 */
    vcml::u32 value_of_BUFFER_DATA_PORT;
    vcml::u32(host_system.OUT.readw(0x20, value_of_BUFFER_DATA_PORT));  // read BUFFER_DATA_PORT register
    EXPECT_EQ(0x04030201, value_of_BUFFER_DATA_PORT);
    vcml::u32(host_system.OUT.readw(0x20, value_of_BUFFER_DATA_PORT));  // read BUFFER_DATA_PORT register
    EXPECT_EQ(0x08070605, value_of_BUFFER_DATA_PORT);
    vcml::u32(host_system.OUT.readw(0x20, value_of_BUFFER_DATA_PORT));  // read BUFFER_DATA_PORT register
    EXPECT_EQ(0x0C0B0A09, value_of_BUFFER_DATA_PORT);
    vcml::u32(host_system.OUT.readw(0x20, value_of_BUFFER_DATA_PORT));  // read BUFFER_DATA_PORT register
    EXPECT_EQ(0x100F0E0D, value_of_BUFFER_DATA_PORT);

    EXPECT_TRUE(sdhci.IRQ.read());                                      // check whether an interrupt has been triggered

    vcml::u32(host_system.OUT.readw(0x30, value_of_NORMAL_INT_STAT));   // check if it was the right interrupt (Transfer Complete)
    EXPECT_EQ(0x0002, value_of_NORMAL_INT_STAT);

    host_system.OUT.writew(0x30, 0x0002);                               // clear the interrupt

    vcml::u32(host_system.OUT.readw(0x30, value_of_NORMAL_INT_STAT));   // check whether all the interrupts have been cleared
    EXPECT_EQ(0x0000, value_of_NORMAL_INT_STAT);

    vcml::u32(host_system.OUT.readw(0x32, value_of_ERROR_INT_STAT));    // ... and that no error interrupt has been triggered additionally
    EXPECT_EQ(0x0000, value_of_ERROR_INT_STAT);


/********************************************************************************
 *                                                                              *
 *                  test write_multiple_block (without DMA)                     *
 *                                                                              *
 ********************************************************************************/

    host_system.OUT.writew(0x2F, 0x01);                                 // reset the SDHCI controller
    sdhci.DMA_enabled = false;                                          // tests without DMA

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

    EXPECT_CALL(sdcard, sd_transport(_)).WillOnce(DoAll(SetArgReferee<0>(cmd), Return(vcml::SD_OK_RX_RDY)));

    vcml::u8 test_sd_mem[16];

    EXPECT_CALL(sdcard, sd_data_write(_))
          .WillOnce(DoAll(SaveArg<0>(&test_sd_mem[0]), Return(vcml::SDRX_OK)))
          .WillOnce(DoAll(SaveArg<0>(&test_sd_mem[1]), Return(vcml::SDRX_OK)))
          .WillOnce(DoAll(SaveArg<0>(&test_sd_mem[2]), Return(vcml::SDRX_OK)))
          .WillOnce(DoAll(SaveArg<0>(&test_sd_mem[3]), Return(vcml::SDRX_OK)))
          .WillOnce(DoAll(SaveArg<0>(&test_sd_mem[4]), Return(vcml::SDRX_OK)))
          .WillOnce(DoAll(SaveArg<0>(&test_sd_mem[5]), Return(vcml::SDRX_OK)))
          .WillOnce(DoAll(SaveArg<0>(&test_sd_mem[6]), Return(vcml::SDRX_OK)))
          .WillOnce(DoAll(SaveArg<0>(&test_sd_mem[7]), Return(vcml::SDRX_OK_BLK_DONE)))
          .WillOnce(DoAll(SaveArg<0>(&test_sd_mem[8]), Return(vcml::SDRX_OK)))
          .WillOnce(DoAll(SaveArg<0>(&test_sd_mem[9]), Return(vcml::SDRX_OK)))
          .WillOnce(DoAll(SaveArg<0>(&test_sd_mem[10]), Return(vcml::SDRX_OK)))
          .WillOnce(DoAll(SaveArg<0>(&test_sd_mem[11]), Return(vcml::SDRX_OK)))
          .WillOnce(DoAll(SaveArg<0>(&test_sd_mem[12]), Return(vcml::SDRX_OK)))
          .WillOnce(DoAll(SaveArg<0>(&test_sd_mem[13]), Return(vcml::SDRX_OK)))
          .WillOnce(DoAll(SaveArg<0>(&test_sd_mem[14]), Return(vcml::SDRX_OK)))
          .WillOnce(DoAll(SaveArg<0>(&test_sd_mem[15]), Return(vcml::SDRX_OK_BLK_DONE)));

    host_system.OUT.writew(0x04, 0x0008);                               // define block size to eight byte
    host_system.OUT.writew(0x06, 0x0002);                               // write two to BLOCK_COUNT_16BIT register
    host_system.OUT.writew(0x08, 0x00000000);                           // write zero to ARG register
    host_system.OUT.writew(0x0E, 0x193A);                               // write CMD25 (WRITE_MULTIPLE_BLOCK) to CMD register

    vcml::u32(host_system.OUT.readw(0x10, value_of_RESPONSE));          // read the RESPONSE register
    EXPECT_EQ(0x01020304, value_of_RESPONSE);

    EXPECT_TRUE(sdhci.IRQ.read());                                      // check whether an interrupt has been triggered

    vcml::u32(host_system.OUT.readw(0x30, value_of_NORMAL_INT_STAT));   // check if it was the right interrupt (Command Complete = 0x0001, Buffer Write Ready = 0x0010)
    EXPECT_EQ(0x0011, value_of_NORMAL_INT_STAT);
    host_system.OUT.writew(0x30, 0x0011);                               // clear the interrupt

    vcml::u32(host_system.OUT.readw(0x32, value_of_ERROR_INT_STAT));    // ... and that no error interrupt has been triggered additionally
    EXPECT_EQ(0x0000, value_of_ERROR_INT_STAT);

    /* write 2 blocks with blocksize 8 */
    vcml::u32(host_system.OUT.writew(0x20, 0x04030201));                // write BUFFER_DATA_PORT register
    vcml::u32(host_system.OUT.writew(0x20, 0x08070605));                // write BUFFER_DATA_PORT register
    vcml::u32(host_system.OUT.writew(0x20, 0x0C0B0A09));                // write BUFFER_DATA_PORT register
    vcml::u32(host_system.OUT.writew(0x20, 0x100F0E0D));                // write BUFFER_DATA_PORT register

    for(int i = 1; i < 17; i++)                                         // check whether the write process was successful
        EXPECT_EQ(vcml::u8(i), test_sd_mem[i-1]);

    EXPECT_TRUE(sdhci.IRQ.read());                                      // check whether an interrupt has been triggered

    vcml::u32(host_system.OUT.readw(0x30, value_of_NORMAL_INT_STAT));   // check if it was the right interrupt (Transfer Complete)
    EXPECT_EQ(0x0002, value_of_NORMAL_INT_STAT);

    host_system.OUT.writew(0x30, 0x0002);                               // clear the interrupt

    vcml::u32(host_system.OUT.readw(0x30, value_of_NORMAL_INT_STAT));   // check whether all the interrupts have been cleared
    EXPECT_EQ(0x0000, value_of_NORMAL_INT_STAT);

    vcml::u32(host_system.OUT.readw(0x32, value_of_ERROR_INT_STAT));    // ... and that no error interrupt has been triggered additionally
    EXPECT_EQ(0x0000, value_of_ERROR_INT_STAT);


/********************************************************************************
 *                                                                              *
 *                  test read_multiple_block (with DMA)                         *
 *                                                                              *
 ********************************************************************************/

    host_system.OUT.writew(0x2F, 0x01);                                 // reset the SDHCI controller
    sdhci.DMA_enabled = true;                                           // tests with DMA

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

    EXPECT_CALL(sdcard, sd_transport(_)).WillOnce(DoAll(SetArgReferee<0>(cmd), Return(vcml::SD_OK_TX_RDY)));

    EXPECT_CALL(sdcard, sd_data_read(_))
          .WillOnce(DoAll(SetArgReferee<0>(0x01),Return(vcml::SDTX_OK)))
          .WillOnce(DoAll(SetArgReferee<0>(0X02),Return(vcml::SDTX_OK)))
          .WillOnce(DoAll(SetArgReferee<0>(0x03),Return(vcml::SDTX_OK)))
          .WillOnce(DoAll(SetArgReferee<0>(0x04),Return(vcml::SDTX_OK)))
          .WillOnce(DoAll(SetArgReferee<0>(0x05),Return(vcml::SDTX_OK)))
          .WillOnce(DoAll(SetArgReferee<0>(0x06),Return(vcml::SDTX_OK)))
          .WillOnce(DoAll(SetArgReferee<0>(0x07),Return(vcml::SDTX_OK)))
          .WillOnce(DoAll(SetArgReferee<0>(0x08),Return(vcml::SDTX_OK_BLK_DONE)))
          .WillOnce(DoAll(SetArgReferee<0>(0x09),Return(vcml::SDTX_OK)))
          .WillOnce(DoAll(SetArgReferee<0>(0x0A),Return(vcml::SDTX_OK)))
          .WillOnce(DoAll(SetArgReferee<0>(0X0B),Return(vcml::SDTX_OK)))
          .WillOnce(DoAll(SetArgReferee<0>(0x0C),Return(vcml::SDTX_OK)))
          .WillOnce(DoAll(SetArgReferee<0>(0x0D),Return(vcml::SDTX_OK)))
          .WillOnce(DoAll(SetArgReferee<0>(0x0E),Return(vcml::SDTX_OK)))
          .WillOnce(DoAll(SetArgReferee<0>(0x0F),Return(vcml::SDTX_OK)))
          .WillOnce(DoAll(SetArgReferee<0>(0x10),Return(vcml::SDTX_OK_BLK_DONE)));

    host_system.OUT.writew(0x00, 0x00000010);                           // set the SDMA address
    host_system.OUT.writew(0x04, 0x0008);                               // define block size to eight byte
    host_system.OUT.writew(0x06, 0x0002);                               // write two to BLOCK_COUNT_16BIT register
    host_system.OUT.writew(0x08, 0x00000000);                           // write zero to ARG register
    host_system.OUT.writew(0x0E, 0x123A);                               // write CMD18 (READ_MULTIPLE_BLOCK) to CMD register

    sc_core::sc_start(quantum);
    vcml::u32(host_system.OUT.readw(0x10, value_of_RESPONSE));          // read the RESPONSE register
    EXPECT_EQ(0x01020304, value_of_RESPONSE);

    EXPECT_TRUE(sdhci.IRQ.read());                                      // check whether an interrupt has been triggered

    vcml::u32(host_system.OUT.readw(0x30, value_of_NORMAL_INT_STAT));   // check if it was the right interrupt (Command Complete = 0x0001)
    EXPECT_EQ(0x0003, value_of_NORMAL_INT_STAT);
    host_system.OUT.writew(0x30, 0x0003);                               // clear the interrupt

    vcml::u32(host_system.OUT.readw(0x32, value_of_ERROR_INT_STAT));    // ... and that no error interrupt has been triggered additionally
    EXPECT_EQ(0x0000, value_of_ERROR_INT_STAT);

    vcml::u64 mem0;
    vcml::u64 mem1;
    vcml::sideband info = vcml::SBI_NONE;
    host_system.mem.read(vcml::range(0x00000010, 0x00000017), &mem0, info);
    host_system.mem.read(vcml::range(0x00000018, 0x0000001F), &mem1, info);

    EXPECT_EQ(mem0, 0x0807060504030201);                                // check whether the write process to the host memory (DMA) was successful
    EXPECT_EQ(mem1, 0x100F0E0D0C0B0A09);


/********************************************************************************
 *                                                                              *
 *                  test write_multiple_block (with DMA)                        *
 *                                                                              *
 ********************************************************************************/

    host_system.OUT.writew(0x2F, 0x01);                                 // reset the SDHCI controller
    sdhci.DMA_enabled = true;                                           // tests with DMA

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

    EXPECT_CALL(sdcard, sd_transport(_)).WillOnce(DoAll(SetArgReferee<0>(cmd), Return(vcml::SD_OK_RX_RDY)));

    for (int i = 0; i < 16; i++) {
        test_sd_mem[i] = 0;                                             // reset the sd buffer
    }

    EXPECT_CALL(sdcard, sd_data_write(_))
          .WillOnce(DoAll(SaveArg<0>(&test_sd_mem[0]), Return(vcml::SDRX_OK)))
          .WillOnce(DoAll(SaveArg<0>(&test_sd_mem[1]), Return(vcml::SDRX_OK)))
          .WillOnce(DoAll(SaveArg<0>(&test_sd_mem[2]), Return(vcml::SDRX_OK)))
          .WillOnce(DoAll(SaveArg<0>(&test_sd_mem[3]), Return(vcml::SDRX_OK)))
          .WillOnce(DoAll(SaveArg<0>(&test_sd_mem[4]), Return(vcml::SDRX_OK)))
          .WillOnce(DoAll(SaveArg<0>(&test_sd_mem[5]), Return(vcml::SDRX_OK)))
          .WillOnce(DoAll(SaveArg<0>(&test_sd_mem[6]), Return(vcml::SDRX_OK)))
          .WillOnce(DoAll(SaveArg<0>(&test_sd_mem[7]), Return(vcml::SDRX_OK_BLK_DONE)))
          .WillOnce(DoAll(SaveArg<0>(&test_sd_mem[8]), Return(vcml::SDRX_OK)))
          .WillOnce(DoAll(SaveArg<0>(&test_sd_mem[9]), Return(vcml::SDRX_OK)))
          .WillOnce(DoAll(SaveArg<0>(&test_sd_mem[10]), Return(vcml::SDRX_OK)))
          .WillOnce(DoAll(SaveArg<0>(&test_sd_mem[11]), Return(vcml::SDRX_OK)))
          .WillOnce(DoAll(SaveArg<0>(&test_sd_mem[12]), Return(vcml::SDRX_OK)))
          .WillOnce(DoAll(SaveArg<0>(&test_sd_mem[13]), Return(vcml::SDRX_OK)))
          .WillOnce(DoAll(SaveArg<0>(&test_sd_mem[14]), Return(vcml::SDRX_OK)))
          .WillOnce(DoAll(SaveArg<0>(&test_sd_mem[15]), Return(vcml::SDRX_OK_BLK_DONE)));


    mem0 = 0x0807060504030201;
    mem1 = 0x100F0E0D0C0B0A09;
    host_system.mem.write(vcml::range(0x00000040, 0x00000047), &mem0, info);
    host_system.mem.write(vcml::range(0x00000048, 0x0000004F), &mem1, info);

    host_system.OUT.writew(0x00, 0x00000040);                           // set the SDMA address
    host_system.OUT.writew(0x04, 0x0008);                               // define block size to eight byte
    host_system.OUT.writew(0x06, 0x0002);                               // write two to BLOCK_COUNT_16BIT register
    host_system.OUT.writew(0x08, 0x00000000);                           // write zero to ARG register
    host_system.OUT.writew(0x0E, 0x193A);                               // write CMD25 (WRITE_MULTIPLE_BLOCK) to CMD register

    sc_core::sc_start(quantum);
    vcml::u32(host_system.OUT.readw(0x10, value_of_RESPONSE));          // read the RESPONSE register
    EXPECT_EQ(0x01020304, value_of_RESPONSE);

    EXPECT_TRUE(sdhci.IRQ.read());                                      // check whether an interrupt has been triggered

    vcml::u32(host_system.OUT.readw(0x30, value_of_NORMAL_INT_STAT));   // check if it was the right interrupt (Command Complete = 0x0001)
    EXPECT_EQ(0x0003, value_of_NORMAL_INT_STAT);
    host_system.OUT.writew(0x30, 0x0003);                               // clear the interrupt

    vcml::u32(host_system.OUT.readw(0x32, value_of_ERROR_INT_STAT));    // ... and that no error interrupt has been triggered additionally
    EXPECT_EQ(0x0000, value_of_ERROR_INT_STAT);

    for(int i = 1; i < 17; i++)                                         // check whether the write process was successful
        EXPECT_EQ(vcml::u8(i), test_sd_mem[i-1]);

    vcml::u32(host_system.OUT.readw(0x30, value_of_NORMAL_INT_STAT));   // check whether all the interrupts have been cleared
    EXPECT_EQ(0x0000, value_of_NORMAL_INT_STAT);

    vcml::u32(host_system.OUT.readw(0x32, value_of_ERROR_INT_STAT));    // ... and that no error interrupt has been triggered additionally
    EXPECT_EQ(0x0000, value_of_ERROR_INT_STAT);

/********************************************************************************
 *                                                                              *
 *                  negative test: SDMA boundary exceeding                      *
 *                                                                              *
 ********************************************************************************/

        host_system.OUT.writew(0x2F, 0x01);                                 // reset the SDHCI controller
        sdhci.DMA_enabled = true;                                           // tests with DMA

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

        EXPECT_CALL(sdcard, sd_transport(_)).WillOnce(DoAll(SetArgReferee<0>(cmd), Return(vcml::SD_OK_RX_RDY)));

        host_system.OUT.writew(0x00, 0x00000040);                           // set the SDMA address
        host_system.OUT.writew(0x04, 0x0FFF);                               // define block size to 4097 byte > boundary
        host_system.OUT.writew(0x06, 0x0002);                               // write two to BLOCK_COUNT_16BIT register
        host_system.OUT.writew(0x08, 0x00000000);                           // write zero to ARG register
        host_system.OUT.writew(0x0E, 0x193A);                               // write CMD25 (WRITE_MULTIPLE_BLOCK) to CMD register

        vcml::u32(host_system.OUT.readw(0x10, value_of_RESPONSE));          // read the RESPONSE register
        EXPECT_EQ(0x01020304, value_of_RESPONSE);

        EXPECT_DEATH(sc_core::sc_start(quantum), "SDMA boundary of the SDHCI exceeded"); // expect VCML_ERROR (abort)
        // the exception should be anything like "Error: (E549) uncaught exception: SDMA boundary of the SDHCI exceeded, not implemented"




/********************************************************************************
 *                                                                              *
 *                  negative test: try to write the Buffer Data Port register   *
 *                                                                              *
 ********************************************************************************/

        host_system.OUT.writew(0x2F, 0x01);                                 // reset the SDHCI controller
        sdhci.DMA_enabled = true;                                           // tests with DMA

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

        EXPECT_CALL(sdcard, sd_transport(_)).WillOnce(DoAll(SetArgReferee<0>(cmd), Return(vcml::SD_OK_RX_RDY)));

        host_system.OUT.writew(0x00, 0x00000040);                           // set the SDMA address
        host_system.OUT.writew(0x04, 0x0FFF);                               // define block size to 4097 byte > boundary
        host_system.OUT.writew(0x06, 0x0002);                               // write two to BLOCK_COUNT_16BIT register
        host_system.OUT.writew(0x08, 0x00000000);                           // write zero to ARG register
        host_system.OUT.writew(0x0E, 0x193A);                               // write CMD25 (WRITE_MULTIPLE_BLOCK) to CMD register

        vcml::u32(host_system.OUT.readw(0x10, value_of_RESPONSE));          // read the RESPONSE register
        EXPECT_EQ(0x01020304, value_of_RESPONSE);

        EXPECT_DEATH(host_system.OUT.writew(0x20, 0x0001), "Actually it is not allowed to write data to the BUFFER_DATA_PORT"); // expect VCML_ERROR (abort)
        // the exception should be anything like "Error: (E549) uncaught exception: Actually it is not allowed to write data to the BUFFER_DATA_PORT!"
}
