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
#include <fstream>

namespace fs = std::filesystem;

enum : u8 {
    IP_HOST = 2,
    IP_GUEST = 15,
};

enum : size_t {
    OFF_ARP_OP = 20,
    OFF_ARP_SHA = 22,
    OFF_ARP_SPA = 28,
    OFF_ARP_THA = 32,
    OFF_ARP_TPA = 38,
    OFF_IP = 14,
    OFF_IP_LEN = 16,
    OFF_IP_PROTO = 23,
    OFF_IP_SRC = 26,
    OFF_IP_DST = 30,
    OFF_ICMP = 34,
    OFF_TCP_DPORT = 36,
    OFF_TCP_FLAGS = 47,
    OFF_IP6_NEXT = 20,
    OFF_ICMP6 = 54,
    OFF_RA_OPTS = 70,
};

static void put16(vector<u8>& v, u16 val) {
    v.push_back(val >> 8);
    v.push_back(val & 0xff);
}

static void append(vector<u8>& v, const vector<u8>& data) {
    v.insert(v.end(), data.begin(), data.end());
}

static u16 checksum(const u8* data, size_t len) {
    u32 sum = 0;
    for (size_t i = 0; i + 1 < len; i += 2)
        sum += (u32)data[i] << 8 | data[i + 1];
    if (len & 1)
        sum += (u32)data[len - 1] << 8;
    while (sum >> 16)
        sum = (sum & 0xffff) + (sum >> 16);
    return ~sum & 0xffff;
}

static vector<u8> ipv4(u8 net, u8 host) {
    return { 10, 0, net, host };
}

static vector<u8> bytes(const eth_frame& frame, size_t off, size_t len) {
    if (off + len > frame.size())
        return {};
    return vector<u8>(frame.begin() + off, frame.begin() + off + len);
}

static mac_addr host_mac(u8 net) {
    return mac_addr(0x52, 0x55, 10, 0, net, IP_HOST);
}

static bool is_arp_reply(const eth_frame& frame) {
    return frame.size() >= 42 &&
           frame.ether_type() == eth_frame::ETHER_TYPE_ARP &&
           frame.read<u16>(OFF_ARP_OP) == 2;
}

static bool is_ipv4(const eth_frame& frame, u8 proto) {
    return frame.size() >= 34 &&
           frame.ether_type() == eth_frame::ETHER_TYPE_IPV4 &&
           frame[OFF_IP_PROTO] == proto;
}

static bool is_echo_reply(const eth_frame& frame) {
    return is_ipv4(frame, eth_frame::IP_ICMP) && frame.size() >= 42 &&
           frame[OFF_ICMP] == 0;
}

static bool is_router_advertisement(const eth_frame& frame) {
    return frame.size() >= OFF_RA_OPTS &&
           frame.ether_type() == eth_frame::ETHER_TYPE_IPV6 &&
           frame[OFF_IP6_NEXT] == eth_frame::IP_ICMP6 &&
           frame[OFF_ICMP6] == 134;
}

class eth_backends_test : public test_base, public eth_host
{
public:
    ethernet::bridge bridge;
    eth_initiator_socket eth_tx;
    eth_target_socket eth_rx;

    const mac_addr guest_mac;
    const mac_addr broadcast;

    vector<eth_frame> received;

    struct scoped_backend {
        ethernet::bridge& br;
        size_t id;

        scoped_backend(ethernet::bridge& b, const string& desc):
            br(b), id(b.create_backend(desc)) {}
        ~scoped_backend() { EXPECT_TRUE(br.destroy_backend(id)); }
    };

    virtual void eth_receive(const eth_target_socket& socket,
                             const eth_frame& frame) override {
        received.push_back(frame);
    }

