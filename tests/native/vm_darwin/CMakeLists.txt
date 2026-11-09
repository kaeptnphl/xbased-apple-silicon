cmake_minimum_required(VERSION 3.20)
project(vm_darwin_test C)

if(NOT APPLE OR NOT CMAKE_SYSTEM_PROCESSOR MATCHES "^(arm64|aarch64)$" OR
   (CMAKE_OSX_ARCHITECTURES AND NOT CMAKE_OSX_ARCHITECTURES STREQUAL "arm64"))
  message(FATAL_ERROR "vm_darwin_test requires an Apple Silicon macOS target")
endif()

set(CMAKE_C_STANDARD 11)
set(CMAKE_C_STANDARD_REQUIRED ON)
get_filename_component(REPO_ROOT "${CMAKE_CURRENT_SOURCE_DIR}/../../.." ABSOLUTE)

add_executable(vm_darwin_test
  vm_darwin_test.c
  "${REPO_ROOT}/native/core/vm.c")
target_include_directories(vm_darwin_test PRIVATE "${REPO_ROOT}")
target_compile_options(vm_darwin_test PRIVATE -ffunction-sections -fdata-sections)
target_link_options(vm_darwin_test PRIVATE -Wl,-dead_strip)

add_executable(net_darwin_test net_darwin_test.c)
target_include_directories(net_darwin_test PRIVATE "${REPO_ROOT}")

add_executable(time_darwin_test time_darwin_test.c)
target_include_directories(time_darwin_test PRIVATE "${REPO_ROOT}")
target_compile_options(time_darwin_test PRIVATE
  "-include${REPO_ROOT}/native/compat/darwin/posix.h")

add_executable(hle_thread_darwin_test
  hle_thread_darwin_test.c
  "${REPO_ROOT}/hw/xbox/hle/util/thread.c")
target_include_directories(hle_thread_darwin_test PRIVATE "${REPO_ROOT}")
target_compile_options(hle_thread_darwin_test PRIVATE
  "-include${REPO_ROOT}/native/compat/darwin/posix.h")

add_executable(mmio_darwin_test
  mmio_darwin_test.c
  "${REPO_ROOT}/native/core/mmio.c")
target_include_directories(mmio_darwin_test PRIVATE "${REPO_ROOT}")
target_compile_options(mmio_darwin_test PRIVATE -ffunction-sections -fdata-sections)
target_link_options(mmio_darwin_test PRIVATE -Wl,-dead_strip)

enable_testing()
add_test(NAME vm_darwin_test COMMAND vm_darwin_test)
add_test(NAME net_darwin_test COMMAND net_darwin_test)
add_test(NAME time_darwin_test COMMAND time_darwin_test)
add_test(NAME hle_thread_darwin_test COMMAND hle_thread_darwin_test)
add_test(NAME mmio_darwin_test COMMAND mmio_darwin_test)

foreach(test vm_darwin_test net_darwin_test time_darwin_test
             hle_thread_darwin_test mmio_darwin_test)
  target_compile_options(${test} PRIVATE -UNDEBUG)
  set_tests_properties(${test} PROPERTIES TIMEOUT 10)
endforeach()
