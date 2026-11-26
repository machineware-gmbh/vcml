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

#include <filesystem>

enum headset_ids : u8 {
    IFX_CONTROL = 0,
    IFX_PLAYBACK = 1,
    IFX_CAPTURE = 2,
    EP_PLAYBACK = 1,
    EP_CAPTURE = 2,
    UNIT_PLAYBACK = 2,
    UNIT_CAPTURE = 5,
    CTRL_MUTE = 1,
    CTRL_VOLUME = 2,
    REQ_SET_CUR = 0x01,
    REQ_SET_MIN = 0x02,
    REQ_GET_CUR = 0x81,
    REQ_GET_MIN = 0x82,
    REQ_GET_MAX = 0x83,
    REQ_GET_RES = 0x84,
    DST_HEADER = 1,
    DST_INPUT_TERMINAL = 2,
    DST_OUTPUT_TERMINAL = 3,
    DST_FEATURE_UNIT = 6,
    DST_AS_GENERAL = 1,
    DST_FORMAT_TYPE = 2,
};

static const u32 SAMPLE_RATE = 48000;
static const size_t CAPTURE_SIZE = 1000;

static string temp_file_path(const string& name) {
    string file = mkstr("vcml_usb_headset_%d_%s.wav", mwr::getpid(),
                        name.c_str());
    return (std::filesystem::path(mwr::temp_dir()) / file).generic_string();
}

static string wav_property(const string& path) {
    string s = "wav:" + escape(path, ":");
    replace(s, "\\", "\\\\\\\\");
    return s;
}

static vector<u8> read_file(const string& path) {
    ifstream file(path.c_str(), std::ios::binary);
    EXPECT_TRUE(file.is_open()) << "cannot open " << path;
    return vector<u8>(std::istreambuf_iterator<char>(file),
                      std::istreambuf_iterator<char>());
}

static void remove_file(const string& path) {
    std::error_code ec;
    std::filesystem::remove(path, ec);
    EXPECT_FALSE(std::filesystem::exists(path)) << "cannot remove " << path;
}

static u16 get_u16(const vector<u8>& data, size_t off) {
    return (u16)(data[off] | data[off + 1] << 8);
}

static u32 get_u24(const vector<u8>& data, size_t off) {
    return (u32)get_u16(data, off) | (u32)data[off + 2] << 16;
}

static u32 get_u32(const vector<u8>& data, size_t off) {
    return (u32)get_u16(data, off) | (u32)get_u16(data, off + 2) << 16;
}

static void put_u16(vector<u8>& data, u16 val) {
    data.push_back(val & 0xff);
    data.push_back(val >> 8);
}

static void put_u32(vector<u8>& data, u32 val) {
    put_u16(data, val & 0xffff);
    put_u16(data, val >> 16);
}

static void put_tag(vector<u8>& data, const string& tag) {
    data.insert(data.end(), tag.begin(), tag.end());
}

static string get_tag(const vector<u8>& data, size_t off) {
    return string(data.begin() + off, data.begin() + off + 4);
}

static vector<u8> make_samples(size_t size, u8 seed) {
    vector<u8> data(size);
    for (size_t i = 0; i < size; i++)
        data[i] = (u8)(seed + i * 7);
    return data;
}

static vector<u8> slice(const vector<u8>& data, size_t off, size_t len) {
    return vector<u8>(data.begin() + off, data.begin() + off + len);
}

static void append(vector<u8>& a, const vector<u8>& b) {
    a.insert(a.end(), b.begin(), b.end());
}

static vector<u8> make_wav(u16 channels, const vector<u8>& data) {
    vector<u8> wav;
    put_tag(wav, "RIFF");
    put_u32(wav, (u32)data.size() + 36);
    put_tag(wav, "WAVE");
    put_tag(wav, "fmt ");
    put_u32(wav, 16);
    put_u16(wav, 1);
    put_u16(wav, channels);
    put_u32(wav, SAMPLE_RATE);
    put_u32(wav, SAMPLE_RATE * channels * 2);
    put_u16(wav, channels * 2);
    put_u16(wav, 16);
    put_tag(wav, "data");
    put_u32(wav, (u32)data.size());
    append(wav, data);
    return wav;
}

