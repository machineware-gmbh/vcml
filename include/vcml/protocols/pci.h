/******************************************************************************
 *                                                                            *
 * Copyright (C) 2022 MachineWare GmbH                                        *
 * All Rights Reserved                                                        *
 *                                                                            *
 * This is work is licensed under the terms described in the LICENSE file     *
 * found in the root directory of this source tree.                           *
 *                                                                            *
 ******************************************************************************/

#ifndef VCML_PROTOCOLS_PCI_H
#define VCML_PROTOCOLS_PCI_H

#include "vcml/core/types.h"
#include "vcml/core/systemc.h"

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
    u64 data;
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
    PCI_BAR_IO = bit(0),
    PCI_BAR_64 = bit(2),
    PCI_BAR_PREFETCH = bit(3),
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
    u8* host;

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

enum pcicap_pm_caps : u16 {
    PCI_PM_CAP_VER_1_1 = 2 << 0,
    PCI_PM_CAP_VER_1_2 = 3 << 0,
    PCI_PM_CAP_PME_CLOCK = bit(3),
    PCI_PM_CAP_DSI = bit(5),
    PCI_PM_CAP_AUX_POWER = bitmask(3, 6),
    PCI_PM_CAP_CAP_D1 = bit(9),
    PCI_PM_CAP_CAP_D2 = bit(10),
    PCI_PM_CAP_DME_D0 = bit(11),
    PCI_PM_CAP_DME_D1 = bit(12),
    PCI_PM_CAP_DME_D2 = bit(13),
    PCI_PM_CAP_DME_D3H = bit(14),
    PCI_PM_CAP_DME_D3C = bit(15),
};

enum pcicap_pm_control : u32 {
    PCI_PM_CTRL_PSTATE_D0 = 0,
    PCI_PM_CTRL_PSTATE_D1 = 1,
    PCI_PM_CTRL_PSTATE_D2 = 2,
    PCI_PM_CTRL_PSTATE_D3H = 3,
    PCI_PM_CTRL_PME_ENABLE = bit(8),
    PCI_PM_CTRL_DATA_SEL = bitmask(4, 9),
    PCI_PM_CTRL_DATA_SCALE = bitmask(2, 13),
    PCI_PM_CTRL_PME = bit(15),
};

enum pcicap_msi_control : u16 {
    PCI_MSI_ENABLE = bit(0),
    PCI_MSI_QMASK = bitmask(3, 1),
    PCI_MSI_QMASK1 = 0 << 1,
    PCI_MSI_QMASK2 = 1 << 1,
    PCI_MSI_QMASK4 = 2 << 1,
    PCI_MSI_QMASK8 = 3 << 1,
    PCI_MSI_QMASK16 = 4 << 1,
    PCI_MSI_QMASK32 = 5 << 1,
    PCI_MSI_QSIZE = bitmask(3, 4),
    PCI_MSI_64BIT = bit(7),
    PCI_MSI_VECTOR = bit(8),
};

enum pcicap_msix_control : u16 {
    PCI_MSIX_ENABLE = bit(15),
    PCI_MSIX_ALL_MASKED = bit(14),
    PCI_MSIX_TABLE_SIZE_MASK = bitmask(11),
};

enum pcicap_msix_table : u32 {
    PCI_MSIX_MASKED = bit(0),
    PCI_MSIX_BIR_MASK = bitmask(3),
};

enum pcicap_exp_flags : u16 {
    PCI_EXP_V2 = 1,
    PCI_EXP_V3 = 2,
    PCI_EXP_V4 = 3,
    PCI_EXP_V5 = 4,
    PCI_EXP_V6 = 5,
    PCI_EXP_TYPE_ENDPOINT = 0x0 << 4,
    PCI_EXP_TYPE_LEG_END = 0x1 << 4,
    PCI_EXP_TYPE_ROOT_PORT = 0x4 << 4,
    PCI_EXP_TYPE_UPSTREAM = 0x5 << 4,
    PCI_EXP_TYPE_DOWNSTREAM = 0x6 << 4,
    PCI_EXP_TYPE_PCI_BRIDGE = 0x7 << 4,
    PCI_EXP_TYPE_PCIE_BRIDGE = 0x8 << 4,
    PCI_EXP_TYPE_RC_END = 0x9 << 4,
    PCI_EXP_TYPE_RC_EC = 0xa << 4,
    PCI_EXP_EXT_SLOT = bit(8),
};

