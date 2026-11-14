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

constexpr size_t BURST = 5000;

// publishes bursts of events without any simulation time passing in between,
// far more than a client buffers
class burster : public module, public debugging::vsppublisher
{
public:
    atomic<size_t> published;
    atomic<bool> done;

    burster(const sc_module_name& nm): module(nm), published(0), done(false) {
        SC_HAS_PROCESS(burster);
        SC_THREAD(run);
    }

    virtual vector<string> published_events() const override {
        return { "burst" };
    }

    void run() {
        while (!done) {
            wait(1, SC_US);
            for (size_t i = 0; i < BURST; i++) {
                publish_event(*this, "burst", sc_time_stamp(),
                              mkstr("%zu", published.load()));
                published++;
            }
        }

        sc_stop();
    }
};

static string events_of(const string& status) {
    vector<string> fields = split(status, ',');
    return fields.size() == 5 ? fields[4] : "";
}

static size_t count(const string& s, const string& what) {
    size_t n = 0;
    for (size_t pos = s.find(what); pos != string::npos;
         pos = s.find(what, pos + what.size()))
        n++;
    return n;
}

static bool is_stopped(const string& status) {
    return starts_with(status, "OK,stopped:");
}

// fetches events a few at a time until the simulation has stopped and none
// are left, checking that they arrive in order and without gaps
static size_t fetch_all(test_vsp_client& vsp, size_t& next) {
    size_t dropped = 0;
    for (;;) {
        string status = vsp.command("status,70");
        string events = events_of(status);
        dropped += count(events, "dropped");

        size_t n = count(events, "\"event\":\"burst\"");
        for (size_t i = 0; i < n; i++, next++) {
            string ev = mkstr("\"payload\":%zu}", next);
            EXPECT_NE(events.find(ev), string::npos) << ev << " missing";
        }

        if (is_stopped(status) && !count(events, "pending"))
            return dropped;
    }
}

TEST(vspserver, backpressure) {
    burster bst("bst");

    vspserver session("localhost", 0);
    thread client([&]() {
        test_vsp_client vsp(session.port()); // client 0
        ASSERT_TRUE(wait_for([&]() { return has_client(session, 0); }));

        EXPECT_EQ(vsp.command("sebm"), "E,insufficient arguments 1");
        EXPECT_EQ(vsp.command("sebm,x"), "E,invalid buffer mode 'x'");
        EXPECT_EQ(vsp.command("sebm,wait,0"), "E,invalid timeout '0'");
        EXPECT_EQ(vsp.command("sebm,wait,x"), "E,invalid timeout 'x'");

        EXPECT_EQ(vsp.command("sub,burst,bst"), "OK");
        EXPECT_EQ(vsp.command("sebs,100"), "OK");
        EXPECT_EQ(vsp.command("sebm,wait"), "OK");

        // the simulation waits for its events to be fetched: none are lost,
        // although every burst is fifty times what the client buffers
        size_t next = 0;
        EXPECT_EQ(vsp.command("resume,3us"), "OK");
        EXPECT_EQ(fetch_all(vsp, next), 0);
        EXPECT_EQ(next, 2 * BURST);
        EXPECT_EQ(bst.published, 2 * BURST);

        // it is waiting in the middle of a burst while nobody fetches
        EXPECT_EQ(vsp.command("resume"), "OK");
        ASSERT_TRUE(wait_for([&]() { return bst.published > next; }));
        mwr::usleep(50000);
        size_t waiting = bst.published;
        EXPECT_LE(waiting, next + 100);
        mwr::usleep(50000);
        EXPECT_EQ(bst.published, waiting);

        // but not once it has to stop: it finishes the burst, keeping its
        // events beyond the size of the buffer
        EXPECT_EQ(vsp.command("stop"), "OK");
        ASSERT_TRUE(wait_for([&]() { return !session.is_running(); }));
        EXPECT_EQ(bst.published, 3 * BURST);
        EXPECT_EQ(fetch_all(vsp, next), 0);
        EXPECT_EQ(next, 3 * BURST);

        // a client that stops fetching its events only holds up the
        // simulation for so long, after that the oldest ones are dropped
        EXPECT_EQ(vsp.command("sebm,wait,100"), "OK");
        EXPECT_EQ(vsp.command("resume,2us"), "OK");
        ASSERT_TRUE(wait_for([&]() { return bst.published == 4 * BURST; }));
        ASSERT_TRUE(wait_for([&]() { return !session.is_running(); }));
        string events = events_of(vsp.command("status"));
        EXPECT_EQ(count(events, "\"event\":\"burst\""), 100);
        EXPECT_NE(events.find(mkstr("\"dropped\":%zu}", BURST - 100)),
                  string::npos);

        // back to dropping right away
        EXPECT_EQ(vsp.command("sebm,drop"), "OK");
        EXPECT_EQ(vsp.command("resume,2us"), "OK");
        ASSERT_TRUE(wait_for([&]() { return bst.published == 6 * BURST; }));
        ASSERT_TRUE(wait_for([&]() { return !session.is_running(); }));
        events = events_of(vsp.command("status"));
        EXPECT_EQ(count(events, "\"event\":\"burst\""), 100);
        EXPECT_EQ(count(events, "dropped"), 1);

        // a client that leaves does not keep the simulation waiting, which
        // then runs on for the one that drops its events
        size_t before = 0;
        {
            test_vsp_client other(session.port()); // client 1
            ASSERT_TRUE(wait_for([&]() { return has_client(session, 1); }));
            EXPECT_EQ(other.command("sub,burst,bst"), "OK");
            EXPECT_EQ(other.command("sebs,100"), "OK");
            EXPECT_EQ(other.command("sebm,wait"), "OK");
            EXPECT_EQ(other.command("resume"), "OK");
            EXPECT_EQ(vsp.command("resume"), "OK");

            before = bst.published;
            ASSERT_TRUE(wait_for([&]() { return bst.published > before; }));
            mwr::usleep(50000);
            EXPECT_LE(bst.published, before + 100);
        }

        ASSERT_TRUE(wait_for([&]() { return !has_client(session, 1); }));
        EXPECT_TRUE(
            wait_for([&]() { return bst.published > before + 2 * BURST; }));

        vsp.command("quit");
    });

    session.start();
    client.join();
}
