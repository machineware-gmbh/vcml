/******************************************************************************
 *                                                                            *
 * Copyright (C) 2026 MachineWare GmbH                                        *
 * All Rights Reserved                                                        *
 *                                                                            *
 * This work is licensed under the terms described in the LICENSE file found  *
 * in the root directory of this source tree.                                 *
 *                                                                            *
 ******************************************************************************/

#include "usb_test.h"

class usb_drive_test : public test_base
{
public:
    enum : u32 {
        CBW_SIG = 0x43425355,
        CSW_SIG = 0x53425355,
        CBW_SIZE = 31,
        CSW_SIZE = 13,
        EP_IN = 1,
        EP_OUT = 2,
        CONFIG = 11,
        BLKSZ = 512,
        NBLKS = 2048,
        STS_PASSED = 0,
        STS_FAILED = 1,
        STS_PHASE = 2,
    };

    enum : u16 {
        REQ_GET_MAX_LUN = USB_REQ_IN | USB_REQ_CLASS | USB_REQ_IFACE | 0xfe,
        REQ_BOT_RESET = USB_REQ_CLASS | USB_REQ_IFACE | 0xff,
    };

    usb_stim stim;
    usb_stim stim_ro;
    usb_stim stim_bad;

    usb::drive drive;
    usb::drive drive_ro;
    usb::drive drive_bad;

    vcml::model drive_default;
    vcml::model drive_image;

    u32 m_tag;
    u32 m_residue;

    static void put32(vector<u8>& v, size_t off, u32 val) {
        for (size_t i = 0; i < 4; i++)
            v[off + i] = (u8)(val >> (8 * i));
    }

    static u32 get32(const u8* p) {
        return (u32)p[0] | (u32)p[1] << 8 | (u32)p[2] << 16 | (u32)p[3] << 24;
    }

    static u64 get_be(const u8* p, size_t n) {
        u64 val = 0;
        for (size_t i = 0; i < n; i++)
            val = val << 8 | p[i];
        return val;
    }

    static vector<u8> cdb_rw10(u8 op, u32 lba, u16 nblks) {
        return { op,      0, (u8)(lba >> 24),  (u8)(lba >> 16), (u8)(lba >> 8),
                 (u8)lba, 0, (u8)(nblks >> 8), (u8)nblks,       0 };
    }

    static vector<u8> cdb_read(u32 lba, u16 nblks) {
        return cdb_rw10(block::SCSI_READ_10, lba, nblks);
    }

    static vector<u8> cdb_write(u32 lba, u16 nblks) {
        return cdb_rw10(block::SCSI_WRITE_10, lba, nblks);
    }

    static vector<u8> cdb6(u8 op, u8 b1 = 0, u8 b2 = 0, u8 b4 = 0) {
        return { op, b1, b2, 0, b4, 0 };
    }

    static vector<u8> pattern(size_t len, u8 seed) {
        vector<u8> data(len);
        for (size_t i = 0; i < len; i++)
            data[i] = (u8)(seed + i * 7 + (i >> 8));
        return data;
    }

    static vector<u8> make_cbw(u32 tag, u32 len, bool is_in,
                               const vector<u8>& cdb, u8 lun = 0) {
        vector<u8> cbw(CBW_SIZE, 0);
        put32(cbw, 0, CBW_SIG);
        put32(cbw, 4, tag);
        put32(cbw, 8, len);
        cbw[12] = is_in ? 0x80 : 0x00;
        cbw[13] = lun;
        cbw[14] = (u8)cdb.size();
        std::copy(cdb.begin(), cdb.end(), cbw.begin() + 15);
        return cbw;
    }

    u32 next_tag() { return 0xa5000000 + ++m_tag; }

    u8 read_csw(usb_stim& s, u32 tag) {
        vector<u8> csw;
        EXPECT_EQ(s.in(EP_IN, csw, CSW_SIZE), USB_RESULT_SUCCESS);
        EXPECT_EQ(get32(csw.data() + 0), CSW_SIG);
        EXPECT_EQ(get32(csw.data() + 4), tag);
        m_residue = get32(csw.data() + 8);
        return csw[12];
    }

    u8 transfer(usb_stim& s, const vector<u8>& cdb, bool is_in,
                vector<u8>& data, size_t len, size_t pktsz = BLKSZ) {
        u32 tag = next_tag();
        EXPECT_EQ(s.out(EP_OUT, make_cbw(tag, (u32)len, is_in, cdb)),
                  USB_RESULT_SUCCESS);

        if (is_in)
            data.assign(len, 0x55);

        for (size_t pos = 0; pos < len; pos += pktsz) {
            size_t n = min(pktsz, len - pos);
            usb_result res = is_in ? s.in(EP_IN, data.data() + pos, n)
                                   : s.out(EP_OUT, data.data() + pos, n);
            EXPECT_EQ(res, USB_RESULT_SUCCESS) << "at offset " << pos;
        }

        return read_csw(s, tag);
    }

