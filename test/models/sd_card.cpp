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

enum : u32 {
    BLKSZ = 512,
    SDSC_CAPACITY = 1 * MiB,
    OCR_VOLTAGES = 0x00ff8000,
    OCR_HCS = bit(30),
    IF_COND = 0x1aa,
};

enum card_state : u32 {
    STATE_IDLE = 0,
    STATE_IDENTIFICATION = 2,
    STATE_STAND_BY = 3,
    STATE_TRANSFER = 4,
};

enum spi_token : u8 {
    SPI_IDLE = 0xff,
    SPI_START_BLOCK = 0xfe,
    SPI_START_MULTI = 0xfc,
    SPI_STOP_MULTI = 0xfd,
    SPI_DATA_ACCEPTED = 0x05,
    SPI_DATA_ERROR = 0x09,
    SPI_DATA_CRC_ERROR = 0x0b,
};

class sd_card_bench : public test_base
{
public:
    sd::card sdsc;
    sd::card sdhc;
    sd::card spicard;
    spi::spi2sd bridge;

    sd_initiator_socket sd_sdsc;
    sd_initiator_socket sd_sdhc;
    spi_initiator_socket spi;
    gpio_initiator_socket cs;

    sd_card_bench(const sc_module_name& nm):
        test_base(nm),
        sdsc("sdsc", "ramdisk:1MiB"),
        sdhc("sdhc", "ramdisk:2GiB"),
        spicard("spicard", "ramdisk:1MiB"),
        bridge("bridge"),
        sd_sdsc("sd_sdsc"),
        sd_sdhc("sd_sdhc"),
        spi("spi"),
        cs("cs") {
        sd_sdsc.bind(sdsc.sd_in);
        sd_sdhc.bind(sdhc.sd_in);
        spi.bind(bridge.spi_in);
        cs.bind(bridge.cs);
        bridge.sd_out.bind(spicard.sd_in);

        rst.bind(sdsc.rst);
        rst.bind(sdhc.rst);
        rst.bind(spicard.rst);
        rst.bind(bridge.rst);
        clk.bind(sdsc.clk);
        clk.bind(sdhc.clk);
        clk.bind(spicard.clk);
        clk.bind(bridge.clk);

        add_test("sd_identification", &sd_card_bench::test_sd_identification);
        add_test("sd_capacity", &sd_card_bench::test_sd_capacity);
        add_test("sd_rca", &sd_card_bench::test_sd_rca);
        add_test("sd_single_block", &sd_card_bench::test_sd_single_block);
        add_test("sd_multi_block", &sd_card_bench::test_sd_multi_block);
        add_test("sd_blocklen", &sd_card_bench::test_sd_blocklen);
        add_test("sd_block_addressing",
                 &sd_card_bench::test_sd_block_addressing);
        add_test("sd_registers", &sd_card_bench::test_sd_registers);
        add_test("sd_command_errors", &sd_card_bench::test_sd_command_errors);
        add_test("sd_data_errors", &sd_card_bench::test_sd_data_errors);
        add_test("spi_initialization",
                 &sd_card_bench::test_spi_initialization);
        add_test("spi_registers", &sd_card_bench::test_spi_registers);
        add_test("spi_single_block", &sd_card_bench::test_spi_single_block);
        add_test("spi_multi_block", &sd_card_bench::test_spi_multi_block);
        add_test("spi_crc_off", &sd_card_bench::test_spi_crc_off);
        add_test("spi_errors", &sd_card_bench::test_spi_errors);
        add_test("spi_write_crc_error",
                 &sd_card_bench::test_spi_write_crc_error);
        add_test("spi_chip_select", &sd_card_bench::test_spi_chip_select);
    }

    static vector<u8> pattern(size_t len, u8 seed) {
        vector<u8> data(len);
        for (size_t i = 0; i < len; i++)
            data[i] = (u8)(seed + i * 7);
        return data;
    }

    sd_command sd_cmd(sd_initiator_socket& sd, u8 opcode, u32 arg = 0) {
        sd_command cmd;
        sd_reset(cmd);
        cmd.opcode = opcode;
        cmd.argument = arg;
        cmd.crc = sd_crc7(cmd);
        sd.transport(cmd);
        return cmd;
    }

    sd_command sd_acmd(sd_initiator_socket& sd, u8 opcode, u32 arg = 0) {
        EXPECT_EQ(sd_cmd(sd, 55).status, SD_OK);
        return sd_cmd(sd, opcode, arg);
    }

    static u32 r1_status(const sd_command& cmd) {
        EXPECT_EQ(cmd.resp_len, 6);
        EXPECT_EQ(cmd.response[5], crc7(cmd.response, 5) | 1);
        return (u32)cmd.response[1] << 24 | (u32)cmd.response[2] << 16 |
               (u32)cmd.response[3] << 8 | (u32)cmd.response[4];
    }

    static u32 r1_state(const sd_command& cmd) {
        return (r1_status(cmd) & sd::card::CURRENT_STATE) >> 9;
    }

    u32 sd_state(sd_initiator_socket& sd) {
        sd_command cmd = sd_cmd(sd, 13);
        EXPECT_EQ(cmd.status, SD_OK);
        return r1_state(cmd);
    }

