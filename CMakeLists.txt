cmake_minimum_required(VERSION 3.13)

include(pico_sdk_import.cmake)

project(pico_spi_pseudoram C CXX ASM)
set(CMAKE_C_STANDARD 11)
set(CMAKE_CXX_STANDARD 17)
set(PICO_COPY_TO_RAM 1)
pico_sdk_init()

add_executable(pico_spi_pseudoram
  pico_spi_pseudoram.c
)

pico_enable_stdio_usb(pico_spi_pseudoram 1)
pico_enable_stdio_uart(pico_spi_pseudoram 0)

target_link_libraries(pico_spi_pseudoram pico_stdlib pico_mem_ops hardware_spi hardware_dma pico_mem_ops)

pico_add_extra_outputs(pico_spi_pseudoram)