    u8 command_in(usb_stim& s, const vector<u8>& cdb, vector<u8>& data,
                  size_t len, size_t pktsz = BLKSZ) {
        return transfer(s, cdb, true, data, len, pktsz);
    }

    u8 command_out(usb_stim& s, const vector<u8>& cdb, vector<u8> data,
                   size_t pktsz = BLKSZ) {
        return transfer(s, cdb, false, data, data.size(), pktsz);
    }

    u8 command(usb_stim& s, const vector<u8>& cdb, bool is_in = false) {
        vector<u8> data;
        return transfer(s, cdb, is_in, data, 0);
    }

    block::scsi_sense request_sense(usb_stim& s) {
        vector<u8> data;
        EXPECT_EQ(
            command_in(s, cdb6(block::SCSI_REQUEST_SENSE, 0, 0, 18), data, 18),
            STS_PASSED);
        EXPECT_EQ(m_residue, 0u);
        return { (u8)(data[2] & 0xf), data[12], data[13] };
    }

    vector<u8> disk_read(usb::drive& d, size_t off, size_t len) {
        vector<u8> data(len, 0);
        EXPECT_TRUE(d.disk.seek(off));
        EXPECT_TRUE(d.disk.read(data.data(), len));
        return data;
    }

    void disk_write(usb::drive& d, size_t off, const vector<u8>& data) {
        EXPECT_TRUE(d.disk.seek(off));
        EXPECT_TRUE(d.disk.write(data.data(), data.size()));
    }

    static usb::drive* as_drive(vcml::model& m) {
        return dynamic_cast<usb::drive*>(m.operator->().get());
    }

    void enumerate(usb_stim& s) {
        ASSERT_EQ(s.set_address(1), USB_RESULT_SUCCESS);
        ASSERT_EQ(s.set_configuration(CONFIG), USB_RESULT_SUCCESS);
    }

    usb_drive_test(const sc_module_name& nm):
        test_base(nm),
        stim("stim"),
        stim_ro("stim_ro"),
        stim_bad("stim_bad"),
        drive("drive"),
        drive_ro("drive_ro"),
        drive_bad("drive_bad"),
        drive_default("drive_default", "vcml::usb::drive"),
        drive_image("drive_image", "vcml::usb::drive ramdisk:64KiB"),
        m_tag(0),
        m_residue(0) {
        stim.usb_out.bind(drive.usb_in);
        stim_ro.usb_out.bind(drive_ro.usb_in);
        stim_bad.usb_out.bind(drive_bad.usb_in);
        as_drive(drive_default)->usb_in.stub();
        as_drive(drive_image)->usb_in.stub();

        add_test("strings", &usb_drive_test::test_strings);
        add_test("properties", &usb_drive_test::test_properties);
        add_test("descriptors", &usb_drive_test::test_descriptors);
        add_test("descriptors_usb2", &usb_drive_test::test_descriptors_usb2);
        add_test("class_requests", &usb_drive_test::test_class_requests);
        add_test("scsi_info", &usb_drive_test::test_scsi_info);
        add_test("scsi_misc", &usb_drive_test::test_scsi_misc);
        add_test("read_write", &usb_drive_test::test_read_write);
        add_test("read_write_split", &usb_drive_test::test_read_write_split);
        add_test("end_of_medium", &usb_drive_test::test_end_of_medium);
        add_test("readonly", &usb_drive_test::test_readonly);
        add_test("no_medium", &usb_drive_test::test_no_medium);
        add_test("invalid_cbw", &usb_drive_test::test_invalid_cbw);
        add_test("invalid_sequence", &usb_drive_test::test_invalid_sequence);
        add_test("residue", &usb_drive_test::test_residue);
        add_test("direction", &usb_drive_test::test_direction);
        add_test("bot_reset", &usb_drive_test::test_bot_reset);
        add_test("device_reset", &usb_drive_test::test_device_reset);
    }

    virtual ~usb_drive_test() = default;

    void test_strings() {
        EXPECT_STREQ(drive.kind(), "vcml::usb::drive");
        EXPECT_STREQ(drive.version(), VCML_VERSION_STRING);
        EXPECT_STREQ(drive_default->kind(), "vcml::usb::drive");
        EXPECT_STREQ(drive_image->kind(), "vcml::usb::drive");
    }

