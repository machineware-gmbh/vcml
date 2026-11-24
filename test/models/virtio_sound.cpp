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

class virtio_sound_stim : public test_base
{
public:
    generic::bus bus;
    generic::memory mem;

    virtio::mmio virtio;
    virtio::sound virtio_sound;

    tlm_initiator_socket out;
    gpio_target_socket irq;

    virtio_stim stim;
    virtio::sound sound;

    enum : u32 {
        VQ_CTRL = 0,
        VQ_EVENT = 1,
        VQ_TX = 2,
        VQ_RX = 3,
        STREAM_TX = 0,
        STREAM_RX = 1,
    };

    enum : u32 {
        R_JACK_INFO = 0x0001,
        R_PCM_INFO = 0x0100,
        R_PCM_SET_PARAMS = 0x0101,
        R_PCM_PREPARE = 0x0102,
        R_PCM_RELEASE = 0x0103,
        R_PCM_START = 0x0104,
        R_PCM_STOP = 0x0105,
        S_OK = 0x8000,
        S_BAD_MSG = 0x8001,
        S_NOT_SUPP = 0x8002,
        S_NONE = 0xffffffff,
    };

    enum : u8 {
        FMT_MU_LAW = 1,
        FMT_S8 = 3,
        FMT_U8 = 4,
        FMT_S16 = 5,
        FMT_U16 = 6,
        FMT_S32 = 17,
        FMT_U32 = 18,
        FMT_FLOAT = 19,
        RATE_8000 = 1,
        RATE_48000 = 7,
        RATE_NUM = 14,
    };

    struct query_info {
        u32 code;
        u32 start_id;
        u32 count;
        u32 size;
    };

    struct pcm_info {
        u32 hda_fn_nid;
        u32 features;
        u64 formats;
        u64 rates;
        u8 direction;
        u8 channels_min;
        u8 channels_max;
        u8 padding[5];
    };

    struct pcm_hdr {
        u32 code;
        u32 stream_id;
    };

    struct pcm_set_params {
        pcm_hdr hdr;
        u32 buffer_bytes;
        u32 period_bytes;
        u32 features;
        u8 channels;
        u8 format;
        u8 rate;
        u8 padding;
    };

    struct pcm_status {
        u32 status;
        u32 latency_bytes;
    };

    virtio_sound_stim(const sc_module_name& nm = sc_gen_unique_name("stim")):
        test_base(nm),
        bus("bus"),
        mem("mem", 0x1000),
        virtio("virtio"),
        virtio_sound("virtio_sound"),
        out("out"),
        irq("irq"),
        stim("stim"),
        sound("sound") {
        stim.virtio_out.bind(sound.virtio_in);

        add_test("mmio", &virtio_sound_stim::test_mmio);
        add_test("identify", &virtio_sound_stim::test_identify);
        add_test("unconfigured", &virtio_sound_stim::test_unconfigured);
        add_test("pcm_info", &virtio_sound_stim::test_pcm_info);
        add_test("set_params", &virtio_sound_stim::test_set_params);
        add_test("control", &virtio_sound_stim::test_control);
        add_test("playback", &virtio_sound_stim::test_playback);
        add_test("release", &virtio_sound_stim::test_release);
        add_test("capture", &virtio_sound_stim::test_capture);
        add_test("malformed", &virtio_sound_stim::test_malformed);

        virtio.virtio_out.bind(virtio_sound.virtio_in);

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
            SOUND_BASE = 0x1000,
            SOUND_MAGIC = SOUND_BASE + 0x00,
            SOUND_VERSION = SOUND_BASE + 0x04,
            SOUND_DEVID = SOUND_BASE + 0x08,
            SOUND_VQ_SEL = SOUND_BASE + 0x30,
            SOUND_VQ_MAX = SOUND_BASE + 0x34,
            SOUND_STATUS = SOUND_BASE + 0x70,
        };

        u32 data;
        ASSERT_OK(out.readw(SOUND_MAGIC, data));
        ASSERT_EQ(data, 0x74726976);

        ASSERT_OK(out.readw(SOUND_VERSION, data));
        ASSERT_EQ(data, 2);

        ASSERT_OK(out.readw(SOUND_DEVID, data));
        ASSERT_EQ(data, VIRTIO_DEVICE_SOUND);

        ASSERT_OK(out.readw(SOUND_STATUS, data));
        ASSERT_EQ(data, 0);

