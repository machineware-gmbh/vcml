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

#include <chrono>
#include <filesystem>
#include <thread>

typedef std::unique_ptr<audio::driver> driver_ptr;

enum wav_format_tag : u16 {
    WAV_TAG_PCM = 1,
    WAV_TAG_FLOAT = 3,
    WAV_TAG_ALAW = 6,
};

struct wav_params {
    u32 format;
    u16 tag;
    u16 bits;
    u16 channels;
    u32 rate;
};

static const wav_params WAV_PARAMS[] = {
    { audio::FORMAT_U8, WAV_TAG_PCM, 8, 1, 8000 },
    { audio::FORMAT_S16LE, WAV_TAG_PCM, 16, 2, 44100 },
    { audio::FORMAT_S32LE, WAV_TAG_PCM, 32, 2, 48000 },
    { audio::FORMAT_F32LE, WAV_TAG_FLOAT, 32, 1, 22050 },
    { audio::FORMAT_S16LE, WAV_TAG_PCM, 16, 1, 96000 },
};

static const u32 ALL_FORMATS[] = {
    audio::FORMAT_U8,    audio::FORMAT_S8,    audio::FORMAT_U16LE,
    audio::FORMAT_U16BE, audio::FORMAT_S16LE, audio::FORMAT_S16BE,
    audio::FORMAT_U32LE, audio::FORMAT_U32BE, audio::FORMAT_S32LE,
    audio::FORMAT_S32BE, audio::FORMAT_F32LE, audio::FORMAT_F32BE,
};

static bool is_wav_format(u32 format) {
    return format == audio::FORMAT_U8 || format == audio::FORMAT_S16LE ||
           format == audio::FORMAT_S32LE || format == audio::FORMAT_F32LE;
}

class log_capture : public mwr::publisher
{
private:
    mutable mutex m_mtx;
    vector<string> m_lines;

public:
    log_capture(): mwr::publisher(LOG_ERROR, LOG_DEBUG), m_mtx(), m_lines() {}
    virtual ~log_capture() = default;

    virtual void publish(const mwr::logmsg& msg) override {
        lock_guard<mutex> guard(m_mtx);
        for (const string& line : msg.lines)
            m_lines.push_back(line);
    }

    size_t count(const string& text) const {
        lock_guard<mutex> guard(m_mtx);
        size_t n = 0;
        for (const string& line : m_lines) {
            if (line.find(text) != string::npos)
                n++;
        }
        return n;
    }
};

static string temp_file_path(const string& name) {
    static size_t count = 0;
    string file = mkstr("vcml_audio_%d_%zu_%s.wav", mwr::getpid(), count++,
                        name.c_str());
    return (std::filesystem::path(mwr::temp_dir()) / file).generic_string();
}

static string wav_driver(const string& path) {
    return "wav:" + escape(path, ":");
}

static string double_backslashes(string s) {
    replace(s, "\\", "\\\\");
    return s;
}

static string wav_property(const string& path) {
    return double_backslashes(wav_driver(path));
}

static vector<u8> read_file(const string& path) {
    ifstream file(path.c_str(), std::ios::binary);
    EXPECT_TRUE(file.is_open()) << "cannot open " << path;
    return vector<u8>(std::istreambuf_iterator<char>(file),
                      std::istreambuf_iterator<char>());
}

static void write_file(const string& path, const vector<u8>& data) {
    ofstream file(path.c_str(), std::ios::binary | std::ios::trunc);
    ASSERT_TRUE(file.is_open()) << "cannot create " << path;
    file.write((const char*)data.data(), data.size());
    ASSERT_TRUE(file.good()) << "cannot write " << path;
}

static void remove_file(const string& path) {
    std::error_code ec;
    std::filesystem::remove(path, ec);
    EXPECT_FALSE(std::filesystem::exists(path)) << "cannot remove " << path;
}