    void sd_init(sd_initiator_socket& sd) {
        EXPECT_EQ(sd_cmd(sd, 0).status, SD_OK);
        EXPECT_EQ(sd_cmd(sd, 8, IF_COND).status, SD_OK);
        EXPECT_EQ(sd_acmd(sd, 41, OCR_HCS | OCR_VOLTAGES).status, SD_OK);
        EXPECT_EQ(sd_cmd(sd, 2).status, SD_OK);
        EXPECT_EQ(sd_cmd(sd, 3).status, SD_OK);
        EXPECT_EQ(sd_cmd(sd, 7).status, SD_OK);
        EXPECT_EQ(sd_state(sd), STATE_TRANSFER);
    }

    vector<u8> sd_read_raw(sd_initiator_socket& sd, size_t len) {
        vector<u8> data(len);
        for (u8& val : data)
            EXPECT_EQ(sd.read_data(val), SDTX_OK);
        u8 dummy = 0;
        EXPECT_EQ(sd.read_data(dummy), SDTX_OK_COMPLETE);
        return data;
    }

    vector<u8> sd_read(sd_initiator_socket& sd, size_t len, sd_status_tx end) {
        vector<u8> data(len);
        for (u8& val : data)
            EXPECT_EQ(sd.read_data(val), SDTX_OK);

        u8 hi = 0, lo = 0, dummy = 0;
        EXPECT_EQ(sd.read_data(hi), SDTX_OK);
        EXPECT_EQ(sd.read_data(lo), SDTX_OK);
        EXPECT_EQ((u16)(hi << 8 | lo), crc16(data.data(), len));
        EXPECT_EQ(sd.read_data(dummy), end);
        return data;
    }

    sd_status_rx sd_write(sd_initiator_socket& sd, const vector<u8>& data,
                          u16 crc_error = 0) {
        u16 crc = crc16(data.data(), data.size()) ^ crc_error;
        for (u8 val : data)
            EXPECT_EQ(sd.write_data(val), SDRX_OK);
        EXPECT_EQ(sd.write_data(crc >> 8), SDRX_OK);
        EXPECT_EQ(sd.write_data(crc & 0xff), SDRX_OK);
        return sd.write_data(0xff);
    }

    vector<u8> sd_read_block(sd_initiator_socket& sd, u32 addr) {
        EXPECT_EQ(sd_cmd(sd, 17, addr).status, SD_OK_TX_RDY);
        return sd_read(sd, BLKSZ, SDTX_OK_COMPLETE);
    }

    void sd_write_block(sd_initiator_socket& sd, u32 addr,
                        const vector<u8>& data) {
        EXPECT_EQ(sd_cmd(sd, 24, addr).status, SD_OK_RX_RDY);
        EXPECT_EQ(sd_write(sd, data), SDRX_OK_COMPLETE);
    }

    void test_sd_identification() {
        for (auto* sd : { &sd_sdsc, &sd_sdhc }) {
            bool hc = sd == &sd_sdhc;
            sdsc.reset();
            sdhc.reset();

            EXPECT_EQ(sd_cmd(*sd, 0).status, SD_OK);
            EXPECT_EQ(sd_state(*sd), STATE_IDLE);

            sd_command cmd = sd_cmd(*sd, 8, IF_COND);
            EXPECT_EQ(cmd.status, SD_OK);
            ASSERT_EQ(cmd.resp_len, 6);
            EXPECT_EQ(cmd.response[0], 8);
            EXPECT_EQ(cmd.response[3], 0x01);
            EXPECT_EQ(cmd.response[4], 0xaa);
            EXPECT_EQ(cmd.response[5], crc7(cmd.response, 5) | 1);

            cmd = sd_cmd(*sd, 55);
            EXPECT_EQ(cmd.status, SD_OK);
            EXPECT_TRUE(r1_status(cmd) & sd::card::APP_CMD);

            cmd = sd_cmd(*sd, 41, 0);
            EXPECT_EQ(cmd.status, SD_OK);
            ASSERT_EQ(cmd.resp_len, 6);
            EXPECT_EQ(cmd.response[0], 0x3f);
            EXPECT_EQ(cmd.response[1] & 0x80, 0) << "card powered up";
            EXPECT_EQ(cmd.response[2], 0xff);
            EXPECT_EQ(cmd.response[3], 0x80);
            EXPECT_EQ(sd_state(*sd), STATE_IDLE);

            cmd = sd_acmd(*sd, 41, OCR_HCS | OCR_VOLTAGES);
            EXPECT_EQ(cmd.status, SD_OK);
            ASSERT_EQ(cmd.resp_len, 6);
            EXPECT_EQ(cmd.response[1] & 0x80, 0x80) << "card not powered up";
            EXPECT_EQ(cmd.response[1] & 0x40, hc ? 0x40 : 0) << "wrong CCS";

            cmd = sd_cmd(*sd, 2);
            EXPECT_EQ(cmd.status, SD_OK);
            ASSERT_EQ(cmd.resp_len, 17);
            EXPECT_EQ(cmd.response[0], 0x3f);
            EXPECT_EQ(cmd.response[1], 0xbb);
            EXPECT_EQ(string((char*)cmd.response + 2, 2), "JW");
            EXPECT_EQ(string((char*)cmd.response + 4, 5), "VCML ");
            EXPECT_EQ(cmd.response[16] & 1, 1);

            cmd = sd_cmd(*sd, 3);
            EXPECT_EQ(cmd.status, SD_OK);
            ASSERT_EQ(cmd.resp_len, 6);
            EXPECT_EQ(cmd.response[0], 3);
            EXPECT_EQ(cmd.response[5], crc7(cmd.response, 5) | 1);
            u32 rca = (u32)cmd.response[1] << 8 | cmd.response[2];

            EXPECT_EQ(cmd.response[3] >> 5, 0) << "unexpected error bits";
            EXPECT_EQ((cmd.response[3] >> 1) & 0xf, STATE_IDENTIFICATION);
            EXPECT_EQ(sd_state(*sd), STATE_STAND_BY);

            cmd = sd_cmd(*sd, 10, rca << 16);
            EXPECT_EQ(cmd.status, SD_OK);
            ASSERT_EQ(cmd.resp_len, 17);
            EXPECT_EQ(cmd.response[1], 0xbb);

            EXPECT_EQ(sd_cmd(*sd, 7, rca << 16).status, SD_OK);
            EXPECT_EQ(sd_state(*sd), STATE_TRANSFER);
        }
    }