    void test_properties() {
        EXPECT_TRUE(drive.usb3);
        EXPECT_EQ(drive.vendorid.get(), USB_VENDOR_VCML);
        EXPECT_EQ(drive.productid.get(), 1);
        EXPECT_EQ(drive.manufacturer.get(), "MachineWare GmbH");
        EXPECT_EQ(drive.product.get(), "VCML Flash Drive");
        EXPECT_EQ(drive.serialno.get(), VCML_GIT_REV_SHORT);
        EXPECT_EQ(drive.image.get(), "ramdisk:1MiB");
        EXPECT_FALSE(drive.readonly);
        EXPECT_FALSE(drive.writeignore);
        EXPECT_TRUE(drive.disk.has_backing());
        EXPECT_EQ(drive.disk.capacity(), NBLKS * BLKSZ);
        EXPECT_TRUE(drive.disk.removable);

        EXPECT_FALSE(drive_ro.usb3);
        EXPECT_TRUE(drive_ro.readonly);
        EXPECT_TRUE(drive_ro.disk.readonly);
        EXPECT_EQ(drive_ro.disk.capacity(), 64 * KiB);

        EXPECT_FALSE(drive_bad.disk.has_backing());
        EXPECT_EQ(drive_bad.disk.capacity(), 0u);

        auto* def = dynamic_cast<usb::drive*>(
            drive_default.operator->().get());
        ASSERT_NE(def, nullptr);
        EXPECT_EQ(def->image.get(), "ramdisk:512MiB");
        EXPECT_EQ(def->disk.capacity(), 512 * MiB);
        EXPECT_FALSE(def->readonly);

        auto* img = as_drive(drive_image);
        ASSERT_NE(img, nullptr);
        EXPECT_EQ(img->image.get(), "ramdisk:64KiB");
        EXPECT_EQ(img->disk.capacity(), 64 * KiB);
        EXPECT_FALSE(img->readonly);
        EXPECT_FALSE(img->writeignore);

        EXPECT_EQ(stim.attached, 1u);
        EXPECT_EQ(stim.usb_out.connection_speed(), USB_SPEED_SUPER);
        EXPECT_EQ(stim_ro.attached, 1u);
        EXPECT_EQ(stim_ro.usb_out.connection_speed(), USB_SPEED_HIGH);
    }

    void check_descriptors(usb_stim& s, usb::drive& d, size_t mps) {
        bool usb3 = d.usb3;
        vector<u8> desc;
        ASSERT_EQ(s.get_descriptor(USB_DT_DEVICE, 0, desc, 18),
                  USB_RESULT_SUCCESS);
        EXPECT_EQ(desc[0], 18);
        EXPECT_EQ(desc[1], USB_DT_DEVICE);
        EXPECT_EQ(desc[2] | desc[3] << 8, usb3 ? 0x300 : 0x200);
        EXPECT_EQ(desc[4], 0);
        EXPECT_EQ(desc[5], 0);
        EXPECT_EQ(desc[6], 0);
        EXPECT_EQ(desc[7], usb3 ? 9 : 64);
        EXPECT_EQ(desc[8] | desc[9] << 8, d.vendorid.get());
        EXPECT_EQ(desc[10] | desc[11] << 8, d.productid.get());
        EXPECT_EQ(desc[14], 1);
        EXPECT_EQ(desc[15], 2);
        EXPECT_EQ(desc[16], 3);
        EXPECT_EQ(desc[17], 1);

        size_t total = 9 + 9 + 2 * (usb3 ? 13 : 7);
        ASSERT_EQ(s.get_descriptor(USB_DT_CONFIG, 0, desc, 9),
                  USB_RESULT_SUCCESS);
        EXPECT_EQ(desc[0], 9);
        EXPECT_EQ(desc[1], USB_DT_CONFIG);
        EXPECT_EQ((size_t)(desc[2] | desc[3] << 8), total);
        EXPECT_EQ(desc[4], 1);
        EXPECT_EQ(desc[5], CONFIG);
        EXPECT_EQ(desc[7], USB_CFG_ONE | USB_CFG_REMOTE_WAKEUP);
        EXPECT_EQ(desc[8], 40);

        ASSERT_EQ(s.get_descriptor(USB_DT_CONFIG, 0, desc, total),
                  USB_RESULT_SUCCESS);
        const u8* ifx = desc.data() + 9;
        EXPECT_EQ(ifx[0], 9);
        EXPECT_EQ(ifx[1], USB_DT_INTERFACE);
        EXPECT_EQ(ifx[2], 0);
        EXPECT_EQ(ifx[3], 0);
        EXPECT_EQ(ifx[4], 2);
        EXPECT_EQ(ifx[5], USB_CLASS_MASS_STORAGE);
        EXPECT_EQ(ifx[6], 0x06);
        EXPECT_EQ(ifx[7], 0x50);

        const u8* ep = ifx + 9;
        for (u8 addr : { (u8)0x81, (u8)0x02 }) {
            EXPECT_EQ(ep[0], 7);
            EXPECT_EQ(ep[1], USB_DT_ENDPOINT);
            EXPECT_EQ(ep[2], addr);
            EXPECT_EQ(ep[3], USB_EP_BULK);
            EXPECT_EQ((size_t)(ep[4] | ep[5] << 8), mps);
            ep += 7;
            if (usb3) {
                EXPECT_EQ(ep[0], 6);
                EXPECT_EQ(ep[1], USB_DT_ENDPOINT_COMPANION);
                ep += 6;
            }
        }

        EXPECT_EQ(s.get_descriptor(USB_DT_CONFIG, 1, desc, 9),
                  USB_RESULT_STALL);

        ASSERT_EQ(s.get_descriptor(USB_DT_STRING, 0, desc, 4),
                  USB_RESULT_SUCCESS);
        EXPECT_EQ(desc[0], 4);
        EXPECT_EQ(desc[1], USB_DT_STRING);
        EXPECT_EQ(desc[2] | desc[3] << 8, 0x0409);

        EXPECT_EQ(s.get_string(1), d.manufacturer.get());
        EXPECT_EQ(s.get_string(2), d.product.get());
        EXPECT_EQ(s.get_string(3), d.serialno.get());
    }