    eth_backends_test(const sc_module_name& nm):
        test_base(nm),
        eth_host(),
        bridge("bridge"),
        eth_tx("eth_tx"),
        eth_rx("eth_rx"),
        guest_mac(0x02, 0x00, 0x00, 0x12, 0x34, 0x56),
        broadcast(0xff, 0xff, 0xff, 0xff, 0xff, 0xff),
        received() {
        eth_tx.bind(bridge.eth_rx);
        bridge.eth_tx.bind(eth_rx);

        add_test("strings", &eth_backends_test::test_strings);
        add_test("unknown", &eth_backends_test::test_unknown);
        add_test("file", &eth_backends_test::test_file);
        add_test("file_default", &eth_backends_test::test_file_default);
        add_test("file_error", &eth_backends_test::test_file_error);
        add_test("slirp_arp", &eth_backends_test::test_slirp_arp);
        add_test("slirp_icmp", &eth_backends_test::test_slirp_icmp);
        add_test("slirp_timer", &eth_backends_test::test_slirp_timer);
        add_test("slirp_forward", &eth_backends_test::test_slirp_forward);
        add_test("slirp_shared", &eth_backends_test::test_slirp_shared);
        add_test("slirp_lifetime", &eth_backends_test::test_slirp_lifetime);
        add_test("slirp_options", &eth_backends_test::test_slirp_options);
        add_test("tap", &eth_backends_test::test_tap);
    }

    string execute(const string& cmd, const vector<string>& args,
                   bool success = true) {
        stringstream ss;
        EXPECT_EQ(bridge.execute(cmd, args, ss), success) << cmd;
        return ss.str();
    }

    bool have_backend(const string& type) {
        stringstream ss;
        bridge.execute("create_backend", { "nothing" }, ss);
        vector<string> lines = split(ss.str(), '\n');
        vector<string> known = split(lines.back());
        if (stl_contains(known, type))
            return true;

        log_warn("%s backend not available, test skipped", type.c_str());
        return false;
    }

    void send(const eth_frame& frame) {
        eth_frame tx = frame;
        eth_tx.send(tx);
    }

    void settle() { wait(clock_cycles(2)); }

    template <typename PRED>
    size_t count_frames(PRED pred) const {
        return std::count_if(received.begin(), received.end(), pred);
    }

    template <typename PRED>
    eth_frame find_frame(PRED pred) const {
        auto it = std::find_if(received.begin(), received.end(), pred);
        return it != received.end() ? *it : eth_frame();
    }

    template <typename PRED>
    bool wait_for_frames(PRED pred, size_t n = 1) {
        using namespace std::chrono;
        auto timeout = steady_clock::now() + seconds(10);
        while (count_frames(pred) < n) {
            if (steady_clock::now() > timeout) {
                ADD_FAILURE() << "timeout waiting for frames from backend";
                return false;
            }

            wait(1, SC_US);
            std::this_thread::yield();
        }

        return true;
    }

    eth_frame make_arp_request(u8 net, u8 host = IP_HOST) const {
        vector<u8> arp;
        put16(arp, 1);
        put16(arp, eth_frame::ETHER_TYPE_IPV4);
        arp.push_back(6);
        arp.push_back(4);
        put16(arp, 1);
        arp.insert(arp.end(), guest_mac.bytes.begin(), guest_mac.bytes.end());
        append(arp, ipv4(net, IP_GUEST));
        arp.insert(arp.end(), 6, 0);
        append(arp, ipv4(net, host));
        return eth_frame(broadcast, guest_mac, eth_frame::ETHER_TYPE_ARP, arp);
    }

    eth_frame make_echo_request(u8 net, u16 id, u16 seq,
                                const vector<u8>& data) const {
        vector<u8> icmp{ 8, 0, 0, 0 };
        put16(icmp, id);
        put16(icmp, seq);
        append(icmp, data);
        u16 sum = checksum(icmp.data(), icmp.size());
        icmp[2] = sum >> 8;
        icmp[3] = sum & 0xff;

        vector<u8> ip{ 0x45, 0 };
        put16(ip, (u16)(20 + icmp.size()));
        put16(ip, 0x1234);
        put16(ip, 0);
        ip.push_back(64);
        ip.push_back(eth_frame::IP_ICMP);
        put16(ip, 0);
        append(ip, ipv4(net, IP_GUEST));
        append(ip, ipv4(net, IP_HOST));
        sum = checksum(ip.data(), ip.size());
        ip[10] = sum >> 8;
        ip[11] = sum & 0xff;

        append(ip, icmp);
        return eth_frame(host_mac(net), guest_mac, eth_frame::ETHER_TYPE_IPV4,
                         ip);
    }

