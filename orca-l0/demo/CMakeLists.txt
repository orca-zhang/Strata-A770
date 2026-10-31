cmake_minimum_required(VERSION 3.24)
project(orca_l0_demo LANGUAGES CXX)
if(NOT CMAKE_CXX_COMPILER_ID MATCHES "IntelLLVM|Clang")
  message(FATAL_ERROR "The comparison demo needs a SYCL compiler (icpx/icx). The runtime itself does not.")
endif()
add_subdirectory(.. runtime)
find_program(ORCA_L0_OCLOC NAMES ocloc REQUIRED)
add_custom_command(OUTPUT "${CMAKE_CURRENT_BINARY_DIR}/inference.spv"
  COMMAND "${ORCA_L0_OCLOC}" -file "${CMAKE_CURRENT_SOURCE_DIR}/../tests/inference.cl"
          -device "${ORCA_L0_DEVICE}" -spv_only -output inference -output_no_suffix
          -out_dir "${CMAKE_CURRENT_BINARY_DIR}"
  DEPENDS ../tests/inference.cl VERBATIM)
add_custom_target(orca-l0-demo-kernel DEPENDS "${CMAKE_CURRENT_BINARY_DIR}/inference.spv")
add_executable(orca-l0-demo dispatch.cpp)
target_compile_features(orca-l0-demo PRIVATE cxx_std_17)
target_compile_options(orca-l0-demo PRIVATE -fsycl)
target_link_options(orca-l0-demo PRIVATE -fsycl)
target_link_libraries(orca-l0-demo PRIVATE Orca::L0)
add_dependencies(orca-l0-demo orca-l0-demo-kernel)
