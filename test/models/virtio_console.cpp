/******************************************************************************
 *                                                                            *
 * Copyright (C) 2022 MachineWare GmbH                                        *
 * All Rights Reserved                                                        *
 *                                                                            *
 * This work is licensed under the terms described in the LICENSE file found  *
 * in the root directory of this source tree.                                 *
 *                                                                            *
 ******************************************************************************/

#include "virtio_test.h"

class virtio_rng_stim : public test_base, public serial_host
{
public:
    generic::bus bus;
    generic::memory mem;

    virtio::mmio virtio;
    virtio::console virtio_console;

    tlm_initiator_socket out;
    gpio_target_socket irq;

    virtio_stim stim;
    virtio::console console;
    serial_initiator_socket serial_tx;
    serial_target_socket serial_rx;
    vector<u8> chars;

    virtual void serial_receive(u8 data) override { chars.push_back(data); }

    enum : u32 {
        VQ_DATA_RX = 0,
        VQ_DATA_TX = 1,
        VQ_CTRL_RX = 2,
        VQ_CTRL_TX = 3,
    };

    enum : u64 {
        F_SIZE = bit(0),
        F_MULTIPORT = bit(1),
        F_EMERG_WRITE = bit(2),
    };

    virtio_rng_stim(const sc_module_name& nm = sc_gen_unique_name("stim")):
        test_base(nm),
        bus("bus"),
        mem("mem", 0x1000),
        virtio("virtio"),
        virtio_console("virtio_input"),
        out("out"),
        irq("irq"),
        stim("stim"),
        console("console"),
        serial_tx("serial_tx"),
        serial_rx("serial_rx"),
        chars() {
        stim.virtio_out.bind(console.virtio_in);
        console.serial_tx.bind(serial_rx);
        serial_tx.bind(console.serial_rx);

        add_test("mmio", &virtio_rng_stim::test_mmio);
        add_test("identify", &virtio_rng_stim::test_identify);
        add_test("config", &virtio_rng_stim::test_config);
        add_test("transmit", &virtio_rng_stim::test_transmit);
        add_test("receive", &virtio_rng_stim::test_receive);
        add_test("receive_unqueued", &virtio_rng_stim::test_receive_unqueued);
        add_test("errors", &virtio_rng_stim::test_errors);

        virtio.virtio_out.bind(virtio_console.virtio_in);
        virtio_console.serial_tx.stub();
        virtio_console.serial_rx.stub();

        bus.bind(mem.in, 0, 0xfff);
        bus.bind(virtio.in, 0x1000, 0x1fff);

        bus.bind(out);
        bus.bind(virtio.out);

        virtio.irq.bind(irq);

        clk.bind(bus.clk);
        clk.bind(mem.clk);
        clk.bind(virtio.clk);

        rst.bind(bus.rst);
        rst.bind(mem.rst);
        rst.bind(virtio.rst);
    }

