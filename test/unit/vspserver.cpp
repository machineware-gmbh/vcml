/******************************************************************************
 *                                                                            *
 * Copyright (C) 2026 MachineWare GmbH                                        *
 * All Rights Reserved                                                        *
 *                                                                            *
 * This work is licensed under the terms described in the LICENSE file found  *
 * in the root directory of this source tree.                                 *
 *                                                                            *
 ******************************************************************************/

#include "vsptest.h"

using vcml::debugging::vspclient;
using vcml::debugging::VSP_EVENT_LED;
using vcml::debugging::VSP_EVENT_TRACE;

constexpr size_t BURST = 40000;

class traffic : public component
{
public:
    tlm_initiator_socket out;
    tlm_target_socket in;
    gpio_initiator_socket led;
    serial_initiator_socket uart;

    atomic<bool> done;

    traffic(const sc_module_name& nm):
        component(nm),
        out("out"),
        in("in"),
        led("led"),
        uart("uart"),
        done(false) {
        out.bind(in);
        clk.stub(100 * MHz);
        rst.stub();
        SC_HAS_PROCESS(traffic);
        SC_THREAD(run);
    }

    virtual unsigned int transport(tlm_generic_payload& tx,
                                   const tlm_sbi& info,
                                   address_space as) override {
        tx.set_response_status(TLM_OK_RESPONSE);
        return tx.get_data_length();
    }

    void run() {
        wait(10, SC_NS);
        EXPECT_OK(out.writew(0x10, 0x11u));
        led = true;
        for (u8 c : { 'h', 'i', '"', '\\', '\n', '\xff' })
            uart.send(c);

        wait(1, SC_US);
        for (size_t i = 0; i < BURST; i++)
            EXPECT_OK(out.writew(0x20, (u32)i));

        while (!done) // keep tracers alive until the client has checked
            wait(1, SC_US);
        sc_stop();
    }
};

static bool is_error(const string& resp) {
    return starts_with(resp, "E,");
}

// returns the unescaped events field of a status response, or "" if none
static string events_of(const string& status) {
    vector<string> fields = split(status, ',');
    EXPECT_GE(fields.size(), 4) << status;
    EXPECT_LE(fields.size(), 5) << status;
    return fields.size() == 5 ? fields[4] : "";
}

// returns s from the first occurrence of start up to the next occurrence of
// stop, e.g. the opening tag of an xml element
static string section(const string& s, const string& start,
                      const string& stop) {
    size_t pos = s.find(start);
    if (pos == string::npos)
        return "";
    size_t end = s.find(stop, pos + start.length());
    return s.substr(pos, end == string::npos ? end : end - pos);
}

static string xml_tag(const string& xml, const string& name) {
    return section(xml, "<object name=\"" + name + "\"", ">");
}

static string json_obj(const string& json, const string& name) {
    return section(json, "{\"name\":\"" + name + "\"", "\"attributes\"");
}

static size_t count(const string& s, const string& what) {
    size_t n = 0;
    for (size_t pos = s.find(what); pos != string::npos;
         pos = s.find(what, pos + what.size()))
        n++;
    return n;
}

// resumes for the given duration and returns the events of the final status
static string run_for(vspserver& session, test_vsp_client& vsp,
                      const string& duration) {
    // wait without polling status, since that would fetch the events
    EXPECT_EQ(vsp.command("resume," + duration), "OK");
    EXPECT_TRUE(wait_for([&]() { return !session.is_running(); }));

    string status = vsp.command("status");
    EXPECT_TRUE(starts_with(status, "OK,stopped:step,")) << status;
    return events_of(status);
}

