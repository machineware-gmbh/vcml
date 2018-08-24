/******************************************************************************
 *                                                                            *
 * Copyright 2018 Jan Henrik Weinstock                                        *
 *                                                                            *
 * Licensed under the Apache License, Version 2.0 (the "License");            *
 * you may not use this file except in compliance with the License.           *
 * You may obtain a copy of the License at                                    *
 *                                                                            *
 *     http://www.apache.org/licenses/LICENSE-2.0                             *
 *                                                                            *
 * Unless required by applicable law or agreed to in writing, software        *
 * distributed under the License is distributed on an "AS IS" BASIS,          *
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.   *
 * See the License for the specific language governing permissions and        *
 * limitations under the License.                                             *
 *                                                                            *
 ******************************************************************************/

#include "vcml/debugging/vspserver.h"

namespace vcml { namespace debugging {

    static vspserver* session = NULL;

    static void cleanup_session() {
        if (session != NULL)
            session->cleanup();
    }

    string vspserver::handle_none(const char* command) {
        return "";
    }

    string vspserver::handle_step(const char* command) {
        vector<string> args = split(command, ',');
        sc_time duration, next;
        if (args.size() > 1) { // step duration passed as argument
            stringstream ss(args[1]);
            double t; ss >> t;
            duration = sc_time(t, SC_SEC);
        } else if (sc_get_curr_simcontext()->next_time(next)) { // next event
            duration = next - sc_time_stamp();
        } else { // step entire quantum
            duration = tlm::tlm_global_quantum::instance().get();
        }

        run_interruptible(duration);

        // Make sure we are still connected, since during an IO interrupt
        // we may have disconnected due to a protocol error.
        return is_connected() ? "OK" : "";
    }

    string vspserver::handle_cont(const char* command) {
        run_interruptible(SC_ZERO_TIME);

        // Make sure we are still connected, since during an IO interrupt
        // we may have disconnected due to a protocol error.
        return is_connected() ? "OK" : "";
    }

    string vspserver::handle_info(const char* command) {
        vector<string> args = split(command, ',');

        // If no arguments are given, return the top level objects as children
        // of a virtual "root" node.
        if (args.size() < 2) {
            stringstream info;
            info << "kind:root,";
            for (const sc_object* obj : sc_core::sc_get_top_level_objects())
                info << "child:" << obj->basename() << ",";
            return info.str();
        }

        // First argument holds the object name that was queried.
        string name = args[1];
        sc_object* obj = find_object(name);
        if (obj == NULL)
            return mkstr("object '%s' not found", name.c_str());

        stringstream info;
        info << "kind:" << obj->kind() << ",";

        // List all child objects
        for (const sc_object* child : obj->get_child_objects())
            info << "child:" << child->basename() << ",";

        // List all attributes
        for (const sc_attr_base* attr : obj->attr_cltn())
            info << "attr:" << attr->name() << ",";
        sc_core::sc_attr_cltn::const_iterator it;

        // List all commands
        component* mod = dynamic_cast<component*>(obj);
        if (mod != NULL) {
            for (const command_base* cmd : mod->get_commands()) {
                info << "cmd:" << cmd->name() << ":" << cmd->argc()
                     << ":" << cmd->desc() << ",";
            }
        }

        return info.str();
    }

    string vspserver::handle_exec(const char* command) {
        vector<string> args = split(command, ',');

        if (args.size() < 3)
            return mkstr("insufficient arguments %d", args.size());

        string name = args[1];
        sc_object* obj = find_object(name);
        if (obj == NULL)
            return mkstr("object '%s' not found", name.c_str());

        component* mod = dynamic_cast<component*>(obj);
        if (!mod)
            return mkstr("object '%s' does not support commands", name.c_str());

        try {
            stringstream ss;
            vector<string> cmdargs(args.begin() + 3, args.end());
            bool success = mod->execute(args[2], cmdargs, ss);
            return mkstr("%s,%s", success ? "OK" : "ERROR", ss.str().c_str());
        } catch (std::exception& e) {
            return mkstr("ERROR,%s", e.what());
        }
    }

    string vspserver::handle_time(const char* command) {
        return mkstr("%4.12f", sc_time_stamp().to_seconds());
    }

    string vspserver::handle_dcyc(const char* command) {
        return mkstr("%llu", sc_delta_count());
    }

    string vspserver::handle_rdgq(const char* command) {
        sc_time quantum = tlm::tlm_global_quantum::instance().get();
        return mkstr("%4.12f", quantum.to_seconds());
    }