    void test_mmio() {
        enum addresses : u64 {
            CONSOLE_BASE = 0x1000,
            CONSOLE_MAGIC = CONSOLE_BASE + 0x00,
            CONSOLE_VERSION = CONSOLE_BASE + 0x04,
            CONSOLE_DEVID = CONSOLE_BASE + 0x08,
            CONSOLE_VQ_SEL = CONSOLE_BASE + 0x30,
            CONSOLE_VQ_MAX = CONSOLE_BASE + 0x34,
            CONSOLE_STATUS = CONSOLE_BASE + 0x70,
        };

        u32 data;
        ASSERT_OK(out.readw(CONSOLE_MAGIC, data));
        ASSERT_EQ(data, fourcc("virt"));

        ASSERT_OK(out.readw(CONSOLE_VERSION, data));
        ASSERT_EQ(data, 2);

        ASSERT_OK(out.readw(CONSOLE_DEVID, data));
        ASSERT_EQ(data, VIRTIO_DEVICE_CONSOLE);

        ASSERT_OK(out.readw(CONSOLE_STATUS, data));
        ASSERT_EQ(data, 0);

        data = VIRTIO_STATUS_ACKNOWLEDGE | VIRTIO_STATUS_DRIVER |
               VIRTIO_STATUS_FEATURES_OK;
        ASSERT_OK(out.writew(CONSOLE_STATUS, data));

        ASSERT_OK(out.readw(CONSOLE_STATUS, data));
        ASSERT_TRUE(data & VIRTIO_STATUS_FEATURES_OK);

        data = 0;
        ASSERT_OK(out.writew(CONSOLE_VQ_SEL, data));
        ASSERT_OK(out.readw(CONSOLE_VQ_MAX, data));
        EXPECT_GT(data, 0);

        data = 1;
        ASSERT_OK(out.writew(CONSOLE_VQ_SEL, data));
        ASSERT_OK(out.readw(CONSOLE_VQ_MAX, data));
        EXPECT_GT(data, 0);

        data = 2;
        ASSERT_OK(out.writew(CONSOLE_VQ_SEL, data));
        ASSERT_OK(out.readw(CONSOLE_VQ_MAX, data));
        EXPECT_GT(data, 0);

        data = 3;
        ASSERT_OK(out.writew(CONSOLE_VQ_SEL, data));
        ASSERT_OK(out.readw(CONSOLE_VQ_MAX, data));
        EXPECT_GT(data, 0);

        data = 4;
        ASSERT_OK(out.writew(CONSOLE_VQ_SEL, data));
        ASSERT_OK(out.readw(CONSOLE_VQ_MAX, data));
        EXPECT_EQ(data, 0);
    }

    void test_identify() {
        virtio_device_desc desc = stim.identify();
        EXPECT_EQ(desc.device_id, VIRTIO_DEVICE_CONSOLE);
        EXPECT_EQ(desc.vendor_id, VIRTIO_VENDOR_VCML);
        EXPECT_EQ(desc.pci_class, PCI_CLASS_COMM_OTHER);
        ASSERT_EQ(desc.virtqueues.size(), 4);
        EXPECT_EQ(desc.virtqueues.at(VQ_DATA_RX).limit, 32);
        EXPECT_EQ(desc.virtqueues.at(VQ_DATA_TX).limit, 32);
        EXPECT_EQ(desc.virtqueues.at(VQ_CTRL_RX).limit, 8);
        EXPECT_EQ(desc.virtqueues.at(VQ_CTRL_TX).limit, 8);

        EXPECT_EQ(stim.features(), F_EMERG_WRITE);
        EXPECT_TRUE(stim.virtio_out->write_features(F_EMERG_WRITE));
        EXPECT_FALSE(stim.virtio_out->write_features(F_MULTIPORT));
        EXPECT_FALSE(stim.virtio_out->write_features(F_SIZE));
    }

    void test_config() {
        u16 cols = 0xffff;
        u16 rows = 0xffff;
        u32 ports = 0;
        stim.identify();
        EXPECT_TRUE(stim.read_config(0, cols));
        EXPECT_TRUE(stim.read_config(2, rows));
        EXPECT_TRUE(stim.read_config(4, ports));
        EXPECT_EQ(cols, 0);
        EXPECT_EQ(rows, 0);
        EXPECT_EQ(ports, 1);

        console.cols = 80;
        console.rows = 25;
        stim.identify();
        EXPECT_TRUE(stim.read_config(0, cols));
        EXPECT_TRUE(stim.read_config(2, rows));
        EXPECT_EQ(cols, 80);
        EXPECT_EQ(rows, 25);
        EXPECT_EQ(stim.features(), F_EMERG_WRITE | F_SIZE);
        EXPECT_TRUE(stim.virtio_out->write_features(F_SIZE));

        u32 data = 0;
        EXPECT_TRUE(stim.read_config(8, data));
        EXPECT_FALSE(stim.read_config(12, data));
        EXPECT_FALSE(stim.read_config(10, data));

        chars.clear();
        data = '!';
        EXPECT_TRUE(stim.write_config(8, data));
        EXPECT_EQ(chars, vector<u8>({ '!' }));
        EXPECT_FALSE(stim.write_config(4, data));
        EXPECT_FALSE(stim.write_config(8, (u8)'?'));
        EXPECT_EQ(chars, vector<u8>({ '!' }));

        console.cols = 0;
        console.rows = 0;
        stim.identify();
    }

