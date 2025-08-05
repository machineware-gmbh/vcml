/******************************************************************************
 *                                                                            *
 * Copyright (C) 2024 MachineWare GmbH                                        *
 * All Rights Reserved                                                        *
 *                                                                            *
 * This is work is licensed under the terms described in the LICENSE file     *
 * found in the root directory of this source tree.                           *
 *                                                                            *
 ******************************************************************************/

#ifndef VCML_FIFO_H
#define VCML_FIFO_H

#include "vcml/core/types.h"
#include "vcml/core/systemc.h"

namespace vcml {

template <typename T>
class fifo
{
private:
    size_t m_capacity;
    std::queue<T> m_queue;

public:
    fifo(size_t capacity): m_capacity(capacity), m_queue() {}
    ~fifo() = default;

    size_t capacity() const { return m_capacity; }
    size_t num_used() const { return m_queue.size(); }
    size_t num_free() const { return m_capacity - m_queue.size(); }

    bool empty() const { return m_queue.empty(); }
    bool full() const { return m_queue.size() == m_capacity; }

    T top() const { return m_queue.front(); }

    T pop() {
        T top = m_queue.front();
        m_queue.pop();
        return top;
    }

    bool push(const T& val) {
        if (m_queue.size() >= m_capacity)
            return false;
        m_queue.push(val);
        return true;
    }

    void reset() { std::queue<T>().swap(m_queue); }
};

} // namespace vcml

#endif
