 ##############################################################################
 #                                                                            #
 # Copyright (C) 2022 MachineWare GmbH                                        #
 # All Rights Reserved                                                        #
 #                                                                            #
 # This is work is licensed under the terms described in the LICENSE file     #
 # found in the root directory of this source tree.                           #
 #                                                                            #
 ##############################################################################

macro(model_test test)
    add_executable(${test} ${test}.cpp)
    target_link_libraries(${test} testing)
    target_compile_options(${test} PRIVATE ${MWR_COMPILER_WARN_FLAGS})
    set_target_properties(${test} PROPERTIES CXX_CLANG_TIDY "${VCML_LINTER}")
    add_test(NAME models/${test} COMMAND ${test} ${VCML_TEST_RESOURCES})
    set_tests_properties(models/${test} PROPERTIES ENVIRONMENT "{VCML_ENVVAR}")
    set_tests_properties(models/${test} PROPERTIES TIMEOUT 4)
endmacro()

model_test("generic_bus")
model_test("generic_memory")
model_test("generic_fbdev")
model_test("sdhci")
model_test("lan9118")
model_test("oci2c")
model_test("arm_gic400")
model_test("riscv_clint")
model_test("riscv_plic")
model_test("meta_loader")
model_test("spi_max31855")
model_test("spi_flash")
model_test("serial_nrf51")
model_test("serial_pl011")
model_test("timer_nrf51")
model_test("timer_sp804")
model_test("timer_pl031")
model_test("pci_device")
model_test("pcie_device")
model_test("virtio_rng")
model_test("virtio_input")
model_test("virtio_console")
model_test("virtio_pci")
model_test("virtio_blk")
