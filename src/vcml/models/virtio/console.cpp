/******************************************************************************
 *                                                                            *
 * Copyright 2020 Jan Henrik Weinstock                                        *
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

#include "vcml/models/virtio/console.h"

namespace vcml { namespace virtio {

    size_t console::rx_data_available() {
        return serial_peek() ? 1u : 0u;
    }

    size_t console::rx_data(u8* data, size_t size) {
        return serial_in(*data) ? 1u : 0u;
    }

    size_t console::tx_data(const u8* data, size_t size) {
        for (size_t i = 0; i < size; i++)
            serial_out(data[i]);
        return size;
    }

    void console::poll() {
        size_t rxbytes = rx_data_available();
        while (rxbytes > 0 && !m_fifo.empty()) {
            vq_message msg(m_fifo.front());
            vector<u8> chars(msg.length_out());

            size_t nread = 0;
            while (rxbytes > 0 && nread < chars.size()) {
                nread += rx_data(chars.data() + nread,
                                 min(chars.size() - nread, rxbytes));
                rxbytes = rx_data_available();
            }

            msg.copy_out(chars);
            msg.trim(nread);

            if (VIRTIO_IN->put(VIRTQUEUE_DATA_RX, msg))
                m_fifo.pop();
        }

        sc_time quantum = tlm_global_quantum::instance().get();
        sc_time polldelay(1.0 / pollrate, SC_SEC);
        next_trigger(max(polldelay, quantum));
    }

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

        while (VIRTIO_IN->get(vqid, msg)) {
            count++;

            switch (vqid) {
            case VIRTQUEUE_DATA_TX: {
                vector<u8> chars(msg.length_in());
                msg.copy_in(chars);
                tx_data(chars.data(), chars.size());
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

            if (!VIRTIO_IN->put(vqid, msg))
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

        tx_data((const u8*)ptr, 1);
        return true;
    }

    console::console(const sc_module_name& nm):
        module(nm),
        virtio_device(),
        serial::port(),
        m_config(),
        cols("cols", 0),
        rows("rows", 0),
        pollrate("pollrate", 1000),
        VIRTIO_IN("VIRTIO_IN") {
        SC_HAS_PROCESS(console);
        SC_METHOD(poll);
    }

    console::~console() {
        // nothing to do
    }

    void console::reset() {
        m_config.cols = cols;
        m_config.rows = rows;
        m_config.max_nr_ports = 1;
    }

}}
