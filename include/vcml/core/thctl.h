/******************************************************************************
 *                                                                            *
 * Copyright (C) 2022 MachineWare GmbH                                        *
 * All Rights Reserved                                                        *
 *                                                                            *
 * This is work is licensed under the terms described in the LICENSE file     *
 * found in the root directory of this source tree.                           *
 *                                                                            *
 ******************************************************************************/

#ifndef VCML_THCTL_H
#define VCML_THCTL_H

#include "vcml/core/types.h"
#include "vcml/core/systemc.h"

namespace vcml {

bool thctl_is_sysc_thread();
bool thctl_is_in_critical();

void thctl_notify();
void thctl_block();

void thctl_enter_critical();
void thctl_exit_critical();
void thctl_suspend();
void thctl_flush();

void thctl_set_sysc_thread(thread::id id = std::this_thread::get_id());

class thctl_guard
{
private:
    bool m_locking;

public:
    thctl_guard();
    ~thctl_guard();
};

inline thctl_guard::thctl_guard():
    m_locking(!thctl_is_sysc_thread() && !thctl_is_in_critical()) {
    if (m_locking)
        thctl_enter_critical();
}

inline thctl_guard::~thctl_guard() {
    if (m_locking)
        thctl_exit_critical();
}

} // namespace vcml

#endif