    void test_sd_capacity() {
        sd_init(sd_sdsc);
        sd_command cmd = sd_cmd(sd_sdsc, 9);
        EXPECT_EQ(cmd.status, SD_OK);
        ASSERT_EQ(cmd.resp_len, 17);
        const u8* csd = cmd.response + 1;
        EXPECT_EQ(csd[0] >> 6, 0) << "not a CSD version 1.0";
        u64 read_bl_len = csd[5] & 0xf;
        u64 c_size = (csd[6] & 3u) << 10 | csd[7] << 2 | csd[8] >> 6;
        u64 c_size_mult = (csd[9] & 3u) << 1 | csd[10] >> 7;
        EXPECT_EQ((c_size + 1) << (c_size_mult + 2 + read_bl_len),
                  SDSC_CAPACITY);

        sd_init(sd_sdhc);
        cmd = sd_cmd(sd_sdhc, 9);
        EXPECT_EQ(cmd.status, SD_OK);
        ASSERT_EQ(cmd.resp_len, 17);
        EXPECT_EQ(csd[0] >> 6, 1) << "not a CSD version 2.0";
        c_size = (csd[7] & 0x3fu) << 16 | csd[8] << 8 | csd[9];
        EXPECT_EQ((c_size + 1) * 512 * KiB, 2 * GiB);
    }

    void test_sd_rca() {
        sd_init(sd_sdsc);
        sd_command cmd = sd_cmd(sd_sdsc, 3);
        u32 other = ((u32)cmd.response[1] << 8 | cmd.response[2]) + 1;
        sd_init(sd_sdsc);

        for (u8 opcode : { 9, 10, 13 }) {
            cmd = sd_cmd(sd_sdsc, opcode, other << 16);
            EXPECT_EQ(cmd.status, SD_OK) << "CMD" << (int)opcode;
            EXPECT_EQ(cmd.resp_len, 0) << "CMD" << (int)opcode;
        }
    }

    void test_sd_single_block() {
        sd_init(sd_sdsc);
        vector<u8> zero(BLKSZ, 0);
        vector<u8> data = pattern(BLKSZ, 0x11);

        sd_write_block(sd_sdsc, 1 * BLKSZ, zero);
        sd_write_block(sd_sdsc, 2 * BLKSZ, data);
        sd_write_block(sd_sdsc, 3 * BLKSZ, zero);
        EXPECT_EQ(sd_state(sd_sdsc), STATE_TRANSFER);

        EXPECT_EQ(sd_read_block(sd_sdsc, 1 * BLKSZ), zero);
        EXPECT_EQ(sd_read_block(sd_sdsc, 2 * BLKSZ), data);
        EXPECT_EQ(sd_read_block(sd_sdsc, 3 * BLKSZ), zero);
        EXPECT_EQ(sd_state(sd_sdsc), STATE_TRANSFER);

        u8 val = 0;
        EXPECT_EQ(sd_sdsc.read_data(val), SDTX_ERR_ILLEGAL);
        EXPECT_EQ(sd_sdsc.write_data(val), SDRX_ERR_ILLEGAL);

        sd_write_block(sd_sdsc, 2 * BLKSZ, zero);
    }

