/******************************************************************************
 *                                                                            *
 * Copyright (C) 2024 MachineWare GmbH                                        *
 * All Rights Reserved                                                        *
 *                                                                            *
 * This is work is licensed under the terms described in the LICENSE file     *
 * found in the root directory of this source tree.                           *
 *                                                                            *
 ******************************************************************************/

#include "vcml/protocols/usb_sockets.h"

namespace vcml {

void usb_host_if::usb_attach(usb_initiator_socket& s) {
    // to be overloaded
}

void usb_host_if::usb_detach(usb_initiator_socket& s) {
    // to be overloaded
}

void usb_dev_if::usb_reset_device() {
    // to be overloaded
}

void usb_dev_if::usb_reset_endpoint(int ep) {
    // to be overloaded
}

void usb_dev_if::usb_transport(const usb_target_socket& socket,
                               usb_packet& p) {
    usb_transport(p);
}

void usb_dev_if::usb_transport(usb_packet& p) {
    // to be overloaded
}

usb_base_initiator_socket::usb_base_initiator_socket(const char* nm,
                                                     address_space space):
    usb_base_initiator_socket_b(nm, space), m_stub(nullptr) {
}

usb_base_initiator_socket::~usb_base_initiator_socket() {
    if (m_stub)
        delete m_stub;
}

void usb_base_initiator_socket::stub() {
    VCML_ERROR_ON(m_stub, "socket '%s' already stubbed", name());
    auto guard = get_hierarchy_scope();
    m_stub = new usb_target_stub(basename());
    bind(m_stub->usb_in);
}

void usb_base_initiator_socket::bind_socket(sc_object& obj) {
    using I = usb_base_initiator_socket;
    using T = usb_base_target_socket;
    bind_generic<I, T>(*this, obj);
}

void usb_base_initiator_socket::stub_socket(void* data) {
    stub();
}

usb_base_target_socket::usb_base_target_socket(const char* nm,
                                               address_space space):
    usb_base_target_socket_b(nm, space), m_stub(nullptr) {
}

usb_base_target_socket::~usb_base_target_socket() {
    if (m_stub)
        delete m_stub;
}

void usb_base_target_socket::stub() {
    VCML_ERROR_ON(m_stub, "socket '%s' already stubbed", name());
    auto guard = get_hierarchy_scope();
    m_stub = new usb_initiator_stub(basename());
    m_stub->usb_out.bind(*this);
}

void usb_base_target_socket::bind_socket(sc_object& obj) {
    using I = usb_base_initiator_socket;
    using T = usb_base_target_socket;
    bind_generic<I, T>(*this, obj);
}

void usb_base_target_socket::stub_socket(void* data) {
    stub();
}

void usb_initiator_socket::usb_connection_update(usb_speed speed) {
    if (m_speed == speed)
        return;

    if (is_attached() && (speed != USB_SPEED_NONE))
        VCML_ERROR("usb socket %s already connected", name());

    if (speed == USB_SPEED_NONE) {
        m_host->usb_detach(*this);
        m_speed = speed;
    } else {
        m_speed = speed;
        m_host->usb_attach(*this);
    }
}

usb_initiator_socket::usb_initiator_socket(const char* nm, address_space as):
    usb_base_initiator_socket(nm, as),
    m_host(hierarchy_search<usb_host_if>()),
    m_speed(USB_SPEED_NONE),
    m_transport(this) {
    bind(m_transport);
    VCML_ERROR_ON(!m_host, "usb socket %s declared outside usb_host", name());
}

void usb_initiator_socket::send(usb_packet& p) {
    trace_fw(p);
    get_interface(0)->usb_transport(p);
    trace_bw(p);
}

void usb_initiator_socket::reset_device() {
    get_interface(0)->usb_reset(-1);
}

void usb_initiator_socket::reset_endpoint(int ep) {
    VCML_ERROR_ON(ep < 0, "invalid endpoint: %d", ep);
    get_interface(0)->usb_reset(ep);
}

void usb_target_socket::usb_reset(int ep) {
    if (is_attached()) {
        if (ep < 0)
            m_dev->usb_reset_device();
        else
            m_dev->usb_reset_endpoint(ep);
    }
}

void usb_target_socket::usb_transport(usb_packet& p) {
    trace_fw(p);
    if (is_attached())
        m_dev->usb_transport(*this, p);
    else
        p.result = USB_RESULT_NACK;
    trace_bw(p);
}

usb_target_socket::usb_target_socket(const char* nm, address_space as):
    usb_base_target_socket(nm, as),
    m_dev(hierarchy_search<usb_dev_if>()),
    m_speed(USB_SPEED_NONE),
    m_transport(this) {
    bind(m_transport);
    VCML_ERROR_ON(!m_dev, "usb socket %s declared outside usb_host", name());
}

void usb_target_socket::attach(usb_speed speed) {
    VCML_ERROR_ON(speed == USB_SPEED_NONE, "unvalid usb connection speed");
    if (m_speed == speed)
        return;

    if (is_attached())
        detach();

    m_speed = speed;
    (*this)->usb_connection_update(m_speed);
}

void usb_target_socket::detach() {
    if (is_attached()) {
        m_speed = USB_SPEED_NONE;
        (*this)->usb_connection_update(m_speed);
    }
}

void usb_initiator_stub::usb_connection_update(usb_speed speed) {
    // nothing to do
}

usb_initiator_stub::usb_initiator_stub(const char* nm):
    usb_bw_transport_if(),
    usb_out(mkstr("%s_stub", nm).c_str(), VCML_AS_DEFAULT) {
    usb_out.bind(*this);
}

void usb_target_stub::usb_reset(int ep) {
    // nothing to do
}

void usb_target_stub::usb_transport(usb_packet& p) {
    // nothing to do
}

usb_target_stub::usb_target_stub(const char* nm):
    usb_fw_transport_if(),
    usb_in(mkstr("%s_stub", nm).c_str(), VCML_AS_DEFAULT) {
    usb_in.bind(*this);
}

void usb_stub(const sc_object& obj, const string& port) {
    stub(mkstr("%s.%s", obj.name(), port.c_str()));
}

void usb_stub(const sc_object& obj, const string& port, size_t idx) {
    stub(mkstr("%s.%s[%zu]", obj.name(), port.c_str(), idx));
}

void usb_bind(const sc_object& obj1, const string& port1,
              const sc_object& obj2, const string& port2) {
    vcml::bind(mkstr("%s.%s", obj1.name(), port1.c_str()),
               mkstr("%s.%s", obj2.name(), port2.c_str()));
}

void usb_bind(const sc_object& obj1, const string& port1,
              const sc_object& obj2, const string& port2, size_t idx2) {
    vcml::bind(mkstr("%s.%s", obj1.name(), port1.c_str()),
               mkstr("%s.%s[%zu]", obj2.name(), port2.c_str(), idx2));
}

void usb_bind(const sc_object& obj1, const string& port1, size_t idx1,
              const sc_object& obj2, const string& port2) {
    vcml::bind(mkstr("%s.%s[%zu]", obj1.name(), port1.c_str(), idx1),
               mkstr("%s.%s", obj2.name(), port2.c_str()));
}

void usb_bind(const sc_object& obj1, const string& port1, size_t idx1,
              const sc_object& obj2, const string& port2, size_t idx2) {
    vcml::bind(mkstr("%s.%s[%zu]", obj1.name(), port1.c_str(), idx1),
               mkstr("%s.%s[%zu]", obj2.name(), port2.c_str(), idx2));
}

} // namespace vcml