static void expect_playback(const string& path, const vector<u8>& data) {
    vector<u8> wav = read_file(path);
    ASSERT_GE(wav.size(), 44u) << path << " has no wav header";
    EXPECT_EQ(get_tag(wav, 0), "RIFF");
    EXPECT_EQ(get_u32(wav, 4), wav.size() - 8);
    EXPECT_EQ(get_tag(wav, 8), "WAVE");
    EXPECT_EQ(get_tag(wav, 12), "fmt ");
    EXPECT_EQ(get_u32(wav, 16), 16u);
    EXPECT_EQ(get_u16(wav, 20), 1u);
    EXPECT_EQ(get_u16(wav, 22), 2u);
    EXPECT_EQ(get_u32(wav, 24), SAMPLE_RATE);
    EXPECT_EQ(get_u32(wav, 28), SAMPLE_RATE * 4);
    EXPECT_EQ(get_u16(wav, 32), 4u);
    EXPECT_EQ(get_u16(wav, 34), 16u);
    EXPECT_EQ(get_tag(wav, 36), "data");
    EXPECT_EQ(get_u32(wav, 40), data.size());
    EXPECT_EQ(slice(wav, 44, wav.size() - 44), data);
}

static vector<vector<u8>> split_descriptors(const vector<u8>& data) {
    vector<vector<u8>> descs;
    size_t pos = 0;
    while (pos < data.size()) {
        size_t len = data[pos];
        EXPECT_GE(len, 2u) << "invalid descriptor at offset " << pos;
        EXPECT_LE(pos + len, data.size()) << "truncated descriptor";
        if (len < 2 || pos + len > data.size())
            break;
        descs.push_back(slice(data, pos, len));
        pos += len;
    }

    EXPECT_EQ(pos, data.size());
    return descs;
}

class headset_test : public test_base
{
public:
    usb_stim stim;
    usb::headset headset;

    string output_path;
    vector<u8> capture_data;

    headset_test(const sc_module_name& nm, const string& outpath,
                 const vector<u8>& capture):
        test_base(nm),
        stim("stim"),
        headset("headset"),
        output_path(outpath),
        capture_data(capture) {
        stim.usb_out.bind(headset.usb_in);
        add_test("test_strings", &headset_test::test_strings);
        add_test("test_properties", &headset_test::test_properties);
        add_test("test_device_desc", &headset_test::test_device_desc);
        add_test("test_config_desc", &headset_test::test_config_desc);
        add_test("test_interfaces", &headset_test::test_interfaces);
        add_test("test_mute_requests", &headset_test::test_mute_requests);
        add_test("test_invalid_requests",
                 &headset_test::test_invalid_requests);
        add_test("test_playback", &headset_test::test_playback);
        add_test("test_playback_muted", &headset_test::test_playback_muted);
        add_test("test_playback_inactive",
                 &headset_test::test_playback_inactive);
        add_test("test_capture", &headset_test::test_capture);
        add_test("test_capture_eof", &headset_test::test_capture_eof);
        add_test("test_capture_muted", &headset_test::test_capture_muted);
        add_test("test_capture_inactive",
                 &headset_test::test_capture_inactive);
        add_test("test_endpoints", &headset_test::test_endpoints);
        add_test("test_reset", &headset_test::test_reset);
    }

    void enumerate() {
        stim.usb_out.reset_device();
        stim.addr = 0;
        ASSERT_TRUE(success(stim.set_address(3)));
        ASSERT_TRUE(success(stim.set_configuration(1)));
    }

    void stop_streams() {
        EXPECT_SUCCESS(stim.set_interface(IFX_PLAYBACK, 0));
        EXPECT_SUCCESS(stim.set_interface(IFX_CAPTURE, 0));
    }

    usb_result audio_get(u8 req, u8 ctrl, u8 ch, u8 unit, u8 ifx,
                         vector<u8>& data, size_t len = 1) {
        u16 rq = USB_REQ_CLASS | USB_REQ_IFACE | req;
        return stim.control_in(rq, ctrl << 8 | ch, unit << 8 | ifx, data, len);
    }

    usb_result audio_set(u8 req, u8 ctrl, u8 ch, u8 unit, u8 ifx,
                         const vector<u8>& data) {
        u16 rq = USB_REQ_CLASS | USB_REQ_IFACE | req;
        return stim.control_out(rq, ctrl << 8 | ch, unit << 8 | ifx, data);
    }

    usb_result get_mute(u8 unit, u8& muted) {
        vector<u8> data;
        usb_result res = audio_get(REQ_GET_CUR, CTRL_MUTE, 0, unit,
                                   IFX_CONTROL, data);
        muted = data[0];
        return res;
    }

    usb_result set_mute(u8 unit, u8 muted) {
        return audio_set(REQ_SET_CUR, CTRL_MUTE, 0, unit, IFX_CONTROL,
                         { muted });
    }

