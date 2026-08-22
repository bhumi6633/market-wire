cmake_minimum_required(VERSION 3.20)
project(exchange_feed_engine VERSION 1.0.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

option(EFE_ENABLE_SANITIZERS "Enable ASan/UBSan" OFF)
option(EFE_ENABLE_FUZZING "Build libFuzzer targets (Clang only)" OFF)
option(EFE_BUILD_BENCHMARKS "Build benchmark executable" ON)

function(efe_warnings target)
  target_compile_options(${target} PRIVATE
    $<$<CXX_COMPILER_ID:GNU,Clang>:-Wall -Wextra -Wpedantic -Wconversion -Wshadow>
  )
  if(EFE_ENABLE_SANITIZERS AND CMAKE_CXX_COMPILER_ID MATCHES "GNU|Clang")
    target_compile_options(${target} PRIVATE -fsanitize=address,undefined -fno-omit-frame-pointer)
    target_link_options(${target} PRIVATE -fsanitize=address,undefined)
  endif()
endfunction()

add_executable(efe_protocolc tools/protocolc/main.cpp)
efe_warnings(efe_protocolc)

set(EFE_GENERATED_DIR ${CMAKE_BINARY_DIR}/generated)
set(EFE_GENERATED_ITCH ${EFE_GENERATED_DIR}/efe/generated/itch50.hpp)
add_custom_command(
  OUTPUT ${EFE_GENERATED_ITCH}
  COMMAND ${CMAKE_COMMAND} -E make_directory ${EFE_GENERATED_DIR}/efe/generated
  COMMAND $<TARGET_FILE:efe_protocolc> ${CMAKE_SOURCE_DIR}/schemas/itch50.efe ${EFE_GENERATED_ITCH}
  DEPENDS efe_protocolc ${CMAKE_SOURCE_DIR}/schemas/itch50.efe
  COMMENT "Generating Nasdaq ITCH 5.0 views from schema"
  VERBATIM
)
add_custom_target(generate_protocols DEPENDS ${EFE_GENERATED_ITCH})

add_library(efe_core
  src/order_book.cpp
  src/itch_decoder.cpp
  src/moldudp64.cpp
  src/recovery_engine.cpp
  src/capture.cpp
  src/replay.cpp
  src/metrics.cpp
  src/udp_receiver.cpp
  ${EFE_GENERATED_ITCH}
)
add_dependencies(efe_core generate_protocols)
target_include_directories(efe_core PUBLIC include ${EFE_GENERATED_DIR})
efe_warnings(efe_core)

add_executable(feed_engine src/main.cpp)
target_link_libraries(feed_engine PRIVATE efe_core)
efe_warnings(feed_engine)

add_executable(efe_tests tests/test_main.cpp)
target_link_libraries(efe_tests PRIVATE efe_core)
efe_warnings(efe_tests)
enable_testing()
add_test(NAME efe_tests COMMAND efe_tests)

if(EFE_BUILD_BENCHMARKS)
  add_executable(efe_bench bench/benchmark.cpp)
  target_link_libraries(efe_bench PRIVATE efe_core)
  efe_warnings(efe_bench)
endif()

if(EFE_ENABLE_FUZZING)
  if(NOT CMAKE_CXX_COMPILER_ID MATCHES "Clang")
    message(FATAL_ERROR "EFE_ENABLE_FUZZING requires Clang")
  endif()
  foreach(name IN ITEMS moldudp itch)
    add_executable(fuzz_${name} fuzz/fuzz_${name}.cpp)
    target_link_libraries(fuzz_${name} PRIVATE efe_core)
    target_compile_options(fuzz_${name} PRIVATE -fsanitize=fuzzer,address,undefined -fno-omit-frame-pointer)
    target_link_options(fuzz_${name} PRIVATE -fsanitize=fuzzer,address,undefined)
  endforeach()
endif()
