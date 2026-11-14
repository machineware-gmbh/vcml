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

using vcml::debugging::vspsubscriber;
using vcml::debugging::vsppublisher;
using vcml::debugging::VSP_EVENT_TRACE;

MATCHER_P(named, name, "matches sender name") {
    return string(arg.name()) == name;
}

static auto fw() {
    return HasSubstr("\"dir\":\"fw\"");
}

static auto bw() {
    return HasSubstr("\"dir\":\"bw\"");
}

class mock_subscriber : public vspsubscriber
{
public:
    MOCK_METHOD(void, on_event,
                (const sc_object&, const string&, const sc_time&,
                 const string&),
                (override));
};

class count_subscriber : public vspsubscriber
{
public:
    atomic<size_t> count;
    count_subscriber(): count(0) {}
    virtual void on_event(const sc_object& sender, const string& event,
                          const sc_time& t, const string& payload) override {
        count++;
    }
};

class test_peripheral : public peripheral
{
public:
    reg<u32> test_reg;
    tlm_target_socket in;

    test_peripheral(const sc_module_name& nm):
        peripheral(nm), test_reg("test_reg", 0x0, 0), in("in") {
        test_reg.allow_read_write();
    }
};

class test_harness : public test_base
{
public:
    mock_subscriber mock;

    tlm_initiator_socket out;
    test_peripheral periph;

    gpio_initiator_array<> gpio_out;
    gpio_target_array<> gpio_in;

    u8 vqmem[0x300];
    split_virtqueue vq;

    static virtio_queue_desc vq_desc() {
        virtio_queue_desc desc(0, 4);
        desc.desc = 0x1000;
        desc.driver = 0x1100;
        desc.device = 0x1200;
        return desc;
    }

    test_harness(const sc_module_name& nm):
        test_base(nm),
        mock(),
        out("out"),
        periph("periph"),
        gpio_out("gpio_out"),
        gpio_in("gpio_in"),
        vqmem(),
        vq(
            vq_desc(),
            [&](u64 addr, u64 len, vcml_access acs) -> u8* {
                addr -= 0x1000;
                return addr + len <= sizeof(vqmem) ? vqmem + addr : nullptr;
            },
            out) {
        out.bind(periph.in);
        clk.bind(periph.clk);
        rst.bind(periph.rst);

        gpio_out[0].bind(gpio_in[0]);
        gpio_out[1].bind(gpio_in[1]);

        add_test("socket", &test_harness::test_socket);
        add_test("errors", &test_harness::test_errors);
        add_test("duplicate", &test_harness::test_duplicate);
        add_test("publishers", &test_harness::test_publishers);
        add_test("replay", &test_harness::test_replay);
        add_test("socket_array", &test_harness::test_socket_array);
        add_test("virtqueue", &test_harness::test_virtqueue);
        add_test("register", &test_harness::test_register);
        add_test("concurrent", &test_harness::test_concurrent);
    }

    void expect_quiet() {
        // ignore activities on everything else, e.g. clock and reset
        EXPECT_CALL(mock, on_event(_, _, _, _)).Times(AnyNumber());
    }

    void test_socket() {
        EXPECT_FALSE(out.has_subscribers());
        EXPECT_FALSE(out.trace_all);

        expect_quiet();
        out.subscribe_event(VSP_EVENT_TRACE, &mock);
        EXPECT_TRUE(out.is_subscribed(VSP_EVENT_TRACE, &mock));
        EXPECT_CALL(mock, on_event(named("harness.out"),
                                   StrEq(VSP_EVENT_TRACE), _, fw()))
            .Times(1);
        EXPECT_CALL(mock, on_event(named("harness.out"),
                                   StrEq(VSP_EVENT_TRACE), _, bw()))
            .Times(1);
        EXPECT_OK(out.writew(0x0, 0x11u));

        out.unsubscribe_event(VSP_EVENT_TRACE, &mock);
        EXPECT_FALSE(out.has_subscribers());
        EXPECT_CALL(
            mock, on_event(named("harness.out"), StrEq(VSP_EVENT_TRACE), _, _))
            .Times(0);
        EXPECT_OK(out.writew(0x0, 0x22u));
        Mock::VerifyAndClearExpectations(&mock);
    }

