/******************************************************************************
 *                                                                            *
 * Copyright 2021 Jan Henrik Weinstock                                        *
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

#ifndef VCML_PROTOCOLS_PCI_H
#define VCML_PROTOCOLS_PCI_H

#include "vcml/common/types.h"
#include "vcml/common/range.h"
#include "vcml/common/bitops.h"
#include "vcml/common/systemc.h"
#include "vcml/module.h"

#include "vcml/protocols/base.h"
#include "vcml/protocols/pci_ids.h"

namespace vcml {

enum pci_address_space : address_space {
    PCI_AS_CFG = VCML_AS_DEFAULT,
    PCI_AS_MMIO,
    PCI_AS_IO,
    PCI_AS_BAR0,
    PCI_AS_BAR1,
    PCI_AS_BAR2,
    PCI_AS_BAR3,
    PCI_AS_BAR4,
    PCI_AS_BAR5,
};

enum pci_command {
    PCI_READ,
    PCI_WRITE,
};

enum pci_response {
    PCI_RESP_INCOMPLETE = 0,
    PCI_RESP_SUCCESS = 1,
    PCI_RESP_ADDRESS_ERROR = -1,
    PCI_RESP_COMMAND_ERROR = -2,
};

enum pci_irq {
    PCI_IRQ_NONE = 0,
    PCI_IRQ_A = 1,
    PCI_IRQ_B = 2,
    PCI_IRQ_C = 3,
    PCI_IRQ_D = 4,
};

const char* pci_address_space_str(pci_address_space as);
const char* pci_command_str(pci_command cmd);
const char* pci_response_str(pci_response resp);
const char* pci_irq_str(pci_irq irq);

constexpr bool success(pci_response resp) {
    return resp > 0;
}
constexpr bool failed(pci_response resp) {
    return resp < 0;
}

tlm_command pci_translate_command(pci_command cmd);
pci_command pci_translate_command(tlm_command cmd);

tlm_response_status pci_translate_response(pci_response resp);
pci_response pci_translate_response(tlm_response_status resp);

struct pci_payload {
    pci_command command;
    pci_response response;
    pci_address_space space;

    u64 addr;
    u32 data;
    u32 size;

    bool debug;

    bool is_read() const { return command == PCI_READ; }
    bool is_write() const { return command == PCI_WRITE; }
    bool is_cfg() const { return space == PCI_AS_CFG; }

    bool is_ok() const { return success(response); }
    bool is_error() const { return failed(response); }

    bool is_address_error() const {
        return response == PCI_RESP_ADDRESS_ERROR;
    }

    bool is_command_error() const {
        return response == PCI_RESP_COMMAND_ERROR;
    }
};

ostream& operator<<(ostream& os, const pci_payload& tx);

template <>
inline bool success(const pci_payload& tx) {
    return success(tx.response);
}

template <>
inline bool failed(const pci_payload& tx) {
    return failed(tx.response);
}

enum pci_bar_status : u32 {
    PCI_BAR_MMIO = 0,
    PCI_BAR_IO = 1 << 0,
    PCI_BAR_64 = 1 << 2,
    PCI_BAR_PREFETCH = 1 << 3,
    PCI_BAR_UNMAPPED = ~0u,
    PCI_NUM_BARS = 6,
};

struct pci_bar {
    int barno;
    bool is_io;
    bool is_64bit;
    bool is_prefetch;
    union {
        u64 addr;
        struct {
            u32 addr_lo;
            u32 addr_hi;
        };
    };
    u64 size;

    u64 mask() const { return ~(size - 1); }
    bool is_mapped() const { return (addr_lo ^ -1) & mask(); }
};

struct pci_config {
    bool pcie;
    u16 vendor_id;
    u16 device_id;
    u16 subvendor_id;
    u16 subsystem_id;
    u32 class_code;
    u8 latency_timer;
    u8 max_latency;
    u8 min_grant;
    pci_irq int_pin;
};

constexpr u8 pci_get_max_lat(const pci_config& cfg) {
    return cfg.pcie ? 0u : cfg.max_latency;
}

constexpr u8 pci_get_min_grant(const pci_config& cfg) {
    return cfg.pcie ? 0u : cfg.min_grant;
}

enum pci_cap_id : u8 {
    PCI_CAPABILITY_PM = 0x01,
    PCI_CAPABILITY_MSI = 0x05,
    PCI_CAPABILITY_VENDOR = 0x09,
    PCI_CAPABILITY_PCIE = 0x10,
    PCI_CAPABILITY_MSIX = 0x11,
};

enum pci_pm_caps : u16 {
    PCI_PM_CAP_VER_1_1 = 2 << 0,
    PCI_PM_CAP_VER_1_2 = 3 << 0,
    PCI_PM_CAP_PME_CLOCK = 1 << 3,
    PCI_PM_CAP_DSI = 1 << 5,
    PCI_PM_CAP_AUX_POWER = 7 << 6,
    PCI_PM_CAP_CAP_D1 = 1 << 9,
    PCI_PM_CAP_CAP_D2 = 1 << 10,
    PCI_PM_CAP_DME_D0 = 1 << 11,
    PCI_PM_CAP_DME_D1 = 1 << 12,
    PCI_PM_CAP_DME_D2 = 1 << 13,
    PCI_PM_CAP_DME_D3H = 1 << 14,
    PCI_PM_CAP_DME_D3C = 1 << 15,
};

enum pci_pm_control : u32 {
    PCI_PM_CTRL_PSTATE_D0 = 0,
    PCI_PM_CTRL_PSTATE_D1 = 1,
    PCI_PM_CTRL_PSTATE_D2 = 2,
    PCI_PM_CTRL_PSTATE_D3H = 3,
    PCI_PM_CTRL_PME_ENABLE = 1 << 8,
    PCI_PM_CTRL_DATA_SEL = 15 << 9,
    PCI_PM_CTRL_DATA_SCALE = 3 << 13,
    PCI_PM_CTRL_PME = 1 << 15,
};

enum pci_msi_control : u16 {
    PCI_MSI_ENABLE = 1 << 0,
    PCI_MSI_QMASK = 7 << 1,
    PCI_MSI_QMASK1 = 0 << 1,
    PCI_MSI_QMASK2 = 1 << 1,
    PCI_MSI_QMASK4 = 2 << 1,
    PCI_MSI_QMASK8 = 3 << 1,
    PCI_MSI_QMASK16 = 4 << 1,
    PCI_MSI_QMASK32 = 5 << 1,
    PCI_MSI_QSIZE = 7 << 4,
    PCI_MSI_64BIT = 1 << 7,
    PCI_MSI_VECTOR = 1 << 8,
};

enum pci_msix_control : u16 {
    PCI_MSIX_ENABLE = 1 << 15,
    PCI_MSIX_ALL_MASKED = 1 << 14,
    PCI_MSIX_TABLE_SIZE_MASK = bitmask(11),
};

enum pci_msix_table : u32 {
    PCI_MSIX_MASKED = 1 << 0,
    PCI_MSIX_BIR_MASK = bitmask(3),
};

class pci_initiator_socket;
class pci_target_socket;
class pci_initiator_stub;
class pci_target_stub;

class pci_initiator
{
public:
    friend class pci_initiator_socket;

    typedef vector<pci_initiator_socket*> pci_initiator_sockets;

    const pci_initiator_sockets& get_pci_initiator_sockets() const {
        return m_sockets;
    }

    pci_initiator() = default;
    virtual ~pci_initiator() = default;
    pci_initiator(pci_initiator&&) = delete;
    pci_initiator(const pci_initiator&) = delete;

    virtual void pci_bar_map(const pci_initiator_socket& socket,
                             const pci_bar& bar) = 0;

    virtual void pci_bar_unmap(const pci_initiator_socket& socket,
                               int barno) = 0;

    virtual void* pci_dma_ptr(const pci_initiator_socket& socket,
                              vcml_access rw, u64 addr, u64 size) = 0;

    virtual bool pci_dma_read(const pci_initiator_socket& socket, u64 addr,
                              u64 size, void* data) = 0;

    virtual bool pci_dma_write(const pci_initiator_socket& socket, u64 addr,
                               u64 size, const void* data) = 0;

    virtual void pci_interrupt(const pci_initiator_socket& socket, pci_irq irq,
                               bool state) = 0;

private:
    pci_initiator_sockets m_sockets;
};

class pci_target
{
public:
    friend class pci_target_socket;

    typedef vector<pci_target_socket*> pci_target_sockets;

    pci_target_sockets& get_pci_target_sockets() { return m_sockets; }
    const pci_target_sockets& get_pci_target_sockets() const {
        return m_sockets;
    }

    pci_target() = default;
    virtual ~pci_target() = default;
    pci_target(pci_target&&) = delete;
    pci_target(const pci_target&) = delete;

    virtual void pci_transport(const pci_target_socket&, pci_payload&) = 0;

protected:
    virtual void pci_bar_map(const pci_bar& bar);
    virtual void pci_bar_unmap(int barno);
    virtual void* pci_dma_ptr(vcml_access rw, u64 addr, u64 size);
    virtual bool pci_dma_read(u64 addr, u64 size, void* data);
    virtual bool pci_dma_write(u64 addr, u64 size, const void* data);
    virtual void pci_interrupt(pci_irq irq, bool state);

private:
    pci_target_sockets m_sockets;
};

class pci_fw_transport_if : public sc_core::sc_interface
{
public:
    typedef pci_payload protocol_types;

    virtual void pci_transport(pci_payload& tx) = 0;
};

class pci_bw_transport_if : public sc_core::sc_interface
{
public:
    typedef pci_payload protocol_types;

    virtual void pci_bar_map(const pci_bar& bar) = 0;
    virtual void pci_bar_unmap(int barno) = 0;
    virtual void* pci_dma_ptr(vcml_access rw, u64 addr, u64 size) = 0;
    virtual bool pci_dma_read(u64 addr, u64 size, void* data) = 0;
    virtual bool pci_dma_write(u64 addr, u64 size, const void* data) = 0;
    virtual void pci_interrupt(pci_irq irq, bool state) = 0;
};

typedef base_initiator_socket<pci_fw_transport_if, pci_bw_transport_if>
    pci_base_initiator_socket_b;

typedef base_target_socket<pci_fw_transport_if, pci_bw_transport_if>
    pci_base_target_socket_b;

class pci_base_initiator_socket : public pci_base_initiator_socket_b
{
private:
    pci_target_stub* m_stub;

public:
    pci_base_initiator_socket(const char* nm, address_space = VCML_AS_DEFAULT);
    virtual ~pci_base_initiator_socket();
    VCML_KIND(pci_base_initiator_socket);

    bool is_stubbed() const { return m_stub != nullptr; }
    void stub();
};

class pci_base_target_socket : public pci_base_target_socket_b
{
private:
    pci_initiator_stub* m_stub;

public:
    pci_base_target_socket(const char* nm, address_space = VCML_AS_DEFAULT);
    virtual ~pci_base_target_socket();
    VCML_KIND(pci_base_target_socket);

    bool is_stubbed() const { return m_stub != nullptr; }
    void stub();
};

template <const size_t MAX_SOCKETS = SIZE_MAX>
using pci_base_initiator_socket_array = socket_array<pci_base_initiator_socket,
                                                     MAX_SOCKETS>;

template <const size_t MAX_SOCKETS = SIZE_MAX>
using pci_base_target_socket_array = socket_array<pci_base_target_socket,
                                                  MAX_SOCKETS>;

class pci_initiator_socket : public pci_base_initiator_socket
{
private:
    pci_initiator* m_initiator;

    struct pci_bw_transport : pci_bw_transport_if {
        pci_initiator_socket* socket;
        pci_bw_transport(pci_initiator_socket* sock): socket(sock) {}

        virtual ~pci_bw_transport() = default;

        virtual void pci_bar_map(const pci_bar& bar) override {
            socket->m_initiator->pci_bar_map(*socket, bar);
        }

        virtual void pci_bar_unmap(int barno) override {
            socket->m_initiator->pci_bar_unmap(*socket, barno);
        }

        virtual void* pci_dma_ptr(vcml_access rw, u64 addr, u64 sz) override {
            return socket->m_initiator->pci_dma_ptr(*socket, rw, addr, sz);
        }

        virtual bool pci_dma_read(u64 addr, u64 sz, void* data) override {
            return socket->m_initiator->pci_dma_read(*socket, addr, sz, data);
        }

        virtual bool pci_dma_write(u64 addr, u64 sz, const void* p) override {
            return socket->m_initiator->pci_dma_write(*socket, addr, sz, p);
        }

        virtual void pci_interrupt(pci_irq irq, bool state) override {
            socket->m_initiator->pci_interrupt(*socket, irq, state);
        }
    } m_transport;

public:
    pci_initiator_socket(const char* nm, address_space as = VCML_AS_DEFAULT);
    virtual ~pci_initiator_socket();
    VCML_KIND(pci_initiator_socket);

    void transport(pci_payload& tx);
};

class pci_target_socket : public pci_base_target_socket
{
private:
    pci_target* m_target;

    struct pci_fw_transport : pci_fw_transport_if {
        pci_target_socket* socket;
        pci_fw_transport(pci_target_socket* s): socket(s) {}
        virtual ~pci_fw_transport() = default;
        virtual void pci_transport(pci_payload& tx) override {
            socket->trace_fw(tx);
            socket->m_target->pci_transport(*socket, tx);
            socket->trace_bw(tx);
        }
    } m_transport;

public:
    pci_target_socket(const char* nm, address_space as = VCML_AS_DEFAULT);
    virtual ~pci_target_socket();
    VCML_KIND(pci_target_socket);
};

constexpr size_t pci_devno(size_t dev, size_t fn = 0) {
    return (dev & 31u) << 3 | (fn & 7u);
}

template <const size_t MAX = SIZE_MAX>
using pci_initiator_socket_array = socket_array<pci_initiator_socket, MAX>;

template <const size_t MAX = SIZE_MAX>
using pci_target_socket_array = socket_array<pci_target_socket, MAX>;

class pci_initiator_stub : private pci_bw_transport_if
{
private:
    virtual void pci_bar_map(const pci_bar& bar) override;
    virtual void pci_bar_unmap(int barno) override;
    virtual void* pci_dma_ptr(vcml_access rw, u64 addr, u64 size) override;
    virtual bool pci_dma_read(u64 addr, u64 size, void* data) override;
    virtual bool pci_dma_write(u64 addr, u64 size, const void* data) override;
    virtual void pci_interrupt(pci_irq irq, bool state) override;

public:
    pci_base_initiator_socket pci_out;
    pci_initiator_stub(const char* name);
    virtual ~pci_initiator_stub() = default;
};

class pci_target_stub : private pci_fw_transport_if
{
private:
    virtual void pci_transport(pci_payload& tx) override;

public:
    pci_base_target_socket pci_in;
    pci_target_stub(const char* name);
    virtual ~pci_target_stub() = default;
};

} // namespace vcml

#endif
