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
    stub(mkstr("%s.%s", obj.name(), port.c_str()));
}

void signal_stub(const sc_object& obj, const string& port, size_t idx) {
    stub(mkstr("%s.%s[%zu]", obj.name(), port.c_str(), idx));
}

void signal_bind(const sc_object& obj1, const string& port1,
                 const sc_object& obj2, const string& port2) {
    vcml::bind(mkstr("%s.%s", obj1.name(), port1.c_str()),
               mkstr("%s.%s", obj2.name(), port2.c_str()));
}

void signal_bind(const sc_object& obj1, const string& port1,
                 const sc_object& obj2, const string& port2, size_t idx2) {
    vcml::bind(mkstr("%s.%s", obj1.name(), port1.c_str()),
               mkstr("%s.%s[%zu]", obj2.name(), port2.c_str(), idx2));
}

void signal_bind(const sc_object& obj1, const string& port1, size_t idx1,
                 const sc_object& obj2, const string& port2) {
    vcml::bind(mkstr("%s.%s[%zu]", obj1.name(), port1.c_str(), idx1),
               mkstr("%s.%s", obj2.name(), port2.c_str()));
}

void signal_bind(const sc_object& obj1, const string& port1, size_t idx1,
                 const sc_object& obj2, const string& port2, size_t idx2) {
    vcml::bind(mkstr("%s.%s[%zu]", obj1.name(), port1.c_str(), idx1),
               mkstr("%s.%s[%zu]", obj2.name(), port2.c_str(), idx2));
}

} // namespace vcml
