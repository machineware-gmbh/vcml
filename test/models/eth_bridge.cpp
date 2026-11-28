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

class test_backend : public ethernet::backend
{
public:
    static vector<test_backend*> all;

    vector<eth_frame> frames;

    test_backend(ethernet::bridge* br): ethernet::backend(br), frames() {
        m_type = "test";
        all.push_back(this);
    }

    virtual ~test_backend() { stl_remove(all, this); }

    virtual void send_to_host(const eth_frame& frame) override {
        frames.push_back(frame);
    }

    static ethernet::backend* create(ethernet::bridge* br,
                                     const vector<string>& args) {
        if (!args.empty() && args[0] == "fail")
            VCML_REPORT("failure requested");
        return new test_backend(br);
    }
};

vector<test_backend*> test_backend::all;

class eth_bridge_test : public test_base, public eth_host
{
public:
    ethernet::bridge bridge;
    eth_initiator_socket eth_tx;
    eth_target_socket eth_rx;

    vector<eth_frame> received;

    virtual void eth_receive(const eth_target_socket& socket,
                             const eth_frame& frame) override {
        received.push_back(frame);
    }

    eth_bridge_test(const sc_module_name& nm):
        test_base(nm),
        eth_host(),
        bridge("bridge"),
        eth_tx("eth_tx"),
        eth_rx("eth_rx"),
        received() {
        eth_tx.bind(bridge.eth_rx);
        bridge.eth_tx.bind(eth_rx);

        add_test("strings", &eth_bridge_test::test_strings);
        add_test("lookup", &eth_bridge_test::test_lookup);
        add_test("property", &eth_bridge_test::test_property);
        add_test("commands", &eth_bridge_test::test_commands);
        add_test("to_host", &eth_bridge_test::test_to_host);
        add_test("to_guest", &eth_bridge_test::test_to_guest);
    }

    static eth_frame make_frame(u8 id, const vector<u8>& data) {
        mac_addr dest(0x12, 0x34, 0x56, 0x78, 0x9a, id);
        mac_addr src(0xaa, 0xbb, 0xcc, 0xdd, 0xee, id);
        return eth_frame(dest, src, data);
    }

    string execute(const string& cmd, const vector<string>& args,
                   bool success = true) {
        stringstream ss;
        EXPECT_EQ(bridge.execute(cmd, args, ss), success) << cmd;
        return ss.str();
    }

    void test_strings() {
        EXPECT_STREQ(bridge.kind(), "vcml::ethernet::bridge");
        EXPECT_STREQ(bridge.version(), VCML_VERSION_STRING);
    }

    void test_lookup() {
        EXPECT_EQ(ethernet::bridge::find("test.bridge"), &bridge);
        EXPECT_EQ(ethernet::bridge::find("test.nothing"), nullptr);
        EXPECT_THAT(ethernet::bridge::all(), ElementsAre(&bridge));
    }

    void test_property() {
        EXPECT_EQ(bridge.backends.get(), "test nothing test:fail");
        ASSERT_EQ(test_backend::all.size(), 1);
        EXPECT_EQ(test_backend::all[0]->parent(), &bridge);
        EXPECT_EQ(execute("list_backends", {}), "0: test,");
    }

    void test_commands() {
        EXPECT_EQ(execute("create_backend", { "test" }), "created backend 1");
        EXPECT_EQ(execute("create_backend", { "test:a,b" }),
                  "created backend 2");
        EXPECT_EQ(test_backend::all.size(), 3);

        EXPECT_THAT(execute("create_backend", { "nothing" }, false),
                    StartsWith("error creating backend nothing:"));
        EXPECT_THAT(execute("create_backend", { "test:fail" }, false),
                    HasSubstr("failure requested"));
        EXPECT_EQ(test_backend::all.size(), 3);

        EXPECT_EQ(execute("list_backends", { "2", "7", "0" }),
                  "2: test,7: none,0: test,");
        string list = execute("list_backends", {});
        EXPECT_THAT(list, HasSubstr("0: test,"));
        EXPECT_THAT(list, HasSubstr("1: test,"));
        EXPECT_THAT(list, HasSubstr("2: test,"));

        EXPECT_EQ(execute("destroy_backend", { "1" }), "");
        EXPECT_EQ(test_backend::all.size(), 2);
        EXPECT_EQ(execute("destroy_backend", { "1" }),
                  "invalid backend id: 1");
        EXPECT_EQ(execute("list_backends", { "1" }), "1: none,");

        EXPECT_EQ(execute("destroy_backend", { "ALL" }), "");
        EXPECT_TRUE(test_backend::all.empty());
        EXPECT_EQ(execute("list_backends", {}), "");
        EXPECT_FALSE(bridge.destroy_backend(0));

        stringstream ss;
        EXPECT_FALSE(bridge.execute("create_backend", ss));
        EXPECT_FALSE(bridge.execute("destroy_backend", ss));
    }

    void test_to_host() {
        eth_frame frame0 = make_frame(0x12, { 1, 2, 3 });
        eth_frame frame1 = make_frame(0x45, { 4 });

        eth_frame tx = frame0;
        eth_tx.send(tx);

        size_t id0 = bridge.create_backend("test");
        size_t id1 = bridge.create_backend("test");
        ASSERT_EQ(test_backend::all.size(), 2);
        test_backend* backend0 = test_backend::all[0];
        test_backend* backend1 = test_backend::all[1];

        tx = frame0;
        eth_tx.send(tx);
        tx = frame1;
        eth_tx.send(tx);
        EXPECT_THAT(backend0->frames, ElementsAre(frame0, frame1));
        EXPECT_THAT(backend1->frames, ElementsAre(frame0, frame1));

        EXPECT_TRUE(bridge.destroy_backend(id0));
        tx = frame1;
        eth_tx.send(tx);
        EXPECT_THAT(backend1->frames, ElementsAre(frame0, frame1, frame1));

        EXPECT_TRUE(bridge.destroy_backend(id1));
        EXPECT_TRUE(received.empty());
    }

    void test_to_guest() {
        eth_frame frame0 = make_frame(0x32, { 9, 8, 7, 6 });
        eth_frame frame1 = make_frame(0x65, {});

        size_t id = bridge.create_backend("test");
        ASSERT_EQ(test_backend::all.size(), 1);
        test_backend* backend = test_backend::all[0];

        received.clear();
        backend->send_to_guest(frame0);
        backend->send_to_guest(frame1);
        EXPECT_TRUE(received.empty());
        wait(clock_cycles(2));
        EXPECT_THAT(received, ElementsAre(frame0, frame1));
        EXPECT_TRUE(backend->frames.empty());

        received.clear();
        thread t([&] { backend->send_to_guest(frame1); });
        t.join();
        wait(clock_cycles(2));
        EXPECT_THAT(received, ElementsAre(frame1));

        EXPECT_TRUE(bridge.destroy_backend(id));
    }
};

TEST(ethernet, bridge) {
    ethernet::backend::define("test", test_backend::create);
    broker broker("test");
    broker.define("test.bridge.backends", "test nothing test:fail");

    eth_bridge_test test("test");
    sc_core::sc_start();
}