    void test_descriptors() {
        check_descriptors(stim, drive, 1024);
        enumerate(stim);
        EXPECT_TRUE(drive.is_configured());
        EXPECT_EQ(drive.get_address(), 1);
    }

    void test_descriptors_usb2() {
        EXPECT_EQ(drive_ro.vendorid.get(), 0x1234);
        EXPECT_EQ(drive_ro.productid.get(), 0x5678);
        check_descriptors(stim_ro, drive_ro, 512);
        enumerate(stim_ro);
        EXPECT_TRUE(drive_ro.is_configured());
    }

    void test_class_requests() {
        vector<u8> data;
        ASSERT_EQ(stim.control_in(REQ_GET_MAX_LUN, 0, 0, data, 1),
                  USB_RESULT_SUCCESS);
        EXPECT_EQ(data[0], 0);

        EXPECT_EQ(stim.control_out(REQ_BOT_RESET, 0, 0), USB_RESULT_SUCCESS);

        u16 unsupported = USB_REQ_CLASS | USB_REQ_IFACE | 0xfd;
        EXPECT_EQ(stim.control_out(unsupported, 0, 0), USB_RESULT_STALL);
        EXPECT_EQ(stim.control_in(unsupported, 0, 0, data, 1),
                  USB_RESULT_STALL);

        EXPECT_EQ(command(stim, cdb6(block::SCSI_TEST_UNIT_READY)),
                  STS_PASSED);
    }

    void test_scsi_info() {
        EXPECT_EQ(command(stim, cdb6(block::SCSI_TEST_UNIT_READY)),
                  STS_PASSED);
        EXPECT_EQ(m_residue, 0u);
        EXPECT_EQ(command(stim, cdb6(block::SCSI_TEST_UNIT_READY), true),
                  STS_PASSED);

        vector<u8> data;
        ASSERT_EQ(
            command_in(stim, cdb6(block::SCSI_INQUIRY, 0, 0, 36), data, 36),
            STS_PASSED);
        EXPECT_EQ(m_residue, 0u);
        EXPECT_EQ(data[0], block::SCSI_DEVICE_DIRECT_ACCESS);
        EXPECT_EQ(data[1], 0x80);
        EXPECT_EQ(data[4], 31);
        EXPECT_EQ(string((const char*)data.data() + 8, 5), "MWARE");
        EXPECT_EQ(string((const char*)data.data() + 16, 14), "VCML-SCSIDRIVE");

        ASSERT_EQ(
            command_in(stim, cdb6(block::SCSI_INQUIRY, 1, 0x00, 64), data, 64),
            STS_PASSED);
        EXPECT_EQ(data[1], 0x00);
        EXPECT_EQ(data[3], 6);
        EXPECT_EQ(m_residue, 64u - 10u);

        ASSERT_EQ(command_in(stim, cdb6(block::SCSI_READ_CAPACITY), data, 8),
                  STS_PASSED);
        EXPECT_EQ(get_be(data.data(), 4), NBLKS - 1);
        EXPECT_EQ(get_be(data.data() + 4, 4), BLKSZ);

        vector<u8> rc16(16, 0);
        rc16[0] = block::SCSI_READ_CAPACITY_16;
        rc16[1] = 0x10;
        rc16[13] = 32;
        ASSERT_EQ(command_in(stim, rc16, data, 32), STS_PASSED);
        EXPECT_EQ(get_be(data.data(), 8), NBLKS - 1);
        EXPECT_EQ(get_be(data.data() + 8, 4), BLKSZ);
        EXPECT_EQ(m_residue, 0u);

        ASSERT_EQ(
            command_in(stim, cdb6(block::SCSI_MODE_SENSE_6, 0, 0x3f, 192),
                       data, 192),
            STS_PASSED);
        size_t len = data[0] + 1u;
        EXPECT_GT(len, 12u);
        EXPECT_EQ(data[2], 0x00);
        EXPECT_EQ(data[3], 8);
        EXPECT_EQ(m_residue, 192 - len);
        for (size_t i = len; i < data.size(); i++)
            EXPECT_EQ(data[i], 0) << "at offset " << i;

        vector<u8> ms10 = {
            block::SCSI_MODE_SENSE_10, 0x08, 0x08, 0, 0, 0, 0, 0, 255, 0
        };
        ASSERT_EQ(command_in(stim, ms10, data, 255), STS_PASSED);
        len = get_be(data.data(), 2) + 2;
        EXPECT_EQ(len, 8u + 20u);
        EXPECT_EQ(data[8], block::SCSI_MODE_PAGE_CACHING);
        EXPECT_EQ(m_residue, 255 - len);

        block::scsi_sense sense = request_sense(stim);
        EXPECT_EQ(sense, block::SENSE_NOTHING);
    }