    void test_sd_multi_block() {
        sd_init(sd_sdsc);
        vector<u8> zero(BLKSZ, 0);
        vector<u8> blk0 = pattern(BLKSZ, 0x22);
        vector<u8> blk1 = pattern(BLKSZ, 0x33);

        sd_write_block(sd_sdsc, 6 * BLKSZ, zero);

        EXPECT_EQ(sd_cmd(sd_sdsc, 25, 4 * BLKSZ).status, SD_OK_RX_RDY);
        EXPECT_EQ(sd_write(sd_sdsc, blk0), SDRX_OK_BLK_DONE);
        EXPECT_EQ(sd_write(sd_sdsc, blk1), SDRX_OK_BLK_DONE);
        EXPECT_EQ(sd_cmd(sd_sdsc, 12).status, SD_OK);
        EXPECT_EQ(sd_sdsc.write_data(0), SDRX_ERR_ILLEGAL);
        EXPECT_EQ(sd_state(sd_sdsc), STATE_TRANSFER);

        EXPECT_EQ(sd_acmd(sd_sdsc, 22).status, SD_OK_TX_RDY);
        EXPECT_EQ(sd_read(sd_sdsc, 4, SDTX_OK_COMPLETE),
                  vector<u8>({ 0, 0, 0, 2 }));

        EXPECT_EQ(sd_cmd(sd_sdsc, 18, 4 * BLKSZ).status, SD_OK_TX_RDY);
        EXPECT_EQ(sd_read(sd_sdsc, BLKSZ, SDTX_OK_BLK_DONE), blk0);
        EXPECT_EQ(sd_read(sd_sdsc, BLKSZ, SDTX_OK_BLK_DONE), blk1);
        EXPECT_EQ(sd_read(sd_sdsc, BLKSZ, SDTX_OK_BLK_DONE), zero);
        EXPECT_EQ(sd_cmd(sd_sdsc, 12).status, SD_OK);
        u8 val = 0;
        EXPECT_EQ(sd_sdsc.read_data(val), SDTX_ERR_ILLEGAL);
        EXPECT_EQ(sd_state(sd_sdsc), STATE_TRANSFER);

        u32 last = SDSC_CAPACITY - BLKSZ;
        EXPECT_EQ(sd_cmd(sd_sdsc, 25, last).status, SD_OK_RX_RDY);
        EXPECT_EQ(sd_write(sd_sdsc, blk1), SDRX_OK_COMPLETE);
        EXPECT_EQ(sd_cmd(sd_sdsc, 18, last).status, SD_OK_TX_RDY);
        EXPECT_EQ(sd_read(sd_sdsc, BLKSZ, SDTX_OK_COMPLETE), blk1);

        sd_write_block(sd_sdsc, 4 * BLKSZ, zero);
        sd_write_block(sd_sdsc, 5 * BLKSZ, zero);
        sd_write_block(sd_sdsc, last, zero);
    }

    void test_sd_blocklen() {
        sd_init(sd_sdsc);
        vector<u8> data = pattern(BLKSZ, 0x44);
        vector<u8> half(data.begin() + BLKSZ / 2, data.end());
        sd_write_block(sd_sdsc, 8 * BLKSZ, data);

        EXPECT_EQ(sd_cmd(sd_sdsc, 16, BLKSZ / 2).status, SD_OK);
        EXPECT_EQ(sd_cmd(sd_sdsc, 17, 8 * BLKSZ + BLKSZ / 2).status,
                  SD_OK_TX_RDY);
        EXPECT_EQ(sd_read(sd_sdsc, BLKSZ / 2, SDTX_OK_COMPLETE), half);

        EXPECT_EQ(sd_cmd(sd_sdsc, 16, BLKSZ).status, SD_OK);
        EXPECT_EQ(sd_read_block(sd_sdsc, 8 * BLKSZ), data);
        sd_write_block(sd_sdsc, 8 * BLKSZ, vector<u8>(BLKSZ, 0));
    }

    void test_sd_block_addressing() {
        sd_init(sd_sdsc);
        sd_init(sd_sdhc);
        vector<u8> zero(BLKSZ, 0);
        vector<u8> data = pattern(BLKSZ, 0x55);

        sd_write_block(sd_sdhc, 2, zero);
        sd_write_block(sd_sdhc, 3, data);
        sd_write_block(sd_sdhc, 4, zero);
        EXPECT_EQ(sd_read_block(sd_sdhc, 2), zero);
        EXPECT_EQ(sd_read_block(sd_sdhc, 3), data);
        EXPECT_EQ(sd_read_block(sd_sdhc, 4), zero);

        u32 nblocks = 2 * GiB / BLKSZ;
        sd_write_block(sd_sdhc, nblocks - 1, data);
        EXPECT_EQ(sd_read_block(sd_sdhc, nblocks - 1), data);

        sd_command cmd = sd_cmd(sd_sdhc, 17, nblocks);
        EXPECT_TRUE(r1_status(cmd) & sd::card::OUT_OF_RANGE);
        cmd = sd_cmd(sd_sdsc, 17, 3);
        EXPECT_TRUE(r1_status(cmd) & sd::card::ADDRESS_ERROR);

        for (u8 opcode : { 28, 29, 30 })
            EXPECT_EQ(sd_cmd(sd_sdhc, opcode).status, SD_ERR_ILLEGAL);

        sd_write_block(sd_sdhc, 3, zero);
        sd_write_block(sd_sdhc, nblocks - 1, zero);
    }