        data = VIRTIO_STATUS_ACKNOWLEDGE | VIRTIO_STATUS_DRIVER |
               VIRTIO_STATUS_FEATURES_OK;
        ASSERT_OK(out.writew(SOUND_STATUS, data));

        ASSERT_OK(out.readw(SOUND_STATUS, data));
        ASSERT_TRUE(data & VIRTIO_STATUS_FEATURES_OK);

        // control virtqueue
        data = 0;
        ASSERT_OK(out.writew(SOUND_VQ_SEL, data));
        ASSERT_OK(out.readw(SOUND_VQ_MAX, data));
        EXPECT_EQ(data, 64);

        // event virtqueue
        data = 1;
        ASSERT_OK(out.writew(SOUND_VQ_SEL, data));
        ASSERT_OK(out.readw(SOUND_VQ_MAX, data));
        EXPECT_EQ(data, 64);

        // tx virtqueue
        data = 2;
        ASSERT_OK(out.writew(SOUND_VQ_SEL, data));
        ASSERT_OK(out.readw(SOUND_VQ_MAX, data));
        EXPECT_EQ(data, 64);

        // rx virtqueue
        data = 3;
        ASSERT_OK(out.writew(SOUND_VQ_SEL, data));
        ASSERT_OK(out.readw(SOUND_VQ_MAX, data));
        EXPECT_EQ(data, 64);

