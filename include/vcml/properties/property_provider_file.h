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

#ifndef VCML_PROPERTY_PROVIDER_FILE_H
#define VCML_PROPERTY_PROVIDER_FILE_H

#include "vcml/common/includes.h"
#include "vcml/common/types.h"
#include "vcml/common/utils.h"
#include "vcml/common/report.h"

#include "vcml/logging/logger.h"
#include "vcml/properties/property_provider.h"

namespace vcml {

    class property_provider_file: public property_provider
    {
    private:
        string m_filename;
        std::map<string, string> m_replacements;

        void parse_file(const string& filename);
        void replace(string& str);

    public:
        property_provider_file() = delete;
        property_provider_file(const string& filename);
        virtual ~property_provider_file();
    };

}

#endif
