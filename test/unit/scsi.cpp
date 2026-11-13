/******************************************************************************
 *                                                                            *
 * Copyright (C) 2025 MachineWare GmbH                                        *
 * All Rights Reserved                                                        *
 *                                                                            *
 * This work is licensed under the terms described in the LICENSE file found  *
 * in the root directory of this source tree.                                 *
 *                                                                            *
 ******************************************************************************/

#include <gtest/gtest.h>
#include "vcml.h"

using namespace vcml;
using namespace vcml::audio;

TEST(scsi, transfer) {
    EXPECT_TRUE(block::scsi_command_transfers_to_device(0x2a));
    EXPECT_FALSE(block::scsi_command_transfers_from_device(0x2a));
    EXPECT_FALSE(block::scsi_command_transfers_to_device(0x28));
    EXPECT_TRUE(block::scsi_command_transfers_from_device(0x28));
    EXPECT_TRUE(block::scsi_command_transfers_to_device(0x41));
    EXPECT_TRUE(block::scsi_command_transfers_to_device(0x93));
}

TEST(scsi, strings) {
    block::scsi_disk disk("disk");
    EXPECT_STREQ(disk.kind(), "vcml::block::scsi_disk");

    for (int i = 0; i < 0xff; i++) {
        EXPECT_NE(block::scsi_command_str(i), nullptr);
        EXPECT_NE(block::scsi_response_str(i), nullptr);
    }
}

TEST(scsi, inquire) {
    block::scsi_disk disk("disk");
    block::scsi_request req{};
    req.command[0] = 0x12;
    ASSERT_TRUE(success(disk.scsi_handle_command(req)));
    EXPECT_EQ(disk.get_sense(), block::SENSE_NOTHING);
    ASSERT_EQ(req.payload.size(), 36);

    char vendor[8]{};
    memcpy(vendor, req.payload.data() + 8, 8);
    EXPECT_STREQ(vendor, "MWARE  ");

    char product[16]{};
    memcpy(product, req.payload.data() + 16, 16);
    EXPECT_STREQ(product, "VCML-SCSIDRIVE ");

    char revision[16]{};
    memcpy(revision, req.payload.data() + 32, 4);
    EXPECT_STREQ(revision, "1.0");
}

TEST(scsi, mode_sense) {
    block::scsi_disk disk("disk");
    block::scsi_request req{};
    req.command[0] = 0x1a;
    req.command[2] = 0x3f;
    ASSERT_TRUE(success(disk.scsi_handle_command(req)));
    EXPECT_FALSE(req.payload.empty());

    req.command[0] = 0x1a;
    req.command[1] = 0x08;
    req.command[2] = 0x08;
    ASSERT_TRUE(success(disk.scsi_handle_command(req)));
    EXPECT_FALSE(req.payload.empty());
}

TEST(scsi, read_sense) {
    block::scsi_disk disk("disk");
    disk.set_sense(block::SENSE_MEDIUM_ERROR);
    EXPECT_NE(disk.get_sense(), block::SENSE_NOTHING);

    block::scsi_request req{};
    req.command[0] = 0x03;
    ASSERT_TRUE(success(disk.scsi_handle_command(req)));
    EXPECT_EQ(req.payload.size(), 18);

    EXPECT_EQ(req.payload[2], block::SENSE_MEDIUM_ERROR.key);
    EXPECT_EQ(req.payload[12], block::SENSE_MEDIUM_ERROR.asc);
    EXPECT_EQ(req.payload[13], block::SENSE_MEDIUM_ERROR.ascq);
}

TEST(scsi, read_capacity) {
    block::scsi_disk disk("disk");
    block::scsi_request req{};
    req.command[0] = 0x25;
    ASSERT_TRUE(success(disk.scsi_handle_command(req)));
    EXPECT_EQ(req.payload.size(), 8);

    u32 bsz = block::scsi_read(&req.payload[4], 4);
    u32 lba = block::scsi_read(&req.payload[0], 4);

    EXPECT_EQ(disk.blocksize(), bsz);
    EXPECT_EQ(udivup(disk.capacity(), disk.blocksize()) - 1, lba);
}

