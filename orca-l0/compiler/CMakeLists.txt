cmake_minimum_required(VERSION 3.24)
project(orca_l0_compiler LANGUAGES CXX)

# SYCL is a compiler input here. Only the host compile uses -fsycl-host-only;
# the final executable links Orca::L0 or Orca::DG2 without -fsycl link flags.
if(NOT CMAKE_CXX_COMPILER_ID STREQUAL "IntelLLVM")
  message(FATAL_ERROR "Use the oneAPI compiler: -DCMAKE_CXX_COMPILER=icpx (Windows: icx)")
endif()

add_subdirectory(.. runtime)
find_package(Python3 REQUIRED COMPONENTS Interpreter)
if(ORCA_L0_BUILD_RUNTIME)
  set(_source "${CMAKE_CURRENT_SOURCE_DIR}/../tests/compiler_check.cpp")
  set(_check orca-l0-compiler-check)
  set(_library Orca::L0)
else()
  if(NOT TARGET Orca::DG2)
    message(FATAL_ERROR "The compiler check requires Orca::L0 or Orca::DG2")
  endif()
  set(_source "${CMAKE_CURRENT_SOURCE_DIR}/../tests/dg2_compiler_check.cpp")
  set(_check orca-l0-dg2-compiler-check)
  set(_library Orca::DG2)
endif()
set(_images "${CMAKE_CURRENT_BINARY_DIR}/images")
set(_abi "${_images}/abi.hpp")
set(_index "${_images}/index.txt")
set(_flags -Wno-ignored-attributes)
set(_includes "-I${CMAKE_CURRENT_SOURCE_DIR}/../include")
if(ORCA_L0_INCLUDE_DIR)
  list(APPEND _includes "-I${ORCA_L0_INCLUDE_DIR}")
endif()
if(WIN32)
  list(APPEND _flags /EHsc /MD /O2 /fp:precise -Qstd:c++20)
else()
  list(APPEND _flags -O2 -std=c++20 -fp-model=precise)
endif()
add_custom_command(OUTPUT "${_abi}" "${_index}"
  COMMAND "${Python3_EXECUTABLE}" "${CMAKE_CURRENT_SOURCE_DIR}/build.py"
    --compiler "${CMAKE_CXX_COMPILER}" --source "${_source}" --out "${_images}" -- ${_flags}
    ${_includes}
  DEPENDS "${_source}" build.py ../include/orca_l0/compiler/sycl.hpp
    ../include/orca_l0/compiler/local_accessor.hpp
    ../include/orca_l0/compiler/dg2.hpp ../include/orca_l0/dg2.hpp
    ../include/orca_l0/compiler/program.hpp ../include/orca_l0/compiler/stream.hpp
    ../include/orca_l0/runtime.hpp
    ../tests/runtime_dependencies.hpp
  VERBATIM)
add_executable(${_check} "${_source}" "${_abi}")
target_compile_features(${_check} PRIVATE cxx_std_20)
target_compile_options(${_check} PRIVATE -fsycl -fsycl-host-only)
target_link_libraries(${_check} PRIVATE ${_library})
enable_testing()
add_test(NAME orca-l0-compiler-flags COMMAND "${Python3_EXECUTABLE}"
  "${CMAKE_CURRENT_SOURCE_DIR}/../tests/compiler_flags_check.py")
if(ORCA_L0_BUILD_RUNTIME)
  add_test(NAME orca-l0-compiler COMMAND ${_check} "${_images}")
  set_tests_properties(orca-l0-compiler PROPERTIES LABELS gpu TIMEOUT 120 RUN_SERIAL TRUE)
else()
  set(ORCA_L0_DG2_COMPILER_PACK "" CACHE PATH "Offline native bake of the DG2 compiler check images")
  add_test(NAME orca-l0-dg2-compiler-index COMMAND ${_check} --index-only "${_images}")
  if(ORCA_L0_DG2_COMPILER_PACK AND ORCA_L0_DG2_MEMORY_ZEBIN)
    add_test(NAME orca-l0-dg2-compiler COMMAND ${_check} "${ORCA_L0_DG2_COMPILER_PACK}" "${ORCA_L0_DG2_MEMORY_ZEBIN}")
    set_tests_properties(orca-l0-dg2-compiler PROPERTIES LABELS gpu TIMEOUT 120 RUN_SERIAL TRUE)
  endif()
endif()

if(NOT ORCA_L0_BUILD_RUNTIME)
  set(_queue_source "${CMAKE_CURRENT_SOURCE_DIR}/../../sycl/orca_queue_check.cpp")
  set(_queue_images "${CMAKE_CURRENT_BINARY_DIR}/queue-images")
  set(_queue_flags ${_flags} -DSTRATA_ORCA_DG2=1)
  add_custom_command(OUTPUT "${_queue_images}/abi.hpp" "${_queue_images}/index.txt"
    COMMAND "${Python3_EXECUTABLE}" "${CMAKE_CURRENT_SOURCE_DIR}/build.py"
      --compiler "${CMAKE_CXX_COMPILER}" --source "${_queue_source}" --out "${_queue_images}"
      -- ${_queue_flags} ${_includes} "-I${CMAKE_CURRENT_SOURCE_DIR}/../../sycl"
    DEPENDS "${_queue_source}" build.py ../../sycl/orca_queue.hpp ../../sycl/orca_dg2_backend.hpp
      ../include/orca_l0/compiler/dg2.hpp ../include/orca_l0/dg2.hpp ../tests/runtime_dependencies.hpp
    VERBATIM)
  add_executable(strata-dg2-queue-check "${_queue_source}" "${_queue_images}/abi.hpp")
  target_compile_features(strata-dg2-queue-check PRIVATE cxx_std_20)
  target_compile_options(strata-dg2-queue-check PRIVATE ${_queue_flags} ${_includes} -fsycl -fsycl-host-only)
  target_link_libraries(strata-dg2-queue-check PRIVATE Orca::DG2)
  set(ORCA_L0_DG2_QUEUE_PACK "" CACHE PATH "Offline native bake of the Strata DG2 queue-check images")
  if(ORCA_L0_DG2_QUEUE_PACK AND ORCA_L0_DG2_MEMORY_ZEBIN)
    add_test(NAME strata-dg2-queues COMMAND strata-dg2-queue-check "${ORCA_L0_DG2_QUEUE_PACK}")
    set_tests_properties(strata-dg2-queues PROPERTIES LABELS gpu TIMEOUT 120 RUN_SERIAL TRUE
      ENVIRONMENT "STRATA_ORCA_DG2_MEMORY=${ORCA_L0_DG2_MEMORY_ZEBIN}")
  endif()
endif()
