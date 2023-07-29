/******************************************************************************
 *                                                                            *
 * Copyright (C) 2022 MachineWare GmbH                                        *
 * All Rights Reserved                                                        *
 *                                                                            *
 * This is work is licensed under the terms described in the LICENSE file     *
 * found in the root directory of this source tree.                           *
 *                                                                            *
 ******************************************************************************/

#include "vcml/models/can/bridge.h"

namespace vcml {
namespace can {

bool bridge::cmd_create_backend(const vector<string>& args, ostream& os) {
    try {
        id_t id = create_backend(args[0]);
        os << "created backend " << id;
        return true;
    } catch (std::exception& ex) {
        os << "error creating backend " << args[0] << ":" << ex.what();
        return false;
    }
}

bool bridge::cmd_destroy_backend(const vector<string>& args, ostream& os) {
    for (const string& arg : args) {
        if (to_lower(arg) == "all") {
            for (auto it : m_dynamic_backends)
                delete it.second;
            m_dynamic_backends.clear();
            return true;
        } else {
            id_t id = from_string<id_t>(arg);
            if (!destroy_backend(id))
                os << "invalid backend id: " << id;
        }
    }

    return true;
}

bool bridge::cmd_list_backends(const vector<string>& args, ostream& os) {
    if (args.empty()) {
        for (auto it : m_dynamic_backends)
            os << it.first << ": " << it.second->type() << ",";
        return true;
    }

    for (const string& arg : args) {
        size_t id = from_string<size_t>(arg);

        auto it = m_dynamic_backends.find(id);
        os << id << ": ";
        os << (it == m_dynamic_backends.end() ? "none" : it->second->type());
        os << ",";
    }

    return true;
}

void bridge::can_receive(can_frame& frame) {
    send_to_host(frame);
}

void bridge::can_transmit() {
    while (true) {
        wait(m_ev);

        lock_guard<mutex> guard(m_mtx);
        while (!m_rx.empty()) {
            can_frame frame = m_rx.front();
            m_rx.pop();
            can_tx.send(frame);
        }
    }
}

unordered_map<string, bridge*>& bridge::bridges() {
    static unordered_map<string, bridge*> instances;
    return instances;
}

bridge::bridge(const sc_module_name& nm):
    module(nm),
    can_host(),
    m_next_id(),
    m_dynamic_backends(),
    m_backends(),
    m_mtx(),
    m_rx(),
    m_ev("rxev"),
    backends("backends", ""),
    can_tx("can_tx"),
    can_rx("can_rx") {
    bridges()[name()] = this;

    vector<string> types = split(backends);
    for (const string& type : types) {
        try {
            create_backend(type);
        } catch (std::exception& ex) {
            log_warn("%s", ex.what());
        }
    }

    SC_HAS_PROCESS(bridge);
    SC_THREAD(can_transmit);

    register_command("create_backend", 1, this, &bridge::cmd_create_backend,
                     "creates a new backend for this gateway of the given "
                     "type, usage: create_backend <type>");
    register_command("destroy_backend", 1, this, &bridge::cmd_destroy_backend,
                     "destroys the backends of this gateway with the "
                     "specified IDs, usage: destroy_backend <ID>...");
    register_command("list_backends", 0, this, &bridge::cmd_list_backends,
                     "lists all known clients of this gateway");
}

bridge::~bridge() {
    for (auto it : m_dynamic_backends)
        delete it.second;

    bridges().erase(name());
}

void bridge::send_to_host(const can_frame& frame) {
    for (backend* b : m_backends)
        b->send_to_host(frame);
}

void bridge::send_to_guest(can_frame frame) {
    lock_guard<mutex> guard(m_mtx);
    m_rx.push(frame);
    on_next_update([&]() -> void { m_ev.notify(SC_ZERO_TIME); });
}

void bridge::attach(backend* b) {
    if (stl_contains(m_backends, b))
        VCML_ERROR("attempt to attach backend twice");
    m_backends.push_back(b);
}

void bridge::detach(backend* b) {
    if (!stl_contains(m_backends, b))
        VCML_ERROR("attempt to detach unknown backend");
    stl_remove(m_backends, b);
}

id_t bridge::create_backend(const string& type) {
    m_dynamic_backends[m_next_id] = backend::create(this, type);
    return m_next_id++;
}

bool bridge::destroy_backend(id_t id) {
    auto it = m_dynamic_backends.find(id);
    if (it == m_dynamic_backends.end())
        return false;

    delete it->second;
    m_dynamic_backends.erase(it);
    return true;
}

bridge* bridge::find(const string& name) {
    auto it = bridges().find(name);
    return it != bridges().end() ? it->second : nullptr;
}

vector<bridge*> bridge::all() {
    vector<bridge*> all;
    all.reserve(bridges().size());
    for (const auto& it : bridges())
        all.push_back(it.second);
    return all;
}

VCML_EXPORT_MODEL(vcml::can::bridge, name, args) {
    return new bridge(name);
}

} // namespace can
} // namespace vcml