    string vspserver::handle_wrgq(const char* command) {
        vector<string> args = split(command, ',');
        if (args.size() < 2)
            return mkstr("insufficient arguments %d", args.size());

        stringstream ss(args[1]);
        double t;
        ss >> t;

        sc_time quantum(t, SC_SEC);
        tlm::tlm_global_quantum::instance().set(quantum);
        return "OK";
    }

    string vspserver::handle_geta(const char* command) {
        vector<string> args = split(command, ',');
        if (args.size() < 2)
            return mkstr("insufficient arguments %d", args.size());

        string name = args[1];
        sc_attr_base* attr = find_attribute(name);
        if (attr == NULL)
            return mkstr("attribute '%s' not found", name.c_str());

        stringstream ss;
        ss << "name:" << attr->name() << ",";

        property_base* prop = dynamic_cast<property_base*>(attr);
        if (prop)
            ss << "value:" << prop->str() << ",";

        return ss.str();
    }

    string vspserver::handle_seta(const char* command) {
        vector<string> args = split(command, ',');
        if (args.size() < 3)
            return mkstr("insufficient arguments %d", args.size());

        string name = args[1];
        string value = args[2];

        sc_attr_base* attr = find_attribute(name);
        if (attr == NULL)
            return mkstr("attribute '%s' not found", name.c_str());

        property_base* prop = dynamic_cast<property_base*>(attr);
        if (prop == NULL)
            return mkstr("attribute '%s' not writable", name.c_str());

        prop->str(value);
        return "OK";
    }

    string vspserver::handle_quit(const char* command) {
        stop();
        sc_stop();
        return "OK";
    }

    string vspserver::handle_vers(const char* command) {
        stringstream ss;
        ss << "sysc:" << SC_VERSION << ";"
           << "vcml:" << VCML_VERSION_STRING << ";";
        return ss.str();
    }

    static void do_interrupt(int fd, int event) {
        assert(session != NULL);
        session->interrupt();
    }

    void vspserver::run_interruptible(const sc_time& duration) {
        thctl_enter_critical();
        aio_notify(get_connection_fd(), &do_interrupt, AIO_ONCE);
        if (duration != SC_ZERO_TIME)
            sc_start(duration);
        else
            sc_start();
        aio_cancel(get_connection_fd());
        thctl_exit_critical();
    }

    vspserver::vspserver(u16 port):
        rspserver(port),
        m_announce(tempdir() + mkstr("vcml_session_%d", (int)port)) {
        VCML_ERROR_ON(session != NULL, "vspserver already created");
        session = this;
        atexit(&cleanup_session);

        using std::placeholders::_1;
        register_handler("n", std::bind(&vspserver::handle_none, this, _1));
        register_handler("s", std::bind(&vspserver::handle_step, this, _1));
        register_handler("c", std::bind(&vspserver::handle_cont, this, _1));
        register_handler("i", std::bind(&vspserver::handle_info, this, _1));
        register_handler("e", std::bind(&vspserver::handle_exec, this, _1));
        register_handler("t", std::bind(&vspserver::handle_time, this, _1));
        register_handler("d", std::bind(&vspserver::handle_dcyc, this, _1));
        register_handler("q", std::bind(&vspserver::handle_rdgq, this, _1));
        register_handler("Q", std::bind(&vspserver::handle_wrgq, this, _1));
        register_handler("a", std::bind(&vspserver::handle_geta, this, _1));
        register_handler("A", std::bind(&vspserver::handle_seta, this, _1));
        register_handler("x", std::bind(&vspserver::handle_quit, this, _1));
        register_handler("v", std::bind(&vspserver::handle_vers, this, _1));
    }

    vspserver::~vspserver() {
        remove(m_announce.c_str());
        session = NULL;
    }

    void vspserver::start() {
        // Create announce file
        remove(m_announce.c_str());
        ofstream of(m_announce.c_str());
        of << "localhost:" << std::dec << get_port() << ":" << username()
           << ":" << progname() << std::endl;

        // Finish elaboration first before processing commands
        sc_start(SC_ZERO_TIME);
        log_info("vspserver listening on port %d", (int)get_port());

        thctl_exit_critical();
        run();
    }

    void vspserver::interrupt() {
        try {
            switch (recv_signal(0)) {
            case 0x00: // terminate request
            case  'x':
                sc_stop();
                return;

            case 0x03: // interrupt request
            case  'a':
                sc_pause();
                return;

            default:
                VCML_ERROR("invalid signal received");
            }
        } catch (...) {
            sc_pause();
            disconnect();
        }
    }

    void vspserver::cleanup() {
        remove(m_announce.c_str());
    }

    void vspserver::handle_connect(const char* peer) {
        log_info("vspserver connected to %s", peer);
    }

    void vspserver::handle_disconnect() {
        log_info("vspserver listening on port %d", (int)get_port());

    }

}}
