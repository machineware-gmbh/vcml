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

static usb_base_initiator_socket* get_initiator_socket(sc_object* port) {
    return dynamic_cast<usb_base_initiator_socket*>(port);
}

static usb_base_target_socket* get_target_socket(sc_object* port) {
    return dynamic_cast<usb_base_target_socket*>(port);
}

static usb_base_initiator_socket* get_initiator_socket(sc_object* array,
                                                       size_t idx) {
    auto* base = dynamic_cast<usb_base_initiator_array*>(array);
    if (base)
        return &base->get(idx);
    auto* main = dynamic_cast<usb_initiator_array*>(array);
    if (main)
        return &main->get(idx);
    return nullptr;
}

static usb_base_target_socket* get_target_socket(sc_object* array,
                                                 size_t idx) {
    auto* base = dynamic_cast<usb_base_target_array*>(array);
    if (base)
        return &base->get(idx);
    auto* main = dynamic_cast<usb_target_array*>(array);
    if (main)
        return &main->get(idx);
    return nullptr;
}

usb_base_initiator_socket& usb_initiator(const sc_object& parent,
                                         const string& port) {
    sc_object* child = find_child(parent, port);
    VCML_ERROR_ON(!child, "%s.%s does not exist", parent.name(), port.c_str());
    auto* sock = get_initiator_socket(child);
    VCML_ERROR_ON(!sock, "%s is not a valid initiator socket", child->name());
    return *sock;
}

usb_base_initiator_socket& usb_initiator(const sc_object& parent,
                                         const string& port, size_t idx) {
    sc_object* child = find_child(parent, port);
    VCML_ERROR_ON(!child, "%s.%s does not exist", parent.name(), port.c_str());
    auto* sock = get_initiator_socket(child, idx);
    VCML_ERROR_ON(!sock, "%s is not a valid initiator socket", child->name());
    return *sock;
}

usb_base_target_socket& usb_target(const sc_object& parent,
                                   const string& port) {
    sc_object* child = find_child(parent, port);
    VCML_ERROR_ON(!child, "%s.%s does not exist", parent.name(), port.c_str());
    auto* sock = get_target_socket(child);
    VCML_ERROR_ON(!sock, "%s is not a valid target socket", child->name());
    return *sock;
}

usb_base_target_socket& usb_target(const sc_object& parent, const string& port,
                                   size_t idx) {
    sc_object* child = find_child(parent, port);
    VCML_ERROR_ON(!child, "%s.%s does not exist", parent.name(), port.c_str());
    auto* sock = get_target_socket(child, idx);
    VCML_ERROR_ON(!sock, "%s is not a valid target socket", child->name());
    return *sock;
}

void usb_stub(const sc_object& obj, const string& port) {
    sc_object* child = find_child(obj, port);
    VCML_ERROR_ON(!child, "%s.%s does not exist", obj.name(), port.c_str());

    auto* ini = get_initiator_socket(child);
    auto* tgt = get_target_socket(child);

    if (!ini && !tgt)
        VCML_ERROR("%s is not a valid usb socket", child->name());

    if (ini)
        ini->stub();
    if (tgt)
        tgt->stub();
}

void usb_stub(const sc_object& obj, const string& port, size_t idx) {
    sc_object* child = find_child(obj, port);
    VCML_ERROR_ON(!child, "%s.%s does not exist", obj.name(), port.c_str());

    auto* ini = get_initiator_socket(child, idx);
    auto* tgt = get_target_socket(child, idx);

    if (!ini && !tgt)
        VCML_ERROR("%s is not a valid usb socket", child->name());

    if (ini)
        ini->stub();
    if (tgt)
        tgt->stub();
}

