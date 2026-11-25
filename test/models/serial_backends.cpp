/******************************************************************************
 *                                                                            *
 * Copyright (C) 2026 MachineWare GmbH                                        *
 * All Rights Reserved                                                        *
 *                                                                            *
 * This work is licensed under the terms described in the LICENSE file found  *
 * in the root directory of this source tree.                                 *
 *                                                                            *
 ******************************************************************************/

#if defined(__linux__) || defined(__APPLE__)
#define HAVE_PTY
#include <fcntl.h>
#include <poll.h>
#include <stdlib.h>
#include <sys/ioctl.h>
#include <termios.h>
#include <unistd.h>
#endif

#include "testing.h"

#include <filesystem>

enum : u64 { TIMEOUT_MS = 10000 };

static string temp_path(const string& name) {
    string file = mkstr("vcml_serial_backends_%d_%s", mwr::getpid(),
                        name.c_str());
    return (std::filesystem::path(mwr::temp_dir()) / file).generic_string();
}

static string read_file(const string& path) {
    ifstream file(path.c_str(), ifstream::binary);
    stringstream ss;
    ss << file.rdbuf();
    return ss.str();
}

static void write_file(const string& path, const string& data) {
    ofstream file(path.c_str(), ofstream::binary);
    file << data;
}

static string recv_from(mwr::socket& sock, size_t n) {
    string s;
    while (s.length() < n && sock.peek(TIMEOUT_MS) > 0)
        s += (char)sock.recv_char();
    return s;
}

static serial::backend* create_failing_backend(serial::terminal* term,
                                               const vector<string>& args) {
    if (!args.empty() && args[0] == "std")
        throw std::runtime_error("std failure");
    throw 42;
}

#ifdef HAVE_PTY
class pty
{
private:
    int m_master;
    int m_slave;
    int m_stdin;
    int m_stdout;
    atomic<bool> m_running;
    mutable mutex m_mtx;
    std::condition_variable m_cv;
    string m_output;
    thread m_reader;

    void reader() {
        char buf[256];
        while (m_running) {
            pollfd pfd = { m_master, POLLIN, 0 };
            if (poll(&pfd, 1, 20) <= 0)
                continue;

            ssize_t n = read(m_master, buf, sizeof(buf));
            if (n <= 0)
                continue;

            lock_guard<mutex> guard(m_mtx);
            m_output.append(buf, n);
            m_cv.notify_all();
        }
    }

    bool open_slave(size_t cols) {
        if (m_master < 0 || grantpt(m_master) || unlockpt(m_master))
            return false;

        const char* path = ptsname(m_master);
        if (path == nullptr)
            return false;

        m_slave = open(path, O_RDWR | O_NOCTTY);
        if (m_slave < 0)
            return false;

        termios attr{};
        if (tcgetattr(m_slave, &attr))
            return false;

        attr.c_iflag &= ~ICRNL;
        attr.c_oflag &= ~OPOST;
        attr.c_lflag |= ICANON | ECHO | ISIG;
        if (tcsetattr(m_slave, TCSANOW, &attr))
            return false;

        winsize size{};
        size.ws_row = 24;
        size.ws_col = (unsigned short)cols;
        return ioctl(m_slave, TIOCSWINSZ, &size) == 0;
    }

public:
    bool valid() const { return m_running; }

    pty(size_t cols):
        m_master(posix_openpt(O_RDWR | O_NOCTTY)),
        m_slave(-1),
        m_stdin(-1),
        m_stdout(-1),
        m_running(false),
        m_mtx(),
        m_cv(),
        m_output(),
        m_reader() {
        if (!open_slave(cols))
            return;

        fflush(stdout);
        m_stdin = dup(STDIN_FILENO);
        m_stdout = dup(STDOUT_FILENO);
        dup2(m_slave, STDIN_FILENO);
        dup2(m_slave, STDOUT_FILENO);

        m_running = true;
        m_reader = thread(&pty::reader, this);
    }

    ~pty() {
        if (m_running) {
            m_running = false;
            m_reader.join();

            fflush(stdout);
            if (m_stdin >= 0)
                dup2(m_stdin, STDIN_FILENO);
            else
                close(STDIN_FILENO);
            if (m_stdout >= 0)
                dup2(m_stdout, STDOUT_FILENO);
            else
                close(STDOUT_FILENO);
        }

        for (int fd : { m_stdin, m_stdout, m_slave, m_master }) {
            if (fd >= 0)
                close(fd);
        }
    }