    bool resolve_host(u8 net, size_t replies = 1) {
        received.clear();
        send(make_arp_request(net));
        if (!wait_for_frames(is_arp_reply, replies))
            return false;
        settle();
        return true;
    }

    static string read_file(const string& path) {
        std::ifstream file(path);
        stringstream ss;
        ss << file.rdbuf();
        return ss.str();
    }

    static fs::path temp_path() {
        string name = mkstr("vcml_eth_backends_%d", mwr::getpid());
        fs::path path = fs::temp_directory_path() / name;
        fs::create_directories(path);
        return path;
    }

    static u16 unused_port() {
        mwr::server_socket server(1);
        server.listen(0, "127.0.0.1");
        return server.port();
    }

    static size_t count_open_files() {
        size_t n = 0;
#ifdef MWR_LINUX
        for (const auto& entry : fs::directory_iterator("/proc/self/fd")) {
            (void)entry;
            n++;
        }
#endif
        return n;
    }

    void test_strings() {
        EXPECT_STREQ(bridge.kind(), "vcml::ethernet::bridge");
        EXPECT_STREQ(bridge.version(), VCML_VERSION_STRING);
    }

    void test_unknown() {
        EXPECT_THROW(bridge.create_backend("nothing"), std::exception);
        EXPECT_THROW(bridge.create_backend("nothing:1,2"), std::exception);
        EXPECT_THROW(bridge.create_backend(""), std::exception);

        string msg = execute("create_backend", { "nothing:1" }, false);
        EXPECT_THAT(msg, StartsWith("error creating backend nothing:1:"));
        EXPECT_THAT(msg, HasSubstr("unknown network backend 'nothing'"));
        EXPECT_THAT(msg, HasSubstr("the following network backends are "
                                   "known:"));
        EXPECT_THAT(msg, HasSubstr("  file"));
        EXPECT_EQ(execute("list_backends", {}), "");

        EXPECT_TRUE(have_backend("file"));
    }

    void test_file() {
        fs::path dir = temp_path();
        string path = (dir / "frames.tx").generic_string();

        eth_frame frame0 = make_arp_request(1);
        eth_frame frame1 = make_echo_request(1, 1, 2, { 1, 2, 3 });
        eth_frame frame2(guest_mac, broadcast, { 1, 2, 3 });
        frame2.resize(20);

        received.clear();
        size_t id = bridge.create_backend("file:" + path);
        EXPECT_EQ(execute("list_backends", { to_string(id) }),
                  mkstr("%zu: file:%s,", id, path.c_str()));

        sc_time t0 = sc_time_stamp();
        send(frame0);
        wait(1, SC_SEC);
        sc_time t1 = sc_time_stamp();
        send(frame1);
        send(frame2);
        settle();

        EXPECT_TRUE(received.empty());
        EXPECT_TRUE(bridge.destroy_backend(id));
        send(frame0);

        stringstream ss;
        ss << "[" << t0 << "] packet #1 " << frame0 << std::endl
           << "[" << t1 << "] packet #2 " << frame1 << std::endl
           << "[" << t1 << "] packet #3 " << frame2 << std::endl;

        string contents = read_file(path);
        EXPECT_EQ(contents, ss.str());
        EXPECT_THAT(contents,
                    HasSubstr("packet #1 ETHERNET_ARP_REQUEST 60bytes from "
                              "02:00:00:12:34:56 to ff:ff:ff:ff:ff:ff\n"
                              "\tff ff ff ff ff ff 02 00 00 12 34 56 08 06"));
        EXPECT_THAT(contents, HasSubstr("packet #2 ETHERNET_ICMP 60bytes"));
        EXPECT_THAT(contents, HasSubstr("packet #3 ETHERNET_INVALID 20bytes"));

        id = bridge.create_backend("file:" + path + ",ignored");
        send(frame1);
        EXPECT_TRUE(bridge.destroy_backend(id));

        ss.str("");
        ss << "[" << sc_time_stamp() << "] packet #1 " << frame1 << std::endl;
        EXPECT_EQ(read_file(path), ss.str());

        fs::remove_all(dir);
    }

