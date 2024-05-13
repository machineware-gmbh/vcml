/******************************************************************************
 *                                                                            *
 * Copyright (C) 2024 MachineWare GmbH                                        *
 * All Rights Reserved                                                        *
 *                                                                            *
 * This is work is licensed under the terms described in the LICENSE file     *
 * found in the root directory of this source tree.                           *
 *                                                                            *
 ******************************************************************************/

#ifndef VCML_PCI_ENDPOINT_H
#define VCML_PCI_ENDPOINT_H

#include "vcml/core/types.h"
#include "vcml/core/range.h"
#include "vcml/core/systemc.h"

#include "vcml/protocols/tlm.h"
#include "vcml/protocols/pci.h"
#include "vcml/protocols/gpio.h"

#include "vcml/models/pci/device.h"

namespace vcml {
namespace pci {

class endpoint : public device
{
public:
    pci_target_socket pci_in;

    gpio_target_array irq_in;
    tlm_target_socket dma_in;
    tlm_initiator_array bar_out;

    endpoint(const sc_module_name& name, const pci_config& config);
    virtual ~endpoint();
    VCML_KIND(pci::endpoint);
    virtual void reset() override;

protected:
    virtual unsigned int receive(tlm_generic_payload& tx, const tlm_sbi& info,
                                 address_space as) override;
    virtual void gpio_notify(const gpio_target_socket& s, bool state,
                             gpio_vector vector) override;

    virtual void end_of_elaboration() override;
};

} // namespace pci
} // namespace vcml

#endif
