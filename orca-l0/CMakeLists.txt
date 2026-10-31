cmake_minimum_required(VERSION 3.24)
project(orca_l0 VERSION 0.1.0 LANGUAGES CXX)

option(ORCA_L0_BUILD_RUNTIME "Build the Level Zero runtime" ON)
option(ORCA_L0_BUILD_CHECKS "Build the standalone checks" ${PROJECT_IS_TOP_LEVEL})
option(ORCA_L0_BUILD_GPU_CHECKS "Compile kernels and register GPU checks" ${ORCA_L0_BUILD_CHECKS})
set(ORCA_L0_DEVICE "dg2" CACHE STRING "ocloc target for the check kernels")
set(ORCA_L0_SDK "" CACHE PATH "Level Zero SDK root")

if(ORCA_L0_BUILD_CHECKS)
  enable_testing()
  find_package(Threads REQUIRED)
  add_executable(orca-l0-ring-check tests/ring_check.cpp)
  target_include_directories(orca-l0-ring-check PRIVATE include)
  target_compile_features(orca-l0-ring-check PRIVATE cxx_std_17)
  target_link_libraries(orca-l0-ring-check PRIVATE Threads::Threads)
  add_test(NAME orca-l0-ring COMMAND orca-l0-ring-check)
  set_tests_properties(orca-l0-ring PROPERTIES TIMEOUT 30)
endif()

if(NOT ORCA_L0_BUILD_RUNTIME)
  return()
endif()
find_path(ORCA_L0_INCLUDE_DIR level_zero/ze_api.h
  HINTS "${ORCA_L0_SDK}/include" "$ENV{LEVEL_ZERO_ROOT}/include")
find_library(ORCA_L0_LOADER NAMES ze_loader
  HINTS "${ORCA_L0_SDK}/lib" "${ORCA_L0_SDK}/lib/x64"
        "$ENV{LEVEL_ZERO_ROOT}/lib" "$ENV{LEVEL_ZERO_ROOT}/lib/x64")
if(NOT ORCA_L0_INCLUDE_DIR OR NOT ORCA_L0_LOADER)
  message(FATAL_ERROR "Level Zero headers/loader missing; set ORCA_L0_SDK (CPU ring check: ORCA_L0_BUILD_RUNTIME=OFF)")
endif()
add_library(orca_l0 STATIC src/runtime.cpp)
add_library(Orca::L0 ALIAS orca_l0)
target_compile_features(orca_l0 PUBLIC cxx_std_17)
target_include_directories(orca_l0 PUBLIC include "${ORCA_L0_INCLUDE_DIR}")
target_link_libraries(orca_l0 PUBLIC "${ORCA_L0_LOADER}")
if(MSVC)
  target_compile_options(orca_l0 PRIVATE /W4)
else()
  target_compile_options(orca_l0 PRIVATE -Wall -Wextra -Wpedantic)
endif()

if(ORCA_L0_BUILD_CHECKS)
  add_executable(orca-l0-check tests/runtime_check.cpp)
  target_link_libraries(orca-l0-check PRIVATE Orca::L0 Threads::Threads)
  add_executable(orca-l0-submit-check EXCLUDE_FROM_ALL tests/submit_check.cpp)
  target_link_libraries(orca-l0-submit-check PRIVATE Orca::L0)
endif()
if(ORCA_L0_BUILD_CHECKS AND ORCA_L0_BUILD_GPU_CHECKS)
  find_program(ORCA_L0_OCLOC NAMES ocloc)
  if(NOT ORCA_L0_OCLOC)
    message(FATAL_ERROR "ocloc missing; set ORCA_L0_OCLOC or disable ORCA_L0_BUILD_GPU_CHECKS")
  endif()
  add_custom_command(OUTPUT "${CMAKE_CURRENT_BINARY_DIR}/inference.spv" "${CMAKE_CURRENT_BINARY_DIR}/inference.bin"
    COMMAND "${ORCA_L0_OCLOC}" -file "${CMAKE_CURRENT_SOURCE_DIR}/tests/inference.cl"
            -device "${ORCA_L0_DEVICE}" -output inference -output_no_suffix
            -out_dir "${CMAKE_CURRENT_BINARY_DIR}"
    DEPENDS tests/inference.cl VERBATIM)
  add_custom_target(orca-l0-check-kernels DEPENDS "${CMAKE_CURRENT_BINARY_DIR}/inference.spv"
                                               "${CMAKE_CURRENT_BINARY_DIR}/inference.bin")
  add_dependencies(orca-l0-check orca-l0-check-kernels)
  add_test(NAME orca-l0-runtime COMMAND orca-l0-check "${CMAKE_CURRENT_BINARY_DIR}/inference.spv")
  add_test(NAME orca-l0-poison COMMAND orca-l0-check "${CMAKE_CURRENT_BINARY_DIR}/inference.spv")
  set_tests_properties(orca-l0-poison PROPERTIES ENVIRONMENT "ORCA_L0_POISON_ALLOCATIONS=1"
    LABELS gpu TIMEOUT 120 RUN_SERIAL TRUE)
  add_test(NAME orca-l0-native COMMAND orca-l0-check "${CMAKE_CURRENT_BINARY_DIR}/inference.bin" native)
  set_tests_properties(orca-l0-runtime orca-l0-native PROPERTIES LABELS gpu TIMEOUT 120 RUN_SERIAL TRUE)
endif()