static u16 get_u16(const vector<u8>& data, size_t off) {
    return (u16)(data[off] | data[off + 1] << 8);
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

static vector<u8> make_silence(size_t size, u32 format) {
    vector<u8> data(size);
    audio::fill_silence(data.data(), data.size(), format);
    return data;
}

static vector<u8> slice(const vector<u8>& data, size_t off, size_t len) {
    return vector<u8>(data.begin() + off, data.begin() + off + len);
}

static vector<u8> concat(const vector<u8>& a, const vector<u8>& b) {
    vector<u8> res(a);
    res.insert(res.end(), b.begin(), b.end());
    return res;
}

static vector<u8> make_wav(const wav_params& params, const vector<u8>& data) {
    vector<u8> wav;
    put_tag(wav, "RIFF");
    put_u32(wav, (u32)data.size() + 36);
    put_tag(wav, "WAVE");
    put_tag(wav, "fmt ");
    put_u32(wav, 16);
    put_u16(wav, params.tag);
    put_u16(wav, params.channels);
    put_u32(wav, params.rate);
    put_u32(wav, params.rate * params.channels * params.bits / 8);
    put_u16(wav, params.channels * params.bits / 8);
    put_u16(wav, params.bits);
    put_tag(wav, "data");
    put_u32(wav, (u32)data.size());
    wav.insert(wav.end(), data.begin(), data.end());
    return wav;
}

static void expect_wav(const string& path, const wav_params& params,
                       const vector<u8>& data) {
    vector<u8> wav = read_file(path);
    ASSERT_GE(wav.size(), 44u) << path << " has no wav header";
    EXPECT_EQ(wav.size(), data.size() + 44);
    EXPECT_EQ(get_tag(wav, 0), "RIFF");
    EXPECT_EQ(get_u32(wav, 4), wav.size() - 8);
    EXPECT_EQ(get_tag(wav, 8), "WAVE");
    EXPECT_EQ(get_tag(wav, 12), "fmt ");
    EXPECT_EQ(get_u32(wav, 16), 16u);
    EXPECT_EQ(get_u16(wav, 20), params.tag);
    EXPECT_EQ(get_u16(wav, 22), params.channels);
    EXPECT_EQ(get_u32(wav, 24), params.rate);
    EXPECT_EQ(get_u32(wav, 28),
              params.rate * params.channels * params.bits / 8);
    EXPECT_EQ(get_u16(wav, 32), params.channels * params.bits / 8);
    EXPECT_EQ(get_u16(wav, 34), params.bits);
    EXPECT_EQ(get_tag(wav, 36), "data");
    EXPECT_EQ(get_u32(wav, 40), wav.size() - 44);
    EXPECT_EQ(slice(wav, 44, wav.size() - 44), data);
}

template <typename PRED>
static bool poll_until(PRED&& pred) {
    using namespace std::chrono;
    auto timeout = steady_clock::now() + seconds(10);
    while (!pred()) {
        if (steady_clock::now() > timeout)
            return false;
        std::this_thread::sleep_for(milliseconds(1));
    }

    return true;
}

static const wav_params STREAM_PARAMS = { audio::FORMAT_S16LE, WAV_TAG_PCM, 16,
                                          2, 44100 };

class audio_harness : public test_base
{
public:
    log_capture& logs;
    string out_path;
    vector<u8> in_data;

    audio::ostream out;
    audio::istream in;
    audio::ostream out_none;
    audio::istream in_none;
    audio::ostream out_bad;
    audio::istream in_bad;

    audio_harness(const sc_module_name& nm, log_capture& capture,
                  const string& outfile, const vector<u8>& indata):
        test_base(nm),
        logs(capture),
        out_path(outfile),
        in_data(indata),
        out("out"),
        in("in"),
        out_none("out_none"),
        in_none("in_none"),
        out_bad("out_bad"),
        in_bad("in_bad") {
        add_test("strings", &audio_harness::test_strings);
        add_test("factory", &audio_harness::test_factory);
        add_test("no_drivers", &audio_harness::test_no_drivers);
        add_test("wav_output", &audio_harness::test_wav_output);
        add_test("wav_output_enable", &audio_harness::test_wav_output_enable);
        add_test("wav_output_reconfigure",
                 &audio_harness::test_wav_output_reconfigure);
        add_test("wav_output_unsupported",
                 &audio_harness::test_wav_output_unsupported);
        add_test("wav_output_stream", &audio_harness::test_wav_output_stream);
        add_test("wav_input", &audio_harness::test_wav_input);
        add_test("wav_input_eof", &audio_harness::test_wav_input_eof);
        add_test("wav_input_reconfigure",
                 &audio_harness::test_wav_input_reconfigure);
        add_test("wav_input_malformed",
                 &audio_harness::test_wav_input_malformed);
        add_test("wav_input_stream", &audio_harness::test_wav_input_stream);
        add_test("sdl_output", &audio_harness::test_sdl_output);
        add_test("sdl_output_reconfigure",
                 &audio_harness::test_sdl_output_reconfigure);
        add_test("sdl_input", &audio_harness::test_sdl_input);
        add_test("sdl_input_reconfigure",
                 &audio_harness::test_sdl_input_reconfigure);
    }

    driver_ptr create_sdl(audio::stream& owner, const string& type) {
        try {
            return driver_ptr(audio::driver::create(owner, type));
        } catch (vcml::report& rep) {
            EXPECT_THAT(rep.message(), HasSubstr("unknown audio driver"));
            log_warn("sdl audio driver not available, test skipped");
            return nullptr;
        }
    }

    void test_strings() {
        EXPECT_STREQ(out.kind(), "vcml::module");
        EXPECT_STREQ(out.version(), VCML_VERSION_STRING);
        EXPECT_STREQ(in.kind(), "vcml::module");
        EXPECT_STREQ(in.version(), VCML_VERSION_STRING);
        EXPECT_EQ(out.drivers.get(), wav_property(out_path));
        EXPECT_EQ(out_none.drivers.get(), "");
        EXPECT_EQ(out_bad.drivers.get(), "nothing");
    }

    void test_factory() {
        EXPECT_THROW(audio::driver::create(out_none, "nothing"), vcml::report);
        EXPECT_THROW(audio::driver::create(in_none, "nothing"), vcml::report);
        EXPECT_THROW(audio::driver::create(out_none, ""), vcml::report);
        EXPECT_THROW(audio::driver::create(out_none, "file:x"), vcml::report);

        try {
            audio::driver::create(out_none, "nothing:else");
            ADD_FAILURE() << "unknown driver did not throw";
        } catch (vcml::report& rep) {
            EXPECT_STREQ(rep.message(),
                         "unknown audio driver \"nothing:else\"");
        }

        EXPECT_EQ(logs.count("unknown audio driver \"nothing\""), 1u);
        EXPECT_EQ(logs.count("unknown audio driver \"missing\""), 1u);

        driver_ptr wav(audio::driver::create(out_none, "wav"));
        ASSERT_TRUE(wav);
        EXPECT_EQ(&wav->log, &out_none.log);
    }

    void test_no_drivers() {
        for (audio::stream* s :
             { (audio::stream*)&out_none, (audio::stream*)&out_bad,
               (audio::stream*)&in_none, (audio::stream*)&in_bad }) {
            EXPECT_EQ(s->min_channels(), 1u) << s->name();
            EXPECT_EQ(s->max_channels(), 8u) << s->name();
            for (u32 format : ALL_FORMATS)
                EXPECT_TRUE(s->supports_format(format)) << s->name();
            EXPECT_FALSE(s->supports_format(audio::FORMAT_INVALID));
            EXPECT_TRUE(s->supports_rate(1)) << s->name();
            EXPECT_TRUE(s->supports_rate(48000)) << s->name();
            EXPECT_TRUE(s->configure(audio::FORMAT_S16BE, 8, 48000));
            s->start();
            s->stop();
            s->shutdown();
        }

        vector<u8> data = make_samples(64, 1);
        vector<u8> copy = data;
        out_none.start();
        out_none.xfer(data);
        out_none.stop();
        in_none.start();
        in_none.xfer(data);
        in_none.stop();
        EXPECT_EQ(data, copy);
    }

    void test_wav_output() {
        for (const wav_params& params : WAV_PARAMS) {
            string path = temp_file_path("output");
            driver_ptr drv(audio::driver::create(out_none, wav_driver(path)));
            ASSERT_TRUE(drv);

            EXPECT_EQ(drv->output_min_channels(), 1u);
            EXPECT_EQ(drv->output_max_channels(), 2u);
            EXPECT_TRUE(drv->output_supports_format(params.format));
            EXPECT_TRUE(drv->output_supports_rate(params.rate));
            EXPECT_FALSE(std::filesystem::exists(path));

            size_t frame = params.channels * params.bits / 8;
            vector<u8> data1 = make_samples(frame * 100, 3);
            vector<u8> data2 = make_samples(frame * 37, 99);

            ASSERT_TRUE(drv->output_configure(params.format, params.channels,
                                              params.rate));
            drv->output_enable(true);
            drv->output_xfer(data1.data(), data1.size());
            drv->output_xfer(data2.data(), data2.size());
            drv->output_enable(false);
            drv->output_shutdown();

            expect_wav(path, params, concat(data1, data2));

            drv->output_enable(true);
            drv->output_xfer(data1.data(), data1.size());
            drv->output_shutdown();
            drv.reset();

            expect_wav(path, params, concat(data1, data2));
            remove_file(path);
        }
    }

    void test_wav_output_enable() {
        const wav_params& params = WAV_PARAMS[1];
        vector<u8> data1 = make_samples(400, 5);
        vector<u8> data2 = make_samples(200, 6);
        vector<u8> junk = make_samples(80, 7);

        string path = temp_file_path("output_enable");
        driver_ptr drv(audio::driver::create(out_none, wav_driver(path)));
        ASSERT_TRUE(drv);

        drv->output_enable(true);
        drv->output_xfer(junk.data(), junk.size());
        drv->output_enable(false);
        drv->output_shutdown();
        EXPECT_FALSE(std::filesystem::exists(path));

        ASSERT_TRUE(drv->output_configure(params.format, params.channels,
                                          params.rate));
        drv->output_xfer(junk.data(), junk.size());
        drv->output_enable(true);
        drv->output_xfer(data1.data(), data1.size());
        drv->output_enable(false);
        drv->output_xfer(junk.data(), junk.size());
        drv->output_enable(true);
        drv->output_xfer(data2.data(), data2.size());
        drv.reset();

        expect_wav(path, params, concat(data1, data2));
        remove_file(path);

        path = temp_file_path("output_empty");
        drv.reset(audio::driver::create(out_none, wav_driver(path)));
        ASSERT_TRUE(drv);
        ASSERT_TRUE(drv->output_configure(params.format, params.channels,
                                          params.rate));
        drv->output_enable(true);
        drv->output_shutdown();
        drv.reset();

        expect_wav(path, params, vector<u8>());
        remove_file(path);
    }

    void test_wav_output_reconfigure() {
        vector<u8> data1 = make_samples(256, 11);
        vector<u8> data2 = make_samples(128, 12);

        string path = temp_file_path("output_reconfigure");
        driver_ptr drv(audio::driver::create(out_none, wav_driver(path)));
        ASSERT_TRUE(drv);

        ASSERT_TRUE(drv->output_configure(
            WAV_PARAMS[1].format, WAV_PARAMS[1].channels, WAV_PARAMS[1].rate));
        drv->output_enable(true);
        drv->output_xfer(data1.data(), data1.size());

        ASSERT_TRUE(drv->output_configure(
            WAV_PARAMS[0].format, WAV_PARAMS[0].channels, WAV_PARAMS[0].rate));
        drv->output_xfer(data2.data(), data2.size());
        drv->output_shutdown();
        expect_wav(path, WAV_PARAMS[0], data2);

        ASSERT_TRUE(drv->output_configure(
            WAV_PARAMS[3].format, WAV_PARAMS[3].channels, WAV_PARAMS[3].rate));
        drv->output_xfer(data1.data(), data1.size());
        drv->output_shutdown();
        expect_wav(path, WAV_PARAMS[3], data1);

        drv.reset();
        remove_file(path);
    }

    void test_wav_output_unsupported() {
        vector<u8> data = make_samples(64, 21);

        string path = temp_file_path("output_unsupported");
        driver_ptr drv(audio::driver::create(out_none, wav_driver(path)));
        ASSERT_TRUE(drv);

        for (u32 format : ALL_FORMATS) {
            EXPECT_EQ(drv->output_supports_format(format),
                      is_wav_format(format))
                << audio::format_str(format);
            if (is_wav_format(format))
                continue;

            EXPECT_FALSE(drv->output_configure(format, 2, 44100))
                << audio::format_str(format);
            drv->output_enable(true);
            drv->output_xfer(data.data(), data.size());
            drv->output_shutdown();
            EXPECT_FALSE(std::filesystem::exists(path));
        }

        EXPECT_FALSE(drv->output_supports_format(audio::FORMAT_INVALID));
        EXPECT_FALSE(drv->output_configure(audio::FORMAT_INVALID, 2, 44100));
        EXPECT_FALSE(std::filesystem::exists(path));

        ASSERT_TRUE(drv->output_configure(
            WAV_PARAMS[1].format, WAV_PARAMS[1].channels, WAV_PARAMS[1].rate));
        drv->output_xfer(data.data(), data.size());
        EXPECT_FALSE(drv->output_configure(audio::FORMAT_S16BE, 2, 44100));
        drv->output_xfer(data.data(), data.size());
        drv->output_shutdown();
        expect_wav(path, WAV_PARAMS[1], data);

        drv.reset();
        remove_file(path);

        auto dir = std::filesystem::path(mwr::temp_dir()) /
                   mkstr("vcml_audio_missing_%d", mwr::getpid());
        ASSERT_FALSE(std::filesystem::exists(dir));
        path = (dir / "output.wav").generic_string();
        drv.reset(audio::driver::create(out_none, wav_driver(path)));
        ASSERT_TRUE(drv);
        EXPECT_FALSE(drv->output_configure(
            WAV_PARAMS[1].format, WAV_PARAMS[1].channels, WAV_PARAMS[1].rate));
        drv->output_enable(true);
        drv->output_xfer(data.data(), data.size());
        drv->output_shutdown();
        drv.reset();
        EXPECT_FALSE(std::filesystem::exists(dir));
    }

    void test_wav_output_stream() {
        EXPECT_EQ(out.min_channels(), 1u);
        EXPECT_EQ(out.max_channels(), 2u);
        EXPECT_TRUE(out.supports_rate(12345));
        EXPECT_FALSE(out.supports_format(audio::FORMAT_INVALID));
        for (u32 format : ALL_FORMATS) {
            EXPECT_EQ(out.supports_format(format), is_wav_format(format))
                << audio::format_str(format);
        }

        size_t nwarn = logs.count("config failed");
        EXPECT_FALSE(out.configure(audio::FORMAT_S16BE, 2, 44100));
        EXPECT_EQ(logs.count("config failed"), nwarn + 1);
        EXPECT_FALSE(std::filesystem::exists(out_path));

        vector<u16> samples = { 0x0123, 0x4567, 0x89ab, 0xcdef };
        u32 frame = 0xa1b2c3d4;
        vector<u8> expected = { 0x23, 0x01, 0x67, 0x45, 0xab, 0x89,
                                0xef, 0xcd, 0xd4, 0xc3, 0xb2, 0xa1 };

        ASSERT_TRUE(out.configure(STREAM_PARAMS.format, STREAM_PARAMS.channels,
                                  STREAM_PARAMS.rate));
        out.xfer(samples);
        out.start();
        out.xfer(samples);
        out.xfer(frame);
        out.stop();
        out.xfer(frame);
        out.shutdown();

        expect_wav(out_path, STREAM_PARAMS, expected);
        remove_file(out_path);
    }

    void test_wav_input() {
        for (const wav_params& params : WAV_PARAMS) {
            size_t frame = params.channels * params.bits / 8;
            size_t half = frame * 32;
            vector<u8> data = make_samples(2 * half, 31);
            vector<u8> silence = make_silence(half, params.format);

            string path = temp_file_path("input");
            write_file(path, make_wav(params, data));
            driver_ptr drv(audio::driver::create(in_none, wav_driver(path)));
            ASSERT_TRUE(drv);

            EXPECT_EQ(drv->input_min_channels(), params.channels);
            EXPECT_EQ(drv->input_max_channels(), params.channels);
            EXPECT_TRUE(drv->input_supports_rate(params.rate));
            EXPECT_FALSE(drv->input_supports_rate(params.rate + 1));
            EXPECT_FALSE(drv->input_supports_rate(params.rate / 2));
            EXPECT_FALSE(drv->input_supports_format(audio::FORMAT_INVALID));
            for (u32 format : ALL_FORMATS) {
                EXPECT_EQ(drv->input_supports_format(format),
                          format == params.format)
                    << audio::format_str(format);
                if (format != params.format) {
                    EXPECT_FALSE(drv->input_configure(format, params.channels,
                                                      params.rate))
                        << audio::format_str(format);
                }
            }

            EXPECT_FALSE(drv->input_configure(
                params.format, params.channels + 1, params.rate));
            EXPECT_FALSE(drv->input_configure(params.format, params.channels,
                                              params.rate * 2));
            ASSERT_TRUE(drv->input_configure(params.format, params.channels,
                                             params.rate));

            vector<u8> buf(half, 0xee);
            drv->input_xfer(buf.data(), buf.size());
            EXPECT_EQ(buf, silence) << "samples received while disabled";

            drv->input_enable(true);
            drv->input_xfer(buf.data(), buf.size());
            EXPECT_EQ(buf, slice(data, 0, half));
            drv->input_xfer(buf.data(), buf.size());
            EXPECT_EQ(buf, slice(data, half, half));

            drv->input_enable(false);
            drv->input_shutdown();
            drv.reset();
            remove_file(path);
        }
    }

    void test_wav_input_eof() {
        const wav_params& params = WAV_PARAMS[4];
        vector<u8> data = make_samples(100, 41);
        vector<u8> silence = make_silence(60, params.format);

        string path = temp_file_path("input_eof");
        write_file(path, make_wav(params, data));
        driver_ptr drv(audio::driver::create(in_none, wav_driver(path)));
        ASSERT_TRUE(drv);
        ASSERT_TRUE(
            drv->input_configure(params.format, params.channels, params.rate));
        drv->input_enable(true);

        vector<u8> buf(60, 0xee);
        drv->input_xfer(buf.data(), buf.size());
        EXPECT_EQ(buf, slice(data, 0, 60));

        std::fill(buf.begin(), buf.end(), 0xee);
        drv->input_xfer(buf.data(), buf.size());
        EXPECT_EQ(buf, concat(slice(data, 60, 40), slice(silence, 0, 20)))
            << "no silence after end of file";

        std::fill(buf.begin(), buf.end(), 0xee);
        drv->input_xfer(buf.data(), buf.size());
        EXPECT_EQ(buf, slice(data, 0, 60)) << "file did not restart";

        drv->input_enable(false);
        std::fill(buf.begin(), buf.end(), 0xee);
        drv->input_xfer(buf.data(), buf.size());
        EXPECT_EQ(buf, silence) << "samples received while disabled";

        drv->input_enable(true);
        buf.assign(20, 0xee);
        drv->input_xfer(buf.data(), buf.size());
        EXPECT_EQ(buf, slice(data, 60, 20)) << "position lost while disabled";

        buf.assign(400, 0xee);
        drv->input_xfer(buf.data(), buf.size());
        EXPECT_EQ(buf, concat(slice(data, 80, 20),
                              make_silence(380, params.format)));

        drv->input_shutdown();
        drv.reset();
        remove_file(path);
    }

    void test_wav_input_reconfigure() {
        const wav_params& params = WAV_PARAMS[1];
        vector<u8> data = make_samples(400, 51);
        vector<u8> silence = make_silence(40, params.format);

        string path = temp_file_path("input_reconfigure");
        write_file(path, make_wav(params, data));
        driver_ptr drv(audio::driver::create(in_none, wav_driver(path)));
        ASSERT_TRUE(drv);

        vector<u8> buf(40, 0xee);
        drv->input_enable(true);
        drv->input_xfer(buf.data(), buf.size());
        EXPECT_EQ(buf, silence) << "samples received before configure";

        ASSERT_TRUE(
            drv->input_configure(params.format, params.channels, params.rate));
        drv->input_xfer(buf.data(), buf.size());
        EXPECT_EQ(buf, slice(data, 0, 40));
        drv->input_xfer(buf.data(), buf.size());
        EXPECT_EQ(buf, slice(data, 40, 40));

        EXPECT_TRUE(
            drv->input_configure(params.format, params.channels, params.rate))
            << "cannot configure input twice";
        drv->input_xfer(buf.data(), buf.size());
        EXPECT_EQ(buf, slice(data, 0, 40)) << "file did not restart";

        drv->input_shutdown();
        std::fill(buf.begin(), buf.end(), 0xee);
        drv->input_xfer(buf.data(), buf.size());
        EXPECT_EQ(buf, silence) << "samples received after shutdown";
        drv->input_shutdown();

        EXPECT_TRUE(
            drv->input_configure(params.format, params.channels, params.rate))
            << "cannot configure input after shutdown";
        drv->input_xfer(buf.data(), buf.size());
        EXPECT_EQ(buf, slice(data, 0, 40));

        EXPECT_FALSE(drv->input_configure(params.format, params.channels,
                                          params.rate + 1));
        std::fill(buf.begin(), buf.end(), 0xee);
        drv->input_xfer(buf.data(), buf.size());
        EXPECT_EQ(buf, silence) << "samples received after failed configure";

        EXPECT_TRUE(
            drv->input_configure(params.format, params.channels, params.rate))
            << "cannot configure input after failed configure";
        drv->input_xfer(buf.data(), buf.size());
        EXPECT_EQ(buf, slice(data, 0, 40));

        drv.reset();
        remove_file(path);
    }

    void test_wav_input_malformed() {
        const wav_params& params = WAV_PARAMS[1];
        vector<u8> good = make_wav(params, make_samples(256, 61));
        vector<std::pair<string, vector<u8>>> files;

        files.emplace_back("empty", vector<u8>());
        files.emplace_back("truncated_riff", slice(good, 0, 3));
        files.emplace_back("truncated_fmt", slice(good, 0, 20));
        files.emplace_back("truncated_header", slice(good, 0, 43));
        files.emplace_back("no_samples", slice(good, 0, 44));

        vector<u8> wav = good;
        wav[0] = 'X';
        files.emplace_back("bad_riff_magic", wav);

        wav = good;
        wav[8] = 'A';
        wav[9] = 'V';
        wav[10] = 'I';
        wav[11] = ' ';
        files.emplace_back("bad_wave_magic", wav);

        wav = good;
        wav[12] = 'L';
        wav[13] = 'I';
        wav[14] = 'S';
        wav[15] = 'T';
        files.emplace_back("bad_fmt_magic", wav);

        files.emplace_back("zeroes", vector<u8>(300, 0));
        files.emplace_back("text", vector<u8>(300, 'a'));

        wav_params bad = params;
        bad.tag = WAV_TAG_ALAW;
        bad.bits = 8;
        files.emplace_back("alaw", make_wav(bad, make_samples(256, 62)));

        bad = params;
        bad.bits = 24;
        files.emplace_back("pcm24", make_wav(bad, make_samples(240, 63)));

        bad = params;
        bad.tag = WAV_TAG_FLOAT;
        bad.bits = 64;
        files.emplace_back("float64", make_wav(bad, make_samples(256, 64)));

        bad = params;
        bad.tag = WAV_TAG_FLOAT;
        files.emplace_back("float16", make_wav(bad, make_samples(256, 65)));

        files.emplace_back("missing", vector<u8>());

        for (const auto& file : files) {
            string path = temp_file_path(file.first);
            if (file.first != "missing")
                write_file(path, file.second);

            driver_ptr drv(audio::driver::create(in_none, wav_driver(path)));
            ASSERT_TRUE(drv) << file.first;

            for (u32 format : ALL_FORMATS) {
                EXPECT_FALSE(drv->input_supports_format(format))
                    << file.first << " " << audio::format_str(format);
                for (u32 channels : { 1u, 2u }) {
                    EXPECT_FALSE(
                        drv->input_configure(format, channels, params.rate))
                        << file.first << " " << audio::format_str(format);
                }
            }

            vector<u8> buf(64, 0xee);
            drv->input_xfer(buf.data(), buf.size());
            EXPECT_EQ(buf, vector<u8>(64, 0)) << file.first;

            drv->input_enable(true);
            std::fill(buf.begin(), buf.end(), 0xee);
            drv->input_xfer(buf.data(), buf.size());
            EXPECT_EQ(buf, vector<u8>(64, 0)) << file.first;

            drv->input_enable(false);
            drv->input_shutdown();
            drv.reset();

            if (file.first != "missing")
                remove_file(path);
            EXPECT_FALSE(std::filesystem::exists(path)) << file.first;
        }
    }

    void test_wav_input_stream() {
        EXPECT_EQ(in.min_channels(), STREAM_PARAMS.channels);
        EXPECT_EQ(in.max_channels(), STREAM_PARAMS.channels);
        EXPECT_TRUE(in.supports_rate(STREAM_PARAMS.rate));
        EXPECT_FALSE(in.supports_rate(STREAM_PARAMS.rate + 1));
        EXPECT_FALSE(in.supports_format(audio::FORMAT_INVALID));
        for (u32 format : ALL_FORMATS) {
            EXPECT_EQ(in.supports_format(format),
                      format == STREAM_PARAMS.format)
                << audio::format_str(format);
        }

        EXPECT_FALSE(in.configure(audio::FORMAT_U8, STREAM_PARAMS.channels,
                                  STREAM_PARAMS.rate));
        ASSERT_TRUE(in.configure(STREAM_PARAMS.format, STREAM_PARAMS.channels,
                                 STREAM_PARAMS.rate));

        ASSERT_EQ(in_data.size(), 64u);
        vector<u8> silence = make_silence(32, STREAM_PARAMS.format);

        vector<u8> buf(32, 0xee);
        in.xfer(buf);
        EXPECT_EQ(buf, silence);

        in.start();
        in.xfer(buf);
        EXPECT_EQ(buf, slice(in_data, 0, 32));

        u32 frame = 0;
        in.xfer(frame);
        EXPECT_EQ(frame, get_u32(in_data, 32));

        in.stop();
        in.xfer(buf);
        EXPECT_EQ(buf, silence);

        in.start();
        vector<u16> samples(4);
        in.xfer(samples);
        for (size_t i = 0; i < samples.size(); i++)
            EXPECT_EQ(samples[i], get_u16(in_data, 36 + 2 * i));

        in.stop();
        in.shutdown();
    }

    void test_sdl_output() {
        size_t nwarn = logs.count("unknown sdl option: bogus=1");
        driver_ptr drv = create_sdl(out_none, "sdl,buflen=100ms,bogus=1");
        if (!drv)
            return;

        EXPECT_EQ(logs.count("unknown sdl option: bogus=1"), nwarn + 1);

        EXPECT_EQ(drv->output_min_channels(), 1u);
        EXPECT_EQ(drv->output_max_channels(), 8u);
        EXPECT_FALSE(drv->output_supports_rate(7999));
        EXPECT_TRUE(drv->output_supports_rate(8000));
        EXPECT_TRUE(drv->output_supports_rate(44100));
        EXPECT_TRUE(drv->output_supports_rate(192000));
        EXPECT_FALSE(drv->output_supports_rate(192001));
        EXPECT_FALSE(drv->output_supports_format(audio::FORMAT_INVALID));
        for (u32 format : ALL_FORMATS) {
            bool supported = format != audio::FORMAT_U32LE &&
                             format != audio::FORMAT_U32BE;
            EXPECT_EQ(drv->output_supports_format(format), supported)
                << audio::format_str(format);
        }

        u32 format = audio::FORMAT_S16LE;
        size_t size = audio::buffer_size(sc_time(100, SC_MS), format, 2,
                                         48000);
        ASSERT_GT(size, 0u);
        vector<u8> data = make_samples(size, 71);

        drv->output_enable(true);
        drv->output_xfer(data.data(), data.size());
        drv->output_enable(false);
        drv->output_shutdown();

        out_none.loglvl = LOG_DEBUG;
        if (!drv->output_configure(format, 2, 48000)) {
            log_warn("sdl audio playback not available, test skipped");
            out_none.loglvl = LOG_INFO;
            return;
        }

        size_t noverflow = logs.count("audio buffer overflow");
        drv->output_xfer(data.data(), data.size());
        EXPECT_EQ(logs.count("audio buffer overflow"), noverflow);
        drv->output_xfer(data.data(), 4);
        EXPECT_EQ(logs.count("audio buffer overflow"), ++noverflow);
        drv->output_xfer(data.data(), 4);
        EXPECT_EQ(logs.count("audio buffer overflow"), ++noverflow);

        drv->output_enable(true);
        EXPECT_TRUE(poll_until([&]() -> bool {
            drv->output_xfer(data.data(), 4);
            return logs.count("audio buffer overflow") == noverflow++;
        })) << "sdl did not play any samples";

        drv->output_enable(false);
        drv->output_enable(true);
        drv->output_xfer(data.data(), 4);
        drv->output_enable(false);
        drv->output_shutdown();

        noverflow = logs.count("audio buffer overflow");
        drv->output_xfer(data.data(), data.size());
        drv->output_xfer(data.data(), data.size());
        EXPECT_EQ(logs.count("audio buffer overflow"), noverflow);
        drv->output_enable(true);
        drv->output_shutdown();
        out_none.loglvl = LOG_INFO;
    }

    void test_sdl_output_reconfigure() {
        driver_ptr drv = create_sdl(out_none, "sdl");
        if (!drv)
            return;

        if (!drv->output_configure(audio::FORMAT_S16LE, 2, 44100)) {
            log_warn("sdl audio playback not available, test skipped");
            return;
        }

        vector<u8> data = make_samples(1024, 81);
        drv->output_enable(true);
        drv->output_xfer(data.data(), data.size());

        EXPECT_TRUE(drv->output_configure(audio::FORMAT_S16LE, 2, 44100));
        drv->output_xfer(data.data(), data.size());

        EXPECT_TRUE(drv->output_configure(audio::FORMAT_U8, 1, 8000));
        drv->output_enable(true);
        drv->output_xfer(data.data(), data.size());

        EXPECT_TRUE(drv->output_configure(audio::FORMAT_F32LE, 2, 48000));
        drv->output_enable(true);
        drv->output_xfer(data.data(), data.size());
        drv->output_enable(false);
        drv->output_shutdown();

        for (u32 format : ALL_FORMATS) {
            if (!drv->output_supports_format(format))
                continue;

            EXPECT_TRUE(drv->output_configure(format, 1, 22050))
                << audio::format_str(format);
            drv->output_enable(true);
            drv->output_xfer(data.data(), data.size());
        }

        EXPECT_FALSE(drv->output_configure(audio::FORMAT_U32LE, 2, 44100));
        drv->output_enable(true);
        drv->output_xfer(data.data(), data.size());
        EXPECT_FALSE(drv->output_configure(audio::FORMAT_U32BE, 2, 44100));
        EXPECT_FALSE(drv->output_configure(audio::FORMAT_INVALID, 2, 44100));
        drv->output_shutdown();

        EXPECT_TRUE(drv->output_configure(audio::FORMAT_S32LE, 2, 96000));
        drv->output_enable(true);
        drv->output_xfer(data.data(), data.size());
        drv.reset();
    }

    void test_sdl_input() {
        driver_ptr drv = create_sdl(in_none, "sdl,buflen=20ms");
        if (!drv)
            return;

        EXPECT_EQ(drv->input_min_channels(), 1u);
        EXPECT_EQ(drv->input_max_channels(), 1u);
        EXPECT_TRUE(drv->input_supports_rate(8000));
        EXPECT_TRUE(drv->input_supports_rate(48000));
        EXPECT_FALSE(drv->input_supports_format(audio::FORMAT_INVALID));
        for (u32 format : ALL_FORMATS) {
            EXPECT_EQ(drv->input_supports_format(format),
                      is_wav_format(format))
                << audio::format_str(format);
        }

        vector<u8> buf(256, 0xee);
        drv->input_enable(true);
        drv->input_xfer(buf.data(), buf.size());
        drv->input_enable(false);
        drv->input_shutdown();
        EXPECT_EQ(buf, vector<u8>(256, 0));

        in_none.loglvl = LOG_DEBUG;
        u32 format = audio::FORMAT_S16LE;
        if (!drv->input_configure(format, 1, 48000)) {
            log_warn("sdl audio capture not available, test skipped");
            in_none.loglvl = LOG_INFO;
            return;
        }

        size_t size = audio::buffer_size(sc_time(20, SC_MS), format, 1, 48000);
        ASSERT_GT(size, 0u);

        buf.assign(size, 0xee);
        drv->input_xfer(buf.data(), buf.size());
        EXPECT_EQ(buf, make_silence(size, format));

        size_t noverflow = logs.count("audio buffer overflow");
        drv->input_enable(true);
        EXPECT_TRUE(poll_until([&]() -> bool {
            return logs.count("audio buffer overflow") > noverflow;
        })) << "sdl did not capture any samples";

        buf.assign(2 * size, 0xee);
        drv->input_xfer(buf.data(), buf.size());
        EXPECT_EQ(buf, make_silence(2 * size, format));

        drv->input_enable(false);
        drv->input_enable(true);
        drv->input_enable(false);
        drv->input_shutdown();
        in_none.loglvl = LOG_INFO;

        buf.assign(size, 0xee);
        drv->input_xfer(buf.data(), buf.size());
        EXPECT_EQ(buf, make_silence(size, format));
    }

    void test_sdl_input_reconfigure() {
        driver_ptr drv = create_sdl(in_none, "sdl");
        if (!drv)
            return;

        if (!drv->input_configure(audio::FORMAT_S16LE, 1, 44100)) {
            log_warn("sdl audio capture not available, test skipped");
            return;
        }

        vector<u8> buf(512, 0xee);
        drv->input_enable(true);
        drv->input_xfer(buf.data(), buf.size());
        EXPECT_EQ(buf, make_silence(512, audio::FORMAT_S16LE));

        EXPECT_TRUE(drv->input_configure(audio::FORMAT_S16LE, 1, 44100));
        drv->input_xfer(buf.data(), buf.size());

        for (u32 format : ALL_FORMATS) {
            if (!drv->input_supports_format(format))
                continue;

            EXPECT_TRUE(drv->input_configure(format, 1, 16000))
                << audio::format_str(format);
            drv->input_enable(true);
            drv->input_xfer(buf.data(), buf.size());
        }

        EXPECT_FALSE(drv->input_configure(audio::FORMAT_U32LE, 1, 44100));
        drv->input_enable(true);
        drv->input_xfer(buf.data(), buf.size());
        EXPECT_FALSE(drv->input_configure(audio::FORMAT_U32BE, 1, 44100));
        EXPECT_FALSE(drv->input_configure(audio::FORMAT_INVALID, 1, 44100));
        drv->input_shutdown();

        EXPECT_TRUE(drv->input_configure(audio::FORMAT_S32LE, 1, 96000));
        drv->input_enable(true);
        drv->input_xfer(buf.data(), buf.size());
        EXPECT_EQ(buf, make_silence(512, audio::FORMAT_S32LE));
        drv.reset();
    }
};

TEST(audio_drivers, simulate) {
    mwr::setenv("SDL_AUDIODRIVER", "dummy");

    string out_path = temp_file_path("stream_output");
    string in_path = temp_file_path("stream_input");
    vector<u8> in_data = make_samples(64, 91);
    write_file(in_path, make_wav(STREAM_PARAMS, in_data));

    log_capture logs;
    broker broker("test");
    broker.define("harness.out.drivers",
                  double_backslashes(wav_property(out_path)));
    broker.define("harness.in.drivers",
                  double_backslashes(wav_property(in_path)));
    broker.define("harness.out_bad.drivers", "nothing");
    broker.define("harness.in_bad.drivers", "missing");

    audio_harness test("harness", logs, out_path, in_data);
    sc_core::sc_start();

    remove_file(in_path);
    remove_file(out_path);
}
