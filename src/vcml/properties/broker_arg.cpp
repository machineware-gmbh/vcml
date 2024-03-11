/******************************************************************************
 *                                                                            *
 * Copyright (C) 2022 MachineWare GmbH                                        *
 * All Rights Reserved                                                        *
 *                                                                            *
 * This is work is licensed under the terms described in the LICENSE file     *
 * found in the root directory of this source tree.                           *
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
                define(key, mwr::escape(val, ""));
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
