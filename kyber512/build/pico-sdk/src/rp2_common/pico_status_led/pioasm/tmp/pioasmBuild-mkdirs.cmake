# Distributed under the OSI-approved BSD 3-Clause License.  See accompanying
# file Copyright.txt or https://cmake.org/licensing for details.

cmake_minimum_required(VERSION 3.5)

file(MAKE_DIRECTORY
  "/home/achintyanigam/Documents/pico-sdk/tools/pioasm"
  "/home/achintyanigam/dev/KyberMulticore/KyberMulticore/kyber512/build/pioasm"
  "/home/achintyanigam/dev/KyberMulticore/KyberMulticore/kyber512/build/pioasm-install"
  "/home/achintyanigam/dev/KyberMulticore/KyberMulticore/kyber512/build/pico-sdk/src/rp2_common/pico_status_led/pioasm/tmp"
  "/home/achintyanigam/dev/KyberMulticore/KyberMulticore/kyber512/build/pico-sdk/src/rp2_common/pico_status_led/pioasm/src/pioasmBuild-stamp"
  "/home/achintyanigam/dev/KyberMulticore/KyberMulticore/kyber512/build/pico-sdk/src/rp2_common/pico_status_led/pioasm/src"
  "/home/achintyanigam/dev/KyberMulticore/KyberMulticore/kyber512/build/pico-sdk/src/rp2_common/pico_status_led/pioasm/src/pioasmBuild-stamp"
)

set(configSubDirs )
foreach(subDir IN LISTS configSubDirs)
    file(MAKE_DIRECTORY "/home/achintyanigam/dev/KyberMulticore/KyberMulticore/kyber512/build/pico-sdk/src/rp2_common/pico_status_led/pioasm/src/pioasmBuild-stamp/${subDir}")
endforeach()
if(cfgdir)
  file(MAKE_DIRECTORY "/home/achintyanigam/dev/KyberMulticore/KyberMulticore/kyber512/build/pico-sdk/src/rp2_common/pico_status_led/pioasm/src/pioasmBuild-stamp${cfgdir}") # cfgdir has leading slash
endif()
