cmake_minimum_required(VERSION 3.25)

include($ENV{PICO_SDK_PATH}/external/pico_sdk_import.cmake)

project(MidiPiano C CXX ASM)

if(NOT CMAKE_BUILD_TYPE)
  set(CMAKE_BUILD_TYPE Release)
endif()

set(CMAKE_C_STANDARD 11)
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

pico_sdk_init()


set(FAMILY rp2040)
set(BOARD pico_sdk)

set(TINYUSB_LWIP_PATH ${PICO_LWIP_PATH})
# Some examples use this, and we need to set this here due to a bug in the TinyUSB CMake config
set(TOP ${PICO_TINYUSB_PATH})

set(SRC ${SRC} src/main.c)
set(SRC ${SRC} src/usb_descriptors.c)

add_executable(${PROJECT_NAME} ${SRC})

target_include_directories(${PROJECT_NAME} PUBLIC include)

pico_add_extra_outputs(${PROJECT_NAME})

target_link_libraries(${PROJECT_NAME}
  pico_stdlib
  pico_multicore
  tinyusb_device
  tinyusb_board
)

pico_enable_stdio_usb(${PROJECT_NAME} 1)
pico_enable_stdio_uart(${PROJECT_NAME} 0)

add_custom_command(
    TARGET ${PROJECT_NAME} POST_BUILD
    COMMAND picotool load -x ${CMAKE_BINARY_DIR}/${PROJECT_NAME}.uf2
    COMMENT "Uploading ${CMAKE_BINARY_DIR}/${PROJECT_NAME}.uf2 to RP2040"
)