TEST(scsi, read_write) {
    block::scsi_disk disk("disk");
    block::scsi_request req_w{};
    req_w.command[0] = 0x2a; // write10
    req_w.command[5] = 0x02; // offset 1024
    req_w.command[8] = 0x01; // size 512
    req_w.payload.assign(512, 0xab);
    ASSERT_TRUE(success(disk.scsi_handle_command(req_w)));

    block::scsi_request req_f{};
    req_f.command[0] = 0x35; // flush
    ASSERT_TRUE(success(disk.scsi_handle_command(req_f)));

    block::scsi_request req_r{};
    req_r.command[0] = 0x28; // read10
    req_r.command[5] = 0x02; // offset 1024
    req_r.command[8] = 0x01; // size 512
    ASSERT_TRUE(success(disk.scsi_handle_command(req_r)));

    EXPECT_EQ(req_w.payload, req_r.payload);
}

TEST(scsi, block_limits) {
    block::scsi_disk disk("disk");
    block::scsi_request req{};
    req.command[0] = 0x12; // inquiry
    req.command[1] = 0x01; // vital product data
    req.command[2] = 0xb0; // block limits
    ASSERT_TRUE(success(disk.scsi_handle_command(req)));
    ASSERT_EQ(req.payload.size(), 64);
    EXPECT_EQ(req.payload[1], 0xb0);
    EXPECT_EQ(req.payload[3], 60);

    // maximum and optimal transfer length are 1MiB
    u32 blocks = MiB / disk.blocksize();
    const vector<u8> length = {
        (u8)(blocks >> 24),
        (u8)(blocks >> 16),
        (u8)(blocks >> 8),
        (u8)(blocks >> 0),
    };

    auto it = req.payload.begin();
    EXPECT_EQ(vector<u8>(it + 8, it + 12), length);
    EXPECT_EQ(vector<u8>(it + 12, it + 16), length);
}

TEST(scsi, write_same) {
    block::scsi_disk disk("disk");
    block::scsi_request req_w{};
    req_w.command[0] = 0x41; // write same 10
    req_w.command[5] = 0x02; // lba 2
    req_w.command[8] = 0x03; // 3 blocks
    req_w.payload.assign(512, 0xab);
    ASSERT_TRUE(success(disk.scsi_handle_command(req_w)));

    block::scsi_request req_r{};
    req_r.command[0] = 0x28; // read10
    req_r.command[5] = 0x01; // lba 1
    req_r.command[8] = 0x05; // 5 blocks
    ASSERT_TRUE(success(disk.scsi_handle_command(req_r)));

    vector<u8> expect(5 * 512, 0x00);
    std::fill(expect.begin() + 512, expect.begin() + 4 * 512, 0xab);
    EXPECT_EQ(req_r.payload, expect);

    // an all zero block clears the range again
    req_w.payload.assign(512, 0x00);
    ASSERT_TRUE(success(disk.scsi_handle_command(req_w)));
    ASSERT_TRUE(success(disk.scsi_handle_command(req_r)));
    EXPECT_EQ(req_r.payload, vector<u8>(5 * 512, 0x00));
}

TEST(scsi, illegal_command) {
    block::scsi_disk disk("disk");
    block::scsi_request req{};
    req.command[0] = 0xee;
    ASSERT_TRUE(failed(disk.scsi_handle_command(req)));
    EXPECT_EQ(disk.get_sense(), block::SENSE_ILLEGAL_REQ);

    block::scsi_request req_s{};
    req_s.command[0] = 0x03;
    ASSERT_TRUE(success(disk.scsi_handle_command(req_s)));
    EXPECT_EQ(req_s.payload.size(), 18);

    EXPECT_EQ(req_s.payload[2], block::SENSE_ILLEGAL_REQ.key);
    EXPECT_EQ(req_s.payload[12], block::SENSE_ILLEGAL_REQ.asc);
    EXPECT_EQ(req_s.payload[13], block::SENSE_ILLEGAL_REQ.ascq);
}