    pty(const pty&) = delete;
    pty& operator=(const pty&) = delete;

    tcflag_t lflag() const {
        termios attr{};
        tcgetattr(m_slave, &attr);
        return attr.c_lflag;
    }

    bool input(const string& data) {
        size_t done = 0;
        while (done < data.length()) {
            ssize_t n = write(m_master, data.data() + done,
                              data.length() - done);
            if (n <= 0)
                return false;
            done += n;
        }

        return true;
    }

    bool wait_output(const string& text) {
        std::unique_lock<mutex> lock(m_mtx);
        return m_cv.wait_for(
            lock, std::chrono::milliseconds(TIMEOUT_MS),
            [&]() -> bool { return m_output.find(text) != string::npos; });
    }
};
#endif

class serial_backends_test : public test_base, public serial_host
{
public:
#ifdef HAVE_PTY
    std::unique_ptr<pty> quit_tty;
#endif

    serial::terminal term;
    serial::terminal term_odd;
    serial::terminal term_even;
    serial::terminal term_mark;
    serial::terminal term_space;
    serial::terminal term_wide;
    serial::terminal term_narrow;
    serial::terminal term_bad;
    serial::terminal term_empty;

    serial_initiator_socket serial_tx;
    serial_target_socket serial_rx;

    string rx;

    virtual void serial_receive(const serial_target_socket& socket,
                                serial_payload& tx) override {
        rx += (char)(tx.data & tx.mask);
    }

    serial_backends_test(const sc_module_name& nm):
        test_base(nm),
        serial_host(),
#ifdef HAVE_PTY
        quit_tty(),
#endif
        term("term"),
        term_odd("term_odd"),
        term_even("term_even"),
        term_mark("term_mark"),
        term_space("term_space"),
        term_wide("term_wide"),
        term_narrow("term_narrow"),
        term_bad("term_bad"),
        term_empty("term_empty"),
        serial_tx("serial_tx"),
        serial_rx("serial_rx"),
        rx() {
        serial_tx.bind(term.serial_rx);
        term.serial_tx.bind(serial_rx);

        for (serial::terminal* t : serial::terminal::all()) {
            if (t != &term) {
                t->serial_tx.stub();
                t->serial_rx.stub();
            }
        }

        add_test("strings", &serial_backends_test::test_strings);
        add_test("lookup", &serial_backends_test::test_lookup);
        add_test("config", &serial_backends_test::test_config);
        add_test("property", &serial_backends_test::test_property);
        add_test("commands", &serial_backends_test::test_commands);
        add_test("history", &serial_backends_test::test_history);
        add_test("null", &serial_backends_test::test_null);
        add_test("file", &serial_backends_test::test_file);
        add_test("file_default", &serial_backends_test::test_file_default);
        add_test("file_missing", &serial_backends_test::test_file_missing);
        add_test("fd", &serial_backends_test::test_fd);
        add_test("tcp", &serial_backends_test::test_tcp);
        add_test("term", &serial_backends_test::test_term);
        add_test("tui", &serial_backends_test::test_tui);
        add_test("term_quit", &serial_backends_test::test_term_quit);
    }

    string execute(const string& cmd, const vector<string>& args,
                   bool success = true) {
        stringstream ss;
        EXPECT_EQ(term.execute(cmd, args, ss), success) << cmd;
        return ss.str();
    }

    string backend_type(size_t id) {
        string list = execute("list_backends", { to_string(id) });
        string prefix = mkstr("%zu: ", id);
        if (list.length() <= prefix.length() || list.back() != ',')
            return "";
        return list.substr(prefix.length(),
                           list.length() - prefix.length() - 1);
    }

    void send(const string& data) {
        for (char c : data)
            serial_tx.send((u8)c);
    }

    bool wait_rx(size_t n) {
        u64 timeout = mwr::timestamp_ms() + TIMEOUT_MS;
        while (rx.length() < n) {
            if (mwr::timestamp_ms() > timeout)
                return false;
            wait(term.serial_tx.cycle());
            std::this_thread::yield();
        }

        return true;
    }

