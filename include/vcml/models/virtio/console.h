/******************************************************************************
 *                                                                            *
 * Copyright (C) 2022 MachineWare GmbH                                        *
 * All Rights Reserved                                                        *
 *                                                                            *
 * This is work is licensed under the terms described in the LICENSE file     *
 * found in the root directory of this source tree.                           *
 *                                                                            *
 ******************************************************************************/

#ifndef VCML_VIRTIO_CONSOLE_H
#define VCML_VIRTIO_CONSOLE_H

#include "vcml/core/types.h"
#include "vcml/core/range.h"
#include "vcml/core/systemc.h"
#include "vcml/core/module.h"
#include "vcml/core/model.h"

#include "vcml/protocols/serial.h"
#include "vcml/protocols/virtio.h"

namespace vcml {
namespace virtio {

class console : public module, public virtio_device, public serial_host
{
private:
    enum virtqueues : int {
        VIRTQUEUE_DATA_RX = 0,
        VIRTQUEUE_DATA_TX = 1,
        VIRTQUEUE_CTRL_RX = 2,
        VIRTQUEUE_CTRL_TX = 3,
    };

    enum features : u64 {
        VIRTIO_CONSOLE_F_SIZE = 1ull << 0,
        VIRTIO_CONSOLE_F_MULTIPORT = 1ull << 1,
        VIRTIO_CONSOLE_F_EMERG_WRITE = 1ull << 2,
    };

    struct console_config {
        u16 cols;
        u16 rows;
        u32 max_nr_ports;
        u32 emerg_write;
    } m_config;

    queue<vq_message> m_fifo;

    // virtio_device
    virtual void identify(virtio_device_desc& desc) override;
    virtual bool notify(u32 vqid) override;

    virtual void read_features(u64& features) override;
    virtual bool write_features(u64 features) override;

    virtual bool read_config(const range& addr, void* ptr) override;
    virtual bool write_config(const range& addr, const void* ptr) override;

    // serial_host
    virtual void serial_receive(u8 data) override;

public:
    property<u16> cols;
    property<u16> rows;

    virtio_target_socket virtio_in;

    serial_initiator_socket serial_tx;
    serial_target_socket serial_rx;

    console(const sc_module_name& nm);
    virtual ~console();
    VCML_KIND(virtio::console);

    virtual void reset();
};

} // namespace virtio
} // namespace vcml

#endif
