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
#include <random>

#if defined(MWR_LINUX) && __has_include(<linux/can/raw.h>)
#define TEST_SOCKETCAN

#include <poll.h>
#include <unistd.h>
#include <sys/ioctl.h>
#include <sys/socket.h>

#include <linux/if.h>
#include <linux/can.h>
#include <linux/can/raw.h>

class host_can
{
private:
    int m_fd;
    size_t m_mtu;
    bool m_xl;

    bool setup(const string& ifname, const vector<u32>& ids) {
        struct ifreq req{};
        strncpy(req.ifr_name, ifname.c_str(), sizeof(req.ifr_name) - 1);
        if (ioctl(m_fd, SIOCGIFMTU, &req) < 0)
            return false;

        m_mtu = req.ifr_mtu;
        int enable = 1;
        if (m_mtu >= CANFD_MTU &&
            setsockopt(m_fd, SOL_CAN_RAW, CAN_RAW_FD_FRAMES, &enable,
                       sizeof(enable)) < 0) {
            return false;
        }

#ifdef CANXL_MTU
        if (m_mtu >= CANXL_MIN_MTU) {
            m_xl = setsockopt(m_fd, SOL_CAN_RAW, CAN_RAW_XL_FRAMES, &enable,
                              sizeof(enable)) == 0;
        }
#endif

        vector<::can_filter> filters;
        for (u32 id : ids) {
            ::can_filter filter{};
            if (id > CAN_SFF_MASK) {
                filter.can_id = id | CAN_EFF_FLAG;
                filter.can_mask = CAN_EFF_MASK | CAN_EFF_FLAG;
            } else {
                filter.can_id = id;
                filter.can_mask = CAN_SFF_MASK | CAN_EFF_FLAG;
            }
            filters.push_back(filter);
        }

        if (setsockopt(m_fd, SOL_CAN_RAW, CAN_RAW_FILTER, filters.data(),
                       filters.size() * sizeof(::can_filter)) < 0) {
            return false;
        }

        if (ioctl(m_fd, SIOCGIFINDEX, &req) < 0)
            return false;

        struct sockaddr_can addr{};
        addr.can_family = AF_CAN;
        addr.can_ifindex = req.ifr_ifindex;
        return bind(m_fd, (struct sockaddr*)&addr, sizeof(addr)) == 0;
    }

public:
    bool is_open() const { return m_fd >= 0; }
    bool has_fd() const { return m_mtu >= CANFD_MTU; }
    bool has_xl() const { return m_xl; }

    host_can(const string& ifname, const vector<u32>& ids):
        m_fd(socket(PF_CAN, SOCK_RAW | SOCK_CLOEXEC, CAN_RAW)),
        m_mtu(0),
        m_xl(false) {
        if (m_fd >= 0 && !setup(ifname, ids)) {
            close(m_fd);
            m_fd = -1;
        }
    }

    ~host_can() {
        if (m_fd >= 0)
            close(m_fd);
    }

    host_can(const host_can&) = delete;
    host_can& operator=(const host_can&) = delete;

    bool send(const void* data, size_t len) {
        return write(m_fd, data, len) == (ssize_t)len;
    }

    size_t recv(void* data, size_t len) {
        struct pollfd pfd{};
        pfd.fd = m_fd;
        pfd.events = POLLIN;
        if (poll(&pfd, 1, 5000) <= 0)
            return 0;

        ssize_t n = read(m_fd, data, len);
        return n < 0 ? 0 : (size_t)n;
    }
};

union host_frame {
    ::can_frame cc;
    ::canfd_frame fd;
#ifdef CANXL_MTU
    ::canxl_frame xl;
#endif
};

static size_t count_open_fds() {
    size_t count = 0;
    for (const auto& entry :
         std::filesystem::directory_iterator("/proc/self/fd")) {
        if (!entry.path().empty())
            count++;
    }
    return count;
}

#endif

enum frame_type { FRAME_CC, FRAME_FD, FRAME_XL };

class can_backends_test : public test_base, public can_host
{
public:
    enum : u32 { SYNC_SID = 0x7e5 };

    const u32 test_sid;
    const u32 test_eid;

    can::bridge bridge;
    can_initiator_socket can_tx;
    can_target_socket can_rx;

    vector<vcml::can_frame> received;

    virtual void can_receive(const can_target_socket& socket,
                             vcml::can_frame& frame) override {
        received.push_back(frame);
    }