    void test_sd_registers() {
        sd_init(sd_sdsc);

        EXPECT_EQ(sd_acmd(sd_sdsc, 51).status, SD_OK_TX_RDY);
        vector<u8> scr = sd_read_raw(sd_sdsc, 8);
        EXPECT_EQ(scr[0] >> 4, 0) << "wrong SCR structure version";
        EXPECT_EQ(scr[0] & 0xf, 2) << "wrong SD specification version";
        EXPECT_EQ(scr[1] & 0xf, 5) << "wrong SD bus widths";

        EXPECT_EQ(sd_acmd(sd_sdsc, 13).status, SD_OK_TX_RDY);
        EXPECT_EQ(sd_read_raw(sd_sdsc, 64), vector<u8>(64, 0));

        for (u32 arg : { 0x00ffffffu, 0x80ffffffu }) {
            EXPECT_EQ(sd_cmd(sd_sdsc, 6, arg).status, SD_OK_TX_RDY);
            vector<u8> swf = sd_read(sd_sdsc, 64, SDTX_OK_COMPLETE);
            EXPECT_EQ(swf[14], 0);
            EXPECT_EQ(swf[15], 0);
            EXPECT_EQ(swf[16], 0);
        }

        EXPECT_EQ(sd_acmd(sd_sdsc, 6, 0).status, SD_OK);
        EXPECT_EQ(sd_acmd(sd_sdsc, 6, 2).status, SD_OK);
        EXPECT_EQ(sd_acmd(sd_sdsc, 6, 0).status, SD_OK);
        EXPECT_EQ(sd_state(sd_sdsc), STATE_TRANSFER);
    }

    void test_sd_command_errors() {
        sd_init(sd_sdsc);

        sd_command cmd;
        sd_reset(cmd);
        cmd.opcode = 13;
        cmd.crc = sd_crc7(cmd) ^ 0x10;
        sd_sdsc.transport(cmd);
        EXPECT_EQ(cmd.status, SD_ERR_CRC);
        EXPECT_TRUE(r1_status(cmd) & sd::card::COM_CRC_ERROR);

        cmd = sd_cmd(sd_sdsc, 13);
        EXPECT_EQ(cmd.status, SD_OK);
        EXPECT_FALSE(r1_status(cmd) & sd::card::COM_CRC_ERROR);
        EXPECT_EQ(r1_state(cmd), STATE_TRANSFER);

        for (u8 opcode : { 1, 4, 5, 11, 14, 15, 19, 20, 23, 27, 32, 33, 38, 42,
                           52, 56, 58, 63 }) {
            cmd = sd_cmd(sd_sdsc, opcode);
            EXPECT_EQ(cmd.status, SD_ERR_ILLEGAL) << "CMD" << (int)opcode;
            EXPECT_TRUE(r1_status(cmd) & sd::card::ILLEGAL_COMMAND);
        }

        for (u8 opcode : { 23, 41, 42, 50 }) {
            cmd = sd_acmd(sd_sdsc, opcode, OCR_VOLTAGES);
            EXPECT_EQ(cmd.status, SD_ERR_ILLEGAL) << "ACMD" << (int)opcode;
            EXPECT_TRUE(r1_status(cmd) & sd::card::ILLEGAL_COMMAND);
        }

        cmd = sd_cmd(sd_sdsc, 13);
        EXPECT_EQ(cmd.status, SD_OK);
        EXPECT_FALSE(r1_status(cmd) & sd::card::ILLEGAL_COMMAND);
        EXPECT_EQ(r1_state(cmd), STATE_TRANSFER);
    }

    void test_sd_data_errors() {
        sd_init(sd_sdsc);
        vector<u8> data = pattern(BLKSZ, 0x66);
        vector<u8> zero(BLKSZ, 0);
        u8 val = 0;

        sd_write_block(sd_sdsc, 10 * BLKSZ, zero);

        for (u8 opcode : { 17, 18 }) {
            sd_command cmd = sd_cmd(sd_sdsc, opcode, 10 * BLKSZ + 3);
            EXPECT_TRUE(r1_status(cmd) & sd::card::ADDRESS_ERROR);
            EXPECT_EQ(sd_sdsc.read_data(val), SDTX_ERR_ILLEGAL);

            cmd = sd_cmd(sd_sdsc, opcode, SDSC_CAPACITY);
            EXPECT_TRUE(r1_status(cmd) & sd::card::OUT_OF_RANGE);
            EXPECT_EQ(sd_sdsc.read_data(val), SDTX_ERR_ILLEGAL);
        }

        for (u8 opcode : { 24, 25 }) {
            sd_command cmd = sd_cmd(sd_sdsc, opcode, 10 * BLKSZ + 3);
            EXPECT_TRUE(r1_status(cmd) & sd::card::ADDRESS_ERROR);
            EXPECT_EQ(sd_sdsc.write_data(val), SDRX_ERR_ILLEGAL);

            cmd = sd_cmd(sd_sdsc, opcode, SDSC_CAPACITY);
            EXPECT_TRUE(r1_status(cmd) & sd::card::OUT_OF_RANGE);
            EXPECT_EQ(sd_sdsc.write_data(val), SDRX_ERR_ILLEGAL);
        }

        sd_command cmd = sd_cmd(sd_sdsc, 13);
        EXPECT_FALSE(r1_status(cmd) & sd::card::ADDRESS_ERROR);
        EXPECT_FALSE(r1_status(cmd) & sd::card::OUT_OF_RANGE);

        EXPECT_EQ(sd_cmd(sd_sdsc, 24, 10 * BLKSZ).status, SD_OK_RX_RDY);
        EXPECT_EQ(sd_write(sd_sdsc, data, 0x0001), SDRX_ERR_CRC);
        EXPECT_EQ(sd_read_block(sd_sdsc, 10 * BLKSZ), zero);
    }

