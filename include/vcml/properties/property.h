/******************************************************************************
 *                                                                            *
 * Copyright (C) 2022 MachineWare GmbH                                        *
 * All Rights Reserved                                                        *
 *                                                                            *
 * This is work is licensed under the terms described in the LICENSE file     *
 * found in the root directory of this source tree.                           *
 *                                                                            *
 ******************************************************************************/

#ifndef VCML_PROPERTY_H
#define VCML_PROPERTY_H

#include "vcml/core/types.h"
#include "vcml/core/range.h"
#include "vcml/logging/logger.h"
#include "vcml/properties/property_base.h"
#include "vcml/properties/broker.h"

namespace vcml {

template <typename T, size_t N = 1>
class property : public property_base
{
private:
    T m_value[N];
    T m_defval;
    bool m_inited;

    mutable string m_str;

public:
    property(const char* nm, const T& def = T());
    property(sc_object* parent, const char* nm, const T& def = T());
    virtual ~property();
    VCML_KIND(property);

    property() = delete;
    property(const property<T, N>&) = delete;

    virtual void reset() override;

    virtual const char* str() const override;
    virtual void str(const string& s) override;

    virtual size_t size() const override;
    virtual size_t count() const override;
    virtual const char* type() const override;

    constexpr bool is_inited() const { return m_inited; }
    constexpr bool is_default() const { return !m_inited; }

    const T& get() const;
    T& get();

    const T& get(size_t idx) const;
    T& get(size_t idx);

    void set(const T& val);
    void set(const T val[N]);
    void set(const T& val, size_t idx);

    const T& get_default() const;
    void set_default(const T& defval);

    void inherit_default();

    const char* c_str() const;
    size_t length() const;

    operator T() const;
    T operator~() const;

    const T& operator[](size_t idx) const;
    T& operator[](size_t idx);

    property<T, N>& operator=(const property<T, N>& other);

    template <typename T2>
    property<T, N>& operator=(const T2& other);
    template <typename T2>
    property<T, N>& operator+=(const T2& other);
    template <typename T2>
    property<T, N>& operator-=(const T2& other);
    template <typename T2>
    property<T, N>& operator*=(const T2& other);
    template <typename T2>
    property<T, N>& operator/=(const T2& other);
    template <typename T2>
    property<T, N>& operator%=(const T2& other);
    template <typename T2>
    property<T, N>& operator&=(const T2& other);
    template <typename T2>
    property<T, N>& operator|=(const T2& other);
    template <typename T2>
    property<T, N>& operator^=(const T2& other);
    template <typename T2>
    property<T, N>& operator<<=(const T2& other);
    template <typename T2>
    property<T, N>& operator>>=(const T2& other);

