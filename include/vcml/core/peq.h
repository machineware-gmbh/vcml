/******************************************************************************
 *                                                                            *
 * Copyright (C) 2023 MachineWare GmbH                                        *
 * All Rights Reserved                                                        *
 *                                                                            *
 * This is work is licensed under the terms described in the LICENSE file     *
 * found in the root directory of this source tree.                           *
 *                                                                            *
 ******************************************************************************/

#ifndef VCML_PEQ_H
#define VCML_PEQ_H

#include "vcml/core/types.h"
#include "vcml/core/systemc.h"

namespace vcml {

template <typename T>
class peq : public sc_object
{
private:
    sc_event m_event;
    std::multimap<sc_time, T> m_schedule;

    void update();

public:
    peq(const char* nm);
    virtual ~peq() = default;
    VCML_KIND(peq);

    void notify(const T& payload, double t, sc_time_unit tu);
    void notify(const T& payload, const sc_time& delta);
    void cancel(const T& obj);
    void wait(T& obj);
};

template <typename T>
inline void peq<T>::update() {
    if (m_schedule.empty()) {
        m_event.cancel();
    } else {
        sc_time next = m_schedule.begin()->first;
        m_event.notify(next - sc_time_stamp());
    }
}

template <typename T>
inline peq<T>::peq(const char* nm):
    sc_object(nm),
    m_event(mkstr("%s_event", basename()).c_str()),
    m_schedule() {
    // nothing to do
}

template <typename T>
inline void peq<T>::notify(const T& payload, double t, sc_time_unit tu) {
    notify(payload, sc_time(t, tu));
}

template <typename T>
inline void peq<T>::notify(const T& payload, const sc_time& delta) {
    sc_time t = sc_time_stamp() + delta;
    auto range = m_schedule.equal_range(t);
    for (auto it = range.first; it != range.second; it++)
        if (it->second == payload)
            return;

    m_schedule.emplace(t, payload);
    update();
}

template <typename T>
inline void peq<T>::cancel(const T& payload) {
    if (m_schedule.empty())
        return;

    mwr::stl_remove(m_schedule, payload);
    update();
}

template <typename T>
inline void peq<T>::wait(T& obj) {
    auto it = m_schedule.find(sc_time_stamp());
    while (it == m_schedule.end()) {
        sc_core::wait(m_event);
        it = m_schedule.find(sc_time_stamp());
        update();
    }

    obj = it->second;
    m_schedule.erase(it);
    update();
}

} // namespace vcml

#endif