    void test_replay() {
        // new subscribers get the current state of stateful sockets, but
        // nothing happened afterwards, so only exactly one event each
        gpio_out[0].write(true);

        EXPECT_CALL(mock, on_event(named("harness.gpio_out[0]"),
                                   StrEq(VSP_EVENT_TRACE), _,
                                   AllOf(fw(), HasSubstr("\"state\":true"))))
            .Times(1);
        EXPECT_CALL(mock, on_event(named("harness.clk"),
                                   StrEq(VSP_EVENT_TRACE), _, fw()))
            .Times(1);
        EXPECT_CALL(
            mock, on_event(named("harness.gpio_in[0]"), StrEq(VSP_EVENT_TRACE),
                           _, AllOf(fw(), HasSubstr("\"state\":true"))))
            .Times(1);

        gpio_out[0].subscribe_event(VSP_EVENT_TRACE, &mock);
        gpio_in[0].subscribe_event(VSP_EVENT_TRACE, &mock);
        clk.subscribe_event(VSP_EVENT_TRACE, &mock);
        Mock::VerifyAndClearExpectations(&mock);

        // resubscribing does not replay again
        EXPECT_CALL(mock, on_event(_, _, _, _)).Times(0);
        gpio_out[0].subscribe_event(VSP_EVENT_TRACE, &mock);
        Mock::VerifyAndClearExpectations(&mock);

        gpio_out[0].unsubscribe_event(VSP_EVENT_TRACE, &mock);
        gpio_in[0].unsubscribe_event(VSP_EVENT_TRACE, &mock);
        clk.unsubscribe_event(VSP_EVENT_TRACE, &mock);
        gpio_out[0].write(false);
    }

    void test_errors() {
        EXPECT_FALSE(out.trace_errors);

        expect_quiet();
        out.subscribe_event(VSP_EVENT_TRACE, &mock);
        EXPECT_CALL(mock,
                    on_event(named("harness.out"), StrEq(VSP_EVENT_TRACE), _,
                             AllOf(bw(), HasSubstr("\"error\":true"))))
            .Times(1);
        EXPECT_CALL(mock, on_event(named("harness.out"),
                                   StrEq(VSP_EVENT_TRACE), _, fw()))
            .Times(1);
        EXPECT_AE(out.writew(0x100, 0x33u));

        out.unsubscribe_event(VSP_EVENT_TRACE, &mock);
        Mock::VerifyAndClearExpectations(&mock);
    }

    void test_duplicate() {
        expect_quiet();
        out.subscribe_event(VSP_EVENT_TRACE, &mock);
        out.subscribe_event(VSP_EVENT_TRACE, &mock);
        EXPECT_EQ(out.subscriptions().size(), 1);
        EXPECT_CALL(mock, on_event(named("harness.out"),
                                   StrEq(VSP_EVENT_TRACE), _, fw()))
            .Times(1);
        EXPECT_CALL(mock, on_event(named("harness.out"),
                                   StrEq(VSP_EVENT_TRACE), _, bw()))
            .Times(1);
        EXPECT_OK(out.writew(0x0, 0x44u));

        out.unsubscribe_event(VSP_EVENT_TRACE, &mock);
        Mock::VerifyAndClearExpectations(&mock);
    }

    static bool publishes_trace(sc_object& obj) {
        auto* pub = dynamic_cast<vsppublisher*>(&obj);
        return pub && pub->publishes(VSP_EVENT_TRACE);
    }