    template <typename T2>
    bool operator==(const T2& other);
    template <typename T2>
    bool operator!=(const T2& other);
    template <typename T2>
    bool operator<=(const T2& other);
    template <typename T2>
    bool operator>=(const T2& other);
    template <typename T2>
    bool operator<(const T2& other);
    template <typename T2>
    bool operator>(const T2& other);
};

template <typename T, size_t N>
property<T, N>::property(const char* nm, const T& def):
    property_base(nm), m_value(), m_defval(def), m_inited(false), m_str() {
    property<T, N>::reset();
}

template <typename T, size_t N>
property<T, N>::property(sc_object* parent, const char* nm, const T& def):
    property_base(parent, nm),
    m_value(),
    m_defval(def),
    m_inited(false),
    m_str() {
    property<T, N>::reset();
}

template <typename T, size_t N>
property<T, N>::~property() {
    // nothing to do
}

template <typename T, size_t N>
inline void property<T, N>::reset() {
    for (size_t i = 0; i < N; i++)
        m_value[i] = m_defval;

    string init;
    if (broker::init(fullname(), init))
        property<T, N>::str(init);
}

template <typename T, size_t N>
inline const char* property<T, N>::str() const {
    static const string delim = " ";

    m_str = "";

    if (N > 0) {
        for (size_t i = 0; i < (N - 1); i++)
            m_str += escape(to_string<T>(m_value[i]), delim) + delim;
        m_str += escape(to_string<T>(m_value[N - 1]), delim);
    }

    return m_str.c_str();
}

template <>
inline const char* property<string, 1>::str() const {
    m_str = m_value[0];
    return m_str.c_str();
}

template <typename T, size_t N>
inline void property<T, N>::str(const string& s) {
    m_inited = true;
    vector<string> args = split(s);
    size_t size = args.size();

    if (size < N) {
        log_warn("property %s has not enough initializers", name().c_str());
    } else if (size > N) {
        log_warn("property %s has too many initializers", name().c_str());
    }

    for (size_t i = 0; i < min(N, size); i++)
        m_value[i] = from_string<T>(trim(args[i]));
}

template <>
inline void property<string, 1>::str(const string& s) {
    m_inited = true;
    if (s.length() > 1 && s.front() == '"' && s.back() == '"')
        m_value[0] = s.substr(1, s.length() - 2);
    else
        m_value[0] = s;
}

template <typename T, size_t N>
inline size_t property<T, N>::size() const {
    return sizeof(T);
}

template <typename T, size_t N>
inline size_t property<T, N>::count() const {
    return N;
}

template <typename T, size_t N>
inline const char* property<T, N>::type() const {
    return type_name<T>();
}

template <typename T, size_t N>
const T& property<T, N>::get() const {
    return get(0);
}

template <typename T, size_t N>
inline T& property<T, N>::get() {
    return get(0);
}

template <typename T, size_t N>
const T& property<T, N>::get(size_t idx) const {
    VCML_ERROR_ON(idx >= N, "index %zu out of bounds", idx);
    return m_value[idx];
}

template <typename T, size_t N>
inline T& property<T, N>::get(size_t idx) {
    VCML_ERROR_ON(idx >= N, "index %zu out of bounds", idx);
    return m_value[idx];
}

template <typename T, size_t N>
inline void property<T, N>::set(const T& val) {
    for (size_t i = 0; i < N; i++)
        m_value[i] = val;
    m_inited = true;
}

template <typename T, size_t N>
inline void property<T, N>::set(const T val[N]) {
    for (size_t i = 0; i < N; i++)
        m_value[i] = val[i];
    m_inited = true;
}

template <typename T, size_t N>
inline void property<T, N>::set(const T& val, size_t idx) {
    VCML_ERROR_ON(idx >= N, "index %zu out of bounds", idx);
    m_value[idx] = val;
    m_inited = true;
}

template <typename T, size_t N>
inline const T& property<T, N>::get_default() const {
    return m_defval;
}

template <typename T, size_t N>
inline void property<T, N>::set_default(const T& defval) {
    m_defval = defval;
    if (!m_inited) {
        for (size_t i = 0; i < N; i++)
            m_value[i] = m_defval;
    }
}

template <typename T, size_t N>
inline void property<T, N>::inherit_default() {
    if (m_inited)
        return;

    const property<T, N>* prop = nullptr;
    const sc_object* obj = parent()->get_parent_object();
    for (; obj && !prop; obj = obj->get_parent_object()) {
        const auto* attr = obj->attr_cltn()[name()];
        if (attr != nullptr)
            prop = dynamic_cast<const property<T, N>*>(attr);
    }

    if (prop != nullptr)
        set_default(prop->get());
}

template <typename T, size_t N>
const char* property<T, N>::c_str() const {
    if constexpr (std::is_same_v<T, string>)
        return get().c_str();
    else
        return m_str.c_str();
}

template <typename T, size_t N>
size_t property<T, N>::length() const {
    if constexpr (std::is_same_v<T, string>)
        return get().length();
    if constexpr (std::is_same_v<T, range>)
        return get().length();
    return size();
}

template <typename T, size_t N>
inline property<T, N>::operator T() const {
    return get(0);
}

template <typename T, size_t N>
inline T property<T, N>::operator~() const {
    return ~get(0);
}

template <typename T, size_t N>
inline const T& property<T, N>::operator[](size_t idx) const {
    return get(idx);
}

template <typename T, size_t N>
inline T& property<T, N>::operator[](size_t idx) {
    return get(idx);
}

template <typename T, size_t N>
inline property<T, N>& property<T, N>::operator=(const property<T, N>& o) {
    for (size_t i = 0; i < N; i++)
        set(o.m_value[i], i);
    return *this;
}

template <typename T, size_t N>
template <typename T2>
inline property<T, N>& property<T, N>::operator=(const T2& other) {
    set(other);
    return *this;
}

template <typename T, size_t N>
template <typename T2>
inline property<T, N>& property<T, N>::operator+=(const T2& other) {
    set(get() + other, 0);
    return *this;
}

template <typename T, size_t N>
template <typename T2>
inline property<T, N>& property<T, N>::operator-=(const T2& other) {
    set(get() - other, 0);
    return *this;
}

template <typename T, size_t N>
template <typename T2>
inline property<T, N>& property<T, N>::operator*=(const T2& other) {
    set(get() * other);
    return *this;
}

template <typename T, size_t N>
template <typename T2>
inline property<T, N>& property<T, N>::operator/=(const T2& other) {
    set(get() / other);
    return *this;
}

template <typename T, size_t N>
template <typename T2>
inline property<T, N>& property<T, N>::operator%=(const T2& other) {
    set(get() % other);
    return *this;
}

template <typename T, size_t N>
template <typename T2>
inline property<T, N>& property<T, N>::operator&=(const T2& other) {
    set(get() & other);
    return *this;
}

template <typename T, size_t N>
template <typename T2>
inline property<T, N>& property<T, N>::operator|=(const T2& other) {
    set(get() | other);
    return *this;
}

template <typename T, size_t N>
template <typename T2>
inline property<T, N>& property<T, N>::operator^=(const T2& other) {
    set(get() ^ other);
    return *this;
}

template <typename T, size_t N>
template <typename T2>
inline property<T, N>& property<T, N>::operator<<=(const T2& other) {
    set(get() << other);
    return *this;
}

template <typename T, size_t N>
template <typename T2>
inline property<T, N>& property<T, N>::operator>>=(const T2& other) {
    set(get() >> other);
    return *this;
}

template <typename T, size_t N>
template <typename T2>
inline bool property<T, N>::operator==(const T2& other) {
    for (size_t i = 0; i < N; i++)
        if (m_value[i] != other)
            return false;
    return true;
}

template <typename T, size_t N>
template <typename T2>
inline bool property<T, N>::operator<(const T2& other) {
    for (size_t i = 0; i < N; i++)
        if (m_value[i] >= other)
            return false;
    return true;
}

template <typename T, size_t N>
template <typename T2>
inline bool property<T, N>::operator>(const T2& other) {
    for (size_t i = 0; i < N; i++)
        if (m_value[i] <= other)
            return false;
    return true;
}

template <typename T, size_t N>
template <typename T2>
inline bool property<T, N>::operator!=(const T2& other) {
    return !operator==(other);
}

template <typename T, size_t N>
template <typename T2>
inline bool property<T, N>::operator<=(const T2& other) {
    return !operator>(other);
}

template <typename T, size_t N>
template <typename T2>
inline bool property<T, N>::operator>=(const T2& other) {
    return !operator<(other);
}

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
    virtual const char* type() const override;