    can_backends_test(const sc_module_name& nm):
        test_base(nm),
        can_host(),
        test_sid(std::random_device()() & CAN_MSG_ID_SID),
        test_eid((std::random_device()() & CAN_MSG_ID_EID) | bit(28)),
        bridge("bridge"),
        can_tx("can_tx"),
        can_rx("can_rx"),
        received() {
        can_tx.bind(bridge.can_rx);
        bridge.can_tx.bind(can_rx);

        add_test("strings", &can_backends_test::test_strings);
        add_test("unknown", &can_backends_test::test_unknown);
        add_test("serialize", &can_backends_test::test_serialize);
        add_test("file", &can_backends_test::test_file);
        add_test("file_default", &can_backends_test::test_file_default);
        add_test("file_error", &can_backends_test::test_file_error);
        add_test("tcp_create", &can_backends_test::test_tcp_create);
        add_test("tcp_to_host", &can_backends_test::test_tcp_to_host);
        add_test("tcp_to_guest", &can_backends_test::test_tcp_to_guest);
        add_test("tcp_clients", &can_backends_test::test_tcp_clients);
        add_test("tcp_disconnect", &can_backends_test::test_tcp_disconnect);
        add_test("tcp_destroy", &can_backends_test::test_tcp_destroy);
        add_test("socket_error", &can_backends_test::test_socket_error);
        add_test("socket", &can_backends_test::test_socket);
    }

    static vcml::can_frame make_frame(frame_type type, u32 id, size_t len,
                                      u8 seed = 0) {
        vcml::can_frame frame{};
        frame.canid = id;
        frame.eff = id > CAN_MSG_ID_SID;
        frame.fdf = type != FRAME_CC;
        frame.xlf = type == FRAME_XL;
        frame.dlc = len2dlc(len, type == FRAME_XL);
        frame.data.resize(len);
        for (size_t i = 0; i < len; i++)
            frame.data[i] = (u8)(seed + i);
        return frame;
    }

    static vector<vcml::can_frame> make_frames() {
        vector<vcml::can_frame> frames;
        frames.push_back(make_frame(FRAME_CC, 0x123, 0));
        frames.push_back(make_frame(FRAME_CC, 0x7ff, 8, 0x10));
        frames.push_back(make_frame(FRAME_CC, 0x1fffffff, 3, 0x20));
        frames.push_back(make_frame(FRAME_CC, 0x234, 0));
        frames.back().rtr = true;
        frames.push_back(make_frame(FRAME_CC, 0x345, 8, 0x30));
        frames.back().err = true;
        frames.push_back(make_frame(FRAME_FD, 0x456, 0));
        frames.push_back(make_frame(FRAME_FD, 0x567, 12, 0x40));
        frames.back().brs = true;
        frames.push_back(make_frame(FRAME_FD, 0x12345678, 64, 0x50));
        frames.back().esi = true;
        frames.back().rrs = true;
        frames.push_back(make_frame(FRAME_XL, 0x678, 1, 0x60));
        frames.push_back(make_frame(FRAME_XL, 0x789, 2048, 0x70));
        frames.back().sec = true;
        frames.back().rrs = true;
        frames.back().sdt = 0x21;
        frames.back().vcid = 0x43;
        frames.back().af = 0xcafe1234;
        return frames;
    }

    static void expect_same(const vcml::can_frame& actual,
                            const vcml::can_frame& expected) {
        EXPECT_EQ(actual, expected);
        EXPECT_EQ(actual.canid, expected.canid) << expected;
        EXPECT_EQ(actual.eff, expected.eff) << expected;
        EXPECT_EQ(actual.rtr, expected.rtr) << expected;
        EXPECT_EQ(actual.err, expected.err) << expected;
        EXPECT_EQ(actual.brs, expected.brs) << expected;
        EXPECT_EQ(actual.esi, expected.esi) << expected;
        EXPECT_EQ(actual.fdf, expected.fdf) << expected;
        EXPECT_EQ(actual.sec, expected.sec) << expected;
        EXPECT_EQ(actual.rrs, expected.rrs) << expected;
        EXPECT_EQ(actual.xlf, expected.xlf) << expected;
        EXPECT_EQ(actual.dlc, expected.dlc) << expected;
        EXPECT_EQ(actual.sdt, expected.sdt) << expected;
        EXPECT_EQ(actual.vcid, expected.vcid) << expected;
        EXPECT_EQ(actual.af, expected.af) << expected;
        EXPECT_EQ(actual.data, expected.data) << expected;
    }

    static vector<u8> encode(const vcml::can_frame& frame) {
        vector<u8> buffer;
        can::serialize(frame, [&](const u8* data, size_t size) {
            buffer.insert(buffer.end(), data, data + size);
        });
        return buffer;
    }