    void test_scsi_misc() {
        vector<u8> sync = {
            block::SCSI_SYNC_CACHE, 0, 0, 0, 0, 0, 0, 0, 0, 0
        };
        EXPECT_EQ(command(stim, sync), STS_PASSED);
        EXPECT_EQ(m_residue, 0u);

        auto prevent = cdb6(block::SCSI_ALLOW_MEDIUM_REMOVAL, 0, 0, 1);
        EXPECT_EQ(command(stim, prevent), STS_PASSED);
        auto allow = cdb6(block::SCSI_ALLOW_MEDIUM_REMOVAL, 0, 0, 0);
        EXPECT_EQ(command(stim, allow), STS_PASSED);

        vector<u8> data;
        vector<u8> luns = {
            block::SCSI_REPORT_LUNS, 0, 0, 0, 0, 0, 0, 0, 0, 16, 0, 0
        };
        ASSERT_EQ(command_in(stim, luns, data, 16), STS_PASSED);
        EXPECT_EQ(get_be(data.data(), 4), 8u);

        EXPECT_EQ(command(stim, cdb6(0xc0)), STS_FAILED);
        EXPECT_EQ(m_residue, 0u);
        EXPECT_EQ(request_sense(stim), block::SENSE_ILLEGAL_REQ);
        EXPECT_EQ(request_sense(stim), block::SENSE_NOTHING);

        EXPECT_EQ(command_in(stim, cdb6(0xc0, 0, 0, 16), data, 16),
                  STS_FAILED);
        EXPECT_EQ(m_residue, 16u);
        EXPECT_EQ(request_sense(stim), block::SENSE_ILLEGAL_REQ);

        EXPECT_EQ(command_out(stim, cdb6(0xc1), vector<u8>(24, 0x11)),
                  STS_FAILED);
        EXPECT_EQ(m_residue, 24u);
        EXPECT_EQ(request_sense(stim), block::SENSE_ILLEGAL_REQ);
    }

    void test_read_write() {
        vector<u8> one = pattern(BLKSZ, 0x10);
        EXPECT_EQ(command_out(stim, cdb_write(3, 1), one), STS_PASSED);
        EXPECT_EQ(m_residue, 0u);
        EXPECT_EQ(disk_read(drive, 3 * BLKSZ, BLKSZ), one);

        vector<u8> data;
        EXPECT_EQ(command_in(stim, cdb_read(3, 1), data, BLKSZ), STS_PASSED);
        EXPECT_EQ(m_residue, 0u);
        EXPECT_EQ(data, one);

        vector<u8> many = pattern(16 * BLKSZ, 0x20);
        EXPECT_EQ(command_out(stim, cdb_write(100, 16), many), STS_PASSED);
        EXPECT_EQ(m_residue, 0u);
        EXPECT_EQ(disk_read(drive, 100 * BLKSZ, many.size()), many);

        EXPECT_EQ(command_in(stim, cdb_read(100, 16), data, many.size()),
                  STS_PASSED);
        EXPECT_EQ(m_residue, 0u);
        EXPECT_EQ(data, many);

        vector<u8> direct = pattern(2 * BLKSZ, 0x30);
        disk_write(drive, 200 * BLKSZ, direct);
        EXPECT_EQ(command_in(stim, cdb_read(200, 2), data, direct.size()),
                  STS_PASSED);
        EXPECT_EQ(data, direct);

        EXPECT_EQ(command(stim, cdb_read(0, 0), true), STS_PASSED);
        EXPECT_EQ(command(stim, cdb_write(0, 0)), STS_PASSED);

        vector<u8> inquiry;
        ASSERT_EQ(command_in(stim, cdb6(block::SCSI_INQUIRY, 1, 0x00, 10),
                             inquiry, 10),
                  STS_PASSED);
        EXPECT_EQ(inquiry[0], block::SCSI_DEVICE_DIRECT_ACCESS);
        EXPECT_EQ(inquiry[1], 0x00);
        EXPECT_EQ(inquiry[3], 6);
        EXPECT_EQ(inquiry[4], 0x00);
        EXPECT_EQ(inquiry[5], 0x80);
    }