    void test_publishers() {
        // sockets and registers publish trace events, modules and socket
        // arrays do not, clients subscribe to their sockets individually
        EXPECT_TRUE(publishes_trace(out));
        EXPECT_TRUE(publishes_trace(periph.in));
        EXPECT_TRUE(publishes_trace(periph.test_reg));
        EXPECT_TRUE(publishes_trace(gpio_out[0]));
        EXPECT_FALSE(publishes_trace(periph));
        EXPECT_FALSE(publishes_trace(gpio_out));
        EXPECT_FALSE(out.publishes("nothing"));
        EXPECT_FALSE(out.subscribe_event("nothing", &mock));
        EXPECT_FALSE(out.has_subscribers());

        periph.in.subscribe_event(VSP_EVENT_TRACE, &mock);
        periph.test_reg.subscribe_event(VSP_EVENT_TRACE, &mock);

        expect_quiet();
        EXPECT_CALL(
            mock, on_event(named("harness.out"), StrEq(VSP_EVENT_TRACE), _, _))
            .Times(0);
        EXPECT_CALL(mock, on_event(named("harness.periph.in"),
                                   StrEq(VSP_EVENT_TRACE), _, fw()))
            .Times(1);
        EXPECT_CALL(mock, on_event(named("harness.periph.in"),
                                   StrEq(VSP_EVENT_TRACE), _, bw()))
            .Times(1);
        EXPECT_CALL(mock, on_event(named("harness.periph.test_reg"),
                                   StrEq(VSP_EVENT_TRACE), _, fw()))
            .Times(1);
        EXPECT_CALL(mock, on_event(named("harness.periph.test_reg"),
                                   StrEq(VSP_EVENT_TRACE), _, bw()))
            .Times(1);
        EXPECT_OK(out.writew(0x0, 0x55u));
        Mock::VerifyAndClearExpectations(&mock);

        periph.in.unsubscribe_event(VSP_EVENT_TRACE, &mock);
        periph.test_reg.unsubscribe_event(VSP_EVENT_TRACE, &mock);

        EXPECT_CALL(mock, on_event(_, _, _, _)).Times(0);
        EXPECT_OK(out.writew(0x0, 0x66u));
        Mock::VerifyAndClearExpectations(&mock);
    }

    void test_socket_array() {
        gpio_out[0].subscribe_event(VSP_EVENT_TRACE, &mock);
        EXPECT_TRUE(gpio_out[0].is_subscribed(VSP_EVENT_TRACE, &mock));
        EXPECT_FALSE(gpio_out[1].has_subscribers());
        EXPECT_FALSE(gpio_in[0].has_subscribers());

        expect_quiet();
        EXPECT_CALL(mock, on_event(named("harness.gpio_in[0]"),
                                   StrEq(VSP_EVENT_TRACE), _, _))
            .Times(0);
        EXPECT_CALL(mock, on_event(named("harness.gpio_out[1]"),
                                   StrEq(VSP_EVENT_TRACE), _, _))
            .Times(0);
        EXPECT_CALL(mock, on_event(named("harness.gpio_out[0]"),
                                   StrEq(VSP_EVENT_TRACE), _, _))
            .Times(AtLeast(1));
        gpio_out[0] = true;
        gpio_out[1] = true;
        Mock::VerifyAndClearExpectations(&mock);

        gpio_out[0].unsubscribe_event(VSP_EVENT_TRACE, &mock);
        EXPECT_FALSE(gpio_out[0].has_subscribers());

        EXPECT_CALL(mock, on_event(_, _, _, _)).Times(0);
        gpio_out[0] = false;
        gpio_out[1] = false;
        Mock::VerifyAndClearExpectations(&mock);
    }

