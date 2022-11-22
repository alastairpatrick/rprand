cmake_minimum_required(VERSION 3.13)

include(pico-sdk/pico_sdk_init.cmake)

project(firmware C CXX ASM)

set(CMAKE_C_STANDARD 11)
set(CMAKE_CXX_STANDARD 17)

pico_sdk_init()

add_library(rprand INTERFACE)

target_sources(rprand INTERFACE
  rprand.c
)

target_link_libraries(rprand INTERFACE
  hardware_dma
  hardware_sync
)

add_executable(rprand_example
  main.c
)

pico_enable_stdio_usb(rprand_example 0)
pico_enable_stdio_uart(rprand_example 1)
pico_add_extra_outputs(rprand_example)

target_link_libraries(rprand_example
  pico_stdlib
  pico_stdio
  rprand
)
