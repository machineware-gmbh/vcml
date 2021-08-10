/******************************************************************************
 *                                                                            *
 * Copyright 2018 Jan Henrik Weinstock                                        *
 *                                                                            *
 * Licensed under the Apache License, Version 2.0 (the "License");            *
 * you may not use this file except in compliance with the License.           *
 * You may obtain a copy of the License at                                    *
 *                                                                            *
 *     http://www.apache.org/licenses/LICENSE-2.0                             *
 *                                                                            *
 * Unless required by applicable law or agreed to in writing, software        *
 * distributed under the License is distributed on an "AS IS" BASIS,          *
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.   *
 * See the License for the specific language governing permissions and        *
 * limitations under the License.                                             *
 *                                                                            *
 ******************************************************************************/

#ifndef VCML_BROKER_H
#define VCML_BROKER_H

#include "vcml/common/types.h"
#include "vcml/common/strings.h"

namespace vcml {

    class broker
    {
    protected:
        struct value {
            string value;
            int uses;
        };

        string m_name;
        std::map<string, struct value> m_values;

    public:
        enum : int {
            PRIO_DEFAULT = 0,
            PRIO_CFGFILE = 10,
            PRIO_ENVIRON = 100,
            PRIO_CMDLINE = 1000,
        };

        const int priority;

                const char* name() const { return m_name.c_str(); }
        virtual const char* kind() const { return "vcml::broker"; }

        broker(const string& name, int priority = PRIO_DEFAULT);
        virtual ~broker();

        virtual bool provides(const string& name) const;
        virtual bool lookup(const string& name, string& value);

        template <typename T>
        void insert(const string& name, const T& value);

        template <typename T>
        static broker* init(const string& name, T& value);
    };

    template <typename T>
    inline void broker::insert(const string& name, const T& value) {
        insert(name, to_string(value));
    }

    template <typename T>
    inline broker* broker::init(const string& name, T& value) {
        string str;
        broker* brkr = broker::init(name, str);
        if (brkr != nullptr)
            value = from_string<T>(str);
        return brkr;
    }

    template <>
    broker* broker::init(const string& name, string& value);

}

#endif
