/******************************************************************************
 *                                                                            *
 * Copyright (C) 2022 MachineWare GmbH                                        *
 * All Rights Reserved                                                        *
 *                                                                            *
 * This is work is licensed under the terms described in the LICENSE file     *
 * found in the root directory of this source tree.                           *
 *                                                                            *
 ******************************************************************************/

#include "vcml/core/system.h"

namespace vcml {

static mwr::option<bool> list_properties("--list-properties",
                                         "Prints a list of all properties");

static void list_object_properties(sc_object* obj) {
    bool colors = mwr::is_tty(STDOUT_FDNO);
    ostream& os = std::cout;

    for (auto attr : obj->attr_cltn()) {
        property_base* prop = dynamic_cast<property_base*>(attr);
        reg_base* reg = dynamic_cast<reg_base*>(attr);
        if (prop != nullptr) {
            if (colors)
                os << mwr::termcolors::BOLD;
            os << (reg ? "reg<" : "property<");
            if (colors)
                os << mwr::termcolors::YELLOW;
            os << prop->type();
            if (colors)
                os << mwr::termcolors::WHITE << mwr::termcolors::BOLD;
            os << ">\t";
            if (colors)
                os << mwr::termcolors::CLEAR;
            os << prop->fullname();
            if (colors)
                os << mwr::termcolors::BLUE;
            os << "=";
            if (colors)
                os << mwr::termcolors::CLEAR;
            os << "\"";
            if (colors)
                os << mwr::termcolors::CYAN;
            os << prop->str();
            if (colors)
                os << mwr::termcolors::CLEAR;
            os << "\"" << std::endl;
        }
    }

    for (auto child : obj->get_child_objects())
        list_object_properties(child);
}

SC_HAS_PROCESS(system);

void system::timeout() {
    VCML_ERROR_ON(duration == SC_ZERO_TIME, "timeout with zero duration");
    while (true) {
        wait(duration);
        request_stop();
    }
}

system::system(const sc_module_name& nm):
    module(nm),
    name("name", mwr::progname()),
    desc("desc", mwr::progname()),
    config("config", ""),
    backtrace("backtrace", true),
    session("session", -1),
    session_debug("session_debug", false),
    quantum("quantum", sc_time(1, SC_US)),
    duration("duration", SC_ZERO_TIME) {
    if (backtrace)
        mwr::report_segfaults();

    if (duration > SC_ZERO_TIME)
        SC_THREAD(timeout);

    if (config.get().empty())
        log_warn("no configuration specified, use -f <config>");
}

system::~system() {
    // nothing to do
}

int system::run() {
    if (list_properties) {
        list_object_properties(this);
        return EXIT_SUCCESS;
    }

    broker::report_unused();
    tlm::tlm_global_quantum::instance().set(quantum);

    try {
        if (session >= 0) {
            vcml::debugging::vspserver vspsession(session);
            vspsession.echo(session_debug);
            vspsession.start();
        } else if (duration != sc_core::SC_ZERO_TIME) {
            log_info("starting simulation until %s using %s quantum",
                     duration.get().to_string().c_str(),
                     quantum.get().to_string().c_str());
            sc_core::sc_start();
            log_info("simulation stopped");
        } else {
            log_info("starting infinite simulation using %s quantum",
                     quantum.get().to_string().c_str());
            sc_core::sc_start();
            log_info("simulation stopped");
        }
    } catch (sc_report& rep) {
        log_error("%s", rep.what());
        return EXIT_FAILURE;
    } catch (std::exception& ex) {
        log_error("Caught c++ exception: %s", ex.what());
        return EXIT_FAILURE;
    } catch (...) {
        log_error("Caught unknown exception");
        return EXIT_FAILURE;
    }

    return EXIT_SUCCESS;
}

} // namespace vcml