    void test_read_write_split() {
        for (size_t pktsz : { 64, 100, 1024 }) {
            vector<u8> data = pattern(8 * BLKSZ, (u8)pktsz);
            EXPECT_EQ(command_out(stim, cdb_write(300, 8), data, pktsz),
                      STS_PASSED);
            EXPECT_EQ(m_residue, 0u);
            EXPECT_EQ(disk_read(drive, 300 * BLKSZ, data.size()), data);

            vector<u8> back;
            EXPECT_EQ(
                command_in(stim, cdb_read(300, 8), back, data.size(), pktsz),
                STS_PASSED);
            EXPECT_EQ(m_residue, 0u);
            EXPECT_EQ(back, data);
        }

        vector<u8> data = pattern(2 * BLKSZ, 0x44);
        u32 tag = next_tag();
        ASSERT_EQ(stim.out(EP_OUT,
                           make_cbw(tag, 2 * BLKSZ, false, cdb_write(400, 2))),
                  USB_RESULT_SUCCESS);
        ASSERT_EQ(stim.out(EP_OUT, data.data(), 1000), USB_RESULT_SUCCESS);
        vector<u8> last(data.begin() + 1000, data.end());
        last.resize(BLKSZ, 0xff);
        ASSERT_EQ(stim.out(EP_OUT, last), USB_RESULT_SUCCESS);
        EXPECT_EQ(read_csw(stim, tag), STS_PASSED);
        EXPECT_EQ(m_residue, 0u);
        EXPECT_EQ(disk_read(drive, 400 * BLKSZ, data.size()), data);
    }

    void test_end_of_medium() {
        vector<u8> data = pattern(BLKSZ, 0x50);
        EXPECT_EQ(command_out(stim, cdb_write(NBLKS - 1, 1), data),
                  STS_PASSED);
        vector<u8> back;
        EXPECT_EQ(command_in(stim, cdb_read(NBLKS - 1, 1), back, BLKSZ),
                  STS_PASSED);
        EXPECT_EQ(back, data);

        EXPECT_EQ(command_in(stim, cdb_read(NBLKS - 1, 2), back, 2 * BLKSZ),
                  STS_FAILED);
        EXPECT_EQ(m_residue, 2 * BLKSZ);
        EXPECT_EQ(request_sense(stim), block::SENSE_ILLEGAL_FIELD);

        EXPECT_EQ(command_in(stim, cdb_read(NBLKS, 1), back, BLKSZ),
                  STS_FAILED);
        EXPECT_EQ(m_residue, BLKSZ);
        EXPECT_EQ(request_sense(stim), block::SENSE_ILLEGAL_FIELD);

        vector<u8> two = pattern(2 * BLKSZ, 0x60);
        EXPECT_EQ(command_out(stim, cdb_write(NBLKS - 1, 2), two), STS_FAILED);
        EXPECT_EQ(m_residue, 2 * BLKSZ);
        EXPECT_EQ(request_sense(stim), block::SENSE_ILLEGAL_FIELD);
        EXPECT_EQ(disk_read(drive, (NBLKS - 1) * BLKSZ, BLKSZ), data);
    }

    void test_readonly() {
        vector<u8> data = pattern(BLKSZ, 0x70);
        EXPECT_EQ(command_out(stim_ro, cdb_write(1, 1), data), STS_FAILED);
        EXPECT_EQ(m_residue, BLKSZ);
        EXPECT_EQ(request_sense(stim_ro), block::SENSE_DATA_PROTECT);
        EXPECT_EQ(disk_read(drive_ro, BLKSZ, BLKSZ), vector<u8>(BLKSZ, 0));

        vector<u8> back;
        EXPECT_EQ(command_in(stim_ro, cdb_read(1, 1), back, BLKSZ),
                  STS_PASSED);
        EXPECT_EQ(back, vector<u8>(BLKSZ, 0));

        EXPECT_EQ(
            command_in(stim_ro, cdb6(block::SCSI_MODE_SENSE_6, 0x08, 0x3f, 64),
                       back, 64),
            STS_PASSED);
        EXPECT_EQ(back[2], 0x80);

        EXPECT_EQ(
            command_in(stim_ro, cdb6(block::SCSI_READ_CAPACITY), back, 8),
            STS_PASSED);
        EXPECT_EQ(get_be(back.data(), 4), 64 * KiB / BLKSZ - 1);
    }

    void test_no_medium() {
        enumerate(stim_bad);

        vector<u8> data;
        EXPECT_EQ(
            command_in(stim_bad, cdb6(block::SCSI_READ_CAPACITY), data, 8),
            STS_FAILED);
        EXPECT_EQ(m_residue, 8u);
        EXPECT_EQ(request_sense(stim_bad), block::SENSE_NO_MEDIUM);

        EXPECT_EQ(command_in(stim_bad, cdb_read(0, 1), data, BLKSZ),
                  STS_FAILED);
        EXPECT_EQ(request_sense(stim_bad), block::SENSE_ILLEGAL_FIELD);
    }