    u8 get_interface(u16 ifx) {
        vector<u8> data;
        u16 rq = USB_REQ_IFACE | USB_REQ_GET_INTERFACE;
        EXPECT_SUCCESS(stim.control_in(rq, 0, ifx, data, 1));
        return data[0];
    }

    void play(const vector<u8>& data, size_t packet) {
        for (size_t pos = 0; pos < data.size(); pos += packet) {
            size_t len = min(packet, data.size() - pos);
            EXPECT_SUCCESS(stim.out(EP_PLAYBACK, data.data() + pos, len));
        }
    }

    vector<u8> record(size_t size, size_t packet) {
        vector<u8> result;
        for (size_t pos = 0; pos < size; pos += packet) {
            vector<u8> data;
            EXPECT_SUCCESS(stim.in(EP_CAPTURE, data, min(packet, size - pos)));
            append(result, data);
        }
        return result;
    }

    void test_strings() {
        EXPECT_STREQ(headset.kind(), "vcml::usb::headset");
        EXPECT_STREQ(headset.version(), VCML_VERSION_STRING);
    }

    void test_properties() {
        EXPECT_EQ(headset.vendorid.get(), 0x1234);
        EXPECT_EQ(headset.productid.get(), 0x1);
        EXPECT_EQ(headset.manufacturer.get(), "MachineWare GmbH");
        EXPECT_EQ(headset.product.get(), "VCML headset");
        EXPECT_EQ(headset.serialno.get(), VCML_GIT_REV_SHORT);
        EXPECT_EQ(headset.keymap.get(), "us");
        EXPECT_TRUE(headset.start_attached.get());
        EXPECT_TRUE(headset.usb_in.is_attached());
        EXPECT_EQ(headset.usb_in.connection_speed(), USB_SPEED_HIGH);
        EXPECT_EQ(stim.attached, 1u);
        EXPECT_EQ(stim.detached, 0u);
    }

    void test_device_desc() {
        enumerate();

        vector<u8> desc;
        ASSERT_TRUE(success(stim.get_descriptor(USB_DT_DEVICE, 0, desc, 18)));
        EXPECT_EQ(desc[0], 18);
        EXPECT_EQ(desc[1], USB_DT_DEVICE);
        EXPECT_EQ(get_u16(desc, 2), 0x200);
        EXPECT_EQ(desc[4], 0);
        EXPECT_EQ(desc[5], 0);
        EXPECT_EQ(desc[6], 0);
        EXPECT_EQ(desc[7], 64);
        EXPECT_EQ(get_u16(desc, 8), 0x1234);
        EXPECT_EQ(get_u16(desc, 10), 0x1);
        EXPECT_EQ(desc[17], 1);

        EXPECT_EQ(stim.get_string(desc[14]), headset.manufacturer.get());
        EXPECT_EQ(stim.get_string(desc[15]), headset.product.get());
        EXPECT_EQ(stim.get_string(desc[16]), headset.serialno.get());

        vector<u8> status;
        u16 rq = USB_REQ_DEVICE | USB_REQ_GET_STATUS;
        ASSERT_TRUE(success(stim.control_in(rq, 0, 0, status, 2)));
        EXPECT_EQ(status[0], 2);
        EXPECT_EQ(status[1], 0);
    }

