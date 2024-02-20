/******************************************************************************
 *                                                                            *
 * Copyright (C) 2024 MachineWare GmbH                                        *
 * All Rights Reserved                                                        *
 *                                                                            *
 * This is work is licensed under the terms described in the LICENSE file     *
 * found in the root directory of this source tree.                           *
 *                                                                            *
 ******************************************************************************/

#ifndef VCML_PROTOCOLS_USB_SOCKETS_H
#define VCML_PROTOCOLS_USB_SOCKETS_H

#include "vcml/core/types.h"
#include "vcml/core/systemc.h"

#include "vcml/protocols/base.h"
#include "vcml/protocols/usb_types.h"

namespace vcml {

class usb_initiator_socket;
class usb_target_socket;
class usb_initiator_stub;
class usb_target_stub;

class usb_host_if
{
public:
    usb_host_if() = default;
    virtual ~usb_host_if() = default;

    virtual void usb_attach(usb_initiator_socket& s);
    virtual void usb_detach(usb_initiator_socket& s);
};

class usb_dev_if
{
public:
    usb_dev_if() = default;
    virtual ~usb_dev_if() = default;

    virtual void usb_reset_device();
    virtual void usb_reset_endpoint(int ep);
    virtual void usb_transport(const usb_target_socket& s, usb_packet& p);
    virtual void usb_transport(usb_packet& p);
};

class usb_fw_transport_if : public sc_core::sc_interface
{
public:
    typedef usb_packet protocol_types;

    virtual void usb_reset(int ep) = 0;
    virtual void usb_transport(usb_packet& p) = 0;
};

class usb_bw_transport_if : public sc_core::sc_interface
{
public:
    typedef usb_packet protocol_types;

    virtual void usb_connection_update(usb_speed speed) = 0;
};

typedef base_initiator_socket<usb_fw_transport_if, usb_bw_transport_if>
    usb_base_initiator_socket_b;

typedef base_target_socket<usb_fw_transport_if, usb_bw_transport_if>
    usb_base_target_socket_b;

class usb_base_initiator_socket : public usb_base_initiator_socket_b
{
private:
    usb_target_stub* m_stub;

public:
    usb_base_initiator_socket(const char*, address_space = VCML_AS_DEFAULT);
    virtual ~usb_base_initiator_socket();
    VCML_KIND(usb_base_initiator_socket);

    bool is_stubbed() const { return m_stub != nullptr; }
    void stub();
};

class usb_base_target_socket : public usb_base_target_socket_b
{
private:
    usb_initiator_stub* m_stub;

public:
    usb_base_target_socket(const char*, address_space = VCML_AS_DEFAULT);
    virtual ~usb_base_target_socket();
    VCML_KIND(usb_base_target_socket);

    bool is_stubbed() const { return m_stub != nullptr; }
    void stub();
};

using usb_base_initiator_array = socket_array<usb_base_initiator_socket>;
using usb_base_target_array = socket_array<usb_base_target_socket>;

class usb_initiator_socket : public usb_base_initiator_socket
{
private:
    usb_host_if* m_host;
    usb_speed m_speed;

    struct usb_bw_transport : usb_bw_transport_if {
        usb_initiator_socket* socket;
        usb_bw_transport(usb_initiator_socket* s):
            usb_bw_transport_if(), socket(s) {}
        virtual ~usb_bw_transport() = default;
        virtual void usb_connection_update(usb_speed speed) override {
            socket->usb_connection_update(speed);
        }
    } m_transport;

    void usb_connection_update(usb_speed speed);

public:
    usb_initiator_socket(const char* name, address_space = VCML_AS_DEFAULT);
    virtual ~usb_initiator_socket() = default;
    VCML_KIND(usb_initiator_socket);

    constexpr usb_speed connection_speed() const { return m_speed; }
    constexpr bool is_attached() const { return m_speed != USB_SPEED_NONE; }

    void send(usb_packet& p);
    void reset_device();
    void reset_endpoint(int ep);
};

class usb_target_socket : public usb_base_target_socket
{
private:
    usb_dev_if* m_dev;
    usb_speed m_speed;

    struct usb_fw_transport : usb_fw_transport_if {
        usb_target_socket* socket;
        usb_fw_transport(usb_target_socket* t):
            usb_fw_transport_if(), socket(t) {}
        virtual ~usb_fw_transport() = default;
        virtual void usb_reset(int ep) override { socket->usb_reset(ep); }
        virtual void usb_transport(usb_packet& p) override {
            socket->usb_transport(p);
        }
    } m_transport;

    void usb_reset(int ep);
    void usb_transport(usb_packet& p);

public:
    usb_target_socket(const char* name, address_space as = VCML_AS_DEFAULT);
    virtual ~usb_target_socket() = default;
    VCML_KIND(usb_target_socket);

    constexpr usb_speed connection_speed() const { return m_speed; }
    constexpr bool is_attached() const { return m_speed != USB_SPEED_NONE; }

    void attach(usb_speed speed);
    void detach();
};

class usb_initiator_stub : private usb_bw_transport_if
{
private:
    virtual void usb_connection_update(usb_speed speed) override;

public:
    usb_base_initiator_socket usb_out;
    usb_initiator_stub(const char* nm);
    virtual ~usb_initiator_stub() = default;
};

class usb_target_stub : private usb_fw_transport_if
{
private:
    virtual void usb_reset(int ep) override;
    virtual void usb_transport(usb_packet& p) override;

public:
    usb_base_target_socket usb_in;
    usb_target_stub(const char* nm);
    virtual ~usb_target_stub() = default;
};

using usb_initiator_array = socket_array<usb_initiator_socket>;
using usb_target_array = socket_array<usb_target_socket>;

usb_base_initiator_socket& usb_initiator(const sc_object& parent,
                                         const string& port);
usb_base_initiator_socket& usb_initiator(const sc_object& parent,
                                         const string& port, size_t idx);

usb_base_target_socket& usb_target(const sc_object& parent,
                                   const string& port);
usb_base_target_socket& usb_target(const sc_object& parent, const string& port,
                                   size_t idx);

void usb_stub(const sc_object& obj, const string& port);
void usb_stub(const sc_object& obj, const string& port, size_t idx);

void usb_bind(const sc_object& obj1, const string& port1,
              const sc_object& obj2, const string& port2);
void usb_bind(const sc_object& obj1, const string& port1,
              const sc_object& obj2, const string& port2, size_t idx2);
void usb_bind(const sc_object& obj1, const string& port1, size_t idx1,
              const sc_object& obj2, const string& port2);
void usb_bind(const sc_object& obj1, const string& port1, size_t idx1,
              const sc_object& obj2, const string& port2, size_t idx2);

} // namespace vcml

#endif
