/******************************************************************************
 *                                                                            *
 * Copyright (C) 2022 MachineWare GmbH                                        *
 * All Rights Reserved                                                        *
 *                                                                            *
 * This is work is licensed under the terms described in the LICENSE file     *
 * found in the root directory of this source tree.                           *
 *                                                                            *
 ******************************************************************************/

#include "vcml/models/virtio/console.h"

namespace vcml {
namespace virtio {

void console::identify(virtio_device_desc& desc) {
    reset();
    desc.device_id = VIRTIO_DEVICE_CONSOLE;
    desc.vendor_id = VIRTIO_VENDOR_VCML;
    desc.pci_class = PCI_CLASS_COMM_OTHER;

    desc.request_virtqueue(VIRTQUEUE_DATA_RX, 32);
    desc.request_virtqueue(VIRTQUEUE_DATA_TX, 32);
    desc.request_virtqueue(VIRTQUEUE_CTRL_RX, 8);
    desc.request_virtqueue(VIRTQUEUE_CTRL_TX, 8);
}

bool console::notify(u32 vqid) {
    vq_message msg;
    int count = 0;

    while (virtio_in->get(vqid, msg)) {
        count++;

        switch (vqid) {
        case VIRTQUEUE_DATA_TX: {
            vector<u8> chars(msg.length_in());
            msg.copy_in(chars);
            for (u8 data : chars)
                serial_tx.send(data);
            break;
        }

        case VIRTQUEUE_DATA_RX: {
            m_fifo.push(msg);
            continue;
        }

        default:
            log_warn("ignoring message in unexpected virtqueue %u", vqid);
            break;
        }

        if (!virtio_in->put(vqid, msg))
            return false;
    }

    if (!count)
        log_warn("notify without messages");

    return true;
}

void console::read_features(u64& features) {
    features |= VIRTIO_CONSOLE_F_EMERG_WRITE;
    features &= ~VIRTIO_CONSOLE_F_MULTIPORT;

    if (rows != 0 && cols != 0)
        features |= VIRTIO_CONSOLE_F_SIZE;
}

bool console::write_features(u64 features) {
    if (features & VIRTIO_CONSOLE_F_MULTIPORT)
        return false;
    if ((features & VIRTIO_CONSOLE_F_SIZE) && (rows == 0 || cols == 0))
        return false;
    return true;
}

bool console::read_config(const range& addr, void* ptr) {
    if (addr.end >= sizeof(m_config))
        return false;

    memcpy(ptr, (u8*)&m_config + addr.start, addr.length());
    return true;
}

bool console::write_config(const range& addr, const void* ptr) {
    if (addr.start != offsetof(console_config, emerg_write))
        return false;

    if (addr.length() != sizeof(m_config.emerg_write))
        return false;

    serial_tx.send(*(const u8*)ptr);
    return true;
}

void console::serial_receive(u8 data) {
    vq_message msg(m_fifo.front());
    msg.copy_out(data);
    msg.trim(1);

    if (virtio_in->put(VIRTQUEUE_DATA_RX, msg))
        m_fifo.pop();
}

console::console(const sc_module_name& nm):
    module(nm),
    virtio_device(),
    serial_host(),
    m_config(),
    cols("cols", 0),
    rows("rows", 0),
    virtio_in("virtio_in"),
    serial_tx("serial_tx"),
    serial_rx("serial_rx") {
    // nothing to do
}

console::~console() {
    // nothing to do
}

void console::reset() {
    m_config.cols = cols;
    m_config.rows = rows;
    m_config.max_nr_ports = 1;
}

VCML_EXPORT_MODEL(vcml::virtio::console, name, args) {
    return new console(name);
}

} // namespace virtio
} // namespace vcml