    void test_config_desc() {
        enumerate();

        vector<u8> hdr;
        ASSERT_TRUE(success(stim.get_descriptor(USB_DT_CONFIG, 0, hdr, 9)));
        u16 total = get_u16(hdr, 2);
        EXPECT_EQ(hdr[0], 9);
        EXPECT_EQ(hdr[1], USB_DT_CONFIG);
        EXPECT_EQ(hdr[4], 3);
        EXPECT_EQ(hdr[5], 1);
        EXPECT_EQ(hdr[7], USB_CFG_ONE | USB_CFG_REMOTE_WAKEUP);
        EXPECT_EQ(hdr[8], 250);

        vector<u8> cfg;
        ASSERT_TRUE(
            success(stim.get_descriptor(USB_DT_CONFIG, 0, cfg, total)));
        vector<vector<u8>> descs = split_descriptors(cfg);
        ASSERT_FALSE(descs.empty());

        std::map<u8, vector<u8>> entities;
        vector<u8> ac_header;
        size_t ac_length = 0;
        int ifx = -1;
        int alt = -1;
        std::map<int, vector<u8>> as_general, as_format, ep_desc, ep_cs;
        std::map<std::pair<int, int>, u8> strids;

        for (const auto& d : descs) {
            switch (d[1]) {
            case USB_DT_CONFIG:
                EXPECT_EQ(&d, &descs.front());
                break;

            case USB_DT_INTERFACE:
                ASSERT_EQ(d.size(), 9u);
                ifx = d[2];
                alt = d[3];
                strids[{ ifx, alt }] = d[8];
                EXPECT_EQ(d[5], USB_CLASS_AUDIO);
                EXPECT_EQ(d[6], ifx == IFX_CONTROL ? 1 : 2);
                EXPECT_EQ(d[4], ifx == IFX_CONTROL || alt == 0 ? 0 : 1);
                break;

            case USB_DT_CS_INTERFACE:
                ASSERT_GE(d.size(), 3u);
                if (ifx == IFX_CONTROL) {
                    ac_length += d.size();
                    if (d[2] == DST_HEADER)
                        ac_header = d;
                    else
                        entities[d[3]] = d;
                } else if (d[2] == DST_AS_GENERAL) {
                    as_general[ifx] = d;
                } else if (d[2] == DST_FORMAT_TYPE) {
                    as_format[ifx] = d;
                } else {
                    ADD_FAILURE() << "unexpected AS subtype " << (int)d[2];
                }
                break;

            case USB_DT_ENDPOINT:
                ep_desc[ifx] = d;
                break;

            case USB_DT_CS_ENDPOINT:
                ep_cs[ifx] = d;
                break;

            default:
                ADD_FAILURE() << "unexpected descriptor " << (int)d[1];
            }
        }

        EXPECT_EQ(cfg.size(), 193u);
        EXPECT_EQ(strids.size(), 5u);
        EXPECT_EQ(stim.get_string(strids[{ 0, 0 }]), "Audio Configuration");
        EXPECT_EQ(stim.get_string(strids[{ 1, 0 }]), "Audio Playback - Off");
        EXPECT_EQ(stim.get_string(strids[{ 1, 1 }]),
                  "Audio Playback - 48kHz Stereo");
        EXPECT_EQ(stim.get_string(strids[{ 2, 0 }]), "Audio Capture - Off");
        EXPECT_EQ(stim.get_string(strids[{ 2, 1 }]),
                  "Audio Capture - 48kHz Mono");

        ASSERT_EQ(ac_header.size(), 10u);
        EXPECT_EQ(get_u16(ac_header, 3), 0x100);
        EXPECT_EQ(get_u16(ac_header, 5), ac_length);
        EXPECT_EQ(ac_header[7], 2);
        EXPECT_EQ(ac_header[8], IFX_PLAYBACK);
        EXPECT_EQ(ac_header[9], IFX_CAPTURE);

        ASSERT_EQ(entities.size(), 6u);
        std::map<u8, u8> channels;
        for (const auto& [id, d] : entities) {
            if (d[2] == DST_INPUT_TERMINAL) {
                ASSERT_EQ(d.size(), 12u);
                channels[id] = d[7];
            }
        }

        for (const auto& [id, d] : entities) {
            switch (d[2]) {
            case DST_INPUT_TERMINAL:
                break;
            case DST_FEATURE_UNIT: {
                ASSERT_TRUE(channels.count(d[4])) << (int)id;
                u8 csize = d[5];
                ASSERT_EQ(csize, 1);
                EXPECT_EQ(d.size(), 7u + (channels[d[4]] + 1) * csize);
                EXPECT_EQ(d[6], 1u << (CTRL_MUTE - 1));
                for (size_t i = 7; i < d.size() - 1; i++)
                    EXPECT_EQ(d[i], 0) << "channel control " << i - 6;
                channels[id] = channels[d[4]];
                break;
            }
            case DST_OUTPUT_TERMINAL:
                break;
            default:
                ADD_FAILURE() << "unexpected AC subtype " << (int)d[2];
            }
        }

        const vector<u8>& pit = entities[1];
        const vector<u8>& pfu = entities[UNIT_PLAYBACK];
        const vector<u8>& pot = entities[3];
        const vector<u8>& cit = entities[4];
        const vector<u8>& cfu = entities[UNIT_CAPTURE];
        const vector<u8>& cot = entities[6];
        ASSERT_EQ(pit.size(), 12u);
        ASSERT_EQ(pfu.size(), 10u);
        ASSERT_EQ(pot.size(), 9u);
        ASSERT_EQ(cit.size(), 12u);
        ASSERT_EQ(cfu.size(), 9u);
        ASSERT_EQ(cot.size(), 9u);

        EXPECT_EQ(get_u16(pit, 4), 0x0101);
        EXPECT_EQ(pit[7], 2);
        EXPECT_EQ(get_u16(pit, 8), 0x0003);
        EXPECT_EQ(pfu[4], pit[3]);
        EXPECT_EQ(pot[2], DST_OUTPUT_TERMINAL);
        EXPECT_EQ(get_u16(pot, 4), 0x0302);
        EXPECT_EQ(pot[7], pfu[3]);

        EXPECT_EQ(get_u16(cit, 4), 0x0201);
        EXPECT_EQ(cit[7], 1);
        EXPECT_EQ(cfu[4], cit[3]);
        EXPECT_EQ(cot[2], DST_OUTPUT_TERMINAL);
        EXPECT_EQ(get_u16(cot, 4), 0x0101);
        EXPECT_EQ(cot[7], cfu[3]);

        for (int i : { (int)IFX_PLAYBACK, (int)IFX_CAPTURE }) {
            bool playback = i == IFX_PLAYBACK;
            u8 nch = playback ? 2 : 1;
            const vector<u8>& gen = as_general[i];
            const vector<u8>& fmt = as_format[i];
            const vector<u8>& ep = ep_desc[i];
            const vector<u8>& cs = ep_cs[i];
            ASSERT_EQ(gen.size(), 7u);
            ASSERT_EQ(fmt.size(), 11u);
            ASSERT_EQ(ep.size(), 9u);
            ASSERT_EQ(cs.size(), 7u);

            EXPECT_EQ(gen[3], playback ? pit[3] : cot[3]);
            EXPECT_EQ(get_u16(gen, 5), 1u);
            EXPECT_EQ(fmt[3], 1);
            EXPECT_EQ(fmt[4], nch);
            EXPECT_EQ(fmt[5], 2);
            EXPECT_EQ(fmt[6], 16);
            EXPECT_EQ(fmt[7], 1);
            EXPECT_EQ(get_u24(fmt, 8), SAMPLE_RATE);
            EXPECT_EQ(fmt[4], playback ? pit[7] : cit[7]);

            u8 addr = playback ? usb_ep_out(EP_PLAYBACK)
                               : usb_ep_in(EP_CAPTURE);
            EXPECT_EQ(ep[2], addr);
            EXPECT_EQ(ep[3] & 3, USB_EP_ISOC);
            EXPECT_GE(get_u16(ep, 4), SAMPLE_RATE / 1000 * nch * 2);
            EXPECT_EQ(ep[6], 1);
            EXPECT_EQ(ep[7], 0);
            EXPECT_EQ(ep[8], 0);

            EXPECT_EQ(cs[1], USB_DT_CS_ENDPOINT);
            EXPECT_EQ(cs[2], 1);
            EXPECT_EQ(cs[3], 0);
        }
    }

