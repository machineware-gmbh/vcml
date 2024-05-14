/******************************************************************************
 *                                                                            *
 * Copyright (C) 2024 MachineWare GmbH                                        *
 * All Rights Reserved                                                        *
 *                                                                            *
 * This is work is licensed under the terms described in the LICENSE file     *
 * found in the root directory of this source tree.                           *
 *                                                                            *
 ******************************************************************************/

#include "vcml/models/pci/endpoint.h"

namespace vcml {
namespace pci {

enum pci_endpoint_address_space : address_space {
    PCI_AS_ENDPOINT_DMA = PCI_AS_BAR5 + 1,
};

endpoint::endpoint(const sc_module_name& nm, const pci_config& config):
    device(nm, config),
    pci_in("pci_in"),
    irq_in("irq_in"),
    dma_in("dma_in", PCI_AS_ENDPOINT_DMA),
    bar_out("bar_out") {
}

endpoint::~endpoint() {
    // nothing to do
}

void endpoint::reset() {
    device::reset();
}

unsigned int endpoint::receive(tlm_generic_payload& tx, const tlm_sbi& info,
                               address_space as) {
    switch (as) {
    case PCI_AS_ENDPOINT_DMA: {
        bool ok;
        if (tx.is_read())
            ok = pci_in->pci_dma_read(tx.get_address(), tx.get_data_length(),
                                      tx.get_data_ptr());
        else
            ok = pci_in->pci_dma_write(tx.get_address(), tx.get_data_length(),
                                       tx.get_data_ptr());

        if (ok)
            tx.set_response_status(TLM_OK_RESPONSE);
        else
            tx.set_response_status(TLM_ADDRESS_ERROR_RESPONSE);

        return ok ? tx.get_data_length() : 0u;
    }

    case PCI_AS_BAR0:
    case PCI_AS_BAR1:
    case PCI_AS_BAR2:
    case PCI_AS_BAR3:
    case PCI_AS_BAR4:
    case PCI_AS_BAR5:
        return bar_out[as - PCI_AS_BAR0].send(tx, info);

    default:
        return device::receive(tx, info, as);
    }
}

void endpoint::gpio_notify(const gpio_target_socket& socket, bool state,
                           gpio_vector vector) {
    if (vector == GPIO_NO_VECTOR)
        vector = irq_in.index_of(socket);
    pci_interrupt(state, vector);
}

void endpoint::end_of_elaboration() {
    device::end_of_elaboration();

    for (size_t idx = 0; idx < PCI_NUM_BARS; idx++) {
        if (m_bars[idx].size == 0 && bar_out.exists(idx))
            log_error("BAR%zu connected but not declared", idx);
        if (m_bars[idx].size > 0 && !bar_out.exists(idx))
            log_error("BAR%zu declared but not connected", idx);
    }
}

} // namespace pci
} // namespace vcml
