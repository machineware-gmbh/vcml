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

static bool parse_indexed_name(const string& name, string& base, size_t& idx) {
    size_t ob = name.find('[');
    size_t cb = name.find(']');
    if (ob == string::npos || cb == string::npos || cb <= ob)
        return false;

    string temp_base = name.substr(0, ob);
    string temp_index = name.substr(ob + 1, cb - ob - 1);

    if (temp_base.empty() || temp_index.empty())
        return false;

    for (char c : temp_index) {
        if (!std::isdigit(c))
            return false;
    }

    base = temp_base;
    idx = from_string<size_t>(temp_index);
    return true;
}

static sc_object& find_indexed_socket(const string& name, size_t idx) {
    auto* array = dynamic_cast<socket_array_if*>(find_object(name));
    VCML_REPORT_ON(!array, "%s is not a valid socket array", name.c_str());
    return *array->fetch(idx, true);
}

sc_object& find_socket(const string& name) {
    sc_object* socket = find_child(nullptr, name);
    if (socket)
        return *socket;

    string base;
    size_t index;
    if (parse_indexed_name(name, base, index))
        return find_indexed_socket(base, index);

    VCML_REPORT("socket %s not found", name.c_str());
}

sc_object& find_socket(const string& name, size_t idx) {
    return find_socket(mkstr("%s[%zu]", name.c_str(), idx));
}

void stub(sc_object& socket, void* data) {
    if (auto* array = dynamic_cast<socket_array_if*>(&socket)) {
        stub(*array->alloc(), data);
        return;
    }

    if (auto* bindable = dynamic_cast<bindable_if*>(&socket)) {
        bindable->stub_socket(data);
        return;
    }

    VCML_REPORT("%s does not support stubbing", socket.name());
}

void stub(const string& name, void* data) {
    sc_object& socket = find_socket(name);
    stub(socket, data);
}

static void do_bind(sc_object* socket1, sc_object* socket2) {
    socket_array_if* array1 = dynamic_cast<socket_array_if*>(socket1);
    socket_array_if* array2 = dynamic_cast<socket_array_if*>(socket2);

    if (array1 && array2)
        VCML_REPORT("attempt to bind two socket arrays");

    if (array1)
        socket1 = array1->alloc();

    if (array2)
        socket2 = array2->alloc();

    if (auto* bindable = dynamic_cast<bindable_if*>(socket1)) {
        bindable->bind_socket(*socket2);
        return;
    }

    if (auto* bindable = dynamic_cast<bindable_if*>(socket2)) {
        bindable->bind_socket(*socket1);
        return;
    }

    if (tlm_try_bind_generic<32>(*socket1, *socket2) ||
        tlm_try_bind_generic<64>(*socket1, *socket2)) {
        return;
    }

    VCML_REPORT("%s cannot be bound to %s", socket1->name(), socket2->name());
}

void bind(sc_object& socket1, sc_object& socket2) {
    do_bind(&socket1, &socket2);
}

void bind(const string& name1, const string& name2) {
    sc_object& socket1 = find_socket(name1);
    sc_object& socket2 = find_socket(name2);
    do_bind(&socket1, &socket2);
}

std::map<string, string> list_sockets(const sc_object& parent) {
    std::map<string, string> ports;
    for (sc_object* obj : parent.get_child_objects()) {
        if (dynamic_cast<base_socket*>(obj) ||
            dynamic_cast<socket_array_if*>(obj) ||
            dynamic_cast<tlm::tlm_base_initiator_socket<>*>(obj) ||
            dynamic_cast<tlm::tlm_base_target_socket<>*>(obj)) {
            ports[obj->basename()] = obj->kind();
        }
    }
    return ports;
}

} // namespace vcml