    void test_invalid_cbw() {
        auto tur = cdb6(block::SCSI_TEST_UNIT_READY);
        vector<u8> cbw = make_cbw(next_tag(), 0, false, tur);

        EXPECT_EQ(stim.out(EP_OUT, cbw.data(), CBW_SIZE - 1),
                  USB_RESULT_STALL);
        vector<u8> longer(cbw);
        longer.push_back(0);
        EXPECT_EQ(stim.out(EP_OUT, longer), USB_RESULT_STALL);

        vector<u8> bad(cbw);
        put32(bad, 0, CSW_SIG);
        EXPECT_EQ(stim.out(EP_OUT, bad), USB_RESULT_STALL);

        EXPECT_EQ(stim.out(EP_OUT, make_cbw(next_tag(), 0, false, tur, 1)),
                  USB_RESULT_STALL);

        bad = cbw;
        bad[14] = 0;
        EXPECT_EQ(stim.out(EP_OUT, bad), USB_RESULT_STALL);
        bad[14] = 17;
        EXPECT_EQ(stim.out(EP_OUT, bad), USB_RESULT_STALL);

        EXPECT_EQ(stim.out(1, cbw), USB_RESULT_STALL);
        EXPECT_EQ(stim.out(3, cbw), USB_RESULT_STALL);

        EXPECT_EQ(command(stim, tur), STS_PASSED);
    }

    void test_invalid_sequence() {
        vector<u8> buf;
        EXPECT_EQ(stim.in(EP_IN, buf, CSW_SIZE), USB_RESULT_STALL);

        u32 tag = next_tag();
        auto tur = cdb6(block::SCSI_TEST_UNIT_READY);
        ASSERT_EQ(stim.out(EP_OUT, make_cbw(tag, 0, false, tur)),
                  USB_RESULT_SUCCESS);
        EXPECT_EQ(stim.out(EP_OUT, make_cbw(next_tag(), 0, false, tur)),
                  USB_RESULT_STALL);
        EXPECT_EQ(stim.in(EP_IN, buf, 64), USB_RESULT_STALL);
        EXPECT_EQ(stim.in(EP_OUT, buf, CSW_SIZE), USB_RESULT_STALL);
        EXPECT_EQ(stim.in(3, buf, CSW_SIZE), USB_RESULT_STALL);
        EXPECT_EQ(read_csw(stim, tag), STS_PASSED);
        EXPECT_EQ(stim.in(EP_IN, buf, CSW_SIZE), USB_RESULT_STALL);

        tag = next_tag();
        ASSERT_EQ(stim.out(EP_OUT, make_cbw(tag, BLKSZ, true, cdb_read(0, 1))),
                  USB_RESULT_SUCCESS);
        EXPECT_EQ(stim.out(EP_OUT, buf), USB_RESULT_STALL);
        ASSERT_EQ(stim.in(EP_IN, buf, BLKSZ), USB_RESULT_SUCCESS);
        EXPECT_EQ(read_csw(stim, tag), STS_PASSED);

        EXPECT_EQ(command(stim, tur), STS_PASSED);
    }

    void test_residue() {
        vector<u8> data;
        ASSERT_EQ(
            command_in(stim, cdb6(block::SCSI_INQUIRY, 0, 0, 255), data, 255),
            STS_PASSED);
        EXPECT_EQ(m_residue, 255u - 36u);
        EXPECT_EQ(string((const char*)data.data() + 8, 5), "MWARE");
        for (size_t i = 36; i < data.size(); i++)
            EXPECT_EQ(data[i], 0) << "at offset " << i;

        ASSERT_EQ(
            command_in(stim, cdb6(block::SCSI_INQUIRY, 0, 0, 8), data, 8),
            STS_PASSED);
        EXPECT_EQ(m_residue, 0u);
        EXPECT_EQ(data[0], block::SCSI_DEVICE_DIRECT_ACCESS);
        EXPECT_EQ(data[4], 31);

        ASSERT_EQ(command_in(stim, cdb6(block::SCSI_MODE_SENSE_6, 0, 0x3f, 4),
                             data, 4),
                  STS_PASSED);
        EXPECT_EQ(m_residue, 0u);
        EXPECT_GT(data[0], 3);

        ASSERT_EQ(command_in(stim, cdb6(block::SCSI_REQUEST_SENSE, 0, 0, 96),
                             data, 96),
                  STS_PASSED);
        EXPECT_EQ(m_residue, 96u - 18u);
        EXPECT_EQ(data[7], 0x0a);

        vector<u8> blocks = pattern(2 * BLKSZ, 0x80);
        disk_write(drive, 500 * BLKSZ, blocks);
        command_in(stim, cdb_read(500, 2), data, BLKSZ);
        EXPECT_TRUE(std::equal(data.begin(), data.end(), blocks.begin()));

        EXPECT_EQ(command(stim, cdb6(block::SCSI_TEST_UNIT_READY)),
                  STS_PASSED);
    }

    void test_direction() {
        vector<u8> orig = pattern(BLKSZ, 0x90);
        disk_write(drive, 600 * BLKSZ, orig);

        vector<u8> data;
        EXPECT_EQ(command_in(stim, cdb_write(600, 1), data, BLKSZ), STS_PHASE);
        EXPECT_EQ(m_residue, BLKSZ);
        EXPECT_EQ(disk_read(drive, 600 * BLKSZ, BLKSZ), orig);

        EXPECT_EQ(command_out(stim, cdb_read(600, 1), pattern(BLKSZ, 0xa0)),
                  STS_PASSED);
        EXPECT_EQ(disk_read(drive, 600 * BLKSZ, BLKSZ), orig);

        EXPECT_EQ(command(stim, cdb6(block::SCSI_TEST_UNIT_READY)),
                  STS_PASSED);
    }