    void test_file_default() {
        fs::path dir = temp_path();
        fs::path cwd = fs::current_path();
        fs::current_path(dir);

        eth_frame frame = make_arp_request(1);
        size_t id = bridge.create_backend("file");
        EXPECT_EQ(execute("list_backends", { to_string(id) }),
                  mkstr("%zu: file:test.bridge.tx,", id));
        send(frame);
        EXPECT_TRUE(bridge.destroy_backend(id));

        stringstream ss;
        ss << "[" << sc_time_stamp() << "] packet #1 " << frame << std::endl;
        EXPECT_EQ(read_file((dir / "test.bridge.tx").generic_string()),
                  ss.str());

        fs::current_path(cwd);
        fs::remove_all(dir);
    }

    void test_file_error() {
        fs::path dir = temp_path();
        string path = (dir / "missing" / "frames.tx").generic_string();

        mock_publisher publisher(LOG_WARN, LOG_WARN);
        EXPECT_CALL(
            publisher,
            publish(match_log(LOG_WARN, "failed to open file '" + path + "'")))
            .Times(1);

        size_t id = bridge.create_backend("file:" + path);
        EXPECT_EQ(execute("list_backends", { to_string(id) }),
                  mkstr("%zu: file:%s,", id, path.c_str()));
        send(make_arp_request(1));
        EXPECT_TRUE(bridge.destroy_backend(id));
        EXPECT_FALSE(fs::exists(path));

        fs::remove_all(dir);
    }

    void test_slirp_arp() {
        if (!have_backend("slirp"))
            return;

        const u8 net = 1;
        scoped_backend backend(bridge, "slirp:1");

        received.clear();
        send(make_arp_request(net));
        EXPECT_TRUE(received.empty());
        if (!wait_for_frames(is_arp_reply))
            return;

        settle();
        ASSERT_EQ(received.size(), 1);
        eth_frame reply = received[0];
        EXPECT_EQ(reply.destination(), guest_mac);
        EXPECT_EQ(reply.source(), host_mac(net));
        EXPECT_EQ(mac_addr(reply, OFF_ARP_SHA), host_mac(net));
        EXPECT_EQ(bytes(reply, OFF_ARP_SPA, 4), ipv4(net, IP_HOST));
        EXPECT_EQ(mac_addr(reply, OFF_ARP_THA), guest_mac);
        EXPECT_EQ(bytes(reply, OFF_ARP_TPA, 4), ipv4(net, IP_GUEST));

        received.clear();
        send(make_arp_request(net, 3));
        if (!wait_for_frames(is_arp_reply))
            return;

        EXPECT_EQ(bytes(received[0], OFF_ARP_SPA, 4), ipv4(net, 3));

        received.clear();
        send(make_arp_request(net + 1));
        send(make_arp_request(net, 77));
        settle();
        EXPECT_TRUE(received.empty());
    }

