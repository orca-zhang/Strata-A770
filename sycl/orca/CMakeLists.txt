cmake_minimum_required(VERSION 3.24)
project(strata_orca VERSION 0.1.39 LANGUAGES C CXX)
if(NOT CMAKE_CXX_COMPILER_ID STREQUAL "IntelLLVM")
  message(FATAL_ERROR "Use the oneAPI compiler: icpx (Windows: icx)")
endif()
get_filename_component(STRATA_ROOT "../.." ABSOLUTE)
option(STRATA_ORCA_BUILD_ENGINE "Build the native Strata executable" ON)
add_subdirectory("${STRATA_ROOT}/orca-l0" runtime)
find_package(Python3 REQUIRED COMPONENTS Interpreter)
set(_generated "${CMAKE_CURRENT_BINARY_DIR}/generated")
set(_includes "${CMAKE_CURRENT_SOURCE_DIR}/include" "${_generated}/include"
    "${STRATA_ROOT}/sycl"
    "${STRATA_ROOT}/sycl/include" "${STRATA_ROOT}/include"
    "${STRATA_ROOT}/third_party/ggml"
    "${STRATA_ROOT}/orca-l0/tests"
    "${STRATA_ROOT}/orca-l0/include" "${ORCA_L0_INCLUDE_DIR}")
set(_flags -DSTRATA_ORCA_NATIVE=1 -Wno-ignored-attributes -Wno-deprecated-declarations
    -fsycl-default-sub-group-size=32)
if(WIN32)
  list(APPEND _flags /EHsc /MD /O2 /fp:precise -Qstd:c++20 -D_USE_STD_VECTOR_ALGORITHMS=0)
else()
  list(APPEND _flags -O2 -std=c++20 -fp-model=precise)
endif()
foreach(_include IN LISTS _includes)
  list(APPEND _flags "-I${_include}")
endforeach()
add_custom_command(OUTPUT "${_generated}/include/dpct/util.hpp"
  COMMAND "${Python3_EXECUTABLE}" "${CMAKE_CURRENT_SOURCE_DIR}/prepare.py"
    "${STRATA_ROOT}/sycl/include/dpct/util.hpp" "${_generated}/include/dpct/util.hpp" --device-util
  DEPENDS prepare.py "${STRATA_ROOT}/sycl/include/dpct/util.hpp" VERBATIM)
add_custom_command(OUTPUT "${_generated}/include/strata/prefill/gdn_xe.hpp"
  COMMAND "${Python3_EXECUTABLE}" "${CMAKE_CURRENT_SOURCE_DIR}/prepare.py"
    "${STRATA_ROOT}/sycl/include/strata/prefill/gdn_xe.hpp" "${_generated}/include/strata/prefill/gdn_xe.hpp"
  DEPENDS prepare.py "${STRATA_ROOT}/sycl/include/strata/prefill/gdn_xe.hpp" VERBATIM)

option(STRATA_ORCA_ALL_KERNELS "Compile all Strata numerical kernel sources through Orca" OFF)
if(STRATA_ORCA_BUILD_ENGINE)
  set(STRATA_ORCA_ALL_KERNELS ON)