    u8 spi_xfer(u8 mosi = SPI_IDLE) {
        spi_payload tx(mosi);
        spi.transport(tx);
        return tx.miso;
    }

    vector<u8> spi_cmd(u8 opcode, u32 arg, size_t resp_len, u8 crc_error = 0) {
        sd_command cmd;
        sd_reset(cmd);
        cmd.opcode = opcode;
        cmd.argument = arg;

        EXPECT_EQ(spi_xfer(0x40 | opcode), SPI_IDLE);
        EXPECT_EQ(spi_xfer(arg >> 24), SPI_IDLE);
        EXPECT_EQ(spi_xfer(arg >> 16), SPI_IDLE);
        EXPECT_EQ(spi_xfer(arg >> 8), SPI_IDLE);
        EXPECT_EQ(spi_xfer(arg >> 0), SPI_IDLE);
        EXPECT_EQ(spi_xfer(sd_crc7(cmd) ^ crc_error), SPI_IDLE);
        EXPECT_EQ(spi_xfer(), SPI_IDLE);

        vector<u8> resp(resp_len);
        for (u8& val : resp)
            val = spi_xfer();
        EXPECT_EQ(spi_xfer(), SPI_IDLE) << "response too long";
        return resp;
    }

    u8 spi_r1(u8 opcode, u32 arg = 0) { return spi_cmd(opcode, arg, 1)[0]; }

    u8 spi_acmd_r1(u8 opcode, u32 arg = 0) {
        spi_r1(55);
        return spi_r1(opcode, arg);
    }

    void spi_init() {
        cs = false;
        EXPECT_EQ(spi_r1(0), sd::card::SPI_IN_IDLE);
        EXPECT_EQ(spi_cmd(8, IF_COND, 5)[0], sd::card::SPI_IN_IDLE);
        EXPECT_EQ(spi_r1(59, 1), sd::card::SPI_IN_IDLE);
        EXPECT_EQ(spi_r1(55), sd::card::SPI_IN_IDLE);
        EXPECT_EQ(spi_r1(41, OCR_HCS), 0);
    }

    vector<u8> spi_read_raw(size_t len) {
        vector<u8> data(len);
        EXPECT_EQ(spi_xfer(), SPI_START_BLOCK);
        for (u8& val : data)
            val = spi_xfer();
        return data;
    }

    vector<u8> spi_read(size_t len = BLKSZ) {
        vector<u8> data = spi_read_raw(len);
        u16 crc = (u16)(spi_xfer() << 8);
        crc |= spi_xfer();
        EXPECT_EQ(crc, crc16(data.data(), len));
        EXPECT_EQ(spi_xfer(), SPI_IDLE);
        return data;
    }

    u8 spi_write(u8 token, const vector<u8>& data, u16 crc_error = 0) {
        u16 crc = crc16(data.data(), data.size()) ^ crc_error;
        EXPECT_EQ(spi_xfer(token), SPI_IDLE);
        for (u8 val : data)
            EXPECT_EQ(spi_xfer(val), SPI_IDLE);
        EXPECT_EQ(spi_xfer(crc >> 8), SPI_IDLE);
        EXPECT_EQ(spi_xfer(crc & 0xff), SPI_IDLE);
        return spi_xfer();
    }

    vector<u8> spi_read_block(u32 addr) {
        EXPECT_EQ(spi_r1(17, addr), 0);
        return spi_read();
    }

    void spi_write_block(u32 addr, const vector<u8>& data) {
        EXPECT_EQ(spi_r1(24, addr), 0);
        EXPECT_EQ(spi_write(SPI_START_BLOCK, data), SPI_DATA_ACCEPTED);
    }

    void test_spi_initialization() {
        cs = false;
        EXPECT_EQ(spi_r1(0), sd::card::SPI_IN_IDLE);

        EXPECT_EQ(spi_cmd(8, IF_COND, 5),
                  vector<u8>({ sd::card::SPI_IN_IDLE, 0, 0, 0x01, 0xaa }));

        vector<u8> ocr = spi_cmd(58, 0, 5);
        EXPECT_EQ(ocr[0], sd::card::SPI_IN_IDLE);
        EXPECT_EQ(ocr[1] & 0x40, 0) << "wrong CCS";
        EXPECT_EQ(ocr[2], 0xff);
        EXPECT_EQ(ocr[3], 0x80);

        EXPECT_EQ(spi_r1(1), sd::card::SPI_IN_IDLE);
        EXPECT_EQ(spi_r1(59, 1), sd::card::SPI_IN_IDLE);
        EXPECT_EQ(spi_r1(55), sd::card::SPI_IN_IDLE);
        EXPECT_EQ(spi_r1(41, OCR_HCS), 0);
        EXPECT_EQ(spi_r1(1), 0);

        EXPECT_EQ(spi_cmd(13, 0, 2), vector<u8>({ 0, 0 }));
        cs = true;
    }