static void run_client(vspserver& session, traffic& tr, gpio::leds& leds,
                       serial::terminal& term, test_vsp_client& vsp,
                       bool& resumed) {
    EXPECT_TRUE(is_error(vsp.command("sub")));
    EXPECT_TRUE(is_error(vsp.command("sub,trace")));
    EXPECT_TRUE(is_error(vsp.command("sub,trace,tr.nothing")));
    EXPECT_TRUE(is_error(vsp.command("sub,nothing,tr.out")));
    EXPECT_TRUE(is_error(vsp.command("sub,led,tr.out")));
    EXPECT_TRUE(is_error(vsp.command("unsub,trace,tr.nothing")));

    // subscriptions are not recursive: modules publish nothing themselves
    EXPECT_TRUE(is_error(vsp.command("sub,trace,tr")));
    EXPECT_TRUE(is_error(vsp.command("sub,uart,term,tr")));
    EXPECT_FALSE(term.has_subscribers()) << "partial subscription";

    // list reports the events published by each object itself
    string xml = vsp.command("list,xml");
    EXPECT_TRUE(ends_with(xml_tag(xml, "leds"), " events=\"led\"")) << xml;
    EXPECT_TRUE(ends_with(xml_tag(xml, "out"), " events=\"trace\"")) << xml;
    EXPECT_TRUE(ends_with(xml_tag(xml, "term"), " events=\"uart\"")) << xml;
    EXPECT_FALSE(contains(xml_tag(xml, "tr"), "events=")) << xml;
    EXPECT_FALSE(xml_tag(xml, "tr").empty()) << xml;

    string json = vsp.command("list,json");
    EXPECT_TRUE(contains(json_obj(json, "leds"), "\"events\":[\"led\"]"));
    EXPECT_TRUE(contains(json_obj(json, "out"), "\"events\":[\"trace\"]"));
    EXPECT_FALSE(contains(json_obj(json, "tr"), "\"events\"")) << json;
    EXPECT_FALSE(json_obj(json, "tr").empty()) << json;

    // no events are reported without subscriptions
    string status = vsp.command("status");
    EXPECT_EQ(split(status, ',').size(), 4) << status;

    // duplicate subscriptions are no-ops, unsubscribing something that is
    // not subscribed is fine, unsubscribing removes exactly what is named
    EXPECT_EQ(vsp.command("sub,trace,tr.out,tr.in"), "OK");
    EXPECT_EQ(vsp.command("sub,trace,tr.out"), "OK");
    EXPECT_EQ(tr.out.subscriptions().size(), 1);
    EXPECT_EQ(vsp.command("unsub,trace,tr.in"), "OK");
    EXPECT_EQ(vsp.command("unsub,trace,tr.in"), "OK");
    EXPECT_EQ(vsp.command("unsub,trace,tr"), "OK");
    EXPECT_EQ(vsp.command("sub,led,leds"), "OK");
    EXPECT_EQ(vsp.command("sub,uart,term"), "OK");

    auto client = session.find_client(0);
    EXPECT_TRUE(tr.out.is_subscribed(VSP_EVENT_TRACE, client.get()));
    EXPECT_FALSE(tr.in.has_subscribers());
    EXPECT_TRUE(leds.is_subscribed(VSP_EVENT_LED, client.get()));

    status = vsp.command("status");
    EXPECT_EQ(events_of(status), "{\"events\":[]}") << status;

    // a second client gets its own subscriptions
    {
        test_vsp_client vsp2(session.port()); // client 1
        EXPECT_EQ(vsp2.command("sub,trace,tr.in"), "OK");

        auto client2 = session.find_client(1);
        EXPECT_TRUE(tr.in.is_subscribed(VSP_EVENT_TRACE, client2.get()));
        EXPECT_FALSE(tr.out.is_subscribed(VSP_EVENT_TRACE, client2.get()));
        EXPECT_FALSE(tr.in.is_subscribed(VSP_EVENT_TRACE, client.get()));
    }

    ASSERT_TRUE(wait_for([&]() { return !has_client(session, 1); }));
    EXPECT_FALSE(tr.in.has_subscribers());

    resumed = true;
    string events = run_for(session, vsp, "500ns");
    EXPECT_EQ(count(events, "\"sender\":\"tr.out\""), 2) << events;
    EXPECT_EQ(count(events, "\"dir\":\"fw\""), 1) << events;
    EXPECT_EQ(count(events, "\"dir\":\"bw\""), 1) << events;
    EXPECT_EQ(count(events, "\"address\":16"), 2) << events;
    EXPECT_EQ(count(events, "tr.in"), 0) << events;
    EXPECT_EQ(count(events, "{\"event\":\"led\",\"sender\":\"leds\""), 1);
    EXPECT_EQ(count(events, "\"payload\":{\"led\":1,\"state\":true}"), 1);
    EXPECT_EQ(count(events, "dropped"), 0) << events;

    // one uart event per character, in order, as json strings
    size_t pos = 0;
    for (const char* c : { "h", "i", "\\\"", "\\\\", "\\u000a", "\\u00ff" }) {
        string ev = mkstr(
            "\"sender\":\"term\",\"time\":10000,\"delta\":3,"
            "\"payload\":\"%s\"}",
            c);
        pos = events.find(ev, pos);
        ASSERT_NE(pos, string::npos) << ev << " not found in " << events;
    }
    EXPECT_EQ(count(events, "{\"event\":\"uart\""), 6) << events;

    // events are cleared once fetched
    status = vsp.command("status");
    EXPECT_EQ(events_of(status), "{\"events\":[]}") << status;

    // data sent while paused is buffered until the simulation resumes, one
    // level of backslashes is consumed by the vsp argument parser
    EXPECT_EQ(vsp.command("exec,term,send,hi\\\\n"), "OK,queued 3 bytes");
    EXPECT_TRUE(is_error(vsp.command("exec,term,send,\\\\q")));
    EXPECT_EQ(term.send_pending(), 3);

    // too many events between two status requests drop the oldest ones
    EXPECT_TRUE(is_error(vsp.command("sebs")));
    EXPECT_TRUE(is_error(vsp.command("sebs,0")));
    EXPECT_TRUE(is_error(vsp.command("sebs,x")));
    EXPECT_EQ(vsp.command("sebs,1000"), "OK");
    events = run_for(session, vsp, "1us");

    // 9600 baud only allows for one byte to be transmitted within 1us
    EXPECT_EQ(term.send_pending(), 2);

    size_t total = 2 * BURST;
    size_t dropped = total - 1000;
    EXPECT_EQ(count(events, "\"sender\":\"tr.out\""), 1000);
    EXPECT_NE(events.find(mkstr("\"dropped\":%zu}", dropped)), string::npos);
    EXPECT_EQ(count(events, "\"data\":[0,0,0,0]"), 0) << "oldest not dropped";
    EXPECT_EQ(count(events, "\"data\":[63,156,0,0]"), 2) << "newest dropped";

    EXPECT_EQ(vsp.command("unsub"), "OK");
    EXPECT_FALSE(tr.out.has_subscribers());
    EXPECT_FALSE(leds.has_subscribers());
    EXPECT_FALSE(term.has_subscribers());
    status = vsp.command("status");
    EXPECT_EQ(split(status, ',').size(), 4) << status;

    EXPECT_EQ(vsp.command("sub,trace,tr.out"), "OK");
    EXPECT_EQ(vsp.command("resume"), "OK");
}