        // no more queues
        data = 4;
        ASSERT_OK(out.writew(SOUND_VQ_SEL, data));
        ASSERT_OK(out.readw(SOUND_VQ_MAX, data));
        EXPECT_EQ(data, 0);
    }

    void settle() { wait(sc_time(1, SC_US)); }

    void prepare() {
        settle();
        stim.clear();
        stim.identify();
    }

    vector<u8> control(const vector<u8>& request, size_t outlen = 4) {
        size_t done = stim.used[VQ_CTRL].size();
        vq_message msg = stim.submit(VQ_CTRL, { request }, { outlen });
        EXPECT_TRUE(stim.kick(VQ_CTRL));
        settle();
        EXPECT_EQ(stim.used[VQ_CTRL].size(), done + 1);
        return stim.response(msg);
    }

    static u32 status(const vector<u8>& response, size_t offset = 0) {
        u32 sts = S_NONE;
        if (response.size() >= offset + sizeof(sts))
            memcpy(&sts, response.data() + offset, sizeof(sts));
        return sts;
    }

    u32 command(u32 code, u32 stream) {
        pcm_hdr hdr = { code, stream };
        return status(control(stim.bytes(hdr)));
    }

    u32 set_params(u32 stream, u8 channels, u8 format, u8 rate) {
        pcm_set_params params = {};
        params.hdr = { R_PCM_SET_PARAMS, stream };
        params.buffer_bytes = 8192;
        params.period_bytes = 4096;
        params.channels = channels;
        params.format = format;
        params.rate = rate;
        return status(control(stim.bytes(params)));
    }

    vq_message queue_tx(size_t len, u8 seed) {
        vector<u8> data(len);
        for (size_t i = 0; i < len; i++)
            data[i] = (u8)(seed + i);
        u32 stream = STREAM_TX;
        vq_message msg = stim.submit(VQ_TX, { stim.bytes(stream), data },
                                     { sizeof(pcm_status) });
        EXPECT_TRUE(stim.kick(VQ_TX));
        return msg;
    }

    vq_message queue_rx(size_t len) {
        u32 stream = STREAM_RX;
        vq_message msg = stim.submit(VQ_RX, { stim.bytes(stream) },
                                     { len, sizeof(pcm_status) });
        EXPECT_TRUE(stim.kick(VQ_RX));
        return msg;
    }

    void test_identify() {
        virtio_device_desc desc = stim.identify();
        EXPECT_EQ(desc.device_id, VIRTIO_DEVICE_SOUND);
        EXPECT_EQ(desc.vendor_id, VIRTIO_VENDOR_VCML);
        EXPECT_EQ(desc.pci_class, PCI_CLASS_MULTIMEDIA_AUDIO);
        ASSERT_EQ(desc.virtqueues.size(), 4);
        for (u32 vqid : { VQ_CTRL, VQ_EVENT, VQ_TX, VQ_RX })
            EXPECT_EQ(desc.virtqueues.at(vqid).limit, 64) << "vq " << vqid;

        EXPECT_EQ(stim.features(), 0);
        EXPECT_TRUE(stim.virtio_out->write_features(0));

        u32 jacks = ~0u;
        u32 streams = 0;
        u32 chmaps = ~0u;
        EXPECT_TRUE(stim.read_config(0, jacks));
        EXPECT_TRUE(stim.read_config(4, streams));
        EXPECT_TRUE(stim.read_config(8, chmaps));
        EXPECT_EQ(jacks, 0);
        EXPECT_EQ(streams, 2);
        EXPECT_EQ(chmaps, 0);
        EXPECT_FALSE(stim.read_config(12, jacks));
        EXPECT_FALSE(stim.write_config(0, jacks));

        EXPECT_TRUE(stim.kick(VQ_EVENT));
        EXPECT_TRUE(stim.kick(9));
    }

    void test_unconfigured() {
        prepare();
        queue_tx(64, 1);
        queue_rx(64);
        EXPECT_EQ(command(R_PCM_START, STREAM_TX), S_BAD_MSG);
        EXPECT_EQ(command(R_PCM_START, STREAM_RX), S_BAD_MSG);
        wait(sc_time(1, SC_MS));
        EXPECT_TRUE(stim.used[VQ_TX].empty());
        EXPECT_TRUE(stim.used[VQ_RX].empty());
    }

    void test_pcm_info() {
        prepare();
        const u64 formats = bit(FMT_S8) | bit(FMT_U8) | bit(FMT_S16) |
                            bit(FMT_U16) | bit(FMT_S32) | bit(FMT_U32) |
                            bit(FMT_FLOAT);

        query_info query = { R_PCM_INFO, 0, 2, sizeof(pcm_info) };
        vector<u8> resp = control(stim.bytes(query), 4 + 2 * sizeof(pcm_info));
        ASSERT_EQ(status(resp), S_OK);

        for (u32 stream : { STREAM_TX, STREAM_RX }) {
            pcm_info info = {};
            memcpy(&info, resp.data() + 4 + stream * sizeof(info),
                   sizeof(info));
            EXPECT_EQ(info.hda_fn_nid, 0) << "stream " << stream;
            EXPECT_EQ(info.features, 0) << "stream " << stream;
            EXPECT_EQ(info.formats, formats) << "stream " << stream;
            EXPECT_EQ(info.rates, bitmask(RATE_NUM)) << "stream " << stream;
            EXPECT_EQ(info.direction, stream) << "stream " << stream;
            EXPECT_EQ(info.channels_min, 1) << "stream " << stream;
            EXPECT_EQ(info.channels_max, 8) << "stream " << stream;
        }

        query = { R_PCM_INFO, 1, 1, sizeof(pcm_info) + 8 };
        resp = control(stim.bytes(query), 4 + sizeof(pcm_info) + 8);
        EXPECT_EQ(status(resp), S_OK);
        EXPECT_EQ(resp[4 + 24], STREAM_RX);

        query = { R_PCM_INFO, 1, 2, sizeof(pcm_info) };
        EXPECT_EQ(status(control(stim.bytes(query), 4 + 2 * sizeof(pcm_info))),
                  S_BAD_MSG);
        query = { R_PCM_INFO, 0, 1, sizeof(pcm_info) - 1 };
        EXPECT_EQ(status(control(stim.bytes(query), 4 + sizeof(pcm_info) - 1)),
                  S_BAD_MSG);
        query = { R_PCM_INFO, 0, 1, sizeof(pcm_info) };
        EXPECT_EQ(status(control(stim.bytes(query), 4 + sizeof(pcm_info) + 1)),
                  S_BAD_MSG);
        EXPECT_EQ(
            status(control(stim.bytes((u32)R_PCM_INFO), 4 + sizeof(pcm_info))),
            S_BAD_MSG);
    }

    void test_set_params() {
        prepare();
        for (u8 format : { FMT_S8, FMT_U8, FMT_S16, FMT_U16, FMT_S32, FMT_U32,
                           FMT_FLOAT }) {
            EXPECT_EQ(set_params(STREAM_TX, 2, format, RATE_48000), S_OK)
                << "format " << (int)format;
        }

        for (u8 rate = 0; rate < RATE_NUM; rate++) {
            EXPECT_EQ(set_params(STREAM_RX, 1, FMT_S16, rate), S_OK)
                << "rate " << (int)rate;
        }

        EXPECT_EQ(set_params(STREAM_TX, 1, FMT_S16, RATE_48000), S_OK);
        EXPECT_EQ(set_params(STREAM_TX, 8, FMT_S16, RATE_48000), S_OK);
        EXPECT_EQ(set_params(STREAM_TX, 0, FMT_S16, RATE_48000), S_NOT_SUPP);
        EXPECT_EQ(set_params(STREAM_TX, 9, FMT_S16, RATE_48000), S_NOT_SUPP);
        EXPECT_EQ(set_params(STREAM_TX, 2, FMT_MU_LAW, RATE_48000),
                  S_NOT_SUPP);
        EXPECT_EQ(set_params(STREAM_TX, 2, 99, RATE_48000), S_NOT_SUPP);
        EXPECT_EQ(set_params(STREAM_TX, 2, FMT_S16, RATE_NUM), S_NOT_SUPP);
        EXPECT_EQ(set_params(2, 2, FMT_S16, RATE_48000), S_BAD_MSG);

        pcm_hdr hdr = { R_PCM_SET_PARAMS, STREAM_TX };
        EXPECT_EQ(status(control(stim.bytes(hdr))), S_BAD_MSG);
    }

    void test_control() {
        prepare();
        EXPECT_EQ(set_params(STREAM_TX, 2, FMT_S16, RATE_48000), S_OK);
        EXPECT_EQ(command(R_PCM_PREPARE, STREAM_TX), S_OK);
        EXPECT_EQ(command(R_PCM_START, STREAM_TX), S_OK);
        EXPECT_EQ(command(R_PCM_STOP, STREAM_TX), S_OK);
        EXPECT_EQ(command(R_PCM_RELEASE, STREAM_TX), S_OK);

        for (u32 code : { R_PCM_START, R_PCM_STOP, R_PCM_RELEASE }) {
            EXPECT_EQ(command(code, 2), S_BAD_MSG) << "code " << code;
            EXPECT_EQ(status(control(stim.bytes(code))), S_BAD_MSG)
                << "code " << code;
        }

        EXPECT_EQ(status(control(stim.bytes((u32)R_PCM_PREPARE))), S_BAD_MSG);
        EXPECT_EQ(command(R_JACK_INFO, 0), S_NOT_SUPP);
        EXPECT_EQ(command(0x999, 0), S_NOT_SUPP);
        EXPECT_EQ(status(control({ 1, 2 })), S_NOT_SUPP);
        EXPECT_EQ(status(control(stim.bytes((u32)R_PCM_PREPARE), 2)), S_NONE);

        stim.submit(VQ_CTRL, { stim.bytes((u32)R_JACK_INFO) }, { 4 });
        stim.submit(VQ_CTRL, { stim.bytes((u32)R_JACK_INFO) }, { 4 });
        stim.accept = false;
        EXPECT_TRUE(stim.kick(VQ_CTRL));
        settle();
        EXPECT_EQ(stim.avail[VQ_CTRL].size(), 1);
        stim.accept = true;
    }

    void test_playback() {
        prepare();
        const size_t len = 480 * 4;
        const sc_time duration(10, SC_MS);
        EXPECT_EQ(set_params(STREAM_TX, 2, FMT_S16, RATE_48000), S_OK);
        EXPECT_EQ(command(R_PCM_PREPARE, STREAM_TX), S_OK);

        vq_message msg0 = queue_tx(len, 0x10);
        vq_message msg1 = queue_tx(len, 0x20);
        wait(duration);
        EXPECT_TRUE(stim.used[VQ_TX].empty());

        EXPECT_EQ(command(R_PCM_START, STREAM_TX), S_OK);
        wait(duration / 2);
        EXPECT_TRUE(stim.used[VQ_TX].empty());
        wait(duration);
        ASSERT_EQ(stim.used[VQ_TX].size(), 1);
        EXPECT_EQ(stim.used[VQ_TX][0].index, msg0.index);
        EXPECT_EQ(status(stim.response(msg0), 0), S_OK);
        EXPECT_EQ(status(stim.response(msg0), 4), len);

        wait(duration);
        ASSERT_EQ(stim.used[VQ_TX].size(), 2);
        EXPECT_EQ(stim.used[VQ_TX][1].index, msg1.index);
        EXPECT_EQ(status(stim.response(msg1), 0), S_OK);
        EXPECT_EQ(status(stim.response(msg1), 4), len);

        EXPECT_EQ(command(R_PCM_STOP, STREAM_TX), S_OK);
        EXPECT_EQ(command(R_PCM_RELEASE, STREAM_TX), S_OK);
        EXPECT_TRUE(stim.used[VQ_RX].empty());
    }

    void test_release() {
        prepare();
        const size_t len = 480 * 4;
        const sc_time duration(10, SC_MS);
        EXPECT_EQ(set_params(STREAM_TX, 2, FMT_S16, RATE_48000), S_OK);

        vq_message msg0 = queue_tx(len, 0x30);
        vq_message msg1 = queue_tx(len, 0x40);
        vq_message msg2 = queue_tx(len, 0x50);
        EXPECT_EQ(command(R_PCM_START, STREAM_TX), S_OK);
        wait(duration / 2);
        EXPECT_EQ(command(R_PCM_RELEASE, STREAM_TX), S_OK);
        wait(duration);
        ASSERT_EQ(stim.used[VQ_TX].size(), 3);
        EXPECT_EQ(status(stim.response(msg0)), S_OK);
        EXPECT_EQ(status(stim.response(msg1)), S_NONE);
        EXPECT_EQ(status(stim.response(msg2)), S_NONE);
    }

    void test_capture() {
        prepare();
        const size_t len = 80;
        const sc_time duration(10, SC_MS);
        EXPECT_EQ(set_params(STREAM_RX, 1, FMT_U8, RATE_8000), S_OK);

        vq_message msg0 = queue_rx(len);
        vq_message msg1 = queue_rx(len);
        wait(duration);
        EXPECT_TRUE(stim.used[VQ_RX].empty());

        EXPECT_EQ(command(R_PCM_START, STREAM_RX), S_OK);
        wait(duration / 2);
        EXPECT_TRUE(stim.used[VQ_RX].empty());
        wait(duration);
        ASSERT_EQ(stim.used[VQ_RX].size(), 1);
        EXPECT_EQ(stim.used[VQ_RX][0].index, msg0.index);

        vector<u8> resp = stim.response(msg0);
        EXPECT_EQ(vector<u8>(resp.begin(), resp.begin() + len),
                  vector<u8>(len, 0));
        EXPECT_EQ(status(resp, len), S_OK);
        EXPECT_EQ(status(resp, len + 4), len);

        wait(duration);
        EXPECT_EQ(stim.used[VQ_RX].size(), 2);
        EXPECT_EQ(status(stim.response(msg1), len), S_OK);
        EXPECT_EQ(command(R_PCM_STOP, STREAM_RX), S_OK);
        EXPECT_EQ(command(R_PCM_RELEASE, STREAM_RX), S_OK);
        EXPECT_TRUE(stim.used[VQ_TX].empty());
    }

    void test_malformed() {
        prepare();
        EXPECT_EQ(set_params(STREAM_TX, 2, FMT_S16, RATE_48000), S_OK);
        EXPECT_EQ(set_params(STREAM_RX, 1, FMT_U8, RATE_8000), S_OK);

        vq_message tx = stim.submit(VQ_TX, { { 1, 2 } },
                                    { sizeof(pcm_status) });
        vq_message rx = stim.submit(VQ_RX, { { 1, 2, 3, 4 } }, { 4 });
        EXPECT_EQ(command(R_PCM_START, STREAM_TX), S_OK);
        EXPECT_EQ(command(R_PCM_START, STREAM_RX), S_OK);
        wait(sc_time(1, SC_MS));
        ASSERT_EQ(stim.used[VQ_TX].size(), 1);
        ASSERT_EQ(stim.used[VQ_RX].size(), 1);
        EXPECT_EQ(status(stim.response(tx)), S_BAD_MSG);
        EXPECT_EQ(status(stim.response(rx)), S_BAD_MSG);

        EXPECT_EQ(command(R_PCM_RELEASE, STREAM_TX), S_OK);
        EXPECT_EQ(command(R_PCM_RELEASE, STREAM_RX), S_OK);
    }
};

TEST(virtio, sound) {
    virtio_sound_stim stim;
    EXPECT_STREQ(stim.sound.kind(), "vcml::virtio::sound");
    sc_core::sc_start();
}
