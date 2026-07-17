# Distributed under the OSI-approved BSD 3-Clause License.  See accompanying
# file Copyright.txt or https://cmake.org/licensing for details.

cmake_minimum_required(VERSION 3.5)

file(MAKE_DIRECTORY
  "C:/Users/Umut/pico-sdk/tools/pioasm"
  "D:/citroen-can-interface/build/pioasm"
  "D:/citroen-can-interface/build/pioasm-install"
  "D:/citroen-can-interface/build/pico-sdk/src/rp2_common/pico_cyw43_driver/pioasm/tmp"
  "D:/citroen-can-interface/build/pico-sdk/src/rp2_common/pico_cyw43_driver/pioasm/src/pioasmBuild-stamp"
  "D:/citroen-can-interface/build/pico-sdk/src/rp2_common/pico_cyw43_driver/pioasm/src"
  "D:/citroen-can-interface/build/pico-sdk/src/rp2_common/pico_cyw43_driver/pioasm/src/pioasmBuild-stamp"
)

set(configSubDirs )
foreach(subDir IN LISTS configSubDirs)
    file(MAKE_DIRECTORY "D:/citroen-can-interface/build/pico-sdk/src/rp2_common/pico_cyw43_driver/pioasm/src/pioasmBuild-stamp/${subDir}")
endforeach()
if(cfgdir)
  file(MAKE_DIRECTORY "D:/citroen-can-interface/build/pico-sdk/src/rp2_common/pico_cyw43_driver/pioasm/src/pioasmBuild-stamp${cfgdir}") # cfgdir has leading slash
endif()
