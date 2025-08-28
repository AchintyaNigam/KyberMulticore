# Generated Cmake Pico project file

cmake_minimum_required(VERSION 3.13)

set(CMAKE_C_STANDARD 11)
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

# Initialise pico_sdk from installed location
# (note this can come from environment, CMake cache etc)

# == DO NOT EDIT THE FOLLOWING LINES for the Raspberry Pi Pico VS Code Extension to work ==
if(WIN32)
    set(USERHOME $ENV{USERPROFILE})
else()
    set(USERHOME $ENV{HOME})
endif()
set(sdkVersion 2.1.1)
set(toolchainVersion 14_2_Rel1)
set(picotoolVersion 2.1.1)
set(picoVscode ${USERHOME}/.pico-sdk/cmake/pico-vscode.cmake)
if (EXISTS ${picoVscode})
    include(${picoVscode})
endif()
# ====================================================================================
set(PICO_BOARD pico2_w CACHE STRING "Board type")

# Pull in Raspberry Pi Pico SDK (must be before project)
include(pico_sdk_import.cmake)

project(Kyber_singlecore C CXX ASM)

# Initialise the Raspberry Pi Pico SDK
pico_sdk_init()

# Add executable. Default name is the project name, version 0.1

add_executable(Kyber_singlecore test_kyber_separate.c
    kem.c indcpa.c polyvec.c poly.c ntt.c cbd.c reduce.c verify.c
    fips202.c symmetric-shake.c
    randombytes.c
    )

target_compile_definitions(Kyber_singlecore PRIVATE
PICO_CYW43_ARCH_DEBUG_ENABLED=0
CYW43_ENABLE_DEBUG=0
CYW43_LOG_ENABLED=0
)

# pull in common dependencies
target_link_libraries(Kyber_singlecore 
    pico_stdlib
    pico_stdio_usb
    pico_rand
    pico_cyw43_arch_none
    pico_time
)

if (PICO_CYW43_SUPPORTED)
    target_link_libraries(Kyber_singlecore pico_cyw43_arch_none)
endif()

# create map/bin/hex file etc.
pico_add_extra_outputs(Kyber_singlecore)

# add url via pico_set_program_url
