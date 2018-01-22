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

#ifndef VCML_PROPERTY_H
#define VCML_PROPERTY_H

#include "vcml/common/includes.h"
#include "vcml/common/types.h"
#include "vcml/common/utils.h"
#include "vcml/common/report.h"

#include "vcml/properties/property_base.h"
#include "vcml/properties/property_provider.h"

namespace vcml {

    template <typename T>
    class property: public property_base
    {
    private:
        T      m_value;
        T      m_defval;
        string m_strval;
        bool   m_inited;

        // disabled
        property();
        property(const property&);

    public:
        property(const char* nm, const T& defval = T(), sc_module* mod = NULL):
            property_base(nm, mod),
            m_value(defval),
            m_defval(defval),
            m_strval(to_string(defval)),
            m_inited(false) {
            m_inited = property_provider::init(hierarchy_name(), m_strval);
            if (m_inited)
                m_value = from_string<T>(m_strval);
        }

        virtual ~property() {
            /* nothing to do */
        }

        inline const char* str() const {
            return m_strval.c_str();
        }

        virtual void str(const string& s) {
            m_value = from_string<T>(s);
            m_inited = true;
            m_strval = s;
        }

        inline const T& get() const {
            return m_value;
        }

        inline T& get() {
            return m_value;
        }

        virtual void set(const T& val) {
            m_value = val;
            m_inited = true;
            m_strval = to_string(val);
        }

        inline const T& get_default() const {
            return m_defval;
        }

        inline void set_default(const T& defval) {
            m_defval = defval;
            if (!m_inited)
                set(defval);
        }

        inline operator T() const {
            return get();
        }

        inline property<T>& operator = (const property<T>& other) {
            set(other.m_value);
            return *this;
        }

        template <typename T2>
        inline property<T>& operator = (const T2& other) {
            set(other);
            return *this;
        }

        template <typename T2>
        inline property<T>& operator += (const T2& other) {
            set(m_value + other);
            return *this;
        }

        template <typename T2>
        inline property<T>& operator -= (const T2& other) {
            set(m_value - other);
            return *this;
        }

        template <typename T2>
        inline property<T>& operator *= (const T2& other) {
            set(m_value * other);
            return *this;
        }

        template <typename T2>
        inline property<T>& operator /= (const T2& other) {
            set(m_value / other);
            return *this;
        }

        template <typename T2>
        inline property<T>& operator %= (const T& other) {
            set(m_value % other);
            return *this;
        }

        template <typename T2>
        inline property<T>& operator &= (const T2& other) {
            set(m_value & other);
            return *this;
        }

        template <typename T2>
        inline property<T>& operator |= (const T2& other) {
            set(m_value | other);
            return *this;
        }

        template <typename T2>
        inline property<T>& operator ^= (const T2& other) {
            set(m_value ^ other);
            return *this;
        }

        template <typename T2>
        inline property<T>& operator <<= (const T2& other) {
            set(m_value << other);
            return *this;
        }

        template <typename T2>
        inline property<T>& operator >>= (const T2& other) {
            set(m_value >> other);
            return *this;
        }

        template <typename T2>
        inline bool operator == (const T2& other) {
            return m_value == other;
        }

        template <typename T2>
        inline bool operator != (const T2& other) {
            return m_value != other;
        }

        template <typename T2>
        inline bool operator <= (const T2& other) {
            return m_value <= other;
        }

        template <typename T2>
        inline bool operator >= (const T2& other) {
            return m_value >= other;
        }

        template <typename T2>
        inline bool operator < (const T2& other) {
            return m_value < other;
        }

        template <typename T2>
        inline bool operator > (const T2& other) {
            return m_value > other;
        }

    };

}

#endif
