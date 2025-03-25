/******************************************************************************
 *                                                                            *
 * Copyright (C) 2025 MachineWare GmbH                                        *
 * All Rights Reserved                                                        *
 *                                                                            *
 * This is work is licensed under the terms described in the LICENSE file     *
 * found in the root directory of this source tree.                           *
 *                                                                            *
 ******************************************************************************/

#include "vcml/protocols/signal.h"

namespace vcml {

ostream& operator<<(ostream& os, const signal_payload_base& tx) {
    stream_guard guard(os);
    os << "SIGNAL val=" << tx.to_string();
    return os;
}

static signal_socket_if* get_signal_socket(sc_object* port) {
    return dynamic_cast<signal_socket_if*>(port);
}

static signal_socket_if* get_signal_socket(sc_object* array, size_t idx) {
    if (auto* aif = dynamic_cast<socket_array_if*>(array))
        return aif->fetch_as<signal_socket_if>(idx, true);
    return nullptr;
}

void signal_stub(const sc_object& obj, const string& port) {
    sc_object* child = find_child(obj, port);
    VCML_ERROR_ON(!child, "%s.%s does not exist", obj.name(), port.c_str());
    auto* socket = get_signal_socket(child);
    VCML_ERROR_ON(!socket, "%s is not a valid signal socket", child->name());
    socket->stub();
}

void signal_stub(const sc_object& obj, const string& port, size_t idx) {
    sc_object* child = find_child(obj, port);
    VCML_ERROR_ON(!child, "%s.%s does not exist", obj.name(), port.c_str());
    auto* s = get_signal_socket(child, idx);
    VCML_ERROR_ON(!s, "%s is not a valid signal socket array", child->name());
    s->stub();
}

void signal_bind(const sc_object& obj1, const string& port1,
                 const sc_object& obj2, const string& port2) {
    auto* p1 = find_child(obj1, port1);
    auto* p2 = find_child(obj2, port2);

    VCML_ERROR_ON(!p1, "%s.%s does not exist", obj1.name(), port1.c_str());
    VCML_ERROR_ON(!p2, "%s.%s does not exist", obj2.name(), port2.c_str());

    auto* s1 = get_signal_socket(p1);
    auto* s2 = get_signal_socket(p2);

    VCML_ERROR_ON(!s1, "%s is not a valid signal port", p1->name());
    VCML_ERROR_ON(!s2, "%s is not a valid signal port", p2->name());

    s1->try_bind(*p2);
}

void signal_bind(const sc_object& obj1, const string& port1,
                 const sc_object& obj2, const string& port2, size_t idx2) {
    auto* p1 = find_child(obj1, port1);
    auto* p2 = find_child(obj2, port2);

    VCML_ERROR_ON(!p1, "%s.%s does not exist", obj1.name(), port1.c_str());
    VCML_ERROR_ON(!p2, "%s.%s does not exist", obj2.name(), port2.c_str());

    auto* s1 = get_signal_socket(p1);
    auto* s2 = get_signal_socket(p2, idx2);

    VCML_ERROR_ON(!s1, "%s is not a valid signal port", p1->name());
    VCML_ERROR_ON(!s2, "%s is not a valid signal port", p2->name());

    s1->try_bind(*p2);
}

void signal_bind(const sc_object& obj1, const string& port1, size_t idx1,
                 const sc_object& obj2, const string& port2) {
    auto* p1 = find_child(obj1, port1);
    auto* p2 = find_child(obj2, port2);

    VCML_ERROR_ON(!p1, "%s.%s does not exist", obj1.name(), port1.c_str());
    VCML_ERROR_ON(!p2, "%s.%s does not exist", obj2.name(), port2.c_str());

    auto* s1 = get_signal_socket(p1, idx1);
    auto* s2 = get_signal_socket(p2);

    VCML_ERROR_ON(!s1, "%s is not a valid signal port", p1->name());
    VCML_ERROR_ON(!s2, "%s is not a valid signal port", p2->name());

    s1->try_bind(*p2);
}

void signal_bind(const sc_object& obj1, const string& port1, size_t idx1,
                 const sc_object& obj2, const string& port2, size_t idx2) {
    auto* p1 = find_child(obj1, port1);
    auto* p2 = find_child(obj2, port2);

    VCML_ERROR_ON(!p1, "%s.%s does not exist", obj1.name(), port1.c_str());
    VCML_ERROR_ON(!p2, "%s.%s does not exist", obj2.name(), port2.c_str());

    auto* s1 = get_signal_socket(p1, idx1);
    auto* s2 = get_signal_socket(p2, idx2);

    VCML_ERROR_ON(!s1, "%s is not a valid signal port", p1->name());
    VCML_ERROR_ON(!s2, "%s is not a valid signal port", p2->name());

    s1->try_bind(*p2);
}

} // namespace vcml