    string flush_rx() {
        rx.clear();
        term.send("!");
        EXPECT_TRUE(wait_rx(1)) << "timeout waiting for terminal";
        wait(10 * term.serial_tx.cycle());
        return rx;
    }

    void test_strings() {
        EXPECT_STREQ(term.kind(), "vcml::serial::terminal");
        EXPECT_STREQ(term.version(), VCML_VERSION_STRING);
    }

    void test_lookup() {
        EXPECT_EQ(serial::terminal::find("test.term"), &term);
        EXPECT_EQ(serial::terminal::find("test.term_odd"), &term_odd);
        EXPECT_EQ(serial::terminal::find("test.nothing"), nullptr);
        EXPECT_THAT(serial::terminal::all(),
                    UnorderedElementsAre(
                        &term, &term_odd, &term_even, &term_mark, &term_space,
                        &term_wide, &term_narrow, &term_bad, &term_empty));
    }

    void test_config() {
        EXPECT_EQ(term.serial_tx.baud(), SERIAL_9600BD);
        EXPECT_EQ(term.serial_tx.parity(), SERIAL_PARITY_NONE);
        EXPECT_EQ(term.serial_tx.data_width(), SERIAL_8_BITS);

        EXPECT_EQ(term_odd.serial_tx.baud(), SERIAL_38400BD);
        EXPECT_EQ(term_odd.serial_tx.parity(), SERIAL_PARITY_ODD);
        EXPECT_EQ(term_odd.serial_tx.data_width(), SERIAL_7_BITS);

        EXPECT_EQ(term_even.serial_tx.baud(), SERIAL_57600BD);
        EXPECT_EQ(term_even.serial_tx.parity(), SERIAL_PARITY_EVEN);
        EXPECT_EQ(term_even.serial_tx.data_width(), SERIAL_6_BITS);

        EXPECT_EQ(term_mark.serial_tx.baud(), SERIAL_115200BD);
        EXPECT_EQ(term_mark.serial_tx.parity(), SERIAL_PARITY_MARK);
        EXPECT_EQ(term_mark.serial_tx.data_width(), SERIAL_5_BITS);

        EXPECT_EQ(term_space.serial_tx.baud(), SERIAL_1200BD);
        EXPECT_EQ(term_space.serial_tx.parity(), SERIAL_PARITY_SPACE);
        EXPECT_EQ(term_space.serial_tx.data_width(), SERIAL_8_BITS);

        EXPECT_EQ(term_wide.serial_tx.baud(), SERIAL_4800BD);
        EXPECT_EQ(term_wide.serial_tx.parity(), SERIAL_PARITY_NONE);
        EXPECT_EQ(term_wide.serial_tx.data_width(), SERIAL_8_BITS);

        EXPECT_EQ(term_narrow.serial_tx.baud(), SERIAL_2400BD);
        EXPECT_EQ(term_narrow.serial_tx.parity(), SERIAL_PARITY_NONE);
        EXPECT_EQ(term_narrow.serial_tx.data_width(), SERIAL_5_BITS);

        EXPECT_EQ(term_bad.serial_tx.baud(), SERIAL_9600BD);
        EXPECT_EQ(term_bad.serial_tx.parity(), SERIAL_PARITY_NONE);
        EXPECT_EQ(term_bad.serial_tx.data_width(), SERIAL_8_BITS);

        EXPECT_EQ(term_empty.serial_tx.baud(), SERIAL_9600BD);
        EXPECT_EQ(term_empty.serial_tx.parity(), SERIAL_PARITY_NONE);
        EXPECT_EQ(term_empty.serial_tx.data_width(), SERIAL_8_BITS);
    }

    void test_property() {
        EXPECT_EQ(term.backends.get(), "null nothing");
        EXPECT_EQ(execute("list_backends", {}), "0: null,");
        EXPECT_TRUE(term.destroy_backend(0));
        EXPECT_EQ(execute("list_backends", {}), "");
    }