    void test_register() {
        EXPECT_FALSE(periph.trace_all);

        // tracing a single register does not trace its peripheral socket
        expect_quiet();
        periph.test_reg.subscribe_event(VSP_EVENT_TRACE, &mock);
        EXPECT_CALL(mock, on_event(named("harness.periph.in"),
                                   StrEq(VSP_EVENT_TRACE), _, _))
            .Times(0);
        EXPECT_CALL(mock, on_event(named("harness.periph.test_reg"),
                                   StrEq(VSP_EVENT_TRACE), _, fw()))
            .Times(1);
        EXPECT_CALL(mock, on_event(named("harness.periph.test_reg"),
                                   StrEq(VSP_EVENT_TRACE), _, bw()))
            .Times(1);
        EXPECT_OK(out.writew(0x0, 0x77u));
        Mock::VerifyAndClearExpectations(&mock);

        periph.test_reg.unsubscribe_event(VSP_EVENT_TRACE, &mock);
        EXPECT_CALL(mock, on_event(_, _, _, _)).Times(0);
        EXPECT_OK(out.writew(0x0, 0x88u));
        Mock::VerifyAndClearExpectations(&mock);
    }

    void test_concurrent() {
        // attach, detach and delete tracers from another thread while the
        // simulation keeps tracing on the same socket
        atomic<bool> stop(false);
        atomic<size_t> rounds(0);
        thread other([&]() {
            while (!stop) {
                auto* tr = new count_subscriber();
                out.subscribe_event(VSP_EVENT_TRACE, tr);
                out.unsubscribe_event(VSP_EVENT_TRACE, tr);
                delete tr;
                rounds++;
            }
        });

        count_subscriber steady;
        out.subscribe_event(VSP_EVENT_TRACE, &steady);
        for (int i = 0; i < 10000; i++)
            EXPECT_OK(out.writew(0x0, (u32)i));
        while (rounds < 1000)
            std::this_thread::yield();

        stop = true;
        other.join();
        out.unsubscribe_event(VSP_EVENT_TRACE, &steady);

        EXPECT_EQ(steady.count, 20000); // fw and bw for every write
        EXPECT_FALSE(out.has_subscribers());
    }

    void test_virtqueue() {
        vq_message msg;
        msg.index = 0;

        // virtqueue reports to the tracers of its traceable
        out.subscribe_event(VSP_EVENT_TRACE, &mock);
        EXPECT_CALL(mock, on_event(named("harness.vq0"),
                                   StrEq(VSP_EVENT_TRACE), _, bw()))
            .Times(1);
        EXPECT_TRUE(vq.put(msg));
        Mock::VerifyAndClearExpectations(&mock);

        out.unsubscribe_event(VSP_EVENT_TRACE, &mock);
        EXPECT_CALL(mock, on_event(_, _, _, _)).Times(0);
        EXPECT_TRUE(vq.put(msg));
        Mock::VerifyAndClearExpectations(&mock);
    }
};

TEST(vspevents, trace) {
    test_harness test("harness");
    sc_core::sc_start();
}

using vcml::debugging::symbol;
using vcml::debugging::stackframe;
using vcml::debugging::VSP_EVENT_CALLSTACK;

class test_callstack_cpu : public sc_object, public debugging::target
{
public:
    vector<symbol> syms;
    vector<stackframe> frames;

    test_callstack_cpu(const char* nm):
        sc_object(nm),
        debugging::target(static_cast<sc_object&>(*this)),
        syms(),
        frames() {
        for (const char* name : { "main", "func", "printf", "puts" })
            syms.emplace_back(name, debugging::SYMKIND_FUNCTION, ENDIAN_LITTLE,
                              4, 0, 0);
    }

    // innermost frame first, just like a real stacktrace
    void set_callstack(const vector<size_t>& idx) {
        frames.clear();
        for (auto it = idx.rbegin(); it != idx.rend(); it++)
            frames.push_back({ 0, 0, &syms[*it] });
    }

    // pushes a frame without a symbol on top of the current callstack
    void push_unknown(u64 pc) { frames.insert(frames.begin(), { pc, 0, 0 }); }

    void sample() { notify_callstack(frames, sc_time_stamp()); }

    virtual void stacktrace(vector<stackframe>& trace, size_t limit) override {
        trace = frames;
    }
};

