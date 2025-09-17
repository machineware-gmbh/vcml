/******************************************************************************
 *                                                                            *
 * Copyright (C) 2022 MachineWare GmbH                                        *
 * All Rights Reserved                                                        *
 *                                                                            *
 * This is work is licensed under the terms described in the LICENSE file     *
 * found in the root directory of this source tree.                           *
 *                                                                            *
 ******************************************************************************/

#ifndef VCML_ETHERNET_BACKEND_H
#define VCML_ETHERNET_BACKEND_H

#include "vcml/core/types.h"
#include "vcml/protocols/eth.h"
#include "vcml/logging/logger.h"

namespace vcml {
namespace ethernet {

class bridge;

class backend
{
protected:
    bridge* m_parent;
    string m_type;

public:
    logger& log;

    bridge* parent() { return m_parent; }
    const char* type() const { return m_type.c_str(); }

    backend(bridge* gw);
    virtual ~backend();

    backend() = delete;
    backend(const backend&) = delete;
    backend(backend&&) = default;

    virtual void send_to_host(const eth_frame& frame) = 0;
    virtual void send_to_guest(eth_frame frame);

    using create_fn = function<backend*(bridge*, const vector<string>&)>;
    static void define(const string& type, create_fn create);
    static backend* create(bridge* br, const string& type);
};

#define VCML_DEFINE_ETHERNET_BACKEND(name, fn)        \
    MWR_CONSTRUCTOR(define_ethernet_backend_##name) { \
        vcml::ethernet::backend::define(#name, fn);   \
    }

} // namespace ethernet
} // namespace vcml

#endif