    static vcml::can_frame decode(const vector<u8>& buffer) {
        size_t pos = 0;
        vcml::can_frame frame{};
        can::deserialize(frame, [&](u8* data, size_t size) {
            VCML_REPORT_ON(pos + size > buffer.size(), "buffer underrun");
            std::copy_n(buffer.begin() + pos, size, data);
            pos += size;
        });
        EXPECT_EQ(pos, buffer.size());
        return frame;
    }

    static void send_frame(mwr::socket& sock, const vcml::can_frame& frame) {
        can::serialize(frame, [&](const u8* data, size_t size) {
            sock.send(data, size);
        });
    }

    static vcml::can_frame recv_frame(mwr::socket& sock) {
        vcml::can_frame frame{};
        can::deserialize(frame, [&](u8* data, size_t size) {
            while (size > 0) {
                size_t n = min(sock.peek(5000), size);
                VCML_REPORT_ON(n == 0, "timeout waiting for frame data");
                sock.recv(data, n);
                data += n;
                size -= n;
            }
        });
        return frame;
    }

    void send_to_bridge(const vcml::can_frame& frame) {
        vcml::can_frame tx = frame;
        can_tx.send(tx);
    }

    bool wait_until(const function<bool(void)>& done) {
        u64 timeout = mwr::timestamp_ms() + 10000;
        while (!done()) {
            if (mwr::timestamp_ms() > timeout)
                return false;
            wait(clock_cycles(1));
            std::this_thread::yield();
        }
        return true;
    }

    bool wait_received(size_t count) {
        return wait_until([&]() { return received.size() >= count; });
    }

    bool wait_received(u32 id, vcml::can_frame& frame) {
        return wait_until([&]() {
            for (const vcml::can_frame& rx : received) {
                if (rx.id() == id && rx.eff == (id > CAN_MSG_ID_SID)) {
                    frame = rx;
                    return true;
                }
            }
            return false;
        });
    }

    string backend_type(size_t id) {
        stringstream ss;
        EXPECT_TRUE(bridge.execute("list_backends", { to_string(id) }, ss));
        string prefix = to_string(id) + ": ";
        string list = ss.str();
        if (!starts_with(list, prefix) || !ends_with(list, ","))
            return list;
        return list.substr(prefix.length(),
                           list.length() - prefix.length() - 1);
    }

    string create_error(const string& desc) {
        try {
            size_t id = bridge.create_backend(desc);
            bridge.destroy_backend(id);
            return "";
        } catch (std::exception& ex) {
            return ex.what();
        }
    }

    u16 backend_port(size_t id) {
        string type = backend_type(id);
        if (!starts_with(type, "tcp:"))
            return 0;
        return from_string<u16>(type.substr(4));
    }

    bool connect(mwr::socket& sock, u16 port) {
        size_t count = received.size();
        sock.connect("localhost", port);
        send_frame(sock, make_frame(FRAME_CC, SYNC_SID, 0));
        if (!wait_received(count + 1))
            return false;

        bool match = received.back().id() == SYNC_SID;
        received.pop_back();
        return match;
    }

    static string temp_file(const string& name) {
        string file = mkstr("vcml_can_backends_%d_%llu_%s", mwr::getpid(),
                            (unsigned long long)mwr::timestamp_ns(),
                            name.c_str());
        std::filesystem::path dir(mwr::temp_dir());
        return (dir / file).generic_string();
    }

    static vector<string> read_lines(const string& path) {
        vector<string> lines;
        ifstream file(path);
        string line;
        while (std::getline(file, line))
            lines.push_back(line);
        return lines;
    }

    void test_strings() {
        EXPECT_STREQ(bridge.kind(), "vcml::can::bridge");
        EXPECT_STREQ(bridge.version(), VCML_VERSION_STRING);
    }

    void test_unknown() {
        string err = create_error("nothing:1,2");
        EXPECT_THAT(err, HasSubstr("unknown"));
        EXPECT_THAT(err, HasSubstr("'nothing:1,2'"));
        EXPECT_THAT(err, HasSubstr("file"));
        EXPECT_THAT(err, HasSubstr("tcp"));
        EXPECT_THAT(create_error(""), HasSubstr("unknown"));

        stringstream ss;
        EXPECT_TRUE(bridge.execute("list_backends", ss));
        EXPECT_EQ(ss.str(), "");
    }