    void test_commands() {
        string resp = execute("create_backend", { "null" });
        ASSERT_THAT(resp, StartsWith("created backend "));
        size_t id0 = from_string<size_t>(resp.substr(16));
        size_t id1 = term.create_backend("stderr");
        EXPECT_EQ(id1, id0 + 1);

        resp = execute("create_backend", { "nothing" }, false);
        EXPECT_THAT(resp, StartsWith("error creating backend nothing:"));
        EXPECT_THAT(resp, HasSubstr("unknown serial backend 'nothing'"));
        EXPECT_THAT(resp, HasSubstr("  tcp"));
        EXPECT_THAT(execute("create_backend", { "failing:std" }, false),
                    HasSubstr("failing: std failure"));
        EXPECT_THAT(execute("create_backend", { "failing" }, false),
                    HasSubstr("failing: unknown error"));
        EXPECT_THROW(term.create_backend("nothing"), std::exception);

        EXPECT_EQ(execute("list_backends",
                          { to_string(id1), "9999", to_string(id0) }),
                  mkstr("%zu: stderr,9999: none,%zu: null,", id1, id0));
        string list = execute("list_backends", {});
        EXPECT_THAT(list, HasSubstr(mkstr("%zu: null,", id0)));
        EXPECT_THAT(list, HasSubstr(mkstr("%zu: stderr,", id1)));

        EXPECT_EQ(execute("destroy_backend", { to_string(id0) }), "");
        EXPECT_EQ(execute("destroy_backend", { to_string(id0) }),
                  mkstr("invalid backend id: %zu", id0));
        EXPECT_EQ(backend_type(id0), "none");
        EXPECT_EQ(backend_type(id1), "stderr");

        EXPECT_EQ(execute("destroy_backend", { "ALL" }), "");
        EXPECT_EQ(execute("list_backends", {}), "");
        EXPECT_FALSE(term.destroy_backend(id1));

        stringstream ss;
        EXPECT_FALSE(term.execute("create_backend", ss));
        EXPECT_FALSE(term.execute("destroy_backend", ss));
    }

    void test_history() {
        vector<u8> history;
        term.clear_history();
        EXPECT_EQ(execute("history", {}), "");

        send("a,b");
        EXPECT_EQ(execute("history", {}), "a\\,b");
        term.fetch_history(history);
        EXPECT_THAT(history, ElementsAre('a', ',', 'b'));

        string data;
        for (size_t i = 0; i < 4100; i++)
            data += (char)('0' + i % 10);
        send(data);
        EXPECT_EQ(execute("history", {}), data.substr(4));
        term.fetch_history(history);
        ASSERT_EQ(history.size(), 4096);
        EXPECT_EQ(history.front(), '4');
        EXPECT_EQ(history.back(), '9');

        term.clear_history();
        term.fetch_history(history);
        EXPECT_TRUE(history.empty());
    }

    void test_null() {
        size_t id = term.create_backend("null:ignored,args");
        EXPECT_EQ(backend_type(id), "null");
        send("swallowed");
        EXPECT_EQ(flush_rx(), "!");
        EXPECT_TRUE(term.destroy_backend(id));
    }

    void test_file() {
        const string tx = temp_path("tx");
        const string rx_file = temp_path("rx");
        const string desc = mkstr("file:%s,%s", tx.c_str(), rx_file.c_str());
        std::remove(tx.c_str());
        write_file(rx_file, string("rx\n\0\xff", 5));

        size_t id = term.create_backend(desc);
        EXPECT_EQ(backend_type(id),
                  mkstr("file:%s:%s", rx_file.c_str(), tx.c_str()));
        EXPECT_EQ(flush_rx(), string("rx\n\0\xff!", 6));
        EXPECT_EQ(flush_rx(), "!");
        send("tx!");
        EXPECT_EQ(read_file(tx), "tx!");
        EXPECT_TRUE(term.destroy_backend(id));
        EXPECT_EQ(read_file(tx), "tx!");

        id = term.create_backend(mkstr("file:%s", tx.c_str()));
        EXPECT_EQ(backend_type(id), mkstr("file::%s", tx.c_str()));
        send(string("\0\xff\n", 3));
        EXPECT_EQ(flush_rx(), "!");
        EXPECT_TRUE(term.destroy_backend(id));
        EXPECT_EQ(read_file(tx), string("tx!\0\xff\n", 6));

        std::remove(tx.c_str());
        std::remove(rx_file.c_str());
    }

