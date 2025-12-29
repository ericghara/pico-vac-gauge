cmake_minimum_required(VERSION 3.13)

include(pico-sdk/pico_sdk_init.cmake)

project(pico_vac_gauge C CXX ASM)

set(CMAKE_C_STANDARD 11)
set(CMAKE_CXX_STANDARD 17)

pico_sdk_init()

add_executable(pico_vac_gauge
        src/main.c
)

target_link_libraries(pico_vac_gauge pico_stdlib hardware_adc)

pico_enable_stdio_usb(pico_vac_gauge 1)
pico_enable_stdio_uart(pico_vac_gauge 0)

pico_add_extra_outputs(pico_vac_gauge)
