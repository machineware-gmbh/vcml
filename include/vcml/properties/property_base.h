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

#include "vcml/common/includes.h"
#include "vcml/common/types.h"
#include "vcml/common/utils.h"
#include "vcml/common/report.h"

namespace vcml {

    class property_base: public sc_attr_base
    {
    private:
        string m_name;
        sc_module* m_parent;

        // disabled
        property_base();
        property_base(const property_base&);

    public:
        property_base(const char* name, sc_module* parent = NULL);
        virtual ~property_base();

        virtual const char* kind() const {
            return "vcml::property";
        }

        inline const char* hierarchy_name() const {
            return m_name.c_str();
        }

        inline sc_module* get_module() const {
            return m_parent;
        }

        virtual const char* str() const = 0;
        virtual void str(const string& s) = 0;
    };

}

#endif