    void test_serialize() {
        vcml::can_frame frame = make_frame(FRAME_FD, 0x1abcdef5, 12, 0xa0);
        frame.brs = true;
        frame.esi = true;
        frame.sdt = 0x12;
        frame.vcid = 0x34;
        frame.af = 0xa1b2c3d4;

        vector<u8> expected = {
            0xf5, 0xde, 0xbc, 0x1a, 0x39, 0x00, 0x00, 0x00, 0x09, 0x00,
            0x12, 0x34, 0xd4, 0xc3, 0xb2, 0xa1, 0x0c, 0x00, 0xa0, 0xa1,
            0xa2, 0xa3, 0xa4, 0xa5, 0xa6, 0xa7, 0xa8, 0xa9, 0xaa, 0xab,
        };

        EXPECT_EQ(encode(frame), expected);
        expect_same(decode(expected), frame);

        frame = make_frame(FRAME_CC, 0x7ff, 0);
        frame.canid = 0xfffff7ff;
        frame.rtr = true;
        frame.err = true;
        frame.sec = true;
        frame.rrs = true;
        frame.xlf = true;
        expected = {
            0xff, 0x07, 0x00, 0x00, 0xc6, 0x01, 0x00, 0x00, 0x00,
            0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
        };

        EXPECT_EQ(encode(frame), expected);
        frame.canid = 0x7ff;
        expect_same(decode(expected), frame);

        for (const vcml::can_frame& orig : make_frames()) {
            vector<u8> buffer = encode(orig);
            EXPECT_EQ(buffer.size(), 18 + orig.length()) << orig;
            expect_same(decode(buffer), orig);
        }

        vector<u8> truncated = encode(make_frame(FRAME_CC, 0x123, 8));
        truncated.pop_back();
        EXPECT_THROW(decode(truncated), std::exception);
    }

    void test_file() {
        string path = temp_file("frames.tx");
        size_t id0 = bridge.create_backend("file:" + path);
        EXPECT_EQ(backend_type(id0), "file:" + path);
        EXPECT_TRUE(mwr::file_exists(path));
        EXPECT_TRUE(read_lines(path).empty());

        vcml::can_frame frame0 = make_frame(FRAME_CC, 0x123, 3, 1);
        vcml::can_frame frame1 = make_frame(FRAME_FD, 0x1abcdef5, 12, 0xa0);
        frame1.brs = true;
        vcml::can_frame frame2 = make_frame(FRAME_CC, 0x7ff, 0);

        string time0 = sc_time_stamp().to_string();
        send_to_bridge(frame0);
        EXPECT_THAT(
            read_lines(path),
            ElementsAre("[" + time0 + "] frame #1 CAN 123 [3] " + "01 02 03"));

        wait(clock_cycles(10));
        string time1 = sc_time_stamp().to_string();
        EXPECT_NE(time0, time1);
        send_to_bridge(frame1);
        send_to_bridge(frame2);
        EXPECT_TRUE(bridge.destroy_backend(id0));

        send_to_bridge(frame0);
        EXPECT_THAT(
            read_lines(path),
            ElementsAre("[" + time0 + "] frame #1 CAN 123 [3] 01 02 03",
                        "[" + time1 + "] frame #2 CANFD +eff +brs " +
                            "1abcdef5 [c] a0 a1 a2 a3 a4 a5 a6 a7 a8 a9 " +
                            "aa ab",
                        "[" + time1 + "] frame #3 CAN 7ff [0] <no data>"));

        string other = temp_file("other.tx");
        size_t id1 = bridge.create_backend("file:" + path);
        size_t id2 = bridge.create_backend("file:" + other + ",ignored");
        EXPECT_EQ(backend_type(id2), "file:" + other);
        send_to_bridge(frame2);
        EXPECT_TRUE(bridge.destroy_backend(id2));
        send_to_bridge(frame0);
        EXPECT_TRUE(bridge.destroy_backend(id1));

        EXPECT_THAT(
            read_lines(path),
            ElementsAre("[" + time1 + "] frame #1 CAN 7ff [0] " + "<no data>",
                        "[" + time1 + "] frame #2 CAN 123 [3] " + "01 02 03"));
        EXPECT_THAT(read_lines(other),
                    ElementsAre("[" + time1 + "] frame #1 CAN 7ff [0] " +
                                "<no data>"));

        EXPECT_TRUE(received.empty());
        std::filesystem::remove(path);
        std::filesystem::remove(other);
    }

    void test_file_default() {
        string path = mkstr("%s.tx", bridge.name());
        string dir = temp_file("cwd");
        auto cwd = std::filesystem::current_path();
        ASSERT_TRUE(std::filesystem::create_directory(dir));
        std::filesystem::current_path(dir);

        size_t id = bridge.create_backend("file");
        EXPECT_EQ(backend_type(id), "file:" + path);
        string time = sc_time_stamp().to_string();
        send_to_bridge(make_frame(FRAME_CC, 0x42, 1, 0xff));
        EXPECT_TRUE(bridge.destroy_backend(id));

        EXPECT_THAT(read_lines(path),
                    ElementsAre("[" + time + "] frame #1 CAN 42 [1] ff"));
        std::filesystem::current_path(cwd);
        std::filesystem::remove_all(dir);
    }

