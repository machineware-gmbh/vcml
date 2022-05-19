 ##############################################################################
 #                                                                            #
 # Copyright 2020 Jan Henrik Weinstock                                        #
 #                                                                            #
 # Licensed under the Apache License, Version 2.0 (the "License");            #
 # you may not use this file except in compliance with the License.           #
 # You may obtain a copy of the License at                                    #
 #                                                                            #
 #     http://www.apache.org/licenses/LICENSE-2.0                             #
 #                                                                            #
 # Unless required by applicable law or agreed to in writing, software        #
 # distributed under the License is distributed on an "AS IS" BASIS,          #
 # WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.   #
 # See the License for the specific language governing permissions and        #
 # limitations under the License.                                             #
 #                                                                            #
 ##############################################################################

macro(model_test test)
    add_executable(${test} ${test}.cpp)
    target_link_libraries(${test} testing)
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
model_test("arm_pl011")
model_test("arm_sp804")
model_test("arm_gic400")
model_test("riscv_clint")
model_test("riscv_plic")
model_test("meta_loader")
model_test("pci_device")
model_test("pcie_device")
model_test("virtio_rng")
model_test("virtio_input")
model_test("virtio_console")
model_test("virtio_pci")
