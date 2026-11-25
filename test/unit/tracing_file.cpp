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

#include <filesystem>

struct test_activity : trace_activity {
    string text;

    test_activity(trace_direction d, const sc_object& p, const string& s,
                  const sc_time& dt = SC_ZERO_TIME):
        trace_activity(d, p, false, dt), text(s) {}
    virtual ~test_activity() = default;

    virtual size_t protocol_id() const override { return PROTO_EXT0; }
    virtual string protocol_name() const override { return "TEST"; }
    virtual string to_string() const override { return text; }
    virtual string to_json() const override { return "{}"; }
    virtual string termcolor() const override { return ""; }
};

static string temp_file_path(const string& name) {
    static size_t count = 0;
    string file = mkstr("vcml_%s_%d_%zu.txt", name.c_str(), mwr::getpid(),
                        count++);
    return (std::filesystem::path(mwr::temp_dir()) / file).string();
}

static vector<string> read_lines(const string& path) {
    vector<string> lines;
    ifstream file(path.c_str());
    EXPECT_TRUE(file.is_open()) << "cannot open " << path;
    string line;
    while (std::getline(file, line))
        lines.push_back(line);
    return lines;
}

static string timing(const sc_time& t = sc_time_stamp()) {
    u64 ns = time_to_ns(t);
    return mkstr("%llu.%09llu", ns / 1000000000ull, ns % 1000000000ull);
}

