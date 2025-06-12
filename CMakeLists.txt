# SPDX-License-Identifier: MIT

cmake_minimum_required(VERSION 3.13)

set(PROJECT pico-hci-uart)
set(PICO_BOARD pico_w)
set(FAMILY pico_w)

include($ENV{PICO_SDK_PATH}/external/pico_sdk_import.cmake)

project(${PROJECT} C CXX ASM)

pico_sdk_init()
add_compile_definitions(CYW43_ENABLE_BLUETOOTH=1)
# Set this to 0 to disable debug logging on the 2nd CDC interface
add_compile_definitions(HCI_DEBUG_LOG=1)

add_executable(${PROJECT} main.c usb-descriptors.c)

target_include_directories(${PROJECT} PUBLIC ${CMAKE_CURRENT_LIST_DIR})
target_link_libraries(${PROJECT}
	hardware_flash
	pico_multicore
	pico_stdlib
	tinyusb_device
	tinyusb_board
	pico_cyw43_arch_none
	pico_btstack_cyw43
	pico_btstack_ble)

pico_add_extra_outputs(${PROJECT})
