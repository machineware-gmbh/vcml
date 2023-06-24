/******************************************************************************
 *                                                                            *
 * Copyright (C) 2023 MachineWare GmbH                                        *
 * All Rights Reserved                                                        *
 *                                                                            *
 * This is work is licensed under the terms described in the LICENSE file     *
 * found in the root directory of this source tree.                           *
 *                                                                            *
 ******************************************************************************/

#ifndef VCML_PROPERTY_VECTOR_H
#define VCML_PROPERTY_VECTOR_H

#include "vcml/properties/property.h"

namespace vcml {

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

    for (size_t i = 0; i < count() - 1; i++)
        m_str += escape(to_string(get(i)), delim) + delim;
    m_str += escape(to_string(get(count() - 1)), delim);

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

} // namespace vcml

#endif