    void test_transmit() {
        stim.clear();
        chars.clear();
        vq_message msg0 = stim.submit(VQ_DATA_TX, { { 'h', 'e', 'l' } }, {});
        vq_message msg1 = stim.submit(VQ_DATA_TX, { { 'l' }, { 'o', '!' } },
                                      {});
        EXPECT_TRUE(stim.kick(VQ_DATA_TX));
        EXPECT_EQ(chars, vector<u8>({ 'h', 'e', 'l', 'l', 'o', '!' }));
        ASSERT_EQ(stim.used[VQ_DATA_TX].size(), 2);
        EXPECT_EQ(stim.used[VQ_DATA_TX][0].index, msg0.index);
        EXPECT_EQ(stim.used[VQ_DATA_TX][1].index, msg1.index);
        EXPECT_TRUE(stim.used[VQ_DATA_RX].empty());
    }

    void test_receive() {
        stim.clear();
        vq_message msg0 = stim.submit(VQ_DATA_RX, {}, { 4 });
        vq_message msg1 = stim.submit(VQ_DATA_RX, {}, { 4 });
        EXPECT_TRUE(stim.kick(VQ_DATA_RX));
        EXPECT_TRUE(stim.avail[VQ_DATA_RX].empty());
        EXPECT_TRUE(stim.used[VQ_DATA_RX].empty());

        serial_tx.send('x');
        ASSERT_EQ(stim.used[VQ_DATA_RX].size(), 1);
        EXPECT_EQ(stim.used[VQ_DATA_RX][0].index, msg0.index);
        EXPECT_EQ(stim.used[VQ_DATA_RX][0].length_out(), 1);
        EXPECT_EQ(stim.response(stim.used[VQ_DATA_RX][0]),
                  vector<u8>({ 'x' }));

        stim.accept = false;
        serial_tx.send('y');
        EXPECT_EQ(stim.used[VQ_DATA_RX].size(), 1);

        stim.accept = true;
        serial_tx.send('z');
        ASSERT_EQ(stim.used[VQ_DATA_RX].size(), 2);
        EXPECT_EQ(stim.used[VQ_DATA_RX][1].index, msg1.index);
        EXPECT_EQ(stim.response(stim.used[VQ_DATA_RX][1]),
                  vector<u8>({ 'z' }));
    }

    void test_receive_unqueued() {
        stim.clear();
        stim.identify();
        serial_tx.send('a');
        EXPECT_TRUE(stim.used[VQ_DATA_RX].empty());

        stim.submit(VQ_DATA_RX, {}, { 4 });
        EXPECT_TRUE(stim.kick(VQ_DATA_RX));
        serial_tx.send('b');
        ASSERT_EQ(stim.used[VQ_DATA_RX].size(), 1);
        EXPECT_EQ(stim.response(stim.used[VQ_DATA_RX][0]),
                  vector<u8>({ 'b' }));
    }

    void test_errors() {
        stim.clear();
        chars.clear();
        EXPECT_TRUE(stim.kick(VQ_DATA_TX));

        vq_message msg = stim.submit(VQ_CTRL_TX, { { 1, 2, 3 } }, {});
        EXPECT_TRUE(stim.kick(VQ_CTRL_TX));
        ASSERT_EQ(stim.used[VQ_CTRL_TX].size(), 1);
        EXPECT_EQ(stim.used[VQ_CTRL_TX][0].index, msg.index);
        EXPECT_TRUE(chars.empty());

        stim.submit(VQ_DATA_TX, { { 'q' } }, {});
        stim.accept = false;
        EXPECT_FALSE(stim.kick(VQ_DATA_TX));
        stim.accept = true;
    }
};

TEST(virtio, rng) {
    virtio_rng_stim stim;
    stim.clk.stub(100 * MHz);
    stim.rst.stub();

    tlm::tlm_global_quantum::instance().set(sc_time(10, SC_MS));
    sc_core::sc_start();
}