    void test_file_error() {
        string path = temp_file("missing") + "/frames.tx";
        size_t id = 0;

        {
            mock_publisher publisher(LOG_WARN, LOG_WARN);
            publisher.expect(LOG_WARN, "failed to open file '" + path + "'");
            ASSERT_NO_THROW(id = bridge.create_backend("file:" + path));
        }

        EXPECT_EQ(backend_type(id), "file:" + path);
        send_to_bridge(make_frame(FRAME_CC, 0x123, 3));
        send_to_bridge(make_frame(FRAME_FD, 0x123, 64));
        EXPECT_FALSE(mwr::file_exists(path));
        EXPECT_TRUE(bridge.destroy_backend(id));
    }

    void test_tcp_create() {
        size_t id0 = bridge.create_backend("tcp");
        size_t id1 = bridge.create_backend("tcp:0");
        size_t id2 = bridge.create_backend("tcp:0,localhost");

        u16 port0 = backend_port(id0);
        u16 port1 = backend_port(id1);
        u16 port2 = backend_port(id2);

        EXPECT_NE(port0, 0);
        EXPECT_NE(port1, 0);
        EXPECT_NE(port2, 0);
        EXPECT_NE(port0, port1);
        EXPECT_NE(port0, port2);
        EXPECT_NE(port1, port2);

        EXPECT_EQ(backend_type(id0), mkstr("tcp:%hu", port0));

        send_to_bridge(make_frame(FRAME_CC, 0x123, 8));

        EXPECT_TRUE(bridge.destroy_backend(id0));
        EXPECT_TRUE(bridge.destroy_backend(id1));
        EXPECT_TRUE(bridge.destroy_backend(id2));
    }

    void test_tcp_to_host() {
        received.clear();
        size_t id = bridge.create_backend("tcp:0");
        u16 port = backend_port(id);
        ASSERT_NE(port, 0);

        mwr::socket client;
        ASSERT_TRUE(connect(client, port));

        vcml::can_frame frame = make_frame(FRAME_FD, 0x1abcdef5, 12, 0xa0);
        frame.brs = true;
        send_to_bridge(frame);

        vector<u8> expected = encode(frame);
        vector<u8> raw(expected.size());
        ASSERT_GE(client.peek(5000), 1u);
        client.recv(raw.data(), raw.size());
        EXPECT_EQ(raw, expected);

        for (const vcml::can_frame& orig : make_frames()) {
            send_to_bridge(orig);
            vcml::can_frame rx{};
            ASSERT_NO_THROW(rx = recv_frame(client)) << orig;
            expect_same(rx, orig);
        }

        vector<vcml::can_frame> frames = make_frames();
        for (const vcml::can_frame& orig : frames)
            send_to_bridge(orig);
        for (const vcml::can_frame& orig : frames) {
            vcml::can_frame rx{};
            ASSERT_NO_THROW(rx = recv_frame(client)) << orig;
            expect_same(rx, orig);
        }

        EXPECT_EQ(client.peek(0), 0u);
        EXPECT_TRUE(received.empty());
        client.disconnect();
        EXPECT_TRUE(bridge.destroy_backend(id));
    }

    void test_tcp_to_guest() {
        received.clear();
        size_t id = bridge.create_backend("tcp:0");
        u16 port = backend_port(id);
        ASSERT_NE(port, 0);

        mwr::socket client;
        ASSERT_TRUE(connect(client, port));

        vector<vcml::can_frame> frames = make_frames();
        for (size_t i = 0; i < frames.size(); i++) {
            send_frame(client, frames[i]);
            ASSERT_TRUE(wait_received(i + 1)) << frames[i];
            expect_same(received[i], frames[i]);
        }

        received.clear();
        vector<u8> stream;
        for (const vcml::can_frame& frame : frames) {
            vector<u8> buffer = encode(frame);
            stream.insert(stream.end(), buffer.begin(), buffer.end());
        }

        client.send(stream.data(), stream.size());
        ASSERT_TRUE(wait_received(frames.size()));
        ASSERT_EQ(received.size(), frames.size());
        for (size_t i = 0; i < frames.size(); i++)
            expect_same(received[i], frames[i]);

        received.clear();
        vector<u8> buffer = encode(frames[1]);
        client.send(buffer.data(), 7);
        wait(clock_cycles(10));
        EXPECT_TRUE(received.empty());
        client.send(buffer.data() + 7, 13);
        wait(clock_cycles(10));
        EXPECT_TRUE(received.empty());
        client.send(buffer.data() + 20, buffer.size() - 20);
        ASSERT_TRUE(wait_received(1));
        expect_same(received[0], frames[1]);

        EXPECT_EQ(client.peek(0), 0u);
        client.disconnect();
        EXPECT_TRUE(bridge.destroy_backend(id));
    }

