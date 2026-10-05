/******************************************************************************
 *                                                                            *
 * Copyright (C) 2022 MachineWare GmbH                                        *
 * All Rights Reserved                                                        *
 *                                                                            *
 * This work is licensed under the terms described in the LICENSE file found  *
 * in the root directory of this source tree.                                 *
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

// decodes c-style escape sequences, i.e. \n, \r, \t, \e, \0, \\ and \xHH
static bool decode_escapes(const string& s, string& out, ostream& os) {
    for (size_t i = 0; i < s.length(); i++) {
        if (s[i] != '\\') {
            out += s[i];
            continue;
        }

        if (++i == s.length()) {
            os << "incomplete escape sequence";
            return false;
        }

        switch (s[i]) {
        case 'n':
            out += '\n';
            break;
        case 'r':
            out += '\r';
            break;
        case 't':
            out += '\t';
            break;
        case 'e':
            out += '\x1b';
            break;
        case '0':
            out += '\0';
            break;
        case '\\':
            out += '\\';
            break;
        case 'x': {
            string hex = s.substr(i + 1, 2);
            if (hex.length() != 2 || !isxdigit(hex[0]) || !isxdigit(hex[1])) {
                os << "invalid escape sequence \\x" << hex;
                return false;
            }
            out += (char)strtoul(hex.c_str(), nullptr, 16);
            i += 2;
            break;
        }
        default:
            os << "invalid escape sequence \\" << s[i];
            return false;
        }
    }

    return true;
}

bool terminal::cmd_send(const vector<string>& args, ostream& os) {
    string data;
    for (const string& arg : args) {
        if (!decode_escapes(arg, data, os))
            return false;
    }

    send(data);
    os << "queued " << data.length() << " bytes";
    return true;
}

bool terminal::next_byte(u8& data) {
    for (backend* b : m_listeners) {
        if (b->read(data))
            return true;
    }

    lock_guard<mutex> guard(m_send_mtx);
    if (m_send_buf.empty())
        return false;

    data = m_send_buf.front();
    m_send_buf.pop_front();
    return true;
}

void terminal::serial_transmit() {
    while (true) {
        u8 data = 0xff;
        if (!next_byte(data)) {
            wait(m_async_ev);
            continue;
        }

        serial_tx.send(data);
        if (!untimed)
            wait(serial_tx.cycle());
    }
}

// encodes one byte as a json string, bytes that are not printable ascii are
// encoded as \u00XX, i.e. the code point equals the byte value
static string json_char(u8 data) {
    if (data == '"' || data == '\\')
        return mkstr("\"\\%c\"", data);
    if (data < 0x20 || data >= 0x7f)
        return mkstr("\"\\u%04x\"", data);
    return mkstr("\"%c\"", data);
}

void terminal::serial_receive(u8 data) {
    if (has_subscribers()) {
        publish_event(*this, debugging::VSP_EVENT_UART, sc_time_stamp(),
                      json_char(data));
    }

    m_hist.insert(data);
    for (backend* b : m_listeners)
        b->write(data);
}

vector<string> terminal::published_events() const {
    return { debugging::VSP_EVENT_UART };
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
    m_async_ev("async_ev"),
    m_send_mtx(),
    m_send_buf(),
    backends("backends", ""),
    config("config", "9600N8"),
    untimed("untimed", false),
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
    register_command("send", 1, this, &terminal::cmd_send,
                     "queues data to be sent via serial_tx once the "
                     "simulation resumes, supports escape sequences \\n, "
                     "\\r, \\t, \\e, \\0, \\xHH and \\\\, usage: "
                     "send <data> [data]..");

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

void terminal::notify(backend* b) {
    on_next_update([&] { m_async_ev.notify(SC_ZERO_TIME); });
}

void terminal::send(const string& data) {
    if (data.empty())
        return;

    {
        lock_guard<mutex> guard(m_send_mtx);
        m_send_buf.insert(m_send_buf.end(), data.begin(), data.end());
    }

    on_next_update([&] { m_async_ev.notify(SC_ZERO_TIME); });
}

size_t terminal::send_pending() const {
    lock_guard<mutex> guard(m_send_mtx);
    return m_send_buf.size();
}

size_t terminal::create_backend(const string& type) {
    auto guard = get_hierarchy_scope();
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