    void test_interfaces() {
        stim.usb_out.reset_device();
        stim.addr = 0;
        ASSERT_TRUE(success(stim.set_address(3)));
        EXPECT_FAILURE(stim.set_interface(IFX_PLAYBACK, 1));

        ASSERT_TRUE(success(stim.set_configuration(1)));
        vector<u8> data;
        u16 rq = USB_REQ_DEVICE | USB_REQ_GET_CONFIGURATION;
        ASSERT_TRUE(success(stim.control_in(rq, 0, 0, data, 1)));
        EXPECT_EQ(data[0], 1);

        EXPECT_EQ(get_interface(IFX_PLAYBACK), 0);
        EXPECT_EQ(get_interface(IFX_CAPTURE), 0);

        EXPECT_SUCCESS(stim.set_interface(IFX_CONTROL, 0));
        EXPECT_SUCCESS(stim.set_interface(IFX_PLAYBACK, 1));
        EXPECT_SUCCESS(stim.set_interface(IFX_CAPTURE, 1));
        EXPECT_EQ(get_interface(IFX_CONTROL), 0);
        EXPECT_EQ(get_interface(IFX_PLAYBACK), 1);
        EXPECT_EQ(get_interface(IFX_CAPTURE), 1);

        EXPECT_FAILURE(stim.set_interface(IFX_CONTROL, 1));
        EXPECT_FAILURE(stim.set_interface(IFX_PLAYBACK, 2));
        EXPECT_FAILURE(stim.set_interface(IFX_CAPTURE, 2));
        EXPECT_FAILURE(stim.set_interface(3, 0));
        EXPECT_EQ(get_interface(IFX_PLAYBACK), 1);
        EXPECT_EQ(get_interface(IFX_CAPTURE), 1);

        EXPECT_SUCCESS(stim.set_interface(IFX_PLAYBACK, 0));
        EXPECT_SUCCESS(stim.set_interface(IFX_CAPTURE, 0));
        EXPECT_EQ(get_interface(IFX_PLAYBACK), 0);
        EXPECT_EQ(get_interface(IFX_CAPTURE), 0);
    }