    void test_file_default() {
        const string tx = mkstr("%s.tx", term.name());
        std::remove(tx.c_str());

        size_t id = term.create_backend("file");
        EXPECT_EQ(backend_type(id), "file::" + tx);
        EXPECT_FALSE(mwr::file_exists(tx));
        send("default");
        EXPECT_TRUE(term.destroy_backend(id));

        if (mwr::file_exists(tx)) {
            EXPECT_EQ(read_file(tx), "default");
            std::remove(tx.c_str());
        } else {
            log_warn("cannot create %s, default tx file not checked",
                     tx.c_str());
        }
    }

    void test_file_missing() {
        const string tx = temp_path("missing") + "/tx";
        const string rx_file = temp_path("missing") + "/rx";
        const string desc = mkstr("file:%s,%s", tx.c_str(), rx_file.c_str());

        mock_publisher publisher(LOG_WARN, LOG_WARN);
        publisher.expect(LOG_WARN, "failed to open rx file");
        publisher.expect(LOG_WARN, "failed to open tx file");

        size_t id = term.create_backend(desc);
        send("lost");
        EXPECT_EQ(flush_rx(), "!");
        EXPECT_TRUE(term.destroy_backend(id));
        EXPECT_FALSE(mwr::file_exists(tx));
    }

    void test_fd() {
        fflush(stdout);
        internal::CaptureStdout();
        size_t id = term.create_backend("stdout");
        EXPECT_EQ(backend_type(id), "stdout");
        send("data for stdout");
        EXPECT_EQ(flush_rx(), "!");
        EXPECT_TRUE(term.destroy_backend(id));
        string out = internal::GetCapturedStdout();
        EXPECT_THAT(out, HasSubstr("data for stdout"));

        fflush(stderr);
        internal::CaptureStderr();
        id = term.create_backend("stderr");
        EXPECT_EQ(backend_type(id), "stderr");
        send("data for stderr");
        EXPECT_EQ(flush_rx(), "!");
        EXPECT_TRUE(term.destroy_backend(id));
        string err = internal::GetCapturedStderr();
        EXPECT_THAT(err, HasSubstr("data for stderr"));
        EXPECT_THAT(out, Not(HasSubstr("stderr")));
        EXPECT_THAT(err, Not(HasSubstr("stdout")));
    }

    void test_tcp() {
        size_t id = term.create_backend("tcp:0,localhost");
        string type = backend_type(id);
        ASSERT_THAT(type, StartsWith("tcp:"));
        u16 port = from_string<u16>(type.substr(4));
        EXPECT_NE(port, 0);

        mwr::socket client0("localhost", port);
        mwr::socket client1("localhost", port);

        rx.clear();
        client0.send_char('a');
        EXPECT_TRUE(wait_rx(1)) << "timeout waiting for client0";
        client1.send_char('\xff');
        EXPECT_TRUE(wait_rx(2)) << "timeout waiting for client1";
        EXPECT_EQ(rx, "a\xff");

        send(string("xy\0z", 4));
        EXPECT_EQ(recv_from(client0, 4), string("xy\0z", 4));
        EXPECT_EQ(recv_from(client1, 4), string("xy\0z", 4));

        client0.disconnect();
        send("1");
        EXPECT_EQ(recv_from(client1, 1), "1");
        client1.send("bc");
        EXPECT_TRUE(wait_rx(4)) << "timeout waiting for client1";
        EXPECT_EQ(rx,
                  "a\xff"
                  "bc");
        send("2");
        EXPECT_EQ(recv_from(client1, 1), "2");

        EXPECT_TRUE(term.destroy_backend(id));
        EXPECT_ANY_THROW(client1.peek(TIMEOUT_MS));
        EXPECT_FALSE(client1.is_connected());

        id = term.create_backend("tcp");
        EXPECT_THAT(backend_type(id), StartsWith("tcp:"));
        EXPECT_EQ(flush_rx(), "!");
        EXPECT_TRUE(term.destroy_backend(id));
    }

#ifdef HAVE_PTY
    struct tty_result {
        bool valid;
        string type;
        string errors;
        string received;
        bool conflict;
        tcflag_t lflag_before;
        tcflag_t lflag_active;
        tcflag_t lflag_after;
        vector<string> missing;
    };