enum pcicap_exp_devcap : u32 {
    PCI_EXP_DEVCAP_MAX_PAYLOAD_128 = 0,
    PCI_EXP_DEVCAP_MAX_PAYLOAD_256 = 1,
    PCI_EXP_DEVCAP_MAX_PAYLOAD_512 = 2,
    PCI_EXP_DEVCAP_MAX_PAYLOAD_1024 = 3,
    PCI_EXP_DEVCAP_MAX_PAYLOAD_2048 = 4,
    PCI_EXP_DEVCAP_MAX_PAYLOAD_4096 = 5,
    PCI_EXP_DEVCAP_PHANTOM_BITS = 3 << 3,
    PCI_EXP_DEVCAP_EXT_TAG = bit(5),
    PCI_EXP_DEVCAP_L0S_64NS = 0 << 6,
    PCI_EXP_DEVCAP_L0S_128NS = 1 << 6,
    PCI_EXP_DEVCAP_L0S_256NS = 2 << 6,
    PCI_EXP_DEVCAP_L0S_512NS = 3 << 6,
    PCI_EXP_DEVCAP_L0S_1US = 4 << 6,
    PCI_EXP_DEVCAP_L0S_2US = 5 << 6,
    PCI_EXP_DEVCAP_L0S_4US = 6 << 6,
    PCI_EXP_DEVCAP_L0S_UNLIMITED = 7 << 6,
    PCI_EXP_DEVCAP_L1_1US = 0 << 9,
    PCI_EXP_DEVCAP_L1_2US = 1 << 9,
    PCI_EXP_DEVCAP_L1_4US = 2 << 9,
    PCI_EXP_DEVCAP_L1_8US = 3 << 9,
    PCI_EXP_DEVCAP_L1_16US = 4 << 9,
    PCI_EXP_DEVCAP_L1_32US = 5 << 9,
    PCI_EXP_DEVCAP_L1_64US = 6 << 9,
    PCI_EXP_DEVCAP_L1_UNLIMITED = 7 << 9,
    PCI_EXP_DEVCAP_RBE = bit(15),
};

enum pcicap_exp_devcap2 : u32 {
    PCI_EXP_DEVCAP2_CTR_A = bit(0),
    PCI_EXP_DEVCAP2_CTR_B = bit(1),
    PCI_EXP_DEVCAP2_CTR_C = bit(2),
    PCI_EXP_DEVCAP2_CTR_D = bit(3),
    PCI_EXP_DEVCAP2_CTDS = bit(4),
    PCI_EXP_DEVCAP2_ARI = bit(5),
    PCI_EXP_DEVCAP2_ATOMIC_ROUTE = bit(6),
    PCI_EXP_DEVCAP2_ATOMIC_OP32 = bit(7),
    PCI_EXP_DEVCAP2_ATOMIC_OP64 = bit(8),
    PCI_EXP_DEVCAP2_ATOMIC_CAS128 = bit(9),
    PCI_EXP_DEVCAP2_LTR = bit(11),
    PCI_EXP_DEVCAP2_TPH = 1 << 12,
    PCI_EXP_DEVCAP2_ETPH = 3 << 12,
    PCI_EXP_DEVCAP2_EXT_FMT = bit(20),
    PCI_EXP_DEVCAP2_TLP_PREFIX = bit(21),
};

enum pcicap_exp_devctl : u16 {
    PCI_EXP_DEVCTL_CERE = bit(0),
    PCI_EXP_DEVCTL_NFERE = bit(1),
    PCI_EXP_DEVCTL_FERE = bit(2),
    PCI_EXP_DEVCTL_URRE = bit(3),
    PCI_EXP_DEVCTL_RELAX = bit(4),
    PCI_EXP_DEVCTL_MAX_PAYLOAD_128 = 0 << 5,
    PCI_EXP_DEVCTL_MAX_PAYLOAD_256 = 1 << 5,
    PCI_EXP_DEVCTL_MAX_PAYLOAD_512 = 2 << 5,
    PCI_EXP_DEVCTL_MAX_PAYLOAD_1024 = 3 << 5,
    PCI_EXP_DEVCTL_MAX_PAYLOAD_2048 = 4 << 5,
    PCI_EXP_DEVCTL_MAX_PAYLOAD_4096 = 5 << 5,
    PCI_EXP_DEVCTL_EXT_TAG = bit(8),
    PCI_EXP_DEVCTL_PHANTOM = bit(9),
    PCI_EXP_DEVCTL_AUX_PME = bit(10),
    PCI_EXP_DEVCTL_NO_SNOOP = bit(11),
    PCI_EXP_DEVCTL_MAX_READ_128 = 0 << 12,
    PCI_EXP_DEVCTL_MAX_READ_256 = 1 << 12,
    PCI_EXP_DEVCTL_MAX_READ_512 = 2 << 12,
    PCI_EXP_DEVCTL_MAX_READ_1024 = 3 << 12,
    PCI_EXP_DEVCTL_MAX_READ_2048 = 4 << 12,
    PCI_EXP_DEVCTL_MAX_READ_4096 = 5 << 12,
};