    void test_bot_reset() {
        vector<u8> buf;
        u32 tag = next_tag();
        ASSERT_EQ(
            stim.out(EP_OUT, make_cbw(tag, 4 * BLKSZ, true, cdb_read(0, 4))),
            USB_RESULT_SUCCESS);
        ASSERT_EQ(stim.in(EP_IN, buf, BLKSZ), USB_RESULT_SUCCESS);
        EXPECT_EQ(stim.control_out(REQ_BOT_RESET, 0, 0), USB_RESULT_SUCCESS);
        EXPECT_EQ(command(stim, cdb6(block::SCSI_TEST_UNIT_READY)),
                  STS_PASSED);

        vector<u8> orig = disk_read(drive, 700 * BLKSZ, 2 * BLKSZ);
        tag = next_tag();
        ASSERT_EQ(stim.out(EP_OUT,
                           make_cbw(tag, 2 * BLKSZ, false, cdb_write(700, 2))),
                  USB_RESULT_SUCCESS);
        ASSERT_EQ(stim.out(EP_OUT, pattern(BLKSZ, 0xb0)), USB_RESULT_SUCCESS);
        EXPECT_EQ(stim.control_out(REQ_BOT_RESET, 0, 0), USB_RESULT_SUCCESS);
        EXPECT_EQ(disk_read(drive, 700 * BLKSZ, 2 * BLKSZ), orig);

        vector<u8> data = pattern(2 * BLKSZ, 0xc0);
        EXPECT_EQ(command_out(stim, cdb_write(700, 2), data), STS_PASSED);
        EXPECT_EQ(disk_read(drive, 700 * BLKSZ, 2 * BLKSZ), data);

        tag = next_tag();
        ASSERT_EQ(
            stim.out(EP_OUT, make_cbw(tag, 0, false,
                                      cdb6(block::SCSI_TEST_UNIT_READY))),
            USB_RESULT_SUCCESS);
        EXPECT_EQ(stim.control_out(REQ_BOT_RESET, 0, 0), USB_RESULT_SUCCESS);
        EXPECT_EQ(stim.in(EP_IN, buf, CSW_SIZE), USB_RESULT_STALL);
        EXPECT_EQ(command(stim, cdb6(block::SCSI_TEST_UNIT_READY)),
                  STS_PASSED);
    }

    void test_device_reset() {
        vector<u8> orig = disk_read(drive, 800 * BLKSZ, 2 * BLKSZ);
        u32 tag = next_tag();
        ASSERT_EQ(stim.out(EP_OUT,
                           make_cbw(tag, 2 * BLKSZ, false, cdb_write(800, 2))),
                  USB_RESULT_SUCCESS);
        ASSERT_EQ(stim.out(EP_OUT, pattern(BLKSZ, 0xd0)), USB_RESULT_SUCCESS);

        stim.usb_out.reset_device();
        EXPECT_FALSE(drive.is_addressed());
        enumerate(stim);
        EXPECT_EQ(disk_read(drive, 800 * BLKSZ, 2 * BLKSZ), orig);
        EXPECT_EQ(command(stim, cdb6(block::SCSI_TEST_UNIT_READY)),
                  STS_PASSED);

        vector<u8> buf;
        tag = next_tag();
        ASSERT_EQ(
            stim.out(EP_OUT, make_cbw(tag, 2 * BLKSZ, true, cdb_read(800, 2))),
            USB_RESULT_SUCCESS);
        ASSERT_EQ(stim.in(EP_IN, buf, BLKSZ), USB_RESULT_SUCCESS);
        stim.usb_out.reset_device();
        enumerate(stim);
        EXPECT_EQ(stim.in(EP_IN, buf, CSW_SIZE), USB_RESULT_STALL);
        EXPECT_EQ(command(stim, cdb6(block::SCSI_TEST_UNIT_READY)),
                  STS_PASSED);
    }
};

TEST(usb, drive) {
    vcml::broker broker("test");
    broker.define("harness.drive.image", "ramdisk:1MiB");
    broker.define("harness.drive_ro.image", "ramdisk:64KiB");
    broker.define("harness.drive_ro.readonly", "true");
    broker.define("harness.drive_ro.usb3", "false");
    broker.define("harness.drive_ro.vendorid", "0x1234");
    broker.define("harness.drive_ro.productid", "0x5678");
    broker.define("harness.drive_ro.manufacturer", "ACME");
    broker.define("harness.drive_ro.product", "Stick");
    broker.define("harness.drive_ro.serialno", "SN42");
    broker.define("harness.drive_bad.image", "ramdisk:abc");
    broker.define("harness.drive_default.start_attached", "false");
    broker.define("harness.drive_image.start_attached", "false");

    usb_drive_test harness("harness");
    sc_core::sc_start();
}
