cmake_minimum_required(VERSION 3.24)
project(orca_l0_compiler LANGUAGES CXX)

# SYCL is a compiler input here. Only the host compile uses -fsycl-host-only;
# the final executable links Orca::L0 and never receives -fsycl link flags.
if(NOT CMAKE_CXX_COMPILER_ID STREQUAL "IntelLLVM")
  message(FATAL_ERROR "Use the oneAPI compiler: -DCMAKE_CXX_COMPILER=icpx (Windows: icx)")
endif()
add_subdirectory(.. runtime)
find_package(Python3 REQUIRED COMPONENTS Interpreter)
set(_source "${CMAKE_CURRENT_SOURCE_DIR}/../tests/compiler_check.cpp")
set(_images "${CMAKE_CURRENT_BINARY_DIR}/images")
set(_abi "${_images}/abi.hpp")
set(_index "${_images}/index.txt")
set(_flags -Wno-ignored-attributes)
if(WIN32)
  list(APPEND _flags /EHsc /MD /O2 /fp:precise -Qstd:c++20)
else()
  list(APPEND _flags -O2 -std=c++20 -fp-model=precise)
endif()
add_custom_command(OUTPUT "${_abi}" "${_index}"
  COMMAND "${Python3_EXECUTABLE}" "${CMAKE_CURRENT_SOURCE_DIR}/build.py"
    --compiler "${CMAKE_CXX_COMPILER}" --source "${_source}" --out "${_images}" -- ${_flags}
    "-I${CMAKE_CURRENT_SOURCE_DIR}/../include" "-I${ORCA_L0_INCLUDE_DIR}"
  DEPENDS "${_source}" build.py ../include/orca_l0/compiler/sycl.hpp
    ../include/orca_l0/compiler/program.hpp ../include/orca_l0/compiler/stream.hpp
    ../include/orca_l0/runtime.hpp
    ../tests/runtime_dependencies.hpp
  VERBATIM)
add_executable(orca-l0-compiler-check "${_source}" "${_abi}")
target_compile_features(orca-l0-compiler-check PRIVATE cxx_std_20)
target_compile_options(orca-l0-compiler-check PRIVATE -fsycl -fsycl-host-only)
target_link_libraries(orca-l0-compiler-check PRIVATE Orca::L0)
enable_testing()
add_test(NAME orca-l0-compiler-flags COMMAND "${Python3_EXECUTABLE}"
  "${CMAKE_CURRENT_SOURCE_DIR}/../tests/compiler_flags_check.py")
add_test(NAME orca-l0-compiler COMMAND orca-l0-compiler-check "${_images}")
set_tests_properties(orca-l0-compiler PROPERTIES LABELS gpu TIMEOUT 120 RUN_SERIAL TRUE)