enum pcicap_exp_devctl2 : u16 {
    PCI_EXP_DEVCTL2_CTD = bit(4),
    PCI_EXP_DEVCTL2_ARI = bit(5),
    PCI_EXP_DEVCTL2_ATOMIC_RE = bit(6),
    PCI_EXP_DEVCTL2_ATOMIC_EB = bit(7),
    PCI_EXP_DEVCTL2_IDO_RE = bit(8),
    PCI_EXP_DEVCTL2_IDO_CE = bit(9),
    PCI_EXP_DEVCTL2_LTR = bit(10),
    PCI_EXP_DEVCTL2_E2E_PFXBLK = bit(15),
};

enum pcicap_exp_devsts : u16 {
    PCI_EXP_DEVSTS_CED = bit(0),
    PCI_EXP_DEVSTS_NFED = bit(1),
    PCI_EXP_DEVSTS_FED = bit(2),
    PCI_EXP_DEVSTS_URD = bit(3),
    PCI_EXP_DEVSTS_RW1C = PCI_EXP_DEVSTS_CED | PCI_EXP_DEVSTS_NFED |
                          PCI_EXP_DEVSTS_FED | PCI_EXP_DEVSTS_URD,
    PCI_EXP_DEVSTS_AUX_POWER = bit(4),
    PCI_EXP_DEVSTS_TX_PENDING = bit(5),
};

enum pcicap_exp_linkcap : u32 {
    PCI_EXP_LINKCAP_MLS_2_5G = 1 << 0,
    PCI_EXP_LINKCAP_MLS_5G = 2 << 0,
    PCI_EXP_LINKCAP_MLS_8G = 3 << 0,
    PCI_EXP_LINKCAP_MLS_16G = 4 << 0,
    PCI_EXP_LINKCAP_MLS_32G = 5 << 0,
    PCI_EXP_LINKCAP_MLS_64G = 6 << 0,
    PCI_EXP_LINKCAP_MLW_X1 = 1 << 4,
    PCI_EXP_LINKCAP_MLW_X2 = 2 << 4,
    PCI_EXP_LINKCAP_MLW_X4 = 4 << 4,
    PCI_EXP_LINKCAP_MLW_X8 = 8 << 4,
    PCI_EXP_LINKCAP_MLW_X16 = 16 << 4,
    PCI_EXP_LINKCAP_MLW_X32 = 32 << 4,
    PCI_EXP_LINKCAP_ASPM_NONE = 0 << 10,
    PCI_EXP_LINKCAP_ASPM_L0S = 1 << 10,
    PCI_EXP_LINKCAP_ASPM_L1 = 2 << 10,
    PCI_EXP_LINKCAP_ASPM_L0S_L1 = 3 << 10,
    PCI_EXP_LINKCAP_CLKPM = bit(18),
    PCI_EXP_LINKCAP_SDERC = bit(19),
    PCI_EXP_LINKCAP_DLLLARC = bit(20),
    PCI_EXP_LINKCAP_LBNC = bit(21),
    PCI_EXP_LINKCAP_ASPM_OC = bit(22),
    PCI_EXP_LINKCAP_PN_SHIFT = 24,
};

enum pcicap_exp_linkcap2 : u32 {
    PCI_EXP_LINKCAP2_SLS_2_5G = 1 << 1,
    PCI_EXP_LINKCAP2_SLS_5G = 2 << 1,
    PCI_EXP_LINKCAP2_SLS_8G = 3 << 1,
    PCI_EXP_LINKCAP2_SLS_16G = 4 << 1,
    PCI_EXP_LINKCAP2_SLS_32G = 5 << 1,
    PCI_EXP_LINKCAP2_SLS_64G = 6 << 1,
    PCI_EXP_LINKCAP2_CROSSLINK = bit(8),
};

