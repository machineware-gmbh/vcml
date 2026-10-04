/******************************************************************************
 *                                                                            *
 * Copyright (C) 2026 MachineWare GmbH                                        *
 * All Rights Reserved                                                        *
 *                                                                            *
 * This work is licensed under the terms described in the LICENSE file found  *
 * in the root directory of this source tree.                                 *
 *                                                                            *
 ******************************************************************************/

#ifndef VCML_TEST_VSPTEST_H
#define VCML_TEST_VSPTEST_H

#include "testing.h"

using vcml::debugging::vspserver;

class test_vsp_client
{
private:
    mwr::socket m_sock;
    vector<u8> m_buf;
    size_t m_pos;

    // reads whatever is available at once, responses can be megabytes
    int recv_char() {
        if (m_pos == m_buf.size()) {
            size_t n = m_sock.peek();
            if (n == 0)
                return m_sock.recv_char();

            m_buf.resize(n);
            m_sock.recv(m_buf.data(), n);
            m_pos = 0;
        }

        return m_buf[m_pos++];
    }

public:
    test_vsp_client(u16 port): m_sock("localhost", port), m_buf(), m_pos() {}

    string command(const string& cmd) {
        try {
            m_sock.send("$" + cmd + "#00"); // zero checksum is not verified
            while (recv_char() != '+')
                mwr::cpu_yield();

            while (recv_char() != '$')
                mwr::cpu_yield();

            string resp;
            for (int c = recv_char(); c != '#'; c = recv_char())
                resp.push_back(c == '}' ? recv_char() ^ 0x20 : c);

            recv_char(); // checksum
            recv_char(); // checksum
            m_sock.send_char('+');
            return resp;
        } catch (std::exception& ex) {
            (void)ex;
            return "";
        }
    }
};

// a fake cpu that keeps checking its breakpoints, just like a real processor
// would do for every instruction it executes
class test_vsp_cpu : public module, public debugging::target
{
public:
    atomic<bool> done;
    atomic<size_t> steps;
    atomic<size_t> suspends;
    atomic<size_t> steps_without_bps;

    test_vsp_cpu(const sc_module_name& nm):
        module(nm),
        debugging::target(static_cast<sc_object&>(*this)),
        done(false),
        steps(0),
        suspends(0),
        steps_without_bps(0) {
        SC_HAS_PROCESS(test_vsp_cpu);
        SC_THREAD(run);
    }

    virtual bool insert_breakpoint(u64 addr) override { return true; }
    virtual bool remove_breakpoint(u64 addr) override { return true; }

    virtual void session_suspend() override { suspends++; }

    void run() {
        for (u64 pc = 0; !done; pc = (pc + 4) & 0xff, steps++) {
            // never hits the breakpoint at 0x1000, but iterates over it,
            // just like a real processor would for every instruction
            notify_breakpoint_hit(pc, sc_time_stamp());
            if (breakpoints().empty())
                steps_without_bps++;
            wait(1, SC_NS);
        }

        sc_stop();
    }
};

template <typename FN>
inline bool wait_for(FN cond, size_t timeoutms = 5000) {
    for (size_t i = 0; i < timeoutms / 10; i++) {
        if (cond())
            return true;
        mwr::usleep(10000);
    }

    return cond();
}

inline bool has_client(vspserver& session, int id) {
    try {
        return session.find_client(id) != nullptr;
    } catch (std::exception& ex) {
        (void)ex;
        return false;
    }
}

#endif