    void test_tcp_clients() {
        received.clear();
        size_t id0 = bridge.create_backend("tcp:0");
        size_t id1 = bridge.create_backend("tcp:0");
        u16 port0 = backend_port(id0);
        u16 port1 = backend_port(id1);
        ASSERT_NE(port0, 0);
        ASSERT_NE(port1, 0);

        mwr::socket client0;
        mwr::socket client1;
        mwr::socket client2;
        ASSERT_TRUE(connect(client0, port0));
        ASSERT_TRUE(connect(client1, port0));
        ASSERT_TRUE(connect(client2, port1));

        vcml::can_frame frame0 = make_frame(FRAME_CC, 0x101, 8, 1);
        vcml::can_frame frame1 = make_frame(FRAME_FD, 0x102, 64, 2);
        vcml::can_frame frame2 = make_frame(FRAME_CC, 0x103, 0);
        vcml::can_frame frame3 = make_frame(FRAME_CC, 0x104, 4, 4);

        send_to_bridge(frame0);
        for (mwr::socket* client : { &client0, &client1, &client2 }) {
            vcml::can_frame rx{};
            ASSERT_NO_THROW(rx = recv_frame(*client));
            expect_same(rx, frame0);
        }

        send_frame(client0, frame1);
        ASSERT_TRUE(wait_received(1));
        send_frame(client1, frame2);
        ASSERT_TRUE(wait_received(2));
        send_frame(client2, frame3);
        ASSERT_TRUE(wait_received(3));
        EXPECT_THAT(received, ElementsAre(frame1, frame2, frame3));

        EXPECT_EQ(client0.peek(0), 0u);
        EXPECT_EQ(client1.peek(0), 0u);
        EXPECT_EQ(client2.peek(0), 0u);

        EXPECT_TRUE(bridge.destroy_backend(id1));
        EXPECT_THROW(client2.peek(5000), std::exception);

        send_to_bridge(frame3);
        for (mwr::socket* client : { &client0, &client1 }) {
            vcml::can_frame rx{};
            ASSERT_NO_THROW(rx = recv_frame(*client));
            expect_same(rx, frame3);
        }

        client0.disconnect();
        client1.disconnect();
        EXPECT_TRUE(bridge.destroy_backend(id0));
    }

    void test_tcp_disconnect() {
        received.clear();
        size_t id = bridge.create_backend("tcp:0");
        u16 port = backend_port(id);
        ASSERT_NE(port, 0);

        vcml::can_frame frame0 = make_frame(FRAME_CC, 0x201, 8, 1);
        vcml::can_frame frame1 = make_frame(FRAME_FD, 0x202, 64, 2);
        vector<u8> buffer = encode(frame0);

        mwr::socket client0;
        ASSERT_TRUE(connect(client0, port));
        client0.send(buffer.data(), 10);
        client0.disconnect();

        mwr::socket client1;
        ASSERT_TRUE(connect(client1, port));
        client1.send(buffer.data(), buffer.size() - 3);
        client1.disconnect();

        mwr::socket client2;
        ASSERT_TRUE(connect(client2, port));
        client2.disconnect();

        send_to_bridge(frame0);
        send_to_bridge(frame1);

        mwr::socket client3;
        ASSERT_TRUE(connect(client3, port));
        send_frame(client3, frame1);
        ASSERT_TRUE(wait_received(1));
        wait(clock_cycles(10));
        EXPECT_THAT(received, ElementsAre(frame1));

        send_to_bridge(frame0);
        vcml::can_frame rx{};
        ASSERT_NO_THROW(rx = recv_frame(client3));
        expect_same(rx, frame0);

        client3.disconnect();
        send_to_bridge(frame0);
        send_to_bridge(frame1);
        EXPECT_TRUE(bridge.destroy_backend(id));
    }

    void test_tcp_destroy() {
        received.clear();
        size_t id = bridge.create_backend("tcp:0");
        u16 port = backend_port(id);
        ASSERT_NE(port, 0);

        mwr::socket idle;
        mwr::socket busy;
        ASSERT_TRUE(connect(idle, port));
        ASSERT_TRUE(connect(busy, port));

        vcml::can_frame frame = make_frame(FRAME_FD, 0x301, 64, 3);
        vector<u8> buffer = encode(frame);
        busy.send(buffer.data(), buffer.size() - 1);

        EXPECT_TRUE(bridge.destroy_backend(id));
        EXPECT_EQ(backend_type(id), "none");
        EXPECT_THROW(idle.peek(5000), std::exception);
        EXPECT_THROW(busy.peek(5000), std::exception);
        EXPECT_FALSE(idle.is_connected());
        EXPECT_FALSE(busy.is_connected());

        send_to_bridge(frame);
        wait(clock_cycles(10));
        EXPECT_TRUE(received.empty());
    }