    void test_slirp_icmp() {
        if (!have_backend("slirp"))
            return;

        const u8 net = 2;
        scoped_backend backend(bridge, "slirp:2");
        if (!resolve_host(net))
            return;

        vector<u8> data(33);
        for (size_t i = 0; i < data.size(); i++)
            data[i] = (u8)(0xa0 + i);

        received.clear();
        send(make_echo_request(net, 0xbeef, 7, data));
        if (!wait_for_frames(is_echo_reply))
            return;

        settle();
        ASSERT_EQ(received.size(), 1);
        eth_frame reply = received[0];
        EXPECT_EQ(reply.destination(), guest_mac);
        EXPECT_EQ(reply.source(), host_mac(net));
        EXPECT_EQ(bytes(reply, OFF_IP_SRC, 4), ipv4(net, IP_HOST));
        EXPECT_EQ(bytes(reply, OFF_IP_DST, 4), ipv4(net, IP_GUEST));
        EXPECT_EQ(checksum(reply.data() + OFF_IP, 20), 0);

        size_t len = reply.read<u16>(OFF_IP_LEN);
        ASSERT_EQ(len, 20 + 8 + data.size());
        ASSERT_GE(reply.size(), OFF_IP + len);
        EXPECT_EQ(checksum(reply.data() + OFF_ICMP, len - 20), 0);
        EXPECT_EQ(reply.read<u16>(OFF_ICMP + 4), 0xbeef);
        EXPECT_EQ(reply.read<u16>(OFF_ICMP + 6), 7);
        EXPECT_EQ(bytes(reply, OFF_ICMP + 8, data.size()), data);

        eth_frame corrupt = make_echo_request(net, 0xbeef, 8, data);
        corrupt[OFF_ICMP + 8] ^= 0xff;
        received.clear();
        send(corrupt);
        settle();
        EXPECT_TRUE(received.empty());
    }

    void test_slirp_timer() {
        if (!have_backend("slirp"))
            return;

        const u8 net = 3;
        const sc_time max_interval(601, SC_SEC);
        wait(2000, SC_SEC);

        scoped_backend backend(bridge, "slirp:3");
        received.clear();

        for (int i = 0; i < 2; i++) {
            sc_time start = sc_time_stamp();
            while (count_frames(is_router_advertisement) < 1 &&
                   sc_time_stamp() - start < max_interval) {
                wait(1, SC_SEC);
            }

            ASSERT_EQ(count_frames(is_router_advertisement), 1)
                << "no router advertisement after " << max_interval;

            eth_frame ra = find_frame(is_router_advertisement);
            EXPECT_EQ(ra.destination(), mac_addr(0x33, 0x33, 0, 0, 0, 1));

            vector<u8> prefix;
            size_t off = OFF_RA_OPTS;
            while (off + 2 <= ra.size() && ra[off + 1] > 0) {
                if (ra[off] == 3)
                    prefix = bytes(ra, off + 16, 16);
                off += ra[off + 1] * 8;
            }

            vector<u8> expected(16);
            expected[0] = 0xfe;
            expected[1] = 0xc0 + net;
            EXPECT_EQ(prefix, expected);
            received.clear();
        }
    }

