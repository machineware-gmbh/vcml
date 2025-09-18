/******************************************************************************
 *                                                                            *
 * Copyright (C) 2025 MachineWare GmbH                                        *
 * All Rights Reserved                                                        *
 *                                                                            *
 * This is work is licensed under the terms described in the LICENSE file     *
 * found in the root directory of this source tree.                           *
 *                                                                            *
 ******************************************************************************/

#include "vcml/debugging/vspclient.h"

namespace vcml {
namespace debugging {

// converts an array of bytes to an asci string, e.g.
// { aa, bb, cc, dd } -> "ddccbbaa"
static string hexstr(const u8* bytes, size_t len) {
    string res;
    if (len == 0)
        return res;

    res.reserve(len * 2);
    if (bytes == nullptr) {
        res.insert(res.begin(), 2 * len, '0');
        return res;
    }

    for (size_t i = 0; i < len; i++)
        res = mkstr("%02hhx%s", bytes[i], res.c_str());
    return res;
}

void vspclient::resume_simulation(const sc_time& duration) {
    m_stop_reason.clear();
    if (duration < SC_MAX_TIME)
        m_until = sc_time_stamp() + duration;
    else
        m_until = SC_MAX_TIME;
    m_stop = false;
    m_server.update();
}

void vspclient::pause_simulation(const string& reason) {
    m_stop_reason = reason;
    m_stop = true;
    m_server.update();
}

void vspclient::notify_step_complete(target& tgt, const sc_time& t) {
    // target:<target-name>:<time>
    string reason = mkstr("target:%s:%llu", tgt.target_name(), time_to_ns(t));
    pause_simulation(reason);
}

void vspclient::notify_breakpoint_hit(const breakpoint& bp, const sc_time& t) {
    // breakpoint:<id>:<time>
    string reason = mkstr("breakpoint:%llu:%llu", bp.id(), time_to_ns(t));
    pause_simulation(reason);
}

void vspclient::notify_watchpoint_read(const watchpoint& wp, const range& addr,
                                       const sc_time& t) {
    // rwatchpoint:<id>:<addr>:<size>:<time>
    string reason = mkstr("rwatchpoint:%llu:0x%llx:%llu:%llu", wp.id(),
                          addr.start, addr.length(), time_to_ns(t));
    pause_simulation(reason);
}

void vspclient::notify_watchpoint_write(const watchpoint& wp,
                                        const range& addr, const void* newval,
                                        const sc_time& t) {
    // wwatchpoint:<id>:<addr>:<data>:<time>
    string data = hexstr((const u8*)newval, addr.length());
    string reason = mkstr("wwatchpoint:%llu:0x%llx:0x%s:%llu", wp.id(),
                          addr.start, data.c_str(), time_to_ns(t));
    pause_simulation(reason);
}

vspclient::vspclient(vspserver& server, int clientid, const string& peer,
                     u16 port):
    subscriber(),
    m_server(server),
    m_id(clientid),
    m_port(port),
    m_peer(peer),
    m_name(mkstr("vspclient%d[%s:%hu]", clientid, peer.c_str(), port)),
    m_until(SC_MAX_TIME),
    m_stop(true),
    m_stop_reason("user"),
    m_breakpoints(),
    m_watchpoints() {
    // nothing to do
}

vspclient::~vspclient() {
    for (auto [id, bp] : m_breakpoints)
        bp->owner().remove_breakpoint(bp, this);

    for (auto [id, wp] : m_watchpoints)
        wp->owner().remove_watchpoint(wp, VCML_ACCESS_READ_WRITE, this);
}

void vspclient::notify_step_complete() {
    if (sc_time_stamp() >= m_until)
        pause_simulation("step");
}

string vspclient::handle_status(const string& command) {
    u64 delta = sc_delta_count();
    u64 nanos = time_to_ns(sc_time_stamp());
    string status = m_stop ? ("stopped:" + m_stop_reason) : "running";
    return mkstr("OK,%s,%llu,%llu", status.c_str(), nanos, delta);
}

string vspclient::handle_resume(const string& command) {
    if (!is_stopped())
        return "E,simulation running";

    vector<string> args = split(command, ',');
    sc_time duration = SC_MAX_TIME;

    if (args.size() > 1)
        duration = from_string<sc_time>(args[1]);

    resume_simulation(duration);
    return "OK";
}

string vspclient::handle_step(const string& command) {
    if (!is_stopped())
        return "E,simulation running";

    vector<string> args = split(command, ',');
    if (args.size() < 2)
        return mkstr("E,insufficient arguments %zu", args.size());

    target* tgt = target::find(args[1]);
    if (tgt == nullptr)
        return mkstr("E,no such target: %s", args[1].c_str());

    tgt->request_singlestep(this);
    resume_simulation(SC_MAX_TIME);
    return "OK";
}

string vspclient::handle_stop(const string& command) {
    vector<string> args = split(command, ',');
    if (!is_stopped())
        pause_simulation(args.size() > 1 ? args[1] : "user");
    return "OK";
}

string vspclient::handle_mkbp(const string& command) {
    if (!is_stopped())
        return "E,simulation running";

    vector<string> args = split(command, ',');
    if (args.size() < 3)
        return mkstr("E,insufficient arguments %zu", args.size());

    target* tgt = target::find(args[1]);
    if (tgt == nullptr)
        return mkstr("E,no such target: %s", args[1].c_str());

    u64 addr;
    if (is_number(args[2]))
        addr = from_string<u64>(args[2]);
    else {
        const symbol* sym = tgt->symbols().find_symbol(args[2]);
        if (sym == nullptr)
            return mkstr("E,no address or symbol: %s", args[2].c_str());
        addr = sym->virt_addr();
    }

    const breakpoint* bp = tgt->insert_breakpoint(addr, this);
    if (bp == nullptr)
        return mkstr("E,failed to insert breakpoint at 0x%llx", addr);

    m_breakpoints[bp->id()] = bp;
    return mkstr("OK,inserted breakpoint %llu", bp->id());
}

string vspclient::handle_rmbp(const string& command) {
    if (!is_stopped())
        return "E,simulation running";

    vector<string> args = split(command, ',');
    if (args.size() < 2)
        return mkstr("E,insufficient arguments %zu", args.size());

    u64 bpid = from_string<u64>(args[1]);
    auto it = m_breakpoints.find(bpid);
    if (it == m_breakpoints.end())
        return mkstr("E,invalid breakpoint id: %llu", bpid);

    target& tgt = it->second->owner();
    if (!tgt.remove_breakpoint(it->second, this))
        return mkstr("E,model rejected breakpoint deletion");

    m_breakpoints.erase(it);
    return "OK";
}

string vspclient::handle_mkwp(const string& command) {
    if (!is_stopped())
        return "E,simulation running";

    vector<string> args = split(command, ',');
    if (args.size() < 5)
        return mkstr("E,insufficient arguments %zu", args.size());

    target* tgt = target::find(args[1]);
    if (tgt == nullptr)
        return mkstr("E,no such target: %s", args[1].c_str());

    u64 base;
    if (is_number(args[2]))
        base = from_string<u64>(args[2]);
    else {
        const symbol* sym = tgt->symbols().find_symbol(args[2]);
        if (sym == nullptr)
            return mkstr("E,no address or symbol: %s", args[2].c_str());
        base = sym->virt_addr();
    }

    u64 size = from_string<u64>(args[3]);

    string type = args[4];
    vcml_access vcml_type = (type == "r")    ? VCML_ACCESS_READ
                            : (type == "w")  ? VCML_ACCESS_WRITE
                            : (type == "rw") ? VCML_ACCESS_READ_WRITE
                                             : VCML_ACCESS_NONE;

    if (vcml_type == VCML_ACCESS_NONE)
        return mkstr("E,invalid watchpoint type %s", type.c_str());

    range addr(base, base + size - 1);
    const watchpoint* wp = tgt->insert_watchpoint(addr, vcml_type, this);
    if (wp == nullptr) {
        return mkstr("E,failed to insert watchpoint at %s",
                     to_string(addr).c_str());
    }

    m_watchpoints[wp->id()] = wp;
    return mkstr("OK,inserted watchpoint %llu", wp->id());
}

string vspclient::handle_rmwp(const string& command) {
    if (!is_stopped())
        return "E,simulation running";

    vector<string> args = split(command, ',');
    if (args.size() < 2)
        return mkstr("E,insufficient arguments %zu", args.size());

    u64 wpid = from_string<u64>(args[1]);
    auto it = m_watchpoints.find(wpid);
    if (it == m_watchpoints.end())
        return mkstr("E,invalid watchpoint id: %llu", wpid);

    target& tgt = it->second->owner();

    string type = args[2];
    vcml_access vcml_type = (type == "r")    ? VCML_ACCESS_READ
                            : (type == "w")  ? VCML_ACCESS_WRITE
                            : (type == "rw") ? VCML_ACCESS_READ_WRITE
                                             : VCML_ACCESS_NONE;

    if (vcml_type == VCML_ACCESS_NONE)
        return mkstr("E,invalid watchpoint type %s", type.c_str());

    if (!tgt.remove_watchpoint(it->second, vcml_type, this))
        return mkstr("E,model rejected watchpoint deletion");

    m_watchpoints.erase(it);
    return "OK";
}

} // namespace debugging
} // namespace vcml
