/******************************************************************************
 *                                                                            *
 * Copyright (C) 2022 MachineWare GmbH                                        *
 * All Rights Reserved                                                        *
 *                                                                            *
 * This is work is licensed under the terms described in the LICENSE file     *
 * found in the root directory of this source tree.                           *
 *                                                                            *
 ******************************************************************************/

#include "vcml/core/module.h"
#include "vcml/core/systemc.h"
#include "vcml/logging/logger.h"

namespace vcml {

logger::logger(): mwr::logger(), m_parent(nullptr) {
}

logger::logger(sc_object* parent):
    mwr::logger(parent->name()), m_parent(hierarchy_search<module>(parent)) {
}

logger::logger(const string& name):
    mwr::logger(name), m_parent(hierarchy_search<module>()) {
}

bool logger::can_log(log_level lvl) const {
    log_level mylvl = m_parent ? m_parent->loglvl : level();
    return lvl <= mylvl;
}

logger log; // global default logger

u64 log_systemc_time() {
    return time_to_ns(sc_time_stamp());
}

MWR_CONSTRUCTOR(init_systemc_log_time) {
    mwr::publisher::current_timestamp = log_systemc_time;
}

} // namespace vcml
