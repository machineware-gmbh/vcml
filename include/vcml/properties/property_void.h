/******************************************************************************
 *                                                                            *
 * Copyright (C) 2023 MachineWare GmbH                                        *
 * All Rights Reserved                                                        *
 *                                                                            *
 * This is work is licensed under the terms described in the LICENSE file     *
 * found in the root directory of this source tree.                           *
 *                                                                            *
 ******************************************************************************/

#ifndef VCML_PROPERTY_VOID_H
#define VCML_PROPERTY_VOID_H

#include "vcml/properties/property.h"

namespace vcml {

template <size_t N>
class property<void, N> : public property_base
{
private:
    u8* m_data;
    u64 m_default;
    size_t m_size;
    size_t m_count;
    bool m_inited;

    mutable string m_str;

public:
    property(const char* nm, size_t size, size_t count = N, u64 defval = 0);
    property(sc_object* parent, const char* nm, size_t size, size_t count = N,
             u64 defval = 0);
    virtual ~property();
    VCML_KIND(property);

    property() = delete;
    property(const property<void, N>&) = delete;

    virtual void reset() override;

    virtual const char* str() const override;
    virtual void str(const string& s) override;

    virtual size_t size() const override { return m_size; }
    virtual size_t count() const override { return m_count; }
    virtual const char* type() const override { return "void"; }

    constexpr bool is_inited() const { return m_inited; }
    constexpr bool is_default() const { return !m_inited; }

    u64 get(size_t idx = 0) const;
    void set(u64 val, size_t idx = 0);

    u64 get_default() const;
    void set_default(u64 defval);

    void inherit_default();

    u64 operator[](size_t idx) const { return get(idx); }
};

template <size_t N>
inline property<void, N>::property(const char* nm, size_t size, size_t count,
                                   u64 defval):
    property_base(nm),
    m_data(),
    m_default(defval),
    m_size(size),
    m_count(count),
    m_inited(),
    m_str() {
    property<void, N>::reset();
}

template <size_t N>
inline property<void, N>::property(sc_object* parent, const char* nm,
                                   size_t size, size_t count, u64 defval):
    property_base(parent, nm),
    m_data(),
    m_default(defval),
    m_size(size),
    m_count(count),
    m_inited(),
    m_str() {
    property<void, N>::reset();
}

template <size_t N>
inline property<void, N>::~property() {
    if (m_data)
        delete[] m_data;
}

template <size_t N>
inline void property<void, N>::reset() {
    VCML_ERROR_ON(!m_size, "element size cannot be zero");
    VCML_ERROR_ON(m_size > sizeof(u64), "element size too big");
    VCML_ERROR_ON(!m_count, "element count cannot be zero");
    if (!m_data)
        m_data = new u8[m_size * m_count]();

    m_inited = false;
    set_default(m_default);

    string init;
    if (broker::init(fullname(), init))
        property<void, N>::str(init);
}

template <size_t N>
inline const char* property<void, N>::str() const {
    static const string delim = " ";

    m_str = "";

    for (size_t i = 0; i < m_count - 1; i++)
        m_str += escape(to_string(get(i)), delim) + delim;
    m_str += escape(to_string(get(m_count - 1)), delim);

    return m_str.c_str();
}

template <size_t N>
inline void property<void, N>::str(const string& s) {
    m_inited = true;

    vector<string> args = split(s);
    size_t size = args.size();

    if (size < m_size) {
        log_warn("property %s has not enough initializers", name().c_str());
    } else if (size > m_size) {
        log_warn("property %s has too many initializers", name().c_str());
    }

    u8* ptr = m_data;
    for (size_t i = 0; i < min(m_size, size); i++, ptr += m_size) {
        u64 val = from_string<u64>(trim(args[i]));
        if (mwr::encode_size(val) / 8u > m_size) {
            log_warn("property %s initialization value too big: 0x%llx",
                     name().c_str(), val);
        }
        memcpy(ptr, &val, m_size);
    }
}

template <size_t N>
inline u64 property<void, N>::get(size_t idx) const {
    VCML_ERROR_ON(idx >= m_count, "index %zu out of bounds", idx);
    u64 val = 0;
    memcpy(&val, m_data + m_size * idx, m_size);
    return val;
}

template <size_t N>
inline void property<void, N>::set(u64 val, size_t idx) {
    VCML_ERROR_ON(idx >= m_count, "index %zu out of bounds", idx);
    VCML_ERROR_ON(mwr::encode_size(val) / 8u > m_size, "value too big");
    memcpy(m_data + m_size * idx, &val, m_size);
}

template <size_t N>
inline u64 property<void, N>::get_default() const {
    return m_default;
}

template <size_t N>
inline void property<void, N>::set_default(u64 defval) {
    m_default = defval;
    if (!m_inited) {
        u8* ptr = m_data;
        for (size_t i = 0; i < m_count; i++, ptr += m_size)
            memcpy(ptr, &m_default, m_size);
    }
}

template <size_t N>
inline void property<void, N>::inherit_default() {
    if (m_inited)
        return;

    const property<void, N>* prop = nullptr;
    const sc_object* obj = parent()->get_parent_object();
    for (; obj && !prop; obj = obj->get_parent_object()) {
        const auto* attr = obj->attr_cltn()[name()];
        if (attr != nullptr)
            prop = dynamic_cast<const property<void, N>*>(attr);
    }

    if (prop != nullptr)
        set_default(prop->get());
}

} // namespace vcml

#endif