    void test_mute_requests() {
        enumerate();

        for (u8 unit : { (u8)UNIT_PLAYBACK, (u8)UNIT_CAPTURE }) {
            u8 other = unit == UNIT_PLAYBACK ? UNIT_CAPTURE : UNIT_PLAYBACK;
            u8 muted = 0xff;
            EXPECT_SUCCESS(get_mute(unit, muted));
            EXPECT_EQ(muted, 0);

            EXPECT_SUCCESS(set_mute(unit, 1));
            EXPECT_SUCCESS(get_mute(unit, muted));
            EXPECT_EQ(muted, 1);
            EXPECT_SUCCESS(get_mute(other, muted));
            EXPECT_EQ(muted, 0);

            EXPECT_SUCCESS(set_mute(unit, 1));
            EXPECT_SUCCESS(get_mute(unit, muted));
            EXPECT_EQ(muted, 1);

            EXPECT_SUCCESS(set_mute(unit, 0));
            EXPECT_SUCCESS(get_mute(unit, muted));
            EXPECT_EQ(muted, 0);

            EXPECT_SUCCESS(set_mute(unit, 0x80));
            EXPECT_SUCCESS(get_mute(unit, muted));
            EXPECT_EQ(muted, 1);

            EXPECT_SUCCESS(set_mute(unit, 0));
        }
    }

    void test_invalid_requests() {
        enumerate();

        vector<u8> data;
        for (u8 req : { REQ_GET_MIN, REQ_GET_MAX, REQ_GET_RES }) {
            EXPECT_FAILURE(audio_get(req, CTRL_MUTE, 0, UNIT_PLAYBACK,
                                     IFX_CONTROL, data));
        }

        EXPECT_FAILURE(audio_set(REQ_SET_MIN, CTRL_MUTE, 0, UNIT_PLAYBACK,
                                 IFX_CONTROL, { 1 }));
        EXPECT_FAILURE(audio_get(REQ_GET_CUR, CTRL_VOLUME, 0, UNIT_PLAYBACK,
                                 IFX_CONTROL, data, 2));
        EXPECT_FAILURE(audio_set(REQ_SET_CUR, CTRL_VOLUME, 0, UNIT_CAPTURE,
                                 IFX_CONTROL, { 0, 0 }));
        EXPECT_FAILURE(audio_get(REQ_GET_CUR, CTRL_MUTE, 1, UNIT_PLAYBACK,
                                 IFX_CONTROL, data));
        EXPECT_FAILURE(audio_set(REQ_SET_CUR, CTRL_MUTE, 2, UNIT_PLAYBACK,
                                 IFX_CONTROL, { 1 }));
        EXPECT_FAILURE(audio_set(REQ_SET_CUR, CTRL_MUTE, 1, UNIT_CAPTURE,
                                 IFX_CONTROL, { 1 }));
        EXPECT_FAILURE(
            audio_get(REQ_GET_CUR, CTRL_MUTE, 0, 1, IFX_CONTROL, data));
        EXPECT_FAILURE(
            audio_set(REQ_SET_CUR, CTRL_MUTE, 0, 9, IFX_CONTROL, { 1 }));
        EXPECT_FAILURE(audio_get(REQ_GET_CUR, CTRL_MUTE, 0, UNIT_CAPTURE,
                                 IFX_CAPTURE, data));
        EXPECT_FAILURE(audio_set(REQ_SET_CUR, CTRL_MUTE, 0, UNIT_PLAYBACK,
                                 IFX_PLAYBACK, { 1 }));
        EXPECT_FAILURE(audio_get(REQ_GET_CUR, CTRL_MUTE, 0, UNIT_PLAYBACK,
                                 IFX_CONTROL, data, 0));
        EXPECT_FAILURE(audio_set(REQ_SET_CUR, CTRL_MUTE, 0, UNIT_PLAYBACK,
                                 IFX_CONTROL, {}));

        u16 rq = USB_REQ_CLASS | USB_REQ_IFACE | USB_REQ_GET_STATUS;
        EXPECT_FAILURE(stim.control_in(rq, 0, 0, data, 2));

        rq = USB_REQ_CLASS | USB_REQ_ENDPOINT | REQ_SET_CUR;
        u16 idx = usb_ep_out(EP_PLAYBACK);
        EXPECT_FAILURE(stim.control_out(rq, 0x100, idx, { 0x80, 0xbb, 0 }));
        rq = USB_REQ_CLASS | USB_REQ_ENDPOINT | REQ_GET_CUR;
        idx = usb_ep_in(EP_CAPTURE);
        EXPECT_FAILURE(stim.control_in(rq, 0x100, idx, data, 3));

        u8 muted = 0xff;
        EXPECT_SUCCESS(get_mute(UNIT_PLAYBACK, muted));
        EXPECT_EQ(muted, 0);
        EXPECT_SUCCESS(get_mute(UNIT_CAPTURE, muted));
        EXPECT_EQ(muted, 0);

        stim.usb_out.reset_device();
        stim.addr = 0;
        ASSERT_TRUE(success(stim.set_address(3)));
        EXPECT_FAILURE(get_mute(UNIT_PLAYBACK, muted));
        EXPECT_FAILURE(set_mute(UNIT_CAPTURE, 1));
    }