    tty_result run_tty(const string& backend, size_t cols,
                       const string& guest_tx,
                       const vector<string>& expected) {
        const string input = { 'a', 0x01, 'a', 0x01, 'b', '\r', '\n', 'c' };

        tty_result res{};
        res.conflict = true;
        pty tty(cols);
        res.valid = tty.valid();
        if (!res.valid)
            return res;

        res.lflag_before = tty.lflag();
        size_t id = term.create_backend(backend);
        res.lflag_active = tty.lflag();
        res.type = backend_type(id);
        for (const char* other : { "term", "tui" }) {
            stringstream ss;
            res.conflict &= !term.execute("create_backend", { other }, ss);
            res.errors += ss.str();
        }

        send(guest_tx);
        for (const string& text : expected) {
            if (!tty.wait_output(text))
                res.missing.push_back(text);
        }

        rx.clear();
        tty.input(input);
        wait_rx(5);
        wait(10 * term.serial_tx.cycle());
        res.received = rx;

        term.destroy_backend(id);
        res.lflag_after = tty.lflag();
        return res;
    }

    void check_tty(const tty_result& res) {
        const tcflag_t flags = ICANON | ECHO | ISIG;
        const string received = { 'a', 0x01, 'b', '\r', 'c' };
        EXPECT_THAT(res.missing, IsEmpty());
        EXPECT_TRUE(res.conflict);
        EXPECT_THAT(res.errors, HasSubstr("stdin already owned by " +
                                          res.type + " on test.term"));
        EXPECT_EQ(res.received, received);
        EXPECT_EQ(res.lflag_before & flags, flags);
        EXPECT_EQ(res.lflag_active & flags, 0u);
        EXPECT_EQ(res.lflag_after, res.lflag_before);
        EXPECT_EQ(execute("list_backends", {}), "");
    }
#endif

    void test_term() {
#ifdef HAVE_PTY
        tty_result res = run_tty("term", 80, "hello\nterm", { "hello\nterm" });
        if (!res.valid) {
            log_warn("no pty available, term backend not tested");
            return;
        }

        EXPECT_EQ(res.type, "term");
        check_tty(res);
#else
        log_warn("no pty support, term backend not tested");
#endif
    }

    void test_tui() {
#ifdef HAVE_PTY
        const string line0(80, '#');
        const string line1 = "wrap";
        tty_result res = run_tty(
            "tui", line0.length(), line0 + line1 + "\nrest\b\x1b[J",
            { "\r\x1b[K" + line0 + "\n", "\r\x1b[K" + line1 + "\n",
              "\x1b[F\x1b[Krest\b\x1b[J\0337\x1b[E\x1b[7m time ",
              "\x1b[0m\0338" });
        if (!res.valid) {
            log_warn("no pty available, tui backend not tested");
            return;
        }

        EXPECT_EQ(res.type, "tui");
        check_tty(res);
#else
        log_warn("no pty support, tui backend not tested");
#endif
    }

    void test_term_quit() {
#ifdef HAVE_PTY
        quit_tty = std::make_unique<pty>(80);
        if (!quit_tty->valid()) {
            quit_tty.reset();
            log_warn("no pty available, term quit not tested");
            return;
        }

        term.create_backend("term");
        quit_tty->input("\x01x");

        u64 timeout = mwr::timestamp_ms() + TIMEOUT_MS;
        while (mwr::timestamp_ms() < timeout)
            wait(term.serial_tx.cycle());

        execute("destroy_backend", { "all" });
        quit_tty.reset();
        FAIL() << "simulation not stopped by ctrl-a x";
#else
        log_warn("no pty support, term quit not tested");
#endif
    }
};

TEST(serial, backends) {
    serial::backend::define("failing", create_failing_backend);

    broker broker("test");
    broker.define("test.term.backends", "null nothing");
    broker.define("test.term_odd.config", "38400o7");
    broker.define("test.term_even.config", "57600E6");
    broker.define("test.term_mark.config", "115200m5");
    broker.define("test.term_space.config", "1200S8");
    broker.define("test.term_wide.config", "4800n9");
    broker.define("test.term_narrow.config", "2400x4");
    broker.define("test.term_bad.config", "fast");
    broker.define("test.term_empty.config", "");

    serial_backends_test test("test");
    sc_core::sc_start();
}
