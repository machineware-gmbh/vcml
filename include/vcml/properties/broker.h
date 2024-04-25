/******************************************************************************
 *                                                                            *
 * Copyright (C) 2022 MachineWare GmbH                                        *
 * All Rights Reserved                                                        *
 *                                                                            *
 * This is work is licensed under the terms described in the LICENSE file     *
 * found in the root directory of this source tree.                           *
 *                                                                            *
 ******************************************************************************/

#ifndef VCML_BROKER_H
#define VCML_BROKER_H

#include "vcml/core/types.h"
#include "vcml/core/systemc.h"

namespace vcml {

class broker
{
protected:
    struct value {
        string value;
        size_t uses;
    };

    string m_name;
    std::map<string, struct value> m_values;

    string expand(const string& s);

public:
    const char* name() const { return m_name.c_str(); }
    virtual const char* kind() const { return "vcml::broker"; }

    broker(const string& name): broker(name, false) {}
    broker(const string& name, bool insert_front);
    virtual ~broker();

    virtual bool lookup(const string& key, string& value);

    virtual bool defines(const string& key) const;

    template <typename T>
    void define(const string& key, const T& value, size_t uses = 0);

    template <typename T>
    void define(const string& key, const vector<T>& vec, size_t uses = 0);

    template <typename T>
    void define(const string& key, initializer_list<T> list, size_t uses = 0);

    void undefine(const string& key);

    template <typename T>
    static broker* init(const string& key, T& value);

    template <typename T>
    static T get_or_default(const string& key, const T& def = T());

    static vector<pair<string, broker*>> collect_unused();
    static void report_unused();
};

template <typename T>
inline void broker::define(const string& key, const T& val, size_t uses) {
    define(key, to_string(val), uses);
}

template <>
inline void broker::define(const string& key, const string& val, size_t uses) {
    if (!key.empty())
        m_values[expand(key)] = { expand(val), uses };
}

template <typename T>
inline void broker::define(const string& key, const vector<T>& vec,
                           size_t uses) {
    stringstream ss;
    for (const auto& elem : vec)
        ss << elem << " ";
    define(key, expand(ss.str()), uses);
}

template <typename T>
inline void broker::define(const string& key, initializer_list<T> list,
                           size_t uses) {
    stringstream ss;
    for (const auto& elem : list)
        ss << elem << " ";
    define(key, expand(ss.str()), uses);
}

inline void broker::undefine(const string& key) {
    m_values.erase(key);
}

template <typename T>
inline broker* broker::init(const string& key, T& value) {
    string str;
    broker* brkr = broker::init(key, str);
    if (brkr != nullptr)
        value = from_string<T>(str);
    return brkr;
}

template <>
broker* broker::init(const string& key, string& value);

template <typename T>
inline T broker::get_or_default(const string& key, const T& defval) {
    T val = defval;
    init<T>(key, val);
    return val;
}

} // namespace vcml

#endif
