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

#include "vcml/logging/logger.h"
#include "vcml/properties/broker_arg.h"

namespace vcml {

broker_arg::broker_arg(int argc, const char* const* argv): broker("cmdline") {
    int i = 1;
    while (++i < argc) {
        if ((strcmp(argv[i - 1], "--config") == 0) ||
            (strcmp(argv[i - 1], "-c") == 0)) {
            string arg(argv[i++]);
            size_t separator = arg.find('=');

            // if (separator == arg.npos)
            //    log_warning("missing '=' in property '%s'", arg.c_str());

            // this will cause val=key if separator was missing
            string key = arg.substr(0, separator);
            string val = arg.substr(separator + 1);

            try {
                define(key, val);
            } catch (std::exception& ex) {
                log_error("arg %d: %s", i - 1, ex.what());
            }
        }
    }
}

broker_arg::~broker_arg() {
    // nothing to do
}

} // namespace vcml