    void test_spi_registers() {
        spi_init();

        EXPECT_EQ(spi_r1(9), 0);
        vector<u8> csd = spi_read_raw(16);
        EXPECT_EQ(spi_xfer(), SPI_IDLE);
        u64 read_bl_len = csd[5] & 0xf;
        u64 c_size = (csd[6] & 3u) << 10 | csd[7] << 2 | csd[8] >> 6;
        u64 c_size_mult = (csd[9] & 3u) << 1 | csd[10] >> 7;
        EXPECT_EQ((c_size + 1) << (c_size_mult + 2 + read_bl_len),
                  SDSC_CAPACITY);

        EXPECT_EQ(spi_r1(10), 0);
        vector<u8> cid = spi_read_raw(16);
        EXPECT_EQ(spi_xfer(), SPI_IDLE);
        EXPECT_EQ(cid[0], 0xbb);
        EXPECT_EQ(string((char*)cid.data() + 1, 7), "JWVCML ");

        EXPECT_EQ(spi_acmd_r1(51), 0);
        vector<u8> scr = spi_read_raw(8);
        EXPECT_EQ(spi_xfer(), SPI_IDLE);
        EXPECT_EQ(scr[0] & 0xf, 2) << "wrong SD specification version";

        spi_r1(55);
        EXPECT_EQ(spi_cmd(13, 0, 2), vector<u8>({ 0, 0 }));
        EXPECT_EQ(spi_read_raw(64), vector<u8>(64, 0));
        EXPECT_EQ(spi_xfer(), SPI_IDLE);

        EXPECT_EQ(spi_r1(6, 0x00ffffff), 0);
        vector<u8> swf = spi_read(64);
        EXPECT_EQ(swf[16], 0);

        EXPECT_EQ(spi_cmd(13, 0, 2), vector<u8>({ 0, 0 }));
        cs = true;
    }

    void test_spi_single_block() {
        spi_init();
        vector<u8> zero(BLKSZ, 0);
        vector<u8> data = pattern(BLKSZ, 0x77);

        spi_write_block(1 * BLKSZ, zero);
        spi_write_block(2 * BLKSZ, data);
        spi_write_block(3 * BLKSZ, zero);

        EXPECT_EQ(spi_read_block(1 * BLKSZ), zero);
        EXPECT_EQ(spi_read_block(2 * BLKSZ), data);
        EXPECT_EQ(spi_read_block(3 * BLKSZ), zero);

        for (int i = 0; i < 4; i++)
            EXPECT_EQ(spi_xfer(), SPI_IDLE);

        spi_write_block(2 * BLKSZ, zero);
        EXPECT_EQ(spi_cmd(13, 0, 2), vector<u8>({ 0, 0 }));
        cs = true;
    }

    void test_spi_multi_block() {
        spi_init();
        vector<u8> zero(BLKSZ, 0);
        vector<u8> blk0 = pattern(BLKSZ, 0x88);
        vector<u8> blk1 = pattern(BLKSZ, 0x99);

        spi_write_block(6 * BLKSZ, zero);

        EXPECT_EQ(spi_r1(25, 4 * BLKSZ), 0);
        EXPECT_EQ(spi_xfer(), SPI_IDLE);
        EXPECT_EQ(spi_write(SPI_START_MULTI, blk0), SPI_DATA_ACCEPTED);
        EXPECT_EQ(spi_xfer(), SPI_IDLE);
        EXPECT_EQ(spi_write(SPI_START_MULTI, blk1), SPI_DATA_ACCEPTED);
        EXPECT_EQ(spi_xfer(SPI_STOP_MULTI), SPI_IDLE);

        EXPECT_EQ(spi_xfer(SPI_START_MULTI), SPI_IDLE);
        for (size_t i = 0; i < BLKSZ + 3; i++)
            EXPECT_EQ(spi_xfer(), SPI_IDLE);
        EXPECT_EQ(spi_cmd(13, 0, 2), vector<u8>({ 0, 0 }));

        EXPECT_EQ(spi_r1(18, 4 * BLKSZ), 0);
        EXPECT_EQ(spi_read(), blk0);
        EXPECT_EQ(spi_read(), blk1);
        EXPECT_EQ(spi_read(), zero);
        EXPECT_EQ(spi_r1(12), 0);
        EXPECT_EQ(spi_xfer(), SPI_IDLE);

        EXPECT_EQ(spi_r1(25, 4 * BLKSZ), 0);
        EXPECT_EQ(spi_write(SPI_START_MULTI, zero), SPI_DATA_ACCEPTED);
        EXPECT_EQ(spi_r1(24, 5 * BLKSZ), 0);
        EXPECT_EQ(spi_write(SPI_START_BLOCK, zero), SPI_DATA_ACCEPTED);

        EXPECT_EQ(spi_read_block(4 * BLKSZ), zero);
        EXPECT_EQ(spi_read_block(5 * BLKSZ), zero);
        EXPECT_EQ(spi_cmd(13, 0, 2), vector<u8>({ 0, 0 }));
        cs = true;
    }

