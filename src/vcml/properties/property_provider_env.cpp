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

#include "vcml/properties/property_provider_env.h"

namespace vcml {

    property_provider_env::property_provider_env():
        property_provider() {
        /* nothing to do */
    }

    property_provider_env::~property_provider_env() {
        /* nothing to do */
    }

    bool property_provider_env::lookup(const string& key, string& val)
    {
        string nm = key;
        std::replace(nm.begin(), nm.end(), SC_HIERARCHY_CHAR, '_');

        const char* env = std::getenv(nm.c_str());
        if (env == nullptr)
            return false;

        char* str = strdup(env);
        val = string(str);
        free(str);

        return true;
    }

}