void usb_bind(const sc_object& obj1, const string& port1,
              const sc_object& obj2, const string& port2) {
    auto* p1 = find_child(obj1, port1);
    auto* p2 = find_child(obj2, port2);

    VCML_ERROR_ON(!p1, "%s.%s does not exist", obj1.name(), port1.c_str());
    VCML_ERROR_ON(!p2, "%s.%s does not exist", obj2.name(), port2.c_str());

    auto* i1 = get_initiator_socket(p1);
    auto* i2 = get_initiator_socket(p2);
    auto* t1 = get_target_socket(p1);
    auto* t2 = get_target_socket(p2);

    VCML_ERROR_ON(!i1 && !t1, "%s is not a valid usb port", p1->name());
    VCML_ERROR_ON(!i2 && !t2, "%s is not a valid usb port", p2->name());

    if (i1 && i2)
        i1->bind(*i2);
    else if (i1 && t2)
        i1->bind(*t2);
    else if (t1 && i2)
        i2->bind(*t1);
    else if (t1 && t2)
        t1->bind(*t2);
}

void usb_bind(const sc_object& obj1, const string& port1,
              const sc_object& obj2, const string& port2, size_t idx2) {
    auto* p1 = find_child(obj1, port1);
    auto* p2 = find_child(obj2, port2);

    VCML_ERROR_ON(!p1, "%s.%s does not exist", obj1.name(), port1.c_str());
    VCML_ERROR_ON(!p2, "%s.%s does not exist", obj2.name(), port2.c_str());

    auto* i1 = get_initiator_socket(p1);
    auto* i2 = get_initiator_socket(p2, idx2);
    auto* t1 = get_target_socket(p1);
    auto* t2 = get_target_socket(p2, idx2);

    VCML_ERROR_ON(!i1 && !t1, "%s is not a valid usb port", p1->name());
    VCML_ERROR_ON(!i2 && !t2, "%s is not a valid usb port", p2->name());

    if (i1 && i2)
        i1->bind(*i2);
    else if (i1 && t2)
        i1->bind(*t2);
    else if (t1 && i2)
        i2->bind(*t1);
    else if (t1 && t2)
        t1->bind(*t2);
}

void usb_bind(const sc_object& obj1, const string& port1, size_t idx1,
              const sc_object& obj2, const string& port2) {
    auto* p1 = find_child(obj1, port1);
    auto* p2 = find_child(obj2, port2);

    VCML_ERROR_ON(!p1, "%s.%s does not exist", obj1.name(), port1.c_str());
    VCML_ERROR_ON(!p2, "%s.%s does not exist", obj2.name(), port2.c_str());

    auto* i1 = get_initiator_socket(p1, idx1);
    auto* i2 = get_initiator_socket(p2);
    auto* t1 = get_target_socket(p1, idx1);
    auto* t2 = get_target_socket(p2);

    VCML_ERROR_ON(!i1 && !t1, "%s is not a valid usb port", p1->name());
    VCML_ERROR_ON(!i2 && !t2, "%s is not a valid usb port", p2->name());

    if (i1 && i2)
        i1->bind(*i2);
    else if (i1 && t2)
        i1->bind(*t2);
    else if (t1 && i2)
        i2->bind(*t1);
    else if (t1 && t2)
        t1->bind(*t2);
}

void usb_bind(const sc_object& obj1, const string& port1, size_t idx1,
              const sc_object& obj2, const string& port2, size_t idx2) {
    auto* p1 = find_child(obj1, port1);
    auto* p2 = find_child(obj2, port2);

    VCML_ERROR_ON(!p1, "%s.%s does not exist", obj1.name(), port1.c_str());
    VCML_ERROR_ON(!p2, "%s.%s does not exist", obj2.name(), port2.c_str());

    auto* i1 = get_initiator_socket(p1, idx1);
    auto* i2 = get_initiator_socket(p2, idx2);
    auto* t1 = get_target_socket(p1, idx1);
    auto* t2 = get_target_socket(p2, idx2);

    VCML_ERROR_ON(!i1 && !t1, "%s is not a valid usb port", p1->name());
    VCML_ERROR_ON(!i2 && !t2, "%s is not a valid usb port", p2->name());

    if (i1 && i2)
        i1->bind(*i2);
    else if (i1 && t2)
        i1->bind(*t2);
    else if (t1 && i2)
        i2->bind(*t1);
    else if (t1 && t2)
        t1->bind(*t2);
}

} // namespace vcml
