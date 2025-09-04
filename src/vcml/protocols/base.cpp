/******************************************************************************
 *                                                                            *
 * Copyright (C) 2022 MachineWare GmbH                                        *
 * All Rights Reserved                                                        *
 *                                                                            *
 * This is work is licensed under the terms described in the LICENSE file     *
 * found in the root directory of this source tree.                           *
 *                                                                            *
 ******************************************************************************/

#include "vcml/protocols/base.h"

namespace vcml {

void base_socket::bind_socket(sc_object& other) {
    sc_object* obj = dynamic_cast<sc_object*>(this);
    VCML_REPORT("%s does not support binding", obj ? obj->kind() : "socket");
}

void base_socket::stub_socket(void* stub) {
    sc_object* obj = dynamic_cast<sc_object*>(this);
    VCML_REPORT("%s does not support stubbing", obj ? obj->kind() : "socket");
}

static sc_object* find_socket(const sc_object& obj, const string& name) {
    sc_object* sock = find_child(obj, name);
    VCML_REPORT_ON(!sock, "socket %s.%s not found", obj.name(), name.c_str());
    return sock;
}

static sc_object* find_socket(const sc_object& obj, const string& name,
                              size_t idx) {
    auto* array = dynamic_cast<socket_array_if*>(find_child(obj, name));
    VCML_REPORT_ON(!array, "socket array %s.%s not found", obj.name(),
                   name.c_str());
    sc_object* socket = array->fetch(idx, true);
    VCML_REPORT_ON(!socket, "socket %s.%s[%zu] not found", obj.name(),
                   name.c_str(), idx);
    return socket;
}

void stub(sc_object& socket, void* data) {
    if (auto* bindable = dynamic_cast<bindable_if*>(&socket)) {
        bindable->stub_socket(data);
        return;
    }

    VCML_REPORT("%s does not support stubbing", socket.name());
}

void stub(const sc_object& obj, const string& name, void* data) {
    auto* socket = find_socket(obj, name);
    stub(*socket, data);
}

void stub(const sc_object& obj, const string& name, size_t idx, void* data) {
    auto* socket = find_socket(obj, name, idx);
    stub(*socket, data);
}

void bind(sc_object& socket1, sc_object& socket2) {
    if (auto* bindable = dynamic_cast<bindable_if*>(&socket1)) {
        bindable->bind_socket(socket2);
        return;
    }

    if (auto* bindable = dynamic_cast<bindable_if*>(&socket2)) {
        bindable->bind_socket(socket1);
        return;
    }

    try {
        using I = tlm::tlm_base_initiator_socket<>;
        using T = tlm::tlm_base_target_socket<>;
        bind_generic<I, T>(socket1, socket2);
    } catch (...) {
        // fall through
    }

    VCML_REPORT("%s cannot be bound to %s", socket1.name(), socket2.name());
}

void bind(const sc_object& obj1, const string& port1, const sc_object& obj2,
          const string& port2) {
    auto* socket1 = find_socket(obj1, port1);
    auto* socket2 = find_socket(obj2, port2);
    bind(*socket1, *socket2);
}

void bind(const sc_object& obj1, const string& port1, const sc_object& obj2,
          const string& port2, size_t idx2) {
    auto* socket1 = find_socket(obj1, port1);
    auto* socket2 = find_socket(obj2, port2, idx2);
    bind(*socket1, *socket2);
}

void bind(const sc_object& obj1, const string& port1, size_t idx1,
          const sc_object& obj2, const string& port2) {
    auto* socket1 = find_socket(obj1, port1, idx1);
    auto* socket2 = find_socket(obj2, port2);
    bind(*socket1, *socket2);
}

void bind(const sc_object& obj1, const string& port1, size_t idx1,
          const sc_object& obj2, const string& port2, size_t idx2) {
    auto* socket1 = find_socket(obj1, port1, idx1);
    auto* socket2 = find_socket(obj2, port2, idx2);
    bind(*socket1, *socket2);
}

} // namespace vcml
