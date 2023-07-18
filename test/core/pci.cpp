/******************************************************************************
 *                                                                            *
 * Copyright (C) 2022 MachineWare GmbH                                        *
 * All Rights Reserved                                                        *
 *                                                                            *
 * This is work is licensed under the terms described in the LICENSE file     *
 * found in the root directory of this source tree.                           *
 *                                                                            *
 ******************************************************************************/

#include "testing.h"

class pci_harness : public test_base, public pci_initiator, public pci_target
{
private:
    using pci_target::pci_bar_map;
    using pci_target::pci_bar_unmap;
    using pci_target::pci_dma_ptr;
    using pci_target::pci_dma_read;
    using pci_target::pci_dma_write;
    using pci_target::pci_interrupt;

    // PCI initiator interface
    MOCK_METHOD(void, pci_bar_map,
                (const pci_initiator_socket& socket, const pci_bar& bar));
    MOCK_METHOD(void, pci_bar_unmap,
                (const pci_initiator_socket& socket, int barno));
    MOCK_METHOD(void*, pci_dma_ptr,
                (const pci_initiator_socket& socket, vcml_access rw, u64 addr,
                 u64 size));
    MOCK_METHOD(bool, pci_dma_read,
                (const pci_initiator_socket& socket, u64 addr, u64 size,
                 void* data));
    MOCK_METHOD(bool, pci_dma_write,
                (const pci_initiator_socket& socket, u64 addr, u64 size,
                 const void* data));
    MOCK_METHOD(void, pci_interrupt,
                (const pci_initiator_socket& socket, pci_irq irq, bool state));

    // PCI target interface
    virtual void pci_transport(const pci_target_socket& socket,
                               pci_payload& pci) override {
        EXPECT_TRUE(pci.is_read());
        EXPECT_TRUE(pci.is_cfg());
        pci.data = pci_in.index_of(socket);
        pci.response = PCI_RESP_SUCCESS;
    }

public:
    pci_initiator_array pci_out;
    pci_target_array pci_in;

    pci_base_initiator_array pci_out_h;
    pci_base_target_array pci_in_h;

    pci_initiator_socket pci_out_nocon;
    pci_target_socket pci_in_nocon;

    pci_harness(const sc_module_name& nm):
        test_base(nm),
        pci_initiator(),
        pci_target(),
        pci_out("pci_out"),
        pci_in("pci_in"),
        pci_out_h("pci_out_h"),
        pci_in_h("pci_in_h"),
        pci_out_nocon("pci_out_nocon"),
        pci_in_nocon("pci_in_nocon") {
        for (int i = 0; i < 4; i++) {
            pci_bind(*this, "pci_out", i, *this, "pci_out_h", i);
            pci_bind(*this, "pci_in_h", i, *this, "pci_in", i);
            pci_bind(*this, "pci_out_h", i, *this, "pci_in_h", i);
        }

        pci_stub(*this, "pci_out_nocon");
        pci_stub(*this, "pci_in_nocon");

        EXPECT_TRUE(find_object("pci.pci_out_nocon_stub"));
        EXPECT_TRUE(find_object("pci.pci_in_nocon_stub"));
    }

    virtual void run_test() override {
        pci_payload pci{};
        pci.command = PCI_READ;
        pci.space = PCI_AS_CFG;
        pci.addr = 0x12345678;
        pci.data = 0xffffffff;
        pci.size = 4;

        for (auto& port : pci_out) {
            pci.response = PCI_RESP_INCOMPLETE;
            (*port.second)->pci_transport(pci);
            EXPECT_SUCCESS(pci);
            EXPECT_EQ(pci.data, (u32)port.first);
        }

        for (auto& port : pci_in) {
            u32 data = -1;
            EXPECT_CALL(*this, pci_dma_read(_, 0, sizeof(data), &data))
                .Times(1)
                .WillOnce(Return(true));
            EXPECT_TRUE((*port.second)->pci_dma_read(0, sizeof(data), &data));
        }
    }
};

TEST(pci, sockets) {
    pci_harness test("pci");
    sc_core::sc_start();
}
