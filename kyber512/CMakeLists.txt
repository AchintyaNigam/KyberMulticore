cmake_minimum_required(VERSION 3.13)
include(pico_sdk_import.cmake)

project(kyber_pico C CXX ASM)
pico_sdk_init()

add_executable(kyber_test
    kem.c indcpa.c polyvec.c poly.c ntt.c cbd.c reduce.c verify.c
    fips202.c symmetric-shake.c
    randombytes.c
    test/test_kyber.c
)

target_link_libraries(kyber_test pico_stdlib hardware_rng)
pico_add_extra_outputs(kyber_test)