    constexpr bool is_inited() const { return m_inited; }
    constexpr bool is_default() const { return !m_inited; }

    u8* raw_ptr() const { return m_data; }
    size_t raw_len() const { return m_size * m_count; }

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

    if (m_count > 0) {
        for (size_t i = 0; i < m_count - 1; i++)
            m_str += escape(to_string(get(i)), delim) + delim;
        m_str += escape(to_string(get(m_count - 1)), delim);
    }

    return m_str.c_str();
}

template <size_t N>
inline void property<void, N>::str(const string& s) {
    m_inited = true;

    vector<string> args = split(s);
    size_t count = args.size();

    if (count < m_count) {
        log_warn("property %s has not enough initializers", name().c_str());
    } else if (count > m_count) {
        log_warn("property %s has too many initializers", name().c_str());
    }

    u8* ptr = m_data;
    for (size_t i = 0; i < min(m_count, count); i++, ptr += m_size) {
        u64 val = from_string<u64>(trim(args[i]));
        if (mwr::encode_size(val) / 8u > m_size) {
            log_warn("property %s initialization value too big: 0x%llx",
                     name().c_str(), val);
        }
        memcpy(ptr, &val, m_size);
    }
}

template <size_t N>
inline const char* property<void, N>::type() const {
    switch (m_size) {
    case 1:
        return "u8";
    case 2:
        return "u16";
    case 4:
        return "u32";
    case 8:
        return "u64";
    default:
        return "void";
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

template <typename T>
class property<vector<T>, 1> : public property_base
{
private:
    vector<T> m_val;
    vector<T> m_def;
    bool m_inited;
    string m_type;

    mutable string m_str;

public:
    property(const char* nm, vector<T>&& defval);
    property(const char* nm, const vector<T>& defval = vector<T>());
    property(sc_object* parent, const char* nm, vector<T>&& defval);
    property(sc_object* parent, const char* nm,
             const vector<T>& defval = vector<T>());
    virtual ~property() = default;
    VCML_KIND(property);

    property() = delete;
    property(const property<vector<T>, 1>&) = delete;

    virtual void reset() override;

    virtual const char* str() const override;
    virtual void str(const string& s) override;

    virtual size_t size() const override { return sizeof(T); }
    virtual size_t count() const override { return m_val.size(); }
    virtual const char* type() const override { return m_type.c_str(); }

    bool empty() const { return m_val.empty(); }

    constexpr bool is_inited() const { return m_inited; }
    constexpr bool is_default() const { return !m_inited; }

    const vector<T>& get() const { return m_val; }
    vector<T>& get() { return m_val; }
    void set(const vector<T>& val);
    void set(vector<T>&& val);

    const T& get(size_t idx) const { return m_val[idx]; }
    T& get(size_t idx) { return m_val[idx]; }
    void set(const T& val, size_t idx) { m_val[idx] = val; }

    const vector<T>& get_default() const { return m_def; }
    void set_default(const vector<T>& def);
    void set_default(vector<T>&& def);
    void inherit_default();

    operator const vector<T>&() const { return get(); }
    operator vector<T>&() { return get(); }

    const T& operator[](size_t idx) const { return get(idx); }
    T& operator[](size_t idx) { return get(idx); }

    using iterator = typename vector<T>::iterator;
    iterator begin() { return m_val.begin(); }
    iterator end() { return m_val.end(); }

    using const_iterator = typename vector<T>::const_iterator;
    const_iterator begin() const { return m_val.begin(); }
    const_iterator end() const { return m_val.end(); }
};

template <typename T>
property<vector<T>, 1>::property(const char* nm, vector<T>&& defval):
    property_base(nm),
    m_val(),
    m_def(std::move(defval)),
    m_inited(),
    m_type(mkstr("vector<%s>", type_name<T>())),
    m_str() {
    property<vector<T>, 1>::reset();
}

template <typename T>
property<vector<T>, 1>::property(const char* nm, const vector<T>& defval):
    property_base(nm),
    m_val(),
    m_def(defval),
    m_inited(),
    m_type(mkstr("vector<%s>", type_name<T>())),
    m_str() {
    property<vector<T>, 1>::reset();
}

template <typename T>
property<vector<T>, 1>::property(sc_object* parent, const char* nm,
                                 vector<T>&& defval):
    property_base(parent, nm),
    m_val(),
    m_def(std::move(defval)),
    m_inited(),
    m_type(mkstr("vector<%s>", type_name<T>())),
    m_str() {
    property<vector<T>, 1>::reset();
}

template <typename T>
property<vector<T>, 1>::property(sc_object* parent, const char* nm,
                                 const vector<T>& defval):
    property_base(parent, nm),
    m_val(),
    m_def(defval),
    m_inited(),
    m_type(mkstr("vector<%s>", type_name<T>())),
    m_str() {
    property<vector<T>, 1>::reset();
}

template <typename T>
inline void property<vector<T>, 1>::reset() {
    m_inited = false;
    set_default(m_def);

    string init;
    if (broker::init(fullname(), init))
        property<vector<T>, 1>::str(init);
}

template <typename T>
inline const char* property<vector<T>, 1>::str() const {
    static const string delim = " ";

    m_str = "";

    if (count() > 0) {
        for (size_t i = 0; i < count() - 1; i++)
            m_str += escape(to_string(get(i)), delim) + delim;
        m_str += escape(to_string(get(count() - 1)), delim);
    }

    return m_str.c_str();
}

template <typename T>
inline void property<vector<T>, 1>::str(const string& s) {
    m_inited = true;

    vector<string> args = split(s);
    size_t size = args.size();

    m_val.resize(size);
    for (size_t i = 0; i < size; i++)
        m_val[i] = from_string<T>(trim(args[i]));
}

template <typename T>
inline void property<vector<T>, 1>::set(const vector<T>& val) {
    m_inited = true;
    m_val = val;
}

template <typename T>
inline void property<vector<T>, 1>::set(vector<T>&& val) {
    m_inited = true;
    m_val = std::move(val);
}

template <typename T>
inline void property<vector<T>, 1>::set_default(const vector<T>& def) {
    m_def = def;
    if (!m_inited)
        m_val = m_def;
}

template <typename T>
inline void property<vector<T>, 1>::set_default(vector<T>&& def) {
    m_def = std::move(def);
    if (!m_inited)
        m_val = m_def;
}

template <typename T>
inline void property<vector<T>, 1>::inherit_default() {
    if (m_inited)
        return;

    const property<vector<T>, 1>* prop = nullptr;
    const sc_object* obj = parent()->get_parent_object();
    for (; obj && !prop; obj = obj->get_parent_object()) {
        const auto* attr = obj->attr_cltn()[name()];
        if (attr != nullptr)
            prop = dynamic_cast<const property<vector<T>, 1>*>(attr);
    }

    if (prop != nullptr)
        set_default(prop->get());
}

template <typename T, size_t N>
ostream& operator<<(std::ostream& os, const property<T, N>& prop) {
    os << prop.str();
    return os;
}

} // namespace vcml

#endif