    void test_socket_error() {
#ifdef TEST_SOCKETCAN
        size_t fds = count_open_fds();
#endif
        EXPECT_FALSE(create_error("socket:vcml_nodev0").empty());
        EXPECT_FALSE(create_error("socket").empty());
        EXPECT_FALSE(create_error("socket:vcml_nodev1").empty());
#ifdef TEST_SOCKETCAN
        EXPECT_EQ(count_open_fds(), fds);
#endif
    }

#ifdef TEST_SOCKETCAN
    void test_socket_cc(host_can& host) {
        host_frame raw{};
        vcml::can_frame frame = make_frame(FRAME_CC, test_sid, 8, 0x11);
        send_to_bridge(frame);
        ASSERT_EQ(host.recv(&raw, sizeof(raw)), sizeof(raw.cc));
        EXPECT_EQ(raw.cc.can_id, test_sid);
        ASSERT_EQ(raw.cc.can_dlc, 8);
        EXPECT_EQ(vector<u8>(raw.cc.data, raw.cc.data + 8), frame.data);

        frame = make_frame(FRAME_CC, test_eid, 0);
        send_to_bridge(frame);
        raw = host_frame{};
        ASSERT_EQ(host.recv(&raw, sizeof(raw)), sizeof(raw.cc));
        EXPECT_EQ(raw.cc.can_id, test_eid | CAN_EFF_FLAG);
        EXPECT_EQ(raw.cc.can_dlc, 0);

        frame = make_frame(FRAME_CC, test_sid, 0);
        frame.rtr = true;
        send_to_bridge(frame);
        raw = host_frame{};
        ASSERT_EQ(host.recv(&raw, sizeof(raw)), sizeof(raw.cc));
        EXPECT_EQ(raw.cc.can_id, test_sid | CAN_RTR_FLAG);
        EXPECT_EQ(raw.cc.can_dlc, 0);

        received.clear();
        raw = host_frame{};
        raw.cc.can_id = test_sid;
        raw.cc.can_dlc = 8;
        for (size_t i = 0; i < 8; i++)
            raw.cc.data[i] = (u8)(0x22 + i);
        ASSERT_TRUE(host.send(&raw.cc, sizeof(raw.cc)));
        ASSERT_TRUE(wait_received(test_sid, frame));
        expect_same(frame, make_frame(FRAME_CC, test_sid, 8, 0x22));

        received.clear();
        raw = host_frame{};
        raw.cc.can_id = test_eid | CAN_EFF_FLAG;
        ASSERT_TRUE(host.send(&raw.cc, sizeof(raw.cc)));
        ASSERT_TRUE(wait_received(test_eid, frame));
        expect_same(frame, make_frame(FRAME_CC, test_eid, 0));

        received.clear();
        raw = host_frame{};
        raw.cc.can_id = test_sid | CAN_RTR_FLAG;
        ASSERT_TRUE(host.send(&raw.cc, sizeof(raw.cc)));
        ASSERT_TRUE(wait_received(test_sid, frame));
        vcml::can_frame expected = make_frame(FRAME_CC, test_sid, 0);
        expected.rtr = true;
        expect_same(frame, expected);
    }