endif()
file(GLOB_RECURSE _strata_headers CONFIGURE_DEPENDS "${STRATA_ROOT}/sycl/include/strata/*.hpp")
set(_native_headers "${_generated}/include/strata/prefill/gdn_xe.hpp")
foreach(_header IN LISTS _strata_headers)
  file(RELATIVE_PATH _relative "${STRATA_ROOT}/sycl/include" "${_header}")
  if(_relative STREQUAL "strata/prefill/gdn_xe.hpp" OR EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/include/${_relative}")
    continue()
  endif()
  set(_output "${_generated}/include/${_relative}")
  add_custom_command(OUTPUT "${_output}"
    COMMAND "${Python3_EXECUTABLE}" "${CMAKE_CURRENT_SOURCE_DIR}/prepare.py" "${_header}" "${_output}" --engine
    DEPENDS "${_header}" prepare.py VERBATIM)
  list(APPEND _native_headers "${_output}")
endforeach()
add_custom_target(strata-orca-headers DEPENDS ${_native_headers} "${_generated}/include/dpct/util.hpp")
set(_kernel_names elementwise dequant_bf16 iq_kernels xmx native_gemm prefill_helpers)
if(STRATA_ORCA_ALL_KERNELS)
  list(APPEND _kernel_names dequant_s2 s2_gemv s_gemv s2_gemv_quads s2_gemv_fast router_top10 quantize_act
    rope sampler s2_gemv_q8 s2_expert_grouped shared_expert gr gdn qsa bf16_gemv native_mmvq native_bf16
    native_gr_norm native_moe native_gdn native_gr_postops native_router native_gdn_preprocess native_qsa
    native_rope native_ple_postops native_flash_attn native_qsa_indexer native_qsa_score ple kv_q8 kv_stream
    kv_q4 fused_gr cvec qsa_decode_attn qsa_prompt_attn fused_gdn qsa_select verify_kernels qsa_prompt_attn_xmx)
endif()
set(_manifest "")
foreach(_kernel IN LISTS _kernel_names)
  string(APPEND _manifest "${_kernel}\n")
endforeach()
file(GENERATE OUTPUT "${CMAKE_CURRENT_BINARY_DIR}/images/packs.txt" CONTENT "${_manifest}")
set(_sources "")
foreach(_kernel IN LISTS _kernel_names)
  set(_input "${STRATA_ROOT}/sycl/src/kernels/cuda/${_kernel}.dp.cpp")
  set(_prepare_flags "")
  set(_register_flags "")
  if(_kernel STREQUAL "xmx")
    set(_input "${STRATA_ROOT}/sycl/src/prefill/gemm.dp.cpp")
    set(_prepare_flags --xmx-kernel)
    # The FP16 M4 candidate passed the clean 68-request A770 regression; BF16 stays unchanged.
    set(_register_flags "--large-grf-kernel=*native_xmx_launch_m4*")
  elseif(_kernel STREQUAL "native_gemm")
    set(_input "${CMAKE_CURRENT_SOURCE_DIR}/gemm.cpp")
    set(_register_flags "--large-grf-kernel=*OrcaFp16ExpertPair*")
  elseif(_kernel STREQUAL "prefill_helpers")
    set(_input "${STRATA_ROOT}/sycl/src/prefill/kernels.dp.cpp")
  endif()
  set(_source "${_generated}/${_kernel}.cpp")
  set(_pack "${CMAKE_CURRENT_BINARY_DIR}/images/${_kernel}")
  file(GLOB_RECURSE _headers CONFIGURE_DEPENDS "${STRATA_ROOT}/sycl/include/strata/*.hpp"
    "${STRATA_ROOT}/sycl/include/dpct/*.hpp"
    "${STRATA_ROOT}/include/strata/kernels/*.hpp" "${CMAKE_CURRENT_SOURCE_DIR}/include/*.hpp"
    "${STRATA_ROOT}/include/strata/prefill/*.hpp"
    "${STRATA_ROOT}/orca-l0/include/*.hpp")
  # This bootstrap is included only by the host executable. Its API changes
  # must not invalidate every numerical SPIR-V image.
  list(REMOVE_ITEM _headers "${CMAKE_CURRENT_SOURCE_DIR}/include/strata/orca_engine.hpp")
  list(APPEND _headers "${STRATA_ROOT}/include/strata/core/emulate.hpp"
    "${STRATA_ROOT}/include/strata/core/coupled_draft.hpp")
  list(APPEND _headers "${STRATA_ROOT}/orca-l0/tests/runtime_dependencies.hpp")
  add_custom_command(OUTPUT "${_source}"
    COMMAND "${Python3_EXECUTABLE}" "${CMAKE_CURRENT_SOURCE_DIR}/prepare.py" "${_input}" "${_source}" ${_prepare_flags}
    DEPENDS "${_input}" prepare.py VERBATIM)
  add_custom_command(OUTPUT "${_pack}/index.txt" "${_pack}/abi.hpp"
    COMMAND "${Python3_EXECUTABLE}" "${STRATA_ROOT}/orca-l0/compiler/build.py"
      --compiler "${CMAKE_CXX_COMPILER}" --source "${_source}" --out "${_pack}"
      --module-flags=-cl-fp32-correctly-rounded-divide-sqrt ${_register_flags} -- ${_flags}
    DEPENDS "${_source}" "${_generated}/include/dpct/util.hpp"
      "${_generated}/include/strata/prefill/gdn_xe.hpp" ${_headers}
      ${_native_headers}
      "${STRATA_ROOT}/sycl/orca_queue.hpp" "${STRATA_ROOT}/sycl/orca_device.hpp"
      "${STRATA_ROOT}/orca-l0/compiler/build.py" VERBATIM)
  list(APPEND _sources "${_source}" "${_pack}/index.txt" "${_pack}/abi.hpp")
endforeach()
add_library(strata_orca_kernels STATIC ${_sources} "${_generated}/include/dpct/util.hpp")
add_dependencies(strata_orca_kernels strata-orca-headers)
target_compile_features(strata_orca_kernels PUBLIC cxx_std_20)
target_include_directories(strata_orca_kernels PUBLIC ${_includes})
target_compile_options(strata_orca_kernels PRIVATE ${_flags} -fsycl -fsycl-host-only)
target_link_libraries(strata_orca_kernels PUBLIC Orca::L0)
add_executable(strata-orca-kernel-check check.cpp)
target_compile_options(strata-orca-kernel-check PRIVATE ${_flags} -fsycl -fsycl-host-only)
target_link_libraries(strata-orca-kernel-check PRIVATE strata_orca_kernels)
# Manual real-input diagnostic: its frozen model fixtures are outside the repo.
add_executable(strata-orca-gemm-replay-check EXCLUDE_FROM_ALL gemm_replay_check.cpp)
target_compile_options(strata-orca-gemm-replay-check PRIVATE ${_flags} -fsycl -fsycl-host-only)
target_link_libraries(strata-orca-gemm-replay-check PRIVATE strata_orca_kernels)
enable_testing()
add_test(NAME strata-orca-kernels COMMAND strata-orca-kernel-check "${CMAKE_CURRENT_BINARY_DIR}/images")
set_tests_properties(strata-orca-kernels PROPERTIES LABELS gpu TIMEOUT 120 RUN_SERIAL TRUE)

set(_queue_source "${STRATA_ROOT}/sycl/orca_queue_check.cpp")
set(_queue_pack "${CMAKE_CURRENT_BINARY_DIR}/images/queue")
add_custom_command(OUTPUT "${_queue_pack}/index.txt" "${_queue_pack}/abi.hpp"
  COMMAND "${Python3_EXECUTABLE}" "${STRATA_ROOT}/orca-l0/compiler/build.py"
    --compiler "${CMAKE_CXX_COMPILER}" --source "${_queue_source}" --out "${_queue_pack}"
    --module-flags=-cl-fp32-correctly-rounded-divide-sqrt -- ${_flags}
  DEPENDS "${_queue_source}" "${STRATA_ROOT}/sycl/orca_queue.hpp" "${STRATA_ROOT}/sycl/orca_device.hpp"
    "${STRATA_ROOT}/orca-l0/compiler/build.py" ${_headers} VERBATIM)
add_executable(strata-orca-queue-check "${_queue_source}" "${_queue_pack}/index.txt" "${_queue_pack}/abi.hpp")
target_compile_options(strata-orca-queue-check PRIVATE ${_flags} -fsycl -fsycl-host-only)
target_link_libraries(strata-orca-queue-check PRIVATE Orca::L0)
target_compile_features(strata-orca-queue-check PRIVATE cxx_std_20)
add_test(NAME strata-orca-queues COMMAND strata-orca-queue-check "${_queue_pack}")
add_executable(strata-orca-timestamp-check "${STRATA_ROOT}/sycl/orca_timestamp_check.cpp")
target_compile_options(strata-orca-timestamp-check PRIVATE ${_flags} -fsycl -fsycl-host-only)
target_link_libraries(strata-orca-timestamp-check PRIVATE Orca::L0)
target_compile_features(strata-orca-timestamp-check PRIVATE cxx_std_20)
add_dependencies(strata-orca-timestamp-check strata-orca-queue-check)
add_test(NAME strata-orca-timestamps COMMAND strata-orca-timestamp-check "${_queue_pack}")
set_tests_properties(strata-orca-timestamps PROPERTIES TIMEOUT 30)
set_tests_properties(strata-orca-queues PROPERTIES LABELS gpu TIMEOUT 120 RUN_SERIAL TRUE)

if(STRATA_ORCA_ALL_KERNELS)
  # Manual GEMV/router fork-join check and timings; reuse existing model images.
  add_executable(strata-orca-mixed-check EXCLUDE_FROM_ALL mixed_schedule_check.cpp)
  target_compile_options(strata-orca-mixed-check PRIVATE ${_flags} -fsycl -fsycl-host-only)
  target_link_libraries(strata-orca-mixed-check PRIVATE strata_orca_kernels)
  add_executable(strata-orca-gdn-schedule-check EXCLUDE_FROM_ALL gdn_schedule_check.cpp)
  target_compile_options(strata-orca-gdn-schedule-check PRIVATE ${_flags} -fsycl -fsycl-host-only)
  target_link_libraries(strata-orca-gdn-schedule-check PRIVATE strata_orca_kernels)
  add_executable(strata-orca-qsa-schedule-check EXCLUDE_FROM_ALL qsa_schedule_check.cpp)
  target_compile_options(strata-orca-qsa-schedule-check PRIVATE ${_flags} -fsycl -fsycl-host-only)
  target_link_libraries(strata-orca-qsa-schedule-check PRIVATE strata_orca_kernels)
  add_executable(strata-orca-shared-schedule-check EXCLUDE_FROM_ALL shared_schedule_check.cpp)
  target_compile_options(strata-orca-shared-schedule-check PRIVATE ${_flags} -fsycl -fsycl-host-only)
  target_link_libraries(strata-orca-shared-schedule-check PRIVATE strata_orca_kernels)
  set(_iq_source "${_generated}/iq_check.cpp")
  add_custom_command(OUTPUT "${_iq_source}"
    COMMAND "${Python3_EXECUTABLE}" "${CMAKE_CURRENT_SOURCE_DIR}/prepare.py"
      "${STRATA_ROOT}/sycl/src/kernels/iq_parity.cpp" "${_iq_source}" --iq-check
    DEPENDS prepare.py "${STRATA_ROOT}/sycl/src/kernels/iq_parity.cpp" VERBATIM)
  add_executable(strata-orca-iq-check "${_iq_source}")
  target_compile_options(strata-orca-iq-check PRIVATE ${_flags} -fsycl -fsycl-host-only)
  target_link_libraries(strata-orca-iq-check PRIVATE strata_orca_kernels)
  set(STRATA_ORCA_IQ_FIXTURES "" CACHE PATH "Fixtures from tools/iq_fixture.py")
  if(STRATA_ORCA_IQ_FIXTURES)
    add_test(NAME strata-orca-iq COMMAND strata-orca-iq-check
      "${CMAKE_CURRENT_BINARY_DIR}/images" "${STRATA_ORCA_IQ_FIXTURES}")
    set_tests_properties(strata-orca-iq PROPERTIES LABELS gpu TIMEOUT 120 RUN_SERIAL TRUE)
  endif()
endif()

if(STRATA_ORCA_BUILD_ENGINE)
  include(engine.cmake)
endif()