enum pcicap_exp_linkctl : u16 {
    PCI_EXP_LINKCTL_ASPM_L0S = 1 << 0,
    PCI_EXP_LINKCTL_ASPM_L1 = 2 << 0,
    PCI_EXP_LINKCTL_RCB = bit(3),
    PCI_EXP_LINKCTL_LD = bit(4),
    PCI_EXP_LINKCTL_RL = bit(5),
    PCI_EXP_LINKCTL_CCC = bit(6),
    PCI_EXP_LINKCTL_ES = bit(7),
    PCI_EXP_LINKCTL_CLKREQ_EN = bit(8),
    PCI_EXP_LINKCTL_HAWD = bit(9),
    PCI_EXP_LINKCTL_LBMIE = bit(10),
    PCI_EXP_LINKCTL_LABIE = bit(11),
};

enum pcicap_exp_linkctl2 : u16 {
    PCI_EXP_LINKCTL2_TLS_2_5G = 1 << 0,
    PCI_EXP_LINKCTL2_TLS_5G = 2 << 0,
    PCI_EXP_LINKCTL2_TLS_8G = 3 << 0,
    PCI_EXP_LINKCTL2_TLS_16G = 4 << 0,
    PCI_EXP_LINKCTL2_TLS_32G = 5 << 0,
    PCI_EXP_LINKCTL2_TLS_64G = 6 << 0,
    PCI_EXP_LINKCTL2_ENTER_COMP = bit(4),
    PCI_EXP_LINKCTL2_HASD = bit(5),
    PCI_EXP_LINKCTL2_SD = bit(6),
    PCI_EXP_LINKCTL2_ENTER_MODCOMP = bit(10),
    PCI_EXP_LINKCTL2_COMP_SOS = bit(11),
};

enum pcicap_exp_linksts : u16 {
    PCI_EXP_LINKSTS_CLS_2_5G = 1 << 0,
    PCI_EXP_LINKSTS_CLS_5G = 2 << 0,
    PCI_EXP_LINKSTS_CLS_8G = 3 << 0,
    PCI_EXP_LINKSTS_CLS_16G = 4 << 0,
    PCI_EXP_LINKSTS_CLS_32G = 5 << 0,
    PCI_EXP_LINKSTS_CLS_64G = 6 << 0,
    PCI_EXP_LINKSTS_NLW_X1 = 1 << 4,
    PCI_EXP_LINKSTS_NLW_X2 = 2 << 4,
    PCI_EXP_LINKSTS_NLW_X4 = 4 << 4,
    PCI_EXP_LINKSTS_NLW_X8 = 8 << 4,
    PCI_EXP_LINKSTS_NLW_X16 = 16 << 4,
    PCI_EXP_LINKSTS_NLW_X32 = 32 << 4,
    PCI_EXP_LINKSTS_LT = bit(11),
    PCI_EXP_LINKSTS_SLC = bit(12),
    PCI_EXP_LINKSTS_DLLLA = bit(13),
    PCI_EXP_LINKSTS_LBMS = bit(14),
    PCI_EXP_LINKSTS_LABS = bit(15),
};

enum pcicap_exp_linksts2 : u16 {
    PCI_EXP_LINKSTS2_CDL = bit(0),
    PCI_EXP_LINKSTS2_EQC = bit(1),
    PCI_EXP_LINKSTS2_EP1S = bit(2),
    PCI_EXP_LINKSTS2_EP2S = bit(3),
    PCI_EXP_LINKSTS2_EP3S = bit(4),
    PCI_EXP_LINKSTS2_LER = bit(5),
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

using pci_base_initiator_array = socket_array<pci_base_initiator_socket>;
using pci_base_target_array = socket_array<pci_base_target_socket>;

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

using pci_initiator_array = socket_array<pci_initiator_socket>;
using pci_target_array = socket_array<pci_target_socket>;

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

void pci_stub(const sc_object& obj, const string& port);
void pci_stub(const sc_object& obj, const string& port, size_t idx);

void pci_bind(const sc_object& obj1, const string& port1,
              const sc_object& obj2, const string& port2);
void pci_bind(const sc_object& obj1, const string& port1,
              const sc_object& obj2, const string& port2, size_t idx2);
void pci_bind(const sc_object& obj1, const string& port1, size_t idx1,
              const sc_object& obj2, const string& port2);
void pci_bind(const sc_object& obj1, const string& port1, size_t idx1,
              const sc_object& obj2, const string& port2, size_t idx2);

} // namespace vcml

#endif
