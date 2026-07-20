cmake_minimum_required(VERSION 3.13)
# Build two ways:
#   1. Pico firmware:  cmake -B build -DPICO_SDK_PATH=... && cmake --build build
#   2. Host self-check: cmake -B build_host -DHOST_TEST=ON && cmake --build build_host
#                    (no Pico SDK needed; just a C++17 compiler)
option(HOST_TEST "Build the host-compilable self-check instead of Pico firmware" OFF)

if(HOST_TEST)
    project(psa_can_host CXX)
    set(CMAKE_CXX_STANDARD 17)
    add_executable(test_psa tests/test_psa.cpp src/isotp.cpp src/diag_shell.cpp src/flash_engine.cpp src/wifi_server.cpp src/can_sniffer.cpp)
    target_include_directories(test_psa PRIVATE include)
    target_compile_definitions(test_psa PRIVATE HOST_TEST)
    target_compile_options(test_psa PRIVATE -Wall -Wextra -Werror)
    enable_testing()
    add_test(NAME psa_selfcheck COMMAND test_psa)
    return()
endif()

# --- Pico firmware ---
# pico_sdk_import.cmake sets CMAKE_TOOLCHAIN_FILE, which CMake only honours if
# it's included before project() — otherwise project() locks in the host
# compiler and every cross-compiled source/asm file fails to build.
include(pico_sdk_import.cmake)

project(citroen_can_interface C CXX ASM)
set(CMAKE_C_STANDARD 11)
set(CMAKE_CXX_STANDARD 17)

pico_sdk_init()

add_executable(citroen_can_interface
    src/main.cpp
    src/mcp2515.cpp
    src/can_manager.cpp
    src/isotp.cpp
    src/diag_shell.cpp
    src/flash_engine.cpp
    src/wifi_server.cpp
    src/dhcp_server.cpp
    src/can_sniffer.cpp
)
target_include_directories(citroen_can_interface PRIVATE include)
target_link_libraries(citroen_can_interface
    pico_stdlib
    hardware_spi
    hardware_gpio
    hardware_flash
    pico_flash
    pico_cyw43_arch_lwip_threadsafe_background
)
pico_enable_stdio_usb(citroen_can_interface 1)
# USB CDC output/input is gated on DTR by default; GUI terminals (e.g. Termix)
# often don't raise DTR, so the shell looked dead. tud_ready() instead of DTR.
# PICO_FLASH_ASSUME_CORE1_SAFE: this project never launches core1, so
# gsniff save/load's flash_safe_execute() doesn't need multicore lockout.
target_compile_definitions(citroen_can_interface PRIVATE
    PICO_STDIO_USB_CONNECTION_WITHOUT_DTR=1
    PICO_FLASH_ASSUME_CORE1_SAFE=1
)
pico_add_extra_outputs(citroen_can_interface)
