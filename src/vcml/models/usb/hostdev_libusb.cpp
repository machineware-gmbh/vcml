/******************************************************************************
 *                                                                            *
 * Copyright (C) 2024 MachineWare GmbH                                        *
 * All Rights Reserved                                                        *
 *                                                                            *
 * This is work is licensed under the terms described in the LICENSE file     *
 * found in the root directory of this source tree.                           *
 *                                                                            *
 ******************************************************************************/

#include "vcml/models/usb/hostdev.h"

#include <libusb.h>

#if LIBUSB_API_VERSION < 0x0100010008
#define libusb_strerror(err) libusb_strerror((libusb_error)(err)) // NOLINT
#endif

namespace vcml {
namespace usb {

struct libusb {
    libusb_context* context;
    libusb(): context(nullptr) {
        int r = libusb_init(&context);
        VCML_ERROR_ON(r < 0, "failed to initialize libusb");
    }

    ~libusb() {
        if (context)
            libusb_exit(context);
    }

    static libusb& instance() {
        static libusb singleton;
        return singleton;
    }

    libusb_device* find_device(u32 bus, u32 addr) {
        libusb_device* found = nullptr;
        libusb_device** list = nullptr;
        size_t n = libusb_get_device_list(context, &list);
        for (size_t i = 0; i < n; i++) {
            if (libusb_get_bus_number(list[i]) == bus &&
                libusb_get_device_address(list[i]) == addr) {
                found = libusb_ref_device(list[i]);
                break;
            }
        }

        libusb_free_device_list(list, true);
        return found;
    }
};

constexpr usb_result usb_result_from_libusb(int r) {
    switch (r) {
    case LIBUSB_SUCCESS:
        return USB_RESULT_SUCCESS;
    case LIBUSB_ERROR_NO_DEVICE:
        return USB_RESULT_NODEV;
    case LIBUSB_ERROR_TIMEOUT:
        return USB_RESULT_STALL;
    case LIBUSB_ERROR_OVERFLOW:
        return USB_RESULT_BABBLE;
    case LIBUSB_ERROR_IO:
    default:
        return USB_RESULT_IOERROR;
    }
}

static string libusb_str_desc(libusb_device_handle* h, u16 idx) {
    unsigned char buffer[256]{};
    libusb_get_string_descriptor_ascii(h, idx, buffer, sizeof(buffer));
    return string((char*)buffer);
}

void hostdev::init_device() {
    int r = libusb_open(m_device, &m_handle);
    VCML_ERROR_ON(r, "Failed to open USB device: %s", libusb_strerror(r));

    libusb_device_descriptor desc;
    r = libusb_get_device_descriptor(m_device, &desc);
    VCML_ERROR_ON(r, "Failed to fetch USB descriptor: %s", libusb_strerror(r));

    m_desc.bcd_usb = desc.bcdUSB;
    m_desc.device_class = desc.bDeviceClass;
    m_desc.device_subclass = desc.bDeviceSubClass;
    m_desc.device_protocol = desc.bDeviceProtocol;
    m_desc.max_packet_size0 = desc.bMaxPacketSize0;
    m_desc.vendor_id = desc.idVendor;
    m_desc.product_id = desc.idProduct;
    m_desc.bcd_device = desc.bcdDevice;
    m_desc.manufacturer = libusb_str_desc(m_handle, desc.iManufacturer);
    m_desc.product = libusb_str_desc(m_handle, desc.iProduct);
    m_desc.serial_number = libusb_str_desc(m_handle, desc.iSerialNumber);

    for (size_t i = 0; i < MWR_ARRAY_SIZE(m_ifs); i++) {
        int r = libusb_kernel_driver_active(m_handle, i);
        if (r > 0) {
            log_debug("detaching kernel driver from interface %zu", i);
            r = libusb_detach_kernel_driver(m_handle, i);
            VCML_ERROR_ON(r < 0, "libusb_detach_kernel_driver: %s",
                          libusb_strerror(r));
            m_ifs[i].detached = true;
        }
    }

    log_debug("attached to device %04hx:%04hx (%s)", m_desc.vendor_id,
              m_desc.product_id, m_desc.product.c_str());
}

usb_result hostdev::set_config(int config) {
    if (!m_handle)
        return USB_RESULT_NODEV;

    for (size_t i = 0; i < MWR_ARRAY_SIZE(m_ifs); i++) {
        if (m_ifs[i].claimed) {
            libusb_release_interface(m_handle, i);
            log_debug("released interface %zu", i);
        }

        m_ifs[i].claimed = false;
    }

    int r = libusb_set_configuration(m_handle, config);
    VCML_ERROR_ON(r < 0, "libusb_set_config: %s", libusb_strerror(r));

    for (size_t i = 0; i < MWR_ARRAY_SIZE(m_ifs); i++) {
        if (libusb_claim_interface(m_handle, i) == LIBUSB_SUCCESS) {
            log_debug("claimed interface %zu", i);
            m_ifs[i].claimed = true;
        }
    }

    return USB_RESULT_SUCCESS;
}

hostdev::hostdev(const sc_module_name& nm, u32 bus, u32 addr):
    device(nm, device_desc()),
    m_device(),
    m_handle(),
    m_ifs(),
    hostbus("hostbus", bus),
    hostaddr("hostaddr", addr),
    usb_in("usb_in") {
    memset(m_ifs, 0, sizeof(m_ifs));
    if (hostbus > 0u && hostaddr > 0u) {
        m_device = libusb::instance().find_device(hostbus, hostaddr);
        if (!m_device) {
            log_error("no USB device on bus %u at address %u", hostbus.get(),
                      hostaddr.get());
        }
    }

    if (m_device)
        init_device();
}

hostdev::~hostdev() {
    for (size_t i = 0; i < MWR_ARRAY_SIZE(m_ifs); i++) {
        if (m_ifs[i].claimed) {
            log_debug("releasing interface %zu", i);
            libusb_release_interface(m_handle, i);
        }

        if (m_ifs[i].detached) {
            log_debug("re-attaching kernel driver to interface %zu", i);
            libusb_attach_kernel_driver(m_handle, i);
        }
    }

    if (m_handle)
        libusb_close(m_handle);
    if (m_device)
        libusb_unref_device(m_device);
}

usb_result hostdev::get_data(u32 ep, u8* data, size_t len) {
    if (!m_handle)
        return USB_RESULT_NODEV;

    int r = libusb_bulk_transfer(m_handle, usb_ep_in(ep), data, len, NULL, 0);
    return usb_result_from_libusb(r);
}

usb_result hostdev::set_data(u32 ep, const u8* data, size_t len) {
    if (!m_handle)
        return USB_RESULT_NODEV;

    u8* p = const_cast<u8*>(data);
    int r = libusb_bulk_transfer(m_handle, usb_ep_out(ep), p, len, NULL, 0);
    return usb_result_from_libusb(r);
}

usb_result hostdev::handle_control(u16 req, u16 val, u16 idx, u8* data,
                                   size_t length) {
    if (!m_handle)
        return USB_RESULT_NODEV;

    switch (req) {
    case USB_REQ_OUT | USB_REQ_DEVICE | USB_REQ_SET_ADDRESS:
        return device::handle_control(req, val, idx, data, length);
    case USB_REQ_OUT | USB_REQ_DEVICE | USB_REQ_SET_CONFIGURATION:
        return set_config(val & 0xff);
    }

    u16 t = req >> 8;
    u16 q = req & 0xff;
    int r = libusb_control_transfer(m_handle, t, q, val, idx, data, length, 0);
    if (r < 0)
        return usb_result_from_libusb(r);
    return USB_RESULT_SUCCESS;
}

void hostdev::usb_reset_device() {
    if (m_handle)
        libusb_reset_device(m_handle);

    device::usb_reset_device();
}

VCML_EXPORT_MODEL(vcml::usb::hostdev, name, args) {
    return new hostdev(name);
}

} // namespace usb
} // namespace vcml