    void test_slirp_forward() {
        if (!have_backend("slirp"))
            return;

        const u8 net = 4;
        const u16 guest_port0 = 8080;
        const u16 guest_port1 = 8081;
        u16 host_port0 = unused_port();
        u16 host_port1 = unused_port();
        u16 host_port2 = unused_port();

        mock_publisher publisher(LOG_WARN, LOG_WARN);
        EXPECT_CALL(publisher, publish(_)).Times(AnyNumber());
        EXPECT_CALL(publisher, publish(match_log(LOG_WARN,
                                                 "failed to setup slirp host "
                                                 "port forwarding")))
            .Times(0);

        string desc = mkstr(
            "slirp:%d,forward:*:%hu:%hu,"
            "forward-ipv4:10.0.%d.%d:%hu:%hu,"
            "forward-udp:*:%hu:%hu",
            (int)net, guest_port0, host_port0, (int)net, (int)IP_GUEST,
            guest_port1, host_port1, guest_port0, host_port2);
        scoped_backend backend(bridge, desc);
        Mock::VerifyAndClearExpectations(&publisher);
        if (!resolve_host(net))
            return;

        const u16 host_ports[] = { host_port0, host_port1 };
        const u16 guest_ports[] = { guest_port0, guest_port1 };
        for (size_t i = 0; i < 2; i++) {
            u16 port = guest_ports[i];
            auto is_syn = [port](const eth_frame& frame) -> bool {
                return is_ipv4(frame, eth_frame::IP_TCP) &&
                       frame.size() >= 54 &&
                       frame.read<u16>(OFF_TCP_DPORT) == port &&
                       (frame[OFF_TCP_FLAGS] & 0x3f) == 0x02;
            };

            received.clear();
            mwr::socket client;
            try {
                client.connect("127.0.0.1", host_ports[i]);
            } catch (std::exception& ex) {
                ADD_FAILURE() << "connection failed: " << ex.what();
                return;
            }

            if (!wait_for_frames(is_syn))
                return;

            eth_frame syn = find_frame(is_syn);
            EXPECT_EQ(syn.destination(), guest_mac);
            EXPECT_EQ(syn.source(), host_mac(net));
            EXPECT_EQ(bytes(syn, OFF_IP_SRC, 4), ipv4(net, IP_HOST));
            EXPECT_EQ(bytes(syn, OFF_IP_DST, 4), ipv4(net, IP_GUEST));
            EXPECT_EQ(checksum(syn.data() + OFF_IP, 20), 0);
        }

#ifdef MWR_LINUX
        EXPECT_CALL(publisher, publish(_)).Times(AnyNumber());
        EXPECT_CALL(publisher, publish(match_log(LOG_WARN,
                                                 "failed to setup slirp host "
                                                 "port forwarding")))
            .Times(1);
        scoped_backend duplicate(
            bridge, mkstr("slirp:%d,forward:*:%hu:%hu", (int)net, guest_port0,
                          host_port0));
        Mock::VerifyAndClearExpectations(&publisher);
#endif
    }

    void test_slirp_shared() {
        if (!have_backend("slirp"))
            return;

        {
            scoped_backend backend0(bridge, "slirp:5");
            scoped_backend backend1(bridge, "slirp:5");
            if (!resolve_host(5, 4))
                return;
            EXPECT_EQ(received.size(), 4);
        }

        {
            scoped_backend backend0(bridge, "slirp:6");
            scoped_backend backend1(bridge, "slirp:7");
            if (!resolve_host(6))
                return;
            ASSERT_EQ(received.size(), 1);
            EXPECT_EQ(bytes(received[0], OFF_ARP_SPA, 4), ipv4(6, IP_HOST));
            if (!resolve_host(7))
                return;
            ASSERT_EQ(received.size(), 1);
            EXPECT_EQ(bytes(received[0], OFF_ARP_SPA, 4), ipv4(7, IP_HOST));
        }
    }

    void test_slirp_lifetime() {
        if (!have_backend("slirp"))
            return;

        const u8 net = 8;
        size_t id0 = bridge.create_backend("slirp:8");
        size_t id1 = bridge.create_backend("slirp:0x8");
        EXPECT_TRUE(resolve_host(net, 4));
        EXPECT_EQ(received.size(), 4);

        EXPECT_TRUE(bridge.destroy_backend(id0));
        EXPECT_TRUE(resolve_host(net));
        EXPECT_EQ(received.size(), 1);

        EXPECT_TRUE(bridge.destroy_backend(id1));
        received.clear();
        send(make_arp_request(net));
        settle();
        EXPECT_TRUE(received.empty());

        bridge.create_backend("slirp:8");
        id1 = bridge.create_backend("slirp:8");
        EXPECT_TRUE(bridge.destroy_backend(id1));
        EXPECT_TRUE(resolve_host(net));
        EXPECT_EQ(received.size(), 1);

        bridge.create_backend("slirp:8");
        EXPECT_EQ(execute("destroy_backend", { "all" }), "");
        EXPECT_EQ(execute("list_backends", {}), "");
        received.clear();
        send(make_arp_request(net));
        settle();
        EXPECT_TRUE(received.empty());
    }

