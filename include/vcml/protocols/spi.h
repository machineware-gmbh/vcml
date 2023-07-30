/******************************************************************************
 *                                                                            *
 * Copyright (C) 2022 MachineWare GmbH                                        *
 * All Rights Reserved                                                        *
 *                                                                            *
 * This is work is licensed under the terms described in the LICENSE file     *
 * found in the root directory of this source tree.                           *
 *                                                                            *
 ******************************************************************************/

#ifndef VCML_PROTOCOLS_SPI_H
#define VCML_PROTOCOLS_SPI_H

#include "vcml/core/types.h"
#include "vcml/core/systemc.h"

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
    spi_host() = default;
    virtual ~spi_host() = default;
    virtual void spi_transport(const spi_target_socket&, spi_payload&) = 0;
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

typedef multi_initiator_socket<spi_fw_transport_if, spi_bw_transport_if>
    spi_base_initiator_socket_b;

typedef multi_target_socket<spi_fw_transport_if, spi_bw_transport_if>
    spi_base_target_socket_b;

class spi_base_initiator_socket : public spi_base_initiator_socket_b
{
private:
    spi_target_stub* m_stub;

public:
    spi_base_initiator_socket(const char* nm, address_space = VCML_AS_DEFAULT);
    virtual ~spi_base_initiator_socket();
    VCML_KIND(spi_base_initiator_socket);

    bool is_stubbed() const { return m_stub != nullptr; }
    void stub();
};

class spi_base_target_socket : public spi_base_target_socket_b
{
private:
    spi_initiator_stub* m_stub;

public:
    spi_base_target_socket(const char* nm, address_space = VCML_AS_DEFAULT);
    virtual ~spi_base_target_socket();
    VCML_KIND(spi_base_target_socket);

    bool is_stubbed() const { return m_stub != nullptr; }
    void stub();
};

using spi_base_initiator_array = socket_array<spi_base_initiator_socket>;
using spi_base_target_array = socket_array<spi_base_target_socket>;

class spi_initiator_socket : public spi_base_initiator_socket
{
private:
    spi_host* m_host;

    struct spi_bw_transport : spi_bw_transport_if {
        spi_initiator_socket* socket;
        spi_bw_transport(spi_initiator_socket* parent): socket(parent) {}
        virtual ~spi_bw_transport() = default;
    } m_transport;

public:
    spi_initiator_socket(const char* nm, address_space = VCML_AS_DEFAULT);
    virtual ~spi_initiator_socket() = default;
    VCML_KIND(spi_initiator_socket);

    void transport(spi_payload& spi);
};

class spi_target_socket : public spi_base_target_socket
{
private:
    spi_host* m_host;

    struct spi_fw_transport : spi_fw_transport_if {
        spi_target_socket* socket;
        spi_fw_transport(spi_target_socket* parent): socket(parent) {}
        virtual ~spi_fw_transport() = default;
        virtual void spi_transport(spi_payload& spi) override {
            socket->spi_transport(spi);
        }
    } m_transport;

    void spi_transport(spi_payload& spi);

public:
    spi_target_socket(const char* nm, address_space as = VCML_AS_DEFAULT);
    virtual ~spi_target_socket() = default;
    VCML_KIND(spi_target_socket);
};

using spi_initiator_array = socket_array<spi_initiator_socket>;
using spi_target_array = socket_array<spi_target_socket>;

class spi_initiator_stub : private spi_bw_transport_if
{
public:
    spi_base_initiator_socket spi_out;
    spi_initiator_stub(const char* name);
    virtual ~spi_initiator_stub() = default;
};

class spi_target_stub : private spi_fw_transport_if
{
private:
    virtual void spi_transport(spi_payload& spi) override;

public:
    spi_base_target_socket spi_in;
    spi_target_stub(const char* name);
    virtual ~spi_target_stub() = default;
};

spi_base_initiator_socket& spi_initiator(const sc_object& parent,
                                         const string& port);
spi_base_initiator_socket& spi_initiator(const sc_object& parent,
                                         const string& port, size_t idx);

spi_base_target_socket& spi_target(const sc_object& parent,
                                   const string& port);
spi_base_target_socket& spi_target(const sc_object& parent, const string& port,
                                   size_t idx);

void spi_stub(const sc_object& obj, const string& port);
void spi_stub(const sc_object& obj, const string& port, size_t idx);

void spi_bind(const sc_object& obj1, const string& port1,
              const sc_object& obj2, const string& port2);
void spi_bind(const sc_object& obj1, const string& port1,
              const sc_object& obj2, const string& port2, size_t idx2);
void spi_bind(const sc_object& obj1, const string& port1, size_t idx1,
              const sc_object& obj2, const string& port2);
void spi_bind(const sc_object& obj1, const string& port1, size_t idx1,
              const sc_object& obj2, const string& port2, size_t idx2);

} // namespace vcml

#endif
