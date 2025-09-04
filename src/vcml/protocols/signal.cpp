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

void signal_stub(const sc_object& obj, const string& port) {
    stub(obj, port);
}

void signal_stub(const sc_object& obj, const string& port, size_t idx) {
    stub(obj, port, idx);
}

void signal_bind(const sc_object& obj1, const string& port1,
                 const sc_object& obj2, const string& port2) {
    bind(obj1, port1, obj2, port2);
}

void signal_bind(const sc_object& obj1, const string& port1,
                 const sc_object& obj2, const string& port2, size_t idx2) {
    bind(obj1, port1, obj2, port2, idx2);
}

void signal_bind(const sc_object& obj1, const string& port1, size_t idx1,
                 const sc_object& obj2, const string& port2) {
    bind(obj1, port1, idx1, obj2, port2);
}

void signal_bind(const sc_object& obj1, const string& port1, size_t idx1,
                 const sc_object& obj2, const string& port2, size_t idx2) {
    bind(obj1, port1, idx1, obj2, port2, idx2);
}

} // namespace vcml
