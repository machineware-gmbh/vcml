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

#ifndef VCML_PROPERTY_BASE_H
#define VCML_PROPERTY_BASE_H

#include "vcml/common/types.h"
#include "vcml/common/strings.h"
#include "vcml/common/report.h"
#include "vcml/common/systemc.h"

namespace vcml {

    class property_base: public sc_attr_base
    {
    private:
        string     m_base;
        sc_object* m_parent;

    public:
        property_base(const char* name);
        property_base(sc_object* parent, const char* name);
        virtual ~property_base();
        virtual const char* kind() const { return "vcml::property"; }

        property_base() = delete;
        property_base(const property_base&) = delete;
        property_base& operator = (const property_base&) = delete;

        const char* basename() const { return m_base.c_str(); }
        sc_object*  parent()   const { return m_parent; }

        virtual void reset() = 0;

        virtual const char* str() const = 0;
        virtual void str(const string& s) = 0;

        virtual size_t size() const = 0;
        virtual size_t count() const = 0;
        virtual const char* type() const = 0;

        static const char ARRAY_DELIMITER;
    };

}

#endif