    void test_spi_crc_off() {
        spi_init();
        vector<u8> data = pattern(BLKSZ, 0xaa);
        u16 crc = crc16(data.data(), BLKSZ);
        spi_write_block(8 * BLKSZ, data);

        EXPECT_EQ(spi_r1(59, 0), 0);
        EXPECT_EQ(spi_r1(17, 8 * BLKSZ), 0);
        EXPECT_EQ(spi_read_raw(BLKSZ), data);
        u16 nocrc = (u16)(spi_xfer() << 8);
        nocrc |= spi_xfer();
        EXPECT_NE(nocrc, crc);
        EXPECT_EQ(spi_xfer(), SPI_IDLE);

        EXPECT_EQ(spi_r1(59, 1), 0);
        EXPECT_EQ(spi_read_block(8 * BLKSZ), data);

        spi_write_block(8 * BLKSZ, vector<u8>(BLKSZ, 0));
        cs = true;
    }

    void test_spi_errors() {
        spi_init();

        for (u8 opcode : { 2, 3, 7, 23, 63 }) {
            EXPECT_EQ(spi_r1(opcode), sd::card::SPI_ILLEGAL_COMMAND)
                << "CMD" << (int)opcode;
        }

        EXPECT_EQ(spi_acmd_r1(6), sd::card::SPI_ILLEGAL_COMMAND);
        EXPECT_EQ(spi_cmd(13, 0, 1, 0x10)[0], sd::card::SPI_COM_CRC_ERROR);
        EXPECT_EQ(spi_cmd(13, 0, 2), vector<u8>({ 0, 0 }));

        EXPECT_EQ(spi_r1(17, SDSC_CAPACITY), sd::card::SPI_PARAMETER_ERROR);
        EXPECT_EQ(spi_xfer(), SPI_START_BLOCK);
        EXPECT_EQ(spi_xfer(), SPI_DATA_ERROR);

        EXPECT_EQ(spi_r1(17, 3), sd::card::SPI_ADDRESS_ERROR);
        EXPECT_EQ(spi_xfer(), SPI_START_BLOCK);
        EXPECT_EQ(spi_xfer(), SPI_DATA_ERROR);

        EXPECT_EQ(spi_cmd(13, 0, 2), vector<u8>({ 0, 0 }));
        EXPECT_EQ(spi_read_block(0).size(), BLKSZ);
        cs = true;
    }

    void test_spi_write_crc_error() {
        spi_init();
        vector<u8> zero(BLKSZ, 0);
        vector<u8> data = pattern(BLKSZ, 0xbb);
        spi_write_block(12 * BLKSZ, zero);
        spi_write_block(13 * BLKSZ, zero);

        EXPECT_EQ(spi_r1(24, 12 * BLKSZ), 0);
        EXPECT_EQ(spi_write(SPI_START_BLOCK, data, 0x0001),
                  SPI_DATA_CRC_ERROR);
        for (int i = 0; i < 4; i++)
            EXPECT_EQ(spi_xfer(), SPI_IDLE);
        EXPECT_EQ(spi_cmd(13, 0, 2), vector<u8>({ 0, 0 }));
        EXPECT_EQ(spi_read_block(12 * BLKSZ), zero);

        EXPECT_EQ(spi_r1(25, 12 * BLKSZ), 0);
        EXPECT_EQ(spi_write(SPI_START_MULTI, data), SPI_DATA_ACCEPTED);
        EXPECT_EQ(spi_write(SPI_START_MULTI, data, 0x8000),
                  SPI_DATA_CRC_ERROR);
        EXPECT_EQ(spi_xfer(SPI_STOP_MULTI), SPI_IDLE);
        EXPECT_EQ(spi_xfer(), SPI_IDLE);
        EXPECT_EQ(spi_cmd(13, 0, 2), vector<u8>({ 0, 0 }));
        EXPECT_EQ(spi_read_block(12 * BLKSZ), data);
        EXPECT_EQ(spi_read_block(13 * BLKSZ), zero);

        spi_write_block(13 * BLKSZ, data);
        EXPECT_EQ(spi_read_block(13 * BLKSZ), data);

        spi_write_block(12 * BLKSZ, zero);
        spi_write_block(13 * BLKSZ, zero);
        EXPECT_EQ(spi_cmd(13, 0, 2), vector<u8>({ 0, 0 }));
        cs = true;
    }

    void test_spi_chip_select() {
        spi_init();
        cs = true;

        for (u8 mosi : { 0x40, 0x00, 0x00, 0x00, 0x00, 0x95, 0xff, 0xff }) {
            spi_payload tx(mosi, 0xab);
            spi.transport(tx);
            EXPECT_EQ(tx.miso, 0xab);
        }

        cs = false;
        EXPECT_EQ(spi_cmd(13, 0, 2), vector<u8>({ 0, 0 }));
        cs = true;
    }
};

TEST(sd, card) {
    sd_card_bench test("bench");
    EXPECT_STREQ(test.sdsc.kind(), "vcml::sd::card");
    EXPECT_STREQ(test.bridge.kind(), "vcml::spi::spi2sd");
    sc_core::sc_start();
}