    void test_socket_fd(host_can& host) {
        host_frame raw{};
        vcml::can_frame frame = make_frame(FRAME_FD, test_eid, 64, 0x33);
        frame.brs = true;
        frame.esi = true;
        send_to_bridge(frame);
        ASSERT_EQ(host.recv(&raw, sizeof(raw)), CANFD_MTU);
        EXPECT_EQ(raw.fd.can_id, test_eid | CAN_EFF_FLAG);
        EXPECT_TRUE(raw.fd.flags & CANFD_BRS);
        EXPECT_TRUE(raw.fd.flags & CANFD_ESI);
        ASSERT_EQ(raw.fd.len, 64);
        EXPECT_EQ(vector<u8>(raw.fd.data, raw.fd.data + 64), frame.data);

        frame = make_frame(FRAME_FD, test_sid, 0);
        send_to_bridge(frame);
        raw = host_frame{};
        ASSERT_EQ(host.recv(&raw, sizeof(raw)), CANFD_MTU);
        EXPECT_EQ(raw.fd.can_id, test_sid);
        EXPECT_FALSE(raw.fd.flags & CANFD_BRS);
        EXPECT_FALSE(raw.fd.flags & CANFD_ESI);
        EXPECT_EQ(raw.fd.len, 0);

        received.clear();
        raw = host_frame{};
        raw.fd.can_id = test_sid;
        raw.fd.len = 12;
        raw.fd.flags = CANFD_BRS;
#ifdef CANFD_FDF
        raw.fd.flags |= CANFD_FDF;
#endif
        for (size_t i = 0; i < 12; i++)
            raw.fd.data[i] = (u8)(0x44 + i);
        ASSERT_TRUE(host.send(&raw.fd, sizeof(raw.fd)));
        ASSERT_TRUE(wait_received(test_sid, frame));
        vcml::can_frame expected = make_frame(FRAME_FD, test_sid, 12, 0x44);
        expected.brs = true;
        expect_same(frame, expected);

        received.clear();
        raw = host_frame{};
        raw.fd.can_id = test_eid | CAN_EFF_FLAG;
        raw.fd.len = 64;
        raw.fd.flags = CANFD_ESI;
#ifdef CANFD_FDF
        raw.fd.flags |= CANFD_FDF;
#endif
        for (size_t i = 0; i < 64; i++)
            raw.fd.data[i] = (u8)(0x55 + i);
        ASSERT_TRUE(host.send(&raw.fd, sizeof(raw.fd)));
        ASSERT_TRUE(wait_received(test_eid, frame));
        expected = make_frame(FRAME_FD, test_eid, 64, 0x55);
        expected.esi = true;
        expect_same(frame, expected);
    }

    void test_socket_xl(host_can& host) {
#ifdef CANXL_MTU
        auto raw = std::make_unique<host_frame>();
        vcml::can_frame frame = make_frame(FRAME_XL, test_sid, 2048, 0x66);
        frame.sec = true;
        frame.sdt = 0x21;
        frame.af = 0xcafe1234;
        send_to_bridge(frame);
        ASSERT_EQ(host.recv(raw.get(), sizeof(host_frame)), CANXL_MTU);
        EXPECT_EQ(raw->xl.prio & CANXL_PRIO_MASK, test_sid);
        EXPECT_TRUE(raw->xl.flags & CANXL_XLF);
        EXPECT_TRUE(raw->xl.flags & CANXL_SEC);
        EXPECT_EQ(raw->xl.sdt, 0x21);
        EXPECT_EQ(raw->xl.af, 0xcafe1234);
        ASSERT_EQ(raw->xl.len, 2048);
        EXPECT_EQ(vector<u8>(raw->xl.data, raw->xl.data + 2048), frame.data);

        received.clear();
        *raw = host_frame{};
        raw->xl.prio = test_sid;
        raw->xl.flags = CANXL_XLF | CANXL_SEC;
        raw->xl.sdt = 0x65;
        raw->xl.af = 0x1234cafe;
        raw->xl.len = 5;
        for (size_t i = 0; i < 5; i++)
            raw->xl.data[i] = (u8)(0x77 + i);
        ASSERT_TRUE(host.send(&raw->xl, CANXL_HDR_SIZE + 5));
        ASSERT_TRUE(wait_received(test_sid, frame));
        vcml::can_frame expected = make_frame(FRAME_XL, test_sid, 5, 0x77);
        expected.sec = true;
        expected.sdt = 0x65;
        expected.af = 0x1234cafe;
        expect_same(frame, expected);
#endif
    }
#endif

    void test_socket() {
#ifdef TEST_SOCKETCAN
        const string ifname = "vcan0";
        string err = create_error("socket:" + ifname);
        if (!err.empty()) {
            log_warn("socket test skipped: %s", err.c_str());
            return;
        }

        host_can host(ifname, { test_sid, test_eid });
        if (!host.is_open()) {
            log_warn("socket test skipped: cannot open %s", ifname.c_str());
            return;
        }

        size_t id = bridge.create_backend("socket:" + ifname);
        EXPECT_EQ(backend_type(id), "socket:" + ifname);

        test_socket_cc(host);

        if (host.has_fd())
            test_socket_fd(host);
        else
            log_warn("socket test for CAN FD skipped: mtu too small");

        if (host.has_xl())
            test_socket_xl(host);
        else
            log_warn("socket test for CAN XL skipped: not supported");

        EXPECT_TRUE(bridge.destroy_backend(id));
        received.clear();
#else
        log_warn("socket test skipped: not supported on this host");
#endif
    }
};

TEST(can, backends) {
    can_backends_test test("test");
    sc_core::sc_start();
}
