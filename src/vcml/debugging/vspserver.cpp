/******************************************************************************
 *                                                                            *
 * Copyright (C) 2022 MachineWare GmbH                                        *
 * All Rights Reserved                                                        *
 *                                                                            *
 * This is work is licensed under the terms described in the LICENSE file     *
 * found in the root directory of this source tree.                           *
 *                                                                            *
 ******************************************************************************/

#include "vcml/core/thctl.h"
#include "vcml/core/systemc.h"
#include "vcml/core/version.h"
#include "vcml/core/component.h"

#include "vcml/debugging/vspserver.h"
#include "vcml/debugging/target.h"
#include "vcml/debugging/loader.h"

#include "vcml/protocols/base.h"
#include "vcml/ui/input.h"

#include "vcml/models/serial/terminal.h"
#include "vcml/models/ethernet/bridge.h"

#include <filesystem>

namespace vcml {
namespace debugging {

static vspserver* session = nullptr;

static void cleanup_session() {
    if (session != nullptr)
        session->cleanup();
}

static string xml_escape(const string& s) {
    stringstream ss;
    for (char c : s) {
        switch (c) {
        case '<':
            ss << "&lt;";
            break;
        case '>':
            ss << "&gt;";
            break;
        case '&':
            ss << "&amp;";
            break;
        case '\'':
            ss << "&apos;";
            break;
        case '\"':
            ss << "&quot;";
            break;
        default:
            ss << c;
        }
    }

    return escape(ss.str(), ",");
}

static string attr_type(const sc_attr_base* attr) {
    const property_base* prop = dynamic_cast<const property_base*>(attr);
    return prop != nullptr ? prop->type() : "unknown";
}

static string attr_name(const sc_attr_base* attr) {
    const string& name = attr->name();

    size_t pos = name.find_last_of(SC_HIERARCHY_CHAR);
    return pos == string::npos ? name : name.substr(pos + 1);
}

static size_t attr_count(const sc_attr_base* attr) {
    const property_base* prop = dynamic_cast<const property_base*>(attr);
    return prop != nullptr ? prop->count() : 0;
}

static const char* obj_version(sc_object* obj) {
    module* mod = dynamic_cast<module*>(obj);
    if (mod)
        return mod->version();

    base_socket* socket = dynamic_cast<base_socket*>(obj);
    if (socket)
        return socket->version();

    const char* kind = obj->kind();
    return starts_with(kind, "vcml::") ? VCML_VERSION_STRING : SC_VERSION;
}

static void list_object(ostream& os, sc_object* obj) {
    // hide object names starting with $$$
    if (starts_with(obj->basename(), "$$$"))
        return;

    os << "<object"
       << " name=\"" << xml_escape(obj->basename()) << "\""
       << " kind=\"" << xml_escape(obj->kind()) << "\""
       << " version=\"" << xml_escape(obj_version(obj)) << "\">";

    // list object attributes
    for (const sc_attr_base* attr : obj->attr_cltn()) {
        os << "<attribute"
           << " name=\"" << xml_escape(attr_name(attr)) << "\""
           << " type=\"" << xml_escape(attr_type(attr)) << "\""
           << " count=\"" << attr_count(attr) << "\""
           << " />";
    }

    // list object commands
    module* mod = dynamic_cast<module*>(obj);
    if (mod != nullptr) {
        for (const command_base* cmd : mod->get_commands()) {
            os << "<command"
               << " name=\"" << xml_escape(cmd->name()) << "\""
               << " argc=\"" << cmd->argc() << "\""
               << " desc=\"" << xml_escape(cmd->desc()) << "\""
               << " />";
        }
    }

    // list object children
    for (sc_object* child : obj->get_child_objects())
        list_object(os, child); // recursive call

    os << "</object>";
}

vspserver* vspserver::instance() {
    return session;
}

string vspserver::handle_version(const string& cmd) {
    stringstream ss;
    ss << "OK,";
    ss << SC_VERSION << ",";
    ss << VCML_VERSION_STRING;
#ifdef VCML_DEBUG
    ss << "-debug";
#endif
    return ss.str();
}

string vspserver::handle_status(const string& cmd) {
    u64 delta = sc_delta_count();
    u64 nanos = time_to_ns(sc_time_stamp());
    string status = is_running() ? "running" : ("stopped:" + m_stop_reason);
    return mkstr("OK,%s,%llu,%llu", status.c_str(), nanos, delta);
}

string vspserver::handle_resume(const string& cmd) {
    if (is_running())
        return "E,simulation running";

    vector<string> args = split(cmd, ',');
    sc_time duration = SC_MAX_TIME;

    if (args.size() > 1)
        duration = from_string<sc_time>(args[1]);

    resume_simulation(duration);
    return mkstr("OK");
}

string vspserver::handle_step(const string& cmd) {
    if (is_running())
        return "E,simulation running";

    vector<string> args = split(cmd, ',');
    if (args.size() < 2)
        return mkstr("E,insufficient arguments %zu", args.size());

    target* tgt = target::find(args[1]);
    if (tgt == nullptr)
        return mkstr("E,no such target: %s", args[1].c_str());

    tgt->request_singlestep(this);
    resume_simulation(SC_MAX_TIME);
    return mkstr("OK");
}

string vspserver::handle_stop(const string& cmd) {
    vector<string> args = split(cmd, ',');
    if (is_running())
        pause_simulation(args.size() > 1 ? args[1] : "user");
    return "OK";
}

string vspserver::handle_quit(const string& cmd) {
    force_quit();
    return "OK";
}

string vspserver::handle_list(const string& cmd) {
    if (is_running())
        return "E,simulation running";

    string format = "xml";
    vector<string> args = split(cmd, ',');
    if (args.size() > 1)
        format = to_lower(args[1]);

    if (format != "xml")
        return mkstr("E,unknown hierarchy format '%s'", format.c_str());

    stringstream ss;
    ss << "OK,<?xml version=\"1.0\" ?><hierarchy>";

    for (auto obj : sc_core::sc_get_top_level_objects())
        list_object(ss, obj);

    for (auto tgt : debugging::target::all())
        ss << "<target>" << xml_escape(tgt->target_name()) << "</target>";

    for (auto loader : debugging::loader::all())
        ss << "<loader>" << xml_escape(loader->loader_name()) << "</loader>";

    for (auto kbd : ui::keyboard::all())
        ss << "<keyboard>" << xml_escape(kbd->input_name()) << "</keyboard>";

    for (auto ptr : ui::pointer::all())
        ss << "<pointer>" << xml_escape(ptr->input_name()) << "</pointer>";

    for (auto terminal : serial::terminal::all())
        ss << "<terminal>" << xml_escape(terminal->name()) << "</terminal>";

    for (auto bridge : ethernet::bridge::all())
        ss << "<bridge>" << xml_escape(bridge->name()) << "</bridge>";

    ss << "</hierarchy>";
    return ss.str();
}

string vspserver::handle_exec(const string& cmd) {
    if (is_running())
        return "E,simulation running";

    vector<string> args = split(cmd, ',');
    if (args.size() < 3)
        return mkstr("E,insufficient arguments %zu", args.size());

    string name = args[1];
    sc_object* obj = find_object(name);
    if (obj == nullptr)
        return mkstr("E,object '%s' not found", name.c_str());

    module* mod = dynamic_cast<module*>(obj);
    if (mod == nullptr)
        return mkstr("E,object '%s' does not support commands", name.c_str());

    try {
        stringstream ss;
        vector<string> cmdargs(args.begin() + 3, args.end());
        bool success = mod->execute(args[2], cmdargs, ss);
        return mkstr("%s,%s", success ? "OK" : "E", ss.str().c_str());
    } catch (std::exception& e) {
        return mkstr("E,%s", escape(e.what(), ",").c_str());
    }
}

string vspserver::handle_getq(const string& cmd) {
    if (is_running())
        return "E,simulation running";
    sc_time quantum = tlm::tlm_global_quantum::instance().get();
    return mkstr("OK,%llu", time_to_ns(quantum));
}

string vspserver::handle_setq(const string& cmd) {
    if (is_running())
        return "E,simulation running";

    vector<string> args = split(cmd, ',');
    if (args.size() < 2)
        return mkstr("E,insufficient arguments %zu", args.size());

    sc_time quantum((double)from_string<u64>(args[1]), SC_NS);
    tlm::tlm_global_quantum::instance().set(quantum);

    return "OK";
}

string vspserver::handle_geta(const string& cmd) {
    if (is_running())
        return "E,simulation running";

    vector<string> args = split(cmd, ',');
    if (args.size() < 2)
        return mkstr("E,insufficient arguments %zu", args.size());

    string name = args[1];
    sc_attr_base* attr = find_attribute(name);
    if (attr == nullptr)
        return mkstr("E,attribute '%s' not found", name.c_str());

    stringstream ss;
    property_base* prop = dynamic_cast<property_base*>(attr);
    ss << "OK," << (prop ? prop->str() : attr->name());

    return ss.str();
}

string vspserver::handle_seta(const string& cmd) {
    if (is_running())
        return "E,simulation running";

    vector<string> args = split(cmd, ',');
    if (args.size() < 3)
        return mkstr("E,insufficient arguments %zu", args.size());

    string name = args[1];
    vector<string> values(args.begin() + 2, args.end());

    sc_attr_base* attr = find_attribute(name);
    if (attr == nullptr)
        return mkstr("E,attribute '%s' not found", name.c_str());

    property_base* prop = dynamic_cast<property_base*>(attr);
    if (prop == nullptr)
        return mkstr("E,attribute '%s' not writable", name.c_str());

    if (values.size() != prop->count()) {
        return mkstr("E,attribute '%s' needs %zu initializers, %zu given",
                     name.c_str(), prop->count(), values.size());
    }

    stringstream ss;
    for (unsigned int i = 0; i < (prop->count() - 1); i++)
        ss << values[i] << " ";
    ss << values[prop->count() - 1];
    prop->str(ss.str());

    return "OK";
}

string vspserver::handle_mkbp(const string& cmd) {
    if (is_running())
        return "E,simulation running";

    vector<string> args = split(cmd, ',');
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

string vspserver::handle_rmbp(const string& cmd) {
    if (is_running())
        return "E,simulation running";

    vector<string> args = split(cmd, ',');
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

void vspserver::resume_simulation(const sc_time& duration) {
    if (is_suspending()) {
        m_stop_reason.clear();
        m_duration = duration;
        resume();
    }
}

void vspserver::pause_simulation(const string& reason) {
    if (!is_suspending()) {
        m_stop_reason = reason;
        sc_pause();
        suspend();
    }
}

void vspserver::force_quit() {
    stop();
    suspender::quit();

    if (is_connected())
        disconnect();
}

void vspserver::notify_step_complete(target& tgt) {
    pause_simulation(mkstr("target:%s", tgt.target_name()));
}

void vspserver::notify_breakpoint_hit(const breakpoint& bp) {
    pause_simulation(mkstr("breakpoint:%llu", bp.id()));
}

void vspserver::notify_watchpoint_read(const watchpoint& wp,
                                       const range& addr) {
    pause_simulation(mkstr("rwatchpoint:%llu", wp.id()));
}

void vspserver::notify_watchpoint_write(const watchpoint& wp,
                                        const range& addr, u64 newval) {
    pause_simulation(mkstr("wwatchpoint:%llu", wp.id()));
}

vspserver::vspserver(u16 server_port):
    rspserver(server_port),
    suspender("vspserver"),
    subscriber(),
    m_announce(mwr::temp_dir() + mkstr("/vcml_session_%hu", port())),
    m_stop_reason("elaboration"),
    m_duration() {
    VCML_ERROR_ON(session != nullptr, "vspserver already created");
    session = this;
    atexit(&cleanup_session);

    register_handler("version", &vspserver::handle_version);
    register_handler("status", &vspserver::handle_status);
    register_handler("resume", &vspserver::handle_resume);
    register_handler("step", &vspserver::handle_step);
    register_handler("stop", &vspserver::handle_stop);
    register_handler("quit", &vspserver::handle_quit);
    register_handler("list", &vspserver::handle_list);
    register_handler("exec", &vspserver::handle_exec);
    register_handler("getq", &vspserver::handle_getq);
    register_handler("setq", &vspserver::handle_setq);
    register_handler("geta", &vspserver::handle_geta);
    register_handler("seta", &vspserver::handle_seta);
    register_handler("mkbp", &vspserver::handle_mkbp);
    register_handler("rmbp", &vspserver::handle_rmbp);

    // Create announce file
    ofstream of(m_announce.c_str());
    of << "localhost:" << std::dec << port() << ":" << mwr::username() << ":"
       << mwr::progname() << std::endl;
}

vspserver::~vspserver() {
    shutdown();
    cleanup();
    session = nullptr;
}

void vspserver::start() {
    run_async();
    log_info("vspserver waiting on port %hu", port());

    // Finish elaboration first before processing commands
    sc_start(SC_ZERO_TIME);
    suspend();

    while (sim_running()) {
        suspender::handle_requests();
        if (!sim_running())
            break;

        if (m_duration == SC_MAX_TIME)
            sc_start();
        else {
            sc_start(m_duration);
            pause_simulation("step");
        }
    }

    if (is_connected())
        disconnect();
}

void vspserver::cleanup() {
    if (!mwr::file_exists(m_announce))
        return;

    std::error_code ec;
    if (!std::filesystem::remove(m_announce)) {
        log_warn("failed to remove file '%s': %s", m_announce.c_str(),
                 ec.message().c_str());
    }
}

void vspserver::handle_connect(const char* peer) {
    log_info("vspserver connected to %s", peer);
}

void vspserver::handle_disconnect() {
    if (sim_running())
        log_info("vspserver waiting on port %hu", port());
}

} // namespace debugging
} // namespace vcml
