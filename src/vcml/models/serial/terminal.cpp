/******************************************************************************
 *                                                                            *
 * Copyright (C) 2022 MachineWare GmbH                                        *
 * All Rights Reserved                                                        *
 *                                                                            *
 * This is work is licensed under the terms described in the LICENSE file     *
 * found in the root directory of this source tree.                           *
 *                                                                            *
 ******************************************************************************/

#include "vcml/models/serial/terminal.h"

namespace vcml {
namespace serial {

static bool parse_config(const string& config, baud_t& baud,
                         serial_parity& parity, serial_bits& width) {
    baud_t bd;
    size_t bits;
    char c;

    int n = sscanf(config.c_str(), "%zu%c%zu", &bd, &c, &bits);
    if (n != 3)
        return false;

    baud = bd;
    width = (serial_bits)bits;
    if (width < SERIAL_5_BITS)
        width = SERIAL_5_BITS;
    if (width > SERIAL_8_BITS)
        width = SERIAL_8_BITS;

    switch (c) {
    case 'o':
    case 'O':
        parity = SERIAL_PARITY_ODD;
        break;
    case 'e':
    case 'E':
        parity = SERIAL_PARITY_EVEN;
        break;
    case 'm':
    case 'M':
        parity = SERIAL_PARITY_MARK;
        break;
    case 's':
    case 'S':
        parity = SERIAL_PARITY_SPACE;
        break;
    case 'N':
    case 'n':
    default:
        parity = SERIAL_PARITY_NONE;
        break;
    }

    return true;
}

bool terminal::cmd_create_backend(const vector<string>& args, ostream& os) {
    try {
        size_t id = create_backend(args[0]);
        os << "created backend " << id;
        return true;
    } catch (std::exception& ex) {
        os << "error creating backend " << args[0] << ":" << ex.what();
        return false;
    }
}

bool terminal::cmd_destroy_backend(const vector<string>& args, ostream& os) {
    for (const string& arg : args) {
        if (to_lower(arg) == "all") {
            for (auto it : m_backends)
                delete it.second;
            m_backends.clear();
            return true;
        } else {
            size_t id = from_string<size_t>(arg);
            if (!destroy_backend(id))
                os << "invalid backend id: " << id;
        }
    }

    return true;
}

bool terminal::cmd_list_backends(const vector<string>& args, ostream& os) {
    if (args.empty()) {
        for (auto it : m_backends)
            os << it.first << ": " << it.second->type() << ",";
        return true;
    }

    for (const string& arg : args) {
        size_t id = from_string<size_t>(arg);

        auto it = m_backends.find(id);
        os << id << ": ";
        os << (it == m_backends.end() ? "none" : it->second->type());
        os << ",";
    }

    return true;
}

bool terminal::cmd_history(const vector<string>& args, ostream& os) {
    vector<u8> history;
    fetch_history(history);
    for (u8 val : history) {
        if (val == ',')
            os << '\\'; // escape commas
        os << (char)val;
    }

    return true;
}

void terminal::serial_transmit() {
    while (true) {
        sc_time quantum = tlm_global_quantum::instance().get();
        wait(max(serial_tx.cycle(), quantum));

        u8 data = 0xff;
        for (backend* b : m_listeners) {
            if (b->read(data)) {
                serial_tx.send(data);
                wait(serial_tx.cycle());
            }
        }
    }
}

void terminal::serial_receive(u8 data) {
    m_hist.insert(data);
    for (backend* b : m_listeners)
        b->write(data);
}

unordered_map<string, terminal*>& terminal::terminals() {
    static unordered_map<string, terminal*> term;
    return term;
}

terminal::terminal(const sc_module_name& nm):
    module(nm),
    m_hist(),
    m_next_id(),
    m_backends(),
    m_listeners(),
    backends("backends", ""),
    config("config", "9600N8"),
    serial_tx("serial_tx"),
    serial_rx("serial_rx") {
    if (stl_contains(terminals(), string(name())))
        VCML_ERROR("serial terminal '%s' already exists", name());
    terminals()[name()] = this;

    baud_t baud = SERIAL_9600BD;
    serial_bits bits = SERIAL_8_BITS;
    serial_parity parity = SERIAL_PARITY_NONE;
    if (!config.get().empty() && !parse_config(config, baud, parity, bits))
        log_warn("failed to parse configuration");

    serial_tx.set_baud(baud);
    serial_tx.set_parity(parity);
    serial_tx.set_data_width(bits);

    log_debug("using setup %zu%s%zu", baud, serial_parity_str(parity), bits);

    vector<string> types = split(backends);
    for (const auto& type : types) {
        try {
            create_backend(type);
        } catch (std::exception& ex) {
            log_warn("%s", ex.what());
        }
    }

    register_command("create_backend", 1, this, &terminal::cmd_create_backend,
                     "creates a new serial backend for this terminal of a "
                     "given type, usage: create_backend <type>");
    register_command("destroy_backend", 1, this,
                     &terminal::cmd_destroy_backend,
                     "destroys serial backends of this terminal with the "
                     "given IDs, usage: destroy_backend <ID> [ID]..| all");
    register_command("list_backends", 0, this, &terminal::cmd_list_backends,
                     "lists all known backends of this terminal");
    register_command("history", 0, this, &terminal::cmd_history,
                     "show previously transmitted data from this terminal");

    SC_HAS_PROCESS(terminal);
    SC_THREAD(serial_transmit);
}

terminal::~terminal() {
    for (auto it : m_backends)
        delete it.second;

    terminals().erase(name());
}

void terminal::attach(backend* b) {
    if (stl_contains(m_listeners, b))
        VCML_ERROR("attempt to attach backend twice");
    m_listeners.push_back(b);
}

void terminal::detach(backend* b) {
    if (!stl_contains(m_listeners, b))
        VCML_ERROR("attempt to detach unknown backend");
    stl_remove(m_listeners, b);
}

size_t terminal::create_backend(const string& type) {
    hierarchy_guard guard(this);
    m_backends[m_next_id] = backend::create(this, type);
    return m_next_id++;
}

bool terminal::destroy_backend(size_t id) {
    auto it = m_backends.find(id);
    if (it == m_backends.end())
        return false;

    delete it->second;
    m_backends.erase(it);
    return true;
}

terminal* terminal::find(const string& name) {
    auto it = terminals().find(name);
    return it != terminals().end() ? it->second : nullptr;
}

vector<terminal*> terminal::all() {
    vector<terminal*> all;
    all.reserve(terminals().size());
    for (const auto& it : terminals())
        all.push_back(it.second);
    return all;
}

VCML_EXPORT_MODEL(vcml::serial::terminal, name, args) {
    return new terminal(name);
}

} // namespace serial
} // namespace vcml