static vector<string> read_announce() {
    string path = mwr::temp_dir() + mkstr("/vcml_session_%u", mwr::getpid());
    std::ifstream file(path);
    vector<string> lines;
    for (string line; std::getline(file, line);)
        lines.push_back(line);
    return lines;
}

TEST(vspserver, announce) {
    {
        vspserver session("127.0.0.1", 0);
        vector<string> lines = read_announce();
        ASSERT_EQ(lines.size(), 4);
        EXPECT_EQ(lines[0], "127.0.0.1");
        EXPECT_EQ(lines[1], std::to_string(session.port()));
        EXPECT_EQ(lines[2], mwr::username());
        EXPECT_EQ(lines[3], mwr::progname());
    }

    EXPECT_TRUE(read_announce().empty()) << "announce file not removed";

    // wildcard addresses are not reachable, local clients use localhost
    {
        vspserver session("0.0.0.0", 0);
        vector<string> lines = read_announce();
        ASSERT_EQ(lines.size(), 4);
        EXPECT_EQ(lines[0], "localhost");
    }
}

TEST(vspserver, events) {
    traffic tr("tr");
    gpio::leds leds("leds");
    tr.led.bind(leds.gpio_in[1]);

    serial::terminal term("term");
    tr.uart.bind(term.serial_rx);
    term.serial_tx.stub();

    {
        vspserver session("localhost", 0);
        thread client([&]() {
            test_vsp_client vsp(session.port()); // client 0
            bool resumed = false;
            run_client(session, tr, leds, term, vsp, resumed);
            tr.done = true;

            if (!resumed) {
                vsp.command("quit");
                return;
            }

            // the server pauses the simulation when its last client leaves,
            // so stay connected until the simulation has ended
            wait_for([]() {
                return sc_core::sc_get_status() == sc_core::SC_STOPPED;
            });
        });

        session.start();
        client.join();
    }

    // all subscriptions must be gone once the session is gone
    EXPECT_FALSE(tr.out.has_subscribers());
    EXPECT_FALSE(tr.in.has_subscribers());
    EXPECT_FALSE(leds.has_subscribers());
    EXPECT_FALSE(term.has_subscribers());
}