    void test_playback() {
        enumerate();
        ASSERT_TRUE(success(stim.set_interface(IFX_PLAYBACK, 1)));

        vector<u8> small = make_samples(8 * 24, 0x11);
        vector<u8> large = make_samples(2 * 192 + 100, 0x42);
        play(small, 24);
        play(large, 192);

        ASSERT_TRUE(success(stim.set_interface(IFX_PLAYBACK, 0)));

        vector<u8> expect = small;
        append(expect, large);
        expect_playback(output_path, expect);
    }

    void test_playback_muted() {
        enumerate();
        ASSERT_TRUE(success(stim.set_interface(IFX_PLAYBACK, 1)));

        vector<u8> a = make_samples(192, 0x01);
        vector<u8> b = make_samples(192, 0x02);
        vector<u8> c = make_samples(192, 0x03);

        play(a, 192);
        ASSERT_TRUE(success(set_mute(UNIT_PLAYBACK, 1)));
        play(b, 192);
        ASSERT_TRUE(success(set_mute(UNIT_PLAYBACK, 0)));
        play(c, 192);

        ASSERT_TRUE(success(stim.set_interface(IFX_PLAYBACK, 0)));

        vector<u8> expect = a;
        append(expect, c);
        expect_playback(output_path, expect);
    }

    void test_playback_inactive() {
        vector<u8> a = make_samples(96, 0x21);
        vector<u8> b = make_samples(96, 0x22);

        stim.usb_out.reset_device();
        stim.addr = 0;
        ASSERT_TRUE(success(stim.set_address(3)));
        play(a, 96);

        ASSERT_TRUE(success(stim.set_configuration(1)));
        play(a, 96);

        ASSERT_TRUE(success(stim.set_interface(IFX_PLAYBACK, 1)));
        play(b, 96);
        ASSERT_TRUE(success(stim.set_interface(IFX_PLAYBACK, 0)));
        play(a, 96);

        expect_playback(output_path, b);
    }

    void test_capture() {
        enumerate();
        ASSERT_TRUE(success(stim.set_interface(IFX_CAPTURE, 1)));

        size_t uframe = SAMPLE_RATE / 8000 * 2;
        size_t frame = SAMPLE_RATE / 1000 * 2;
        EXPECT_EQ(record(10 * uframe, uframe),
                  slice(capture_data, 0, 10 * uframe));
        EXPECT_EQ(record(5 * frame, frame),
                  slice(capture_data, 10 * uframe, 5 * frame));

        ASSERT_TRUE(success(stim.set_interface(IFX_CAPTURE, 0)));
        ASSERT_TRUE(success(stim.set_interface(IFX_CAPTURE, 1)));
        EXPECT_EQ(record(frame, frame), slice(capture_data, 0, frame));
        ASSERT_TRUE(success(stim.set_interface(IFX_CAPTURE, 0)));
    }

    void test_capture_eof() {
        enumerate();
        ASSERT_TRUE(success(stim.set_interface(IFX_CAPTURE, 1)));

        size_t frame = SAMPLE_RATE / 1000 * 2;
        size_t full = CAPTURE_SIZE / frame;
        size_t rest = CAPTURE_SIZE % frame;
        ASSERT_NE(rest, 0u);

        EXPECT_EQ(record(full * frame, frame),
                  slice(capture_data, 0, full * frame));

        vector<u8> expect = slice(capture_data, full * frame, rest);
        expect.resize(frame, 0);
        EXPECT_EQ(record(frame, frame), expect);
        EXPECT_EQ(record(frame, frame), slice(capture_data, 0, frame));

        ASSERT_TRUE(success(stim.set_interface(IFX_CAPTURE, 0)));
    }

