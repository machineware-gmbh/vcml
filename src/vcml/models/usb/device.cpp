/******************************************************************************
 *                                                                            *
 * Copyright (C) 2024 MachineWare GmbH                                        *
 * All Rights Reserved                                                        *
 *                                                                            *
 * This is work is licensed under the terms described in the LICENSE file     *
 * found in the root directory of this source tree.                           *
 *                                                                            *
 ******************************************************************************/

#include "vcml/models/usb/device.h"

namespace vcml {
namespace usb {

enum usb_strids : u8 {
    STRID_LANGUAGE = 0,
    STRID_MANUFACTURER,
    STRID_PRODUCT,
    STRID_SERIALNO,
};

template <typename T>
void write_data(u8*& ptr, size_t& size, const T& data);

template <>
void write_data(u8*& ptr, size_t& size, const u8& val) {
    if (size > 0) {
        *ptr++ = val;
        size--;
    }
}

template <>
void write_data(u8*& ptr, size_t& size, const u16& val) {
    write_data<u8>(ptr, size, val);
    write_data<u8>(ptr, size, val >> 8);
}

template <>
void write_data(u8*& ptr, size_t& size, const endpoint_desc& desc) {
    write_data<u8>(ptr, size, desc.is_audio ? 9 : 7);
    write_data<u8>(ptr, size, USB_DT_ENDPOINT);
    write_data<u8>(ptr, size, desc.address);
    write_data<u8>(ptr, size, desc.attributes);
    write_data<u16>(ptr, size, desc.max_packet_size);
    write_data<u8>(ptr, size, desc.interval);

    if (desc.is_audio) {
        write_data<u8>(ptr, size, desc.refresh);
        write_data<u8>(ptr, size, desc.sync_address);
    }

    for (auto ch : desc.extra)
        write_data<u8>(ptr, size, ch);
}

static size_t total_length(const config_desc& desc) {
    size_t length = sizeof(usb_config_desc);
    for (auto& ifx : desc.interfaces) {
        length += sizeof(usb_interface_desc);
        length += ifx.extra.size();
        for (auto& ep : ifx.endpoints) {
            if (ep.is_audio)
                length += sizeof(usb_endpoint_desc);
            else
                length += sizeof(usb_endpoint_desc) - 2;
            length += ep.extra.size();
        }
    }

    return length;
}

static size_t count_interfaces(const vector<interface_desc>& v) {
    std::set<u8> ifaces;
    for (auto& desc : v)
        ifaces.insert(desc.interface_number);
    return ifaces.size();
}

template <>
void write_data(u8*& ptr, size_t& size, const config_desc& desc) {
    write_data<u8>(ptr, size, sizeof(usb_config_desc));
    write_data<u8>(ptr, size, USB_DT_CONFIG);
    write_data<u16>(ptr, size, total_length(desc));
    write_data<u8>(ptr, size, count_interfaces(desc.interfaces));
    write_data<u8>(ptr, size, desc.value);
    write_data<u8>(ptr, size, 0); // configuration name
    write_data<u8>(ptr, size, desc.attributes);
    write_data<u8>(ptr, size, desc.max_power);

    for (size_t i = 0; i < desc.interfaces.size(); i++) {
        write_data<u8>(ptr, size, sizeof(usb_interface_desc));
        write_data<u8>(ptr, size, USB_DT_INTERFACE);
        write_data<u8>(ptr, size, desc.interfaces[i].interface_number);
        write_data<u8>(ptr, size, desc.interfaces[i].alternate_setting);
        write_data<u8>(ptr, size, desc.interfaces[i].endpoints.size());
        write_data<u8>(ptr, size, desc.interfaces[i].ifxclass);
        write_data<u8>(ptr, size, desc.interfaces[i].subclass);
        write_data<u8>(ptr, size, desc.interfaces[i].protocol);
        write_data<u8>(ptr, size, desc.interfaces[i].iinterface);

        for (auto& ch : desc.interfaces[i].extra)
            write_data(ptr, size, ch);

        for (auto& ep : desc.interfaces[i].endpoints)
            write_data(ptr, size, ep);
    }
}

template <>
void write_data(u8*& ptr, size_t& size, const device_desc& desc) {
    write_data<u8>(ptr, size, sizeof(usb_device_desc));
    write_data<u8>(ptr, size, USB_DT_DEVICE);
    write_data<u16>(ptr, size, desc.bcd_usb);
    write_data<u8>(ptr, size, desc.device_class);
    write_data<u8>(ptr, size, desc.device_subclass);
    write_data<u8>(ptr, size, desc.device_protocol);
    write_data<u8>(ptr, size, desc.max_packet_size0);
    write_data<u16>(ptr, size, desc.vendor_id);
    write_data<u16>(ptr, size, desc.product_id);
    write_data<u16>(ptr, size, desc.bcd_device);
    write_data<u8>(ptr, size,
                   desc.manufacturer.empty() ? 0 : STRID_MANUFACTURER);
    write_data<u8>(ptr, size, desc.product.empty() ? 0 : STRID_PRODUCT);
    write_data<u8>(ptr, size, desc.serial_number.empty() ? 0 : STRID_SERIALNO);
    write_data<u8>(ptr, size, desc.configs.size());
}

template <>
void write_data(u8*& ptr, size_t& size, const string& desc) {
    write_data<u8>(ptr, size, 2 + 2 * desc.length());
    write_data<u8>(ptr, size, USB_DT_STRING);
    for (auto& ch : desc)
        write_data<u16>(ptr, size, ch);
}

bool device::cmd_usb_attach(const vector<string>& args, ostream& os) {
    usb_speed speed = usb_speed_max(m_desc);
    if (speed == USB_SPEED_NONE) {
        os << "cannot get usb speed from device descriptor" << std::endl;
        return false;
    }

    vector<usb_target_socket*> sockets;
    if (args.empty()) {
        sockets = all_usb_sockets();
    } else {
        for (const string& pname : args) {
            auto uts = find_usb_socket(pname);
            if (uts == nullptr) {
                os << "usb socket not found: " << pname;
                return false;
            }

            sockets.push_back(uts);
        }
    }

    for (auto uts : sockets) {
        os << "attaching " << uts->name() << " (" << usb_speed_str(speed)
           << ")" << std::endl;
        uts->attach(speed);
    }

    return true;
}

bool device::cmd_usb_detach(const vector<string>& args, ostream& os) {
    vector<usb_target_socket*> sockets;
    if (args.empty()) {
        sockets = all_usb_sockets();
    } else {
        for (const string& pname : args) {
            auto uts = find_usb_socket(pname);
            if (uts == nullptr) {
                os << "usb socket not found: " << pname;
                return false;
            }

            sockets.push_back(uts);
        }
    }

    for (auto uts : sockets) {
        os << "detaching " << uts->name() << std::endl;
        uts->detach();
    }

    return true;
}

device::device(const sc_module_name& nm, const device_desc& desc):
    module(nm),
    usb_dev_if(),
    m_address(0),
    m_stalled(false),
    m_state(STATE_DEFAULT),
    m_ep0(),
    m_strtab(),
    start_attached("start_attached", true),
    m_desc(desc),
    m_config(nullptr),
    m_iface_altsettings() {
    memset(&m_ep0, 0, sizeof(m_ep0));

    define_string(STRID_MANUFACTURER, m_desc.manufacturer);
    define_string(STRID_PRODUCT, m_desc.product);
    define_string(STRID_SERIALNO, m_desc.serial_number);

    register_command("usb_attach", 0, &device::cmd_usb_attach,
                     "usb_attach [port] attach the given port to the host");
    register_command("usb_detach", 0, &device::cmd_usb_detach,
                     "usb_detach [port] detach the given port from the host");
}

device::~device() {
    // nothing to do
}

usb_result device::get_configuration(u8& data) {
    data = m_config ? m_config->value : 0;
    return USB_RESULT_SUCCESS;
}

usb_result device::set_configuration(u8 data) {
    if (!is_addressed())
        return USB_RESULT_STALL;

    if (data == 0) {
        m_config = nullptr;
        m_state = STATE_ADDRESSED;
        m_iface_altsettings.clear();
        return USB_RESULT_SUCCESS;
    }

    for (size_t i = 0; i < m_desc.configs.size(); i++) {
        if (m_desc.configs[i].value == data) {
            m_config = m_desc.configs.data() + i;
            m_iface_altsettings.clear();
            m_state = STATE_CONFIGURED;
            return switch_configuration(*m_config);
        }
    }

    return USB_RESULT_STALL;
}

usb_result device::get_interface(size_t idx, u8& data) {
    if (!is_configured())
        return USB_RESULT_STALL;

    if (!m_config)
        return USB_RESULT_STALL;

    if (idx >= m_config->interfaces.size())
        return USB_RESULT_STALL;

    data = m_iface_altsettings[idx];
    return USB_RESULT_SUCCESS;
}

usb_result device::set_interface(size_t idx, u16 altset) {
    if (!is_configured()) {
        log_warn("not configured");
        return USB_RESULT_STALL;
    }

    if (!m_config) {
        log_warn("no config");
        return USB_RESULT_STALL;
    }

    if (idx >= m_config->interfaces.size()) {
        log_warn("invalid index");
        return USB_RESULT_STALL;
    }

    for (const interface_desc& ifx : m_config->interfaces) {
        if (ifx.interface_number == idx && ifx.alternate_setting == altset) {
            m_iface_altsettings[idx] = altset;
            return switch_interface(idx, ifx);
        }
    }

    log_warn("invalid altsetting 0x%04hx for interface %zu", altset, idx);
    return USB_RESULT_STALL;
}

usb_result device::get_descriptor(u8 type, u8 idx, u8* data, size_t size) {
    m_ep0.ptr = m_ep0.buf;
    memset(m_ep0.buf, 0, sizeof(m_ep0.buf));

    switch (type) {
    case USB_DT_DEVICE: {
        write_data(data, size, m_desc);
        return USB_RESULT_SUCCESS;
    }

    case USB_DT_CONFIG: {
        if (idx >= m_desc.configs.size())
            return USB_RESULT_STALL;
        write_data(data, size, m_desc.configs[idx]);
        return USB_RESULT_SUCCESS;
    }

    case USB_DT_STRING: {
        if (idx == STRID_LANGUAGE) {
            write_data<u8>(data, size, 4);
            write_data<u8>(data, size, USB_DT_STRING);
            write_data<u16>(data, size, 0x0409); // en_US
            return USB_RESULT_SUCCESS;
        }

        string value = "???";
        auto it = m_strtab.find(idx);
        if (it != m_strtab.end())
            value = it->second;
        else
            log_warn("unknown string index: %hhu", idx);

        write_data(data, size, value);
        return USB_RESULT_SUCCESS;
    }

    case USB_DT_DEVICE_QUALIFIER: {
        write_data<u8>(data, size, 0xa);
        write_data<u8>(data, size, USB_DT_DEVICE_QUALIFIER);
        write_data<u16>(data, size, m_desc.bcd_usb);
        write_data<u8>(data, size, m_desc.device_class);
        write_data<u8>(data, size, m_desc.device_subclass);
        write_data<u8>(data, size, m_desc.device_protocol);
        write_data<u8>(data, size, m_desc.max_packet_size0);
        write_data<u8>(data, size, m_desc.configs.size());
        write_data<u8>(data, size, 0);
        return USB_RESULT_SUCCESS;
    };

    case USB_DT_BOS: {
        write_data<u8>(data, size, sizeof(usb_bos_desc));
        write_data<u8>(data, size, USB_DT_BOS);
        return USB_RESULT_SUCCESS;
    }

    case USB_DT_DEBUG:
        // silently fail for this one, libusb queries this often
        return USB_RESULT_STALL;

    default:
        log_error("unsupported descriptor: %s", usb_desc_str(type));
        return USB_RESULT_STALL;
    }
}

usb_result device::handle_control(u16 req, u16 val, u16 idx, u8* data,
                                  size_t length) {
    switch (req) {
    case USB_REQ_OUT | USB_REQ_DEVICE | USB_REQ_SET_ADDRESS: {
        log_debug("set_address(%hu)", val);
        m_address = val;
        m_state = STATE_ADDRESSED;
        return USB_RESULT_SUCCESS;
    }

    case USB_REQ_IN | USB_REQ_DEVICE | USB_REQ_GET_STATUS: {
        log_debug("get_device_status(%hu)", val);
        if (val != 0)
            return USB_RESULT_STALL;

        data[0] = 0;
        if (m_config && (m_config->attributes & USB_CFG_SELF_POWERED))
            data[0] |= bit(0);
        if (m_config && (m_config->attributes & USB_CFG_REMOTE_WAKEUP))
            data[0] |= bit(1);
        data[1] = 0;
        return USB_RESULT_SUCCESS;
    }

    case USB_REQ_IN | USB_REQ_DEVICE | USB_REQ_GET_DESCRIPTOR: {
        u8 type = val >> 8;
        u8 index = val & 0xff;
        log_debug("get_descriptor(%s, %hhu)", usb_desc_str(type), index);
        return get_descriptor(type, index, data, length);
    }

    case USB_REQ_IN | USB_REQ_DEVICE | USB_REQ_GET_CONFIGURATION: {
        log_debug("get_configuration");
        return get_configuration(data[0]);
    }

    case USB_REQ_OUT | USB_REQ_DEVICE | USB_REQ_SET_CONFIGURATION: {
        log_debug("set_configuration(%u)", val & 0xff);
        return set_configuration(val & 0xff);
    }

    case USB_REQ_IN | USB_REQ_IFACE | USB_REQ_GET_INTERFACE: {
        log_debug("get_interface(%hu)", idx);
        return get_interface(idx, data[0]);
    }

    case USB_REQ_OUT | USB_REQ_IFACE | USB_REQ_SET_INTERFACE: {
        log_debug("set_interface(%hu, %hu)", idx, val);
        return set_interface(idx, val);
    }

    case USB_REQ_OUT | USB_REQ_DEVICE | USB_REQ_SET_ISOCH_DELAY: {
        log_debug("set isoc delay %hu", val);
        return USB_RESULT_SUCCESS;
    }

    default:
        break;
    }

    const char* inout = req & USB_REQ_IN ? "rd" : "wr";
    const char* vendor = req & USB_REQ_VENDOR ? "vendor " : "";
    const char* clazz = req & USB_REQ_CLASS ? "class " : "";
    const char* target = req & USB_REQ_ENDPOINT ? "endpoint "
                         : req & USB_REQ_IFACE  ? "iface "
                                                : "device ";
    log_error(
        "unknown %s%s%s%s request 0x%04hx val:0x%04hx idx:0x%04hx %zu bytes",
        vendor, clazz, target, inout, req, val, idx, length);

    return USB_RESULT_STALL;
}

usb_result device::handle_ep0(usb_packet& p) {
    if (m_stalled && p.token != USB_TOKEN_SETUP)
        return USB_RESULT_STALL;

    switch (p.token) {
    case USB_TOKEN_SETUP: {
        if (p.length != 8)
            return USB_RESULT_STALL;

        m_stalled = false;
        m_ep0.req = p.data[1] | (u16)p.data[0] << 8;
        m_ep0.val = p.data[2] | (u16)p.data[3] << 8;
        m_ep0.idx = p.data[4] | (u16)p.data[5] << 8;
        m_ep0.len = p.data[6] | (u16)p.data[7] << 8;
        m_ep0.pos = 0;

        if ((size_t)m_ep0.len > sizeof(m_ep0.buf)) {
            log_warn("control request too big: %hu bytes", m_ep0.len);
            return USB_RESULT_BABBLE;
        }

        usb_result res = USB_RESULT_SUCCESS;
        if (m_ep0.req & USB_REQ_IN) {
            res = handle_control(m_ep0.req, m_ep0.val, m_ep0.idx, m_ep0.buf,
                                 m_ep0.len);
        }

        if (failed(res))
            return res;

        m_ep0.state = m_ep0.len ? STATE_DATA : STATE_STATUS;
        return USB_RESULT_SUCCESS;
    }

    case USB_TOKEN_IN: {
        switch (m_ep0.state) {
        case STATE_DATA: {
            if (!(m_ep0.req & USB_REQ_IN))
                return USB_RESULT_STALL;

            size_t length = min<size_t>(p.length, m_ep0.len - m_ep0.pos);
            memcpy(p.data, m_ep0.buf + m_ep0.pos, length);
            m_ep0.pos += length;

            if (m_ep0.pos >= m_ep0.len)
                m_ep0.state = STATE_STATUS;
            return USB_RESULT_SUCCESS;
        }

        case STATE_STATUS: {
            usb_result res = USB_RESULT_SUCCESS;
            if (!(m_ep0.req & USB_REQ_IN)) {
                res = handle_control(m_ep0.req, m_ep0.val, m_ep0.idx,
                                     m_ep0.buf, m_ep0.len);
            }

            m_ep0.state = STATE_SETUP;
            return res;
        }

        default:
            log_error("received input token without prior setup");
            return USB_RESULT_SUCCESS;
        }
    }

    case USB_TOKEN_OUT: {
        switch (m_ep0.state) {
        case STATE_DATA: {
            if (m_ep0.req & USB_REQ_IN)
                return USB_RESULT_STALL;

            size_t length = min<size_t>(p.length, m_ep0.len - m_ep0.pos);
            memcpy(m_ep0.buf + m_ep0.pos, p.data, length);
            m_ep0.pos += length;

            if (m_ep0.pos >= m_ep0.len)
                m_ep0.state = STATE_STATUS;
            return USB_RESULT_SUCCESS;
        }

        case STATE_STATUS: {
            m_ep0.state = STATE_SETUP;
            return USB_RESULT_SUCCESS;
        }

        default:
            log_error("USB_TOKEN_OUT: not in data state!");
            return USB_RESULT_SUCCESS;
        }
    }

    default:
        return USB_RESULT_NODEV;
    }
}

bool device::altset_active(u32 ifx, u32 altset) const {
    auto it = m_iface_altsettings.find(ifx);
    if (it == m_iface_altsettings.end())
        return false;
    return it->second == altset;
}

void device::define_string(u8 idx, const string& str) {
    VCML_ERROR_ON(!idx, "invalid string index: %hhu", idx);

    auto it = m_strtab.find(idx);
    if (it != m_strtab.end())
        VCML_ERROR("string index %hhu already in use", idx);

    m_strtab[idx] = str;
}

void device::start_of_simulation() {
    module::start_of_simulation();

    usb_speed speed = usb_speed_max(m_desc);
    if (start_attached && speed > USB_SPEED_NONE) {
        for (auto* uts : all_usb_sockets())
            uts->attach(speed);
    }
}

static void collect_sockets(sc_module* obj, vector<usb_target_socket*>& s) {
    for (sc_object* obj : obj->get_child_objects()) {
        if (usb_target_socket* uts = dynamic_cast<usb_target_socket*>(obj))
            stl_add_unique(s, uts);

        if (auto* arr = dynamic_cast<usb_target_array<>*>(obj)) {
            for (auto [idx, uts] : *arr)
                stl_add_unique(s, uts);
        }

        if (sc_module* mod = dynamic_cast<sc_module*>(obj))
            collect_sockets(mod, s);
    }
}

vector<usb_target_socket*> device::all_usb_sockets() {
    vector<usb_target_socket*> all;
    collect_sockets(this, all);
    return all;
}

usb_target_socket* device::find_usb_socket(const string& name) {
    sc_object* obj = find_child(name);
    return dynamic_cast<usb_target_socket*>(obj);
}

usb_target_socket* device::find_usb_socket(const string& name, size_t idx) {
    sc_object* obj = find_child(name);
    auto* arr = dynamic_cast<usb_target_array<>*>(obj);
    return arr && arr->exists(idx) ? &arr->get(idx) : nullptr;
}

usb_result device::get_data(u32 ep, u8* data, size_t len) {
    if (len == 0)
        return USB_RESULT_SUCCESS;

    log_warn("unsupported data read request for ep%u", ep);
    return USB_RESULT_STALL;
}

usb_result device::set_data(u32 ep, const u8* data, size_t len) {
    if (len == 0)
        return USB_RESULT_SUCCESS;

    log_warn("unsupported data write request for ep%u", ep);
    return USB_RESULT_STALL;
}

usb_result device::switch_configuration(const config_desc& newcfg) {
    log_debug("switched to configuration %u", newcfg.value);
    return USB_RESULT_SUCCESS;
}

usb_result device::switch_interface(size_t idx, const interface_desc& ifx) {
    log_debug("interface %zu switched to setting %hhu", idx,
              ifx.alternate_setting);
    return USB_RESULT_SUCCESS;
}

usb_result device::handle_data(usb_packet& p) {
    switch (p.token) {
    case USB_TOKEN_IN:
        return get_data(p.epno, p.data, p.length);

    case USB_TOKEN_OUT:
        return set_data(p.epno, p.data, p.length);

    default:
        log_error("invalid usb data token received");
        return USB_RESULT_STALL;
    }
}

void device::usb_reset_device() {
    log_debug("usb reset device");
    usb_reset_endpoint(0);
}

void device::usb_reset_endpoint(int ep) {
    log_debug("usb reset endpoint %d", ep);
    switch (ep) {
    case 0:
        m_ep0.req = 0;
        m_ep0.state = STATE_SETUP;
        m_state = STATE_DEFAULT;
        break;
    }
}

void device::usb_transport(usb_packet& p) {
    if (is_addressed() && p.addr != m_address) {
        p.result = USB_RESULT_NODEV;
        return;
    }

    if (p.epno == 0)
        p.result = handle_ep0(p);
    else
        p.result = handle_data(p);

    if (failed(p)) {
        m_ep0.state = STATE_SETUP;
        m_stalled = true;
    }
}

} // namespace usb
} // namespace vcml