class tracing_file_harness : public test_base,
                             public spi_host,
                             public serial_host
{
public:
    tlm_initiator_socket out;
    tlm_target_socket in;

    gpio_initiator_socket gpio_out;
    gpio_target_socket gpio_in;

    spi_initiator_socket spi_out;
    spi_target_socket spi_in;

    serial_initiator_socket serial_tx;
    serial_target_socket serial_rx;

    tracing_file_harness(const sc_module_name& nm):
        test_base(nm),
        spi_host(),
        serial_host(),
        out("out"),
        in("in"),
        gpio_out("gpio_out"),
        gpio_in("gpio_in"),
        spi_out("spi_out"),
        spi_in("spi_in"),
        serial_tx("serial_tx"),
        serial_rx("serial_rx") {
        out.bind(in);
        gpio_out.bind(gpio_in);
        spi_out.bind(spi_in);
        serial_tx.bind(serial_rx);

        add_test("tlm", &tracing_file_harness::test_tlm);
        add_test("tlm_errors", &tracing_file_harness::test_tlm_errors);
        add_test("gpio", &tracing_file_harness::test_gpio);
        add_test("spi", &tracing_file_harness::test_spi);
        add_test("serial", &tracing_file_harness::test_serial);
        add_test("direction", &tracing_file_harness::test_direction);
        add_test("multiline", &tracing_file_harness::test_multiline);
        add_test("escaping", &tracing_file_harness::test_escaping);
        add_test("multiple", &tracing_file_harness::test_multiple);
        add_test("open_error", &tracing_file_harness::test_open_error);
    }

    virtual unsigned int transport(tlm_generic_payload& tx,
                                   const tlm_sbi& info,
                                   address_space as) override {
        if (tx.get_address() >= 0x1000) {
            tx.set_response_status(TLM_ADDRESS_ERROR_RESPONSE);
            return 0;
        }

        if (tx.is_read())
            memset(tx.get_data_ptr(), 0xab, tx.get_data_length());

        tx.set_response_status(TLM_OK_RESPONSE);
        return tx.get_data_length();
    }

    virtual void spi_transport(const spi_target_socket& socket,
                               spi_payload& spi) override {
        spi.miso = spi.mosi + 1;
    }

    virtual void serial_receive(const serial_target_socket& socket,
                                serial_payload& tx) override {}

    void test_tlm() {
        string path = temp_file_path("tracing_file_tlm");
        auto file = std::make_unique<tracer_file>(path);
        EXPECT_EQ(path, file->filename());

        wait(10, SC_MS);
        string now = timing();

        out.trace_all = true;
        u32 data = 0x11223344;
        EXPECT_OK(out.writew(0x420, data));
        data = 0;
        EXPECT_OK(out.readw(0x80, data));
        EXPECT_EQ(data, 0xabababab);
        out.trace_all = false;
        EXPECT_OK(out.writew(0x420, data));

        file.reset();

        string pfx = "[TLM " + now + "] " + out.name();
        vector<string> lines = read_lines(path);
        ASSERT_EQ(lines.size(), 4);
        EXPECT_EQ(lines[0], pfx +
                                " >> WR 0x00000420 [44 33 22 11] CPU0 "
                                "(TLM_INCOMPLETE_RESPONSE)");
        EXPECT_EQ(lines[1], pfx +
                                " << WR 0x00000420 [44 33 22 11] CPU0 "
                                "(TLM_OK_RESPONSE)");
        EXPECT_EQ(lines[2], pfx +
                                " >> RD 0x00000080 [00 00 00 00] CPU0 "
                                "(TLM_INCOMPLETE_RESPONSE)");
        EXPECT_EQ(lines[3], pfx +
                                " << RD 0x00000080 [ab ab ab ab] CPU0 "
                                "(TLM_OK_RESPONSE)");

        EXPECT_TRUE(std::filesystem::remove(path));
    }

    void test_tlm_errors() {
        string path = temp_file_path("tracing_file_tlm_errors");
        auto file = std::make_unique<tracer_file>(path);

        out.trace_errors = true;
        u32 data = 0x55667788;
        EXPECT_OK(out.writew(0x420, data));
        EXPECT_AE(out.writew(0x2000, data));
        out.trace_errors = false;
        EXPECT_AE(out.writew(0x2000, data));

        file.reset();

        vector<string> lines = read_lines(path);
        ASSERT_EQ(lines.size(), 1);
        EXPECT_EQ(lines[0], "[TLM " + timing() + "] " + out.name() +
                                " << WR 0x00002000 [88 77 66 55] CPU0 "
                                "(TLM_ADDRESS_ERROR_RESPONSE)");

        EXPECT_TRUE(std::filesystem::remove(path));
    }

    void test_gpio() {
        string path = temp_file_path("tracing_file_gpio");
        auto file = std::make_unique<tracer_file>(path);

        gpio_out.trace_all = true;
        gpio_out.raise();
        gpio_out.lower();
        gpio_out.raise(5);
        gpio_out.lower(5);
        gpio_out.trace_all = false;
        gpio_out.raise();
        gpio_out.lower();

        file.reset();

        string pfx = "[GPIO " + timing() + "] " + gpio_out.name();
        vector<string> lines = read_lines(path);
        ASSERT_EQ(lines.size(), 8);
        EXPECT_EQ(lines[0], pfx + " >> GPIO+");
        EXPECT_EQ(lines[1], pfx + " << GPIO+");
        EXPECT_EQ(lines[2], pfx + " >> GPIO-");
        EXPECT_EQ(lines[3], pfx + " << GPIO-");
        EXPECT_EQ(lines[4], pfx + " >> GPIO:5+");
        EXPECT_EQ(lines[5], pfx + " << GPIO:5+");
        EXPECT_EQ(lines[6], pfx + " >> GPIO:5-");
        EXPECT_EQ(lines[7], pfx + " << GPIO:5-");

        EXPECT_TRUE(std::filesystem::remove(path));
    }

    void test_spi() {
        string path = temp_file_path("tracing_file_spi");
        auto file = std::make_unique<tracer_file>(path);

        spi_out.trace_all = true;
        spi_payload spi(0x41);
        spi_out.transport(spi);
        EXPECT_EQ(spi.miso, 0x42);
        spi_out.trace_all = false;
        spi_out.transport(spi);

        file.reset();

        string pfx = "[SPI " + timing() + "] " + spi_out.name();
        vector<string> lines = read_lines(path);
        ASSERT_EQ(lines.size(), 2);
        EXPECT_EQ(lines[0], pfx + " >> [mosi: 0x41 miso: 0x00]");
        EXPECT_EQ(lines[1], pfx + " << [mosi: 0x41 miso: 0x42]");

        EXPECT_TRUE(std::filesystem::remove(path));
    }

    void test_serial() {
        string path = temp_file_path("tracing_file_serial");
        auto file = std::make_unique<tracer_file>(path);

        serial_tx.set_baud(SERIAL_9600BD);
        serial_tx.set_parity(SERIAL_PARITY_NONE);
        serial_tx.set_data_width(SERIAL_8_BITS);
        serial_tx.trace_all = true;
        serial_tx.send('A');
        serial_tx.send('\n');
        serial_tx.trace_all = false;
        serial_tx.send('B');

        file.reset();

        string pfx = "[SERIAL " + timing() + "] " + serial_tx.name();
        vector<string> lines = read_lines(path);
        ASSERT_EQ(lines.size(), 2);
        EXPECT_EQ(lines[0], pfx + " >> SERIAL TX [41] (9600n8)");
        EXPECT_EQ(lines[1], pfx + " >> SERIAL TX [0a] (9600n8)");

        EXPECT_TRUE(std::filesystem::remove(path));
    }

    void test_direction() {
        string path = temp_file_path("tracing_file_direction");
        auto file = std::make_unique<tracer_file>(path);

        wait(1500, SC_MS);
        sc_time dt(250, SC_US);

        file->trace(test_activity(TRACE_FW, in, "forward"));
        file->trace(test_activity(TRACE_FW_NOINDENT, in, "forward noindent"));
        file->trace(test_activity(TRACE_BW_NOINDENT, in, "backward noindent"));
        file->trace(test_activity(TRACE_BW, in, "backward"));
        file->trace(test_activity(TRACE_FW, gpio_in, "delayed", dt));

        file.reset();

        string now = timing();
        string later = timing(sc_time_stamp() + dt);
        EXPECT_NE(now, later);

        string pfx = "[TEST " + now + "] " + in.name();
        vector<string> lines = read_lines(path);
        ASSERT_EQ(lines.size(), 5);
        EXPECT_EQ(lines[0], pfx + " >> forward");
        EXPECT_EQ(lines[1], pfx + " >> forward noindent");
        EXPECT_EQ(lines[2], pfx + " << backward noindent");
        EXPECT_EQ(lines[3], pfx + " << backward");
        EXPECT_EQ(lines[4],
                  "[TEST " + later + "] " + gpio_in.name() + " >> delayed");

        EXPECT_TRUE(std::filesystem::remove(path));
    }

    void test_multiline() {
        string path = temp_file_path("tracing_file_multiline");
        auto file = std::make_unique<tracer_file>(path);

        file->trace(test_activity(TRACE_FW, out, "first\n  second\nthird\n"));
        file->trace(test_activity(TRACE_BW, out, "one\n\ntwo"));
        file->trace(test_activity(TRACE_FW, out, ""));

        vq_message msg;
        msg.index = 3;
        msg.status = VIRTIO_OK;
        msg.append(0x1000, 16, false);
        msg.append(0x100000000ull, 32, true);
        tracer::record(TRACE_BW, out, msg);

        file.reset();

        string pfx = "[TEST " + timing() + "] " + out.name();
        string vpfx = "[VIRTIO " + timing() + "] " + out.name();
        vector<string> lines = read_lines(path);
        ASSERT_EQ(lines.size(), 8);
        EXPECT_EQ(lines[0], pfx + " >> first");
        EXPECT_EQ(lines[1], pfx + " >>   second");
        EXPECT_EQ(lines[2], pfx + " >> third");
        EXPECT_EQ(lines[3], pfx + " << one");
        EXPECT_EQ(lines[4], pfx + " << two");
        EXPECT_EQ(lines[5],
                  vpfx +
                      " << VIRTMSG@3 [in: 1 descriptors | 16 bytes total "
                      "out: 1 descriptors | 32 bytes total] (VIRTIO_OK)");
        EXPECT_EQ(lines[6], vpfx +
                                " <<   IN [0x00001000..0x0000100f] "
                                "(16 bytes)");
        EXPECT_EQ(lines[7],
                  vpfx +
                      " <<   OUT [0x0000000100000000..0x000000010000001f] "
                      "(32 bytes)");

        EXPECT_TRUE(std::filesystem::remove(path));
    }

    void test_escaping() {
        string path = temp_file_path("tracing_file_escaping");
        auto file = std::make_unique<tracer_file>(path);

        file->trace(test_activity(TRACE_FW, out, "say \"hello world\" now"));
        file->trace(test_activity(TRACE_FW, out, "it's 'quoted'"));
        file->trace(test_activity(TRACE_FW, out, "back\\slash c:\\temp\\"));
        file->trace(
            test_activity(TRACE_BW, out, "open \" quote\nnext ' line"));
        file->trace(test_activity(TRACE_BW, out, "tab\there, 100% {} [] <>"));

        file.reset();

        string pfx = "[TEST " + timing() + "] " + out.name();
        vector<string> lines = read_lines(path);
        ASSERT_EQ(lines.size(), 6);
        EXPECT_EQ(lines[0], pfx + " >> say \"hello world\" now");
        EXPECT_EQ(lines[1], pfx + " >> it's 'quoted'");
        EXPECT_EQ(lines[2], pfx + " >> back\\slash c:\\temp\\");
        EXPECT_EQ(lines[3], pfx + " << open \" quote");
        EXPECT_EQ(lines[4], pfx + " << next ' line");
        EXPECT_EQ(lines[5], pfx + " << tab\there, 100% {} [] <>");

        EXPECT_TRUE(std::filesystem::remove(path));
    }

    void test_multiple() {
        string path1 = temp_file_path("tracing_file_multiple");
        string path2 = temp_file_path("tracing_file_multiple");
        ASSERT_NE(path1, path2);

        auto file1 = std::make_unique<tracer_file>(path1);
        auto file2 = std::make_unique<tracer_file>(path2);

        gpio_out.trace_all = true;
        gpio_out.raise();
        file2.reset();
        gpio_out.lower();
        file1.reset();
        gpio_out.raise();
        gpio_out.lower();
        gpio_out.trace_all = false;

        string pfx = "[GPIO " + timing() + "] " + gpio_out.name();
        vector<string> lines1 = read_lines(path1);
        ASSERT_EQ(lines1.size(), 4);
        EXPECT_EQ(lines1[0], pfx + " >> GPIO+");
        EXPECT_EQ(lines1[1], pfx + " << GPIO+");
        EXPECT_EQ(lines1[2], pfx + " >> GPIO-");
        EXPECT_EQ(lines1[3], pfx + " << GPIO-");

        vector<string> lines2 = read_lines(path2);
        ASSERT_EQ(lines2.size(), 2);
        EXPECT_EQ(lines2[0], pfx + " >> GPIO+");
        EXPECT_EQ(lines2[1], pfx + " << GPIO+");

        EXPECT_TRUE(std::filesystem::remove(path1));
        EXPECT_TRUE(std::filesystem::remove(path2));
    }

    void test_open_error() {
        auto dir = std::filesystem::path(mwr::temp_dir()) /
                   mkstr("vcml_tracing_file_missing_%d", mwr::getpid());
        ASSERT_FALSE(std::filesystem::exists(dir));
        string path = (dir / "trace.txt").string();

        EXPECT_DEATH(tracer_file file(path), "failed to open");
        EXPECT_FALSE(std::filesystem::exists(dir));
    }
};

TEST(tracing_file, simulate) {
    tracing_file_harness test("harness");
    sc_core::sc_start();
}
