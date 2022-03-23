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

#ifndef VCML_PROTOCOLS_SPI_H
#define VCML_PROTOCOLS_SPI_H

#include "vcml/common/types.h"
#include "vcml/common/systemc.h"
#include "vcml/module.h"

#include "vcml/protocols/base.h"

namespace vcml {

struct spi_payload {
    u8 mosi;
    u8 miso;

    spi_payload(u8 init): mosi(init), miso() {}
};

ostream& operator<<(ostream& os, const spi_payload& spi);

class spi_initiator_socket;
class spi_target_socket;
class spi_initiator_stub;
class spi_target_stub;

class spi_host
{
public:
    friend class spi_initiator_socket;
    friend class spi_target_socket;

    typedef vector<spi_initiator_socket*> spi_initiator_sockets;
    typedef vector<spi_target_socket*> spi_target_sockets;

    const spi_initiator_sockets& get_spi_initiator_sockets() const {
        return m_initiator_sockets;
    }

    const spi_target_sockets& get_spi_target_sockets() const {
        return m_target_sockets;
    }

    spi_target_sockets get_spi_target_sockets(address_space);

    spi_host()          = default;
    virtual ~spi_host() = default;
    virtual void spi_transport(const spi_target_socket&, spi_payload&) = 0;

private:
    spi_initiator_sockets m_initiator_sockets;
    spi_target_sockets m_target_sockets;
};

class spi_fw_transport_if : public sc_core::sc_interface
{
public:
    typedef spi_payload protocol_types;
    virtual void spi_transport(spi_payload& spi) = 0;
};

class spi_bw_transport_if : public sc_core::sc_interface
{
public:
    typedef spi_payload protocol_types;
};

typedef base_initiator_socket<spi_fw_transport_if, spi_bw_transport_if>
    spi_base_initiator_socket;

typedef base_target_socket<spi_fw_transport_if, spi_bw_transport_if>
    spi_base_target_socket;

class spi_initiator_socket : public spi_base_initiator_socket,
                             private spi_bw_transport_if
{
private:
    spi_host* m_host;
    spi_target_stub* m_stub;

public:
    bool is_stubbed() const { return m_stub != nullptr; }

    spi_initiator_socket(const char* nm, address_space = VCML_AS_DEFAULT);
    virtual ~spi_initiator_socket();
    VCML_KIND(spi_initiator_socket);

    virtual sc_core::sc_type_index get_protocol_types() const override;

    void transport(spi_payload& spi);
    void stub();
};

class spi_target_socket : public spi_base_target_socket,
                          private spi_fw_transport_if
{
private:
    spi_host* m_host;
    spi_initiator_stub* m_stub;

    virtual void spi_transport(spi_payload& spi) override;

public:
    bool is_stubbed() const { return m_stub != nullptr; }
    spi_target_socket(const char* nm, address_space as = VCML_AS_DEFAULT);
    virtual ~spi_target_socket();
    VCML_KIND(spi_target_socket);
    virtual sc_core::sc_type_index get_protocol_types() const override;
    void stub();
};

class spi_initiator_stub : public module
{
public:
    spi_initiator_socket spi_out;
    spi_initiator_stub(const sc_module_name& name);
    virtual ~spi_initiator_stub();
    VCML_KIND(spi_initiator_stub);
};

class spi_target_stub : public module, public spi_host
{
protected:
    virtual void spi_transport(const spi_target_socket& socket,
                               spi_payload& spi) override;

public:
    spi_target_socket spi_in;
    spi_target_stub(const sc_module_name& name);
    virtual ~spi_target_stub();
    VCML_KIND(spi_target_stub);
};

} // namespace vcml

#endif