TEST(vspevents, callstack) {
    test_callstack_cpu cpu("cpu");
    mock_subscriber mock;
    mock_subscriber mock2;

    EXPECT_TRUE(cpu.publishes(VSP_EVENT_CALLSTACK));
    EXPECT_FALSE(cpu.has_subscribers());

    // no subscribers, nothing to publish
    cpu.set_callstack({ 0, 1 });
    cpu.sample();

    // subscribing sends the current callstack once
    cpu.set_callstack({ 0, 1, 2 });
    EXPECT_CALL(
        mock, on_event(named("cpu"), StrEq(VSP_EVENT_CALLSTACK), _,
                       StrEq(R"({"pop":0,"push":["main","func","printf"]})")))
        .Times(1);
    EXPECT_TRUE(cpu.subscribe_event(VSP_EVENT_CALLSTACK, &mock));
    EXPECT_TRUE(cpu.has_subscribers());
    Mock::VerifyAndClearExpectations(&mock);

    // every sample sends an update relative to the previous one
    cpu.set_callstack({ 0, 1, 3 });
    EXPECT_CALL(mock, on_event(named("cpu"), StrEq(VSP_EVENT_CALLSTACK), _,
                               StrEq(R"({"pop":1,"push":["puts"]})")))
        .Times(1);
    cpu.sample();
    Mock::VerifyAndClearExpectations(&mock);

    EXPECT_CALL(mock, on_event(named("cpu"), StrEq(VSP_EVENT_CALLSTACK), _,
                               StrEq(R"({"pop":0,"push":[]})")))
        .Times(1);
    cpu.sample();
    Mock::VerifyAndClearExpectations(&mock);

    // frames without a symbol are reported by their address
    cpu.push_unknown(0x8000f2a4);
    EXPECT_CALL(mock, on_event(named("cpu"), StrEq(VSP_EVENT_CALLSTACK), _,
                               StrEq(R"({"pop":0,"push":["0x8000f2a4"]})")))
        .Times(1);
    cpu.sample();
    Mock::VerifyAndClearExpectations(&mock);

    // late subscriber gets the full callstack, existing ones get synced up
    cpu.set_callstack({ 0 });
    EXPECT_CALL(mock, on_event(named("cpu"), StrEq(VSP_EVENT_CALLSTACK), _,
                               StrEq(R"({"pop":3,"push":[]})")))
        .Times(1);
    EXPECT_CALL(mock2, on_event(named("cpu"), StrEq(VSP_EVENT_CALLSTACK), _,
                                StrEq(R"({"pop":0,"push":["main"]})")))
        .Times(1);
    EXPECT_TRUE(cpu.subscribe_event(VSP_EVENT_CALLSTACK, &mock2));
    Mock::VerifyAndClearExpectations(&mock);
    Mock::VerifyAndClearExpectations(&mock2);

    cpu.set_callstack({ 0, 2 });
    EXPECT_CALL(mock, on_event(named("cpu"), StrEq(VSP_EVENT_CALLSTACK), _,
                               StrEq(R"({"pop":0,"push":["printf"]})")))
        .Times(1);
    EXPECT_CALL(mock2, on_event(named("cpu"), StrEq(VSP_EVENT_CALLSTACK), _,
                                StrEq(R"({"pop":0,"push":["printf"]})")))
        .Times(1);
    cpu.sample();
    Mock::VerifyAndClearExpectations(&mock);
    Mock::VerifyAndClearExpectations(&mock2);

    // no more events after unsubscribing
    cpu.unsubscribe_event(VSP_EVENT_CALLSTACK, &mock);
    cpu.unsubscribe_event(VSP_EVENT_CALLSTACK, &mock2);
    EXPECT_FALSE(cpu.has_subscribers());
    EXPECT_CALL(mock, on_event(_, _, _, _)).Times(0);
    EXPECT_CALL(mock2, on_event(_, _, _, _)).Times(0);
    cpu.sample();
}