    void test_capture_muted() {
        enumerate();
        ASSERT_TRUE(success(stim.set_interface(IFX_CAPTURE, 1)));

        size_t frame = SAMPLE_RATE / 1000 * 2;
        EXPECT_EQ(record(frame, frame), slice(capture_data, 0, frame));

        ASSERT_TRUE(success(set_mute(UNIT_CAPTURE, 1)));
        EXPECT_EQ(record(2 * frame, frame / 2), vector<u8>(2 * frame, 0));

        ASSERT_TRUE(success(set_mute(UNIT_PLAYBACK, 1)));
        ASSERT_TRUE(success(set_mute(UNIT_CAPTURE, 0)));
        EXPECT_EQ(record(frame, frame), slice(capture_data, frame, frame));
        ASSERT_TRUE(success(set_mute(UNIT_PLAYBACK, 0)));

        ASSERT_TRUE(success(stim.set_interface(IFX_CAPTURE, 0)));
    }

    void test_capture_inactive() {
        size_t frame = SAMPLE_RATE / 1000 * 2;
        vector<u8> silence(frame, 0);

        stim.usb_out.reset_device();
        stim.addr = 0;
        ASSERT_TRUE(success(stim.set_address(3)));
        EXPECT_EQ(record(frame, frame), silence);

        ASSERT_TRUE(success(stim.set_configuration(1)));
        EXPECT_EQ(record(frame, frame), silence);

        ASSERT_TRUE(success(stim.set_interface(IFX_CAPTURE, 1)));
        EXPECT_EQ(record(frame, frame), slice(capture_data, 0, frame));
        ASSERT_TRUE(success(stim.set_interface(IFX_CAPTURE, 0)));
        EXPECT_EQ(record(frame, frame), silence);
    }

    void test_endpoints() {
        enumerate();
        ASSERT_TRUE(success(stim.set_interface(IFX_PLAYBACK, 1)));
        ASSERT_TRUE(success(stim.set_interface(IFX_CAPTURE, 1)));

        vector<u8> data = make_samples(96, 0x55);
        vector<u8> buf;
        EXPECT_EQ(stim.in(EP_PLAYBACK, buf, 96), USB_RESULT_STALL);
        EXPECT_EQ(stim.out(EP_CAPTURE, data), USB_RESULT_STALL);
        EXPECT_EQ(stim.in(3, buf, 96), USB_RESULT_STALL);
        EXPECT_EQ(stim.out(3, data), USB_RESULT_STALL);

        EXPECT_EQ(get_interface(IFX_PLAYBACK), 1);

        stop_streams();
        expect_playback(output_path, {});
    }

    void test_reset() {
        enumerate();
        ASSERT_TRUE(success(stim.set_interface(IFX_PLAYBACK, 1)));
        ASSERT_TRUE(success(stim.set_interface(IFX_CAPTURE, 1)));
        ASSERT_TRUE(success(set_mute(UNIT_CAPTURE, 1)));

        stim.usb_out.reset_device();
        stim.addr = 0;
        EXPECT_FAILURE(stim.set_interface(IFX_PLAYBACK, 0));
        u8 muted = 0;
        EXPECT_FAILURE(get_mute(UNIT_CAPTURE, muted));

        ASSERT_TRUE(success(stim.set_address(4)));
        EXPECT_FAILURE(stim.set_interface(IFX_PLAYBACK, 0));
        ASSERT_TRUE(success(stim.set_configuration(1)));
        EXPECT_EQ(get_interface(IFX_PLAYBACK), 0);
        EXPECT_EQ(get_interface(IFX_CAPTURE), 0);

        stop_streams();
        ASSERT_TRUE(success(set_mute(UNIT_CAPTURE, 0)));
    }
};

TEST(usb_headset, simulate) {
    string output = temp_file_path("output");
    string input = temp_file_path("input");
    vector<u8> capture = make_samples(CAPTURE_SIZE, 0x30);

    {
        ofstream file(input.c_str(), std::ios::binary | std::ios::trunc);
        ASSERT_TRUE(file.is_open()) << "cannot create " << input;
        vector<u8> wav = make_wav(1, capture);
        file.write((const char*)wav.data(), wav.size());
    }

    broker broker("test");
    broker.define("harness.headset.output.drivers", wav_property(output));
    broker.define("harness.headset.input.drivers", wav_property(input));
    broker.define("harness.headset.vendorid", "4660");

    {
        headset_test test("harness", output, capture);
        sc_core::sc_start();
    }

    remove_file(output);
    remove_file(input);
}