    void test_slirp_options() {
        if (!have_backend("slirp"))
            return;

        const u8 net = 9;

        {
            mock_publisher publisher(LOG_WARN, LOG_WARN);
            EXPECT_CALL(publisher, publish(_)).Times(AnyNumber());
            EXPECT_CALL(publisher, publish(match_log(LOG_WARN,
                                                     "unknown slirp option: "
                                                     "bogus")))
                .Times(1);
            EXPECT_CALL(publisher, publish(match_log(LOG_WARN,
                                                     "unknown slirp option: "
                                                     "x=y")))
                .Times(1);
            scoped_backend backend(bridge, "slirp:9,bogus,,x=y");
            EXPECT_TRUE(resolve_host(net));
            EXPECT_EQ(received.size(), 1);
        }

        {
            scoped_backend backend(bridge, "slirp");
            EXPECT_TRUE(resolve_host(0));
            EXPECT_EQ(received.size(), 1);
        }

        {
            scoped_backend backend(bridge, "slirp:nan");
            EXPECT_TRUE(resolve_host(0));
            EXPECT_EQ(received.size(), 1);
        }

        EXPECT_THAT(execute("create_backend", { "slirp:9,forward" }, false),
                    HasSubstr("invalid port forwarding: 'forward'"));
        EXPECT_THAT(
            execute("create_backend", { "slirp:9,forward:*:80" }, false),
            HasSubstr("invalid port forwarding: 'forward:*:80'"));
        EXPECT_THAT(
            execute("create_backend", { "slirp:9,forward:*:80:81:82" }, false),
            HasSubstr("invalid port forwarding"));
        EXPECT_THAT(
            execute("create_backend", { "slirp:9,forward-x:*:80:81" }, false),
            HasSubstr("invalid slirp protocol: forward-x"));
        EXPECT_THAT(execute("create_backend",
                            { "slirp:9,forward:1.2.3:80:81" }, false),
                    HasSubstr("cannot parse ipv4 address: 1.2.3"));
        EXPECT_THAT(execute("create_backend", { "slirp:256" }, false),
                    HasSubstr("cannot parse ipv4 address: 10.0.256.0"));
        EXPECT_THAT(execute("create_backend", { "slirp:256" }, false),
                    HasSubstr("cannot parse ipv4 address: 10.0.256.0"));
        EXPECT_EQ(execute("list_backends", {}), "");

        received.clear();
        send(make_arp_request(net));
        settle();
        EXPECT_TRUE(received.empty());

        scoped_backend backend(
            bridge, mkstr("slirp:9,forward-ipv6:*:8082:%hu", unused_port()));
        EXPECT_TRUE(resolve_host(net));
        EXPECT_EQ(received.size(), 1);
    }

    void test_tap() {
        const string desc = "tap:193";
        size_t nfiles = count_open_files();

        stringstream ss;
        if (!bridge.execute("create_backend", { desc }, ss)) {
            EXPECT_THAT(ss.str(),
                        StartsWith("error creating backend " + desc + ":"));
            EXPECT_THAT(ss.str(), AnyOf(HasSubstr("unknown network backend"),
                                        HasSubstr("error opening tundev"),
                                        HasSubstr("error creating tapdev")));
            EXPECT_EQ(execute("list_backends", {}), "");
            EXPECT_EQ(count_open_files(), nfiles);

            received.clear();
            send(make_arp_request(1));
            settle();
            EXPECT_TRUE(received.empty());

            log_warn("tap backend not available, test skipped");
            return;
        }

        EXPECT_THAT(execute("list_backends", {}), EndsWith(": " + desc + ","));
        send(make_arp_request(1));
        settle();
        EXPECT_EQ(execute("destroy_backend", { "all" }), "");
        EXPECT_EQ(execute("list_backends", {}), "");
        EXPECT_EQ(count_open_files(), nfiles);
    }
};

TEST(ethernet, backends) {
    eth_backends_test test("test");
    sc_core::sc_start();
}
