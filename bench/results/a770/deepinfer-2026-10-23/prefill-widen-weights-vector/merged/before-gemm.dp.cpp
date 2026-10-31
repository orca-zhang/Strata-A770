// src/prefill/gemm.cu - see include/strata/prefill/gemm.hpp.
#define DPCT_PROFILING_ENABLED
#include <sycl/sycl.hpp>
#include <sycl/ext/oneapi/matrix/matrix.hpp>
#include <array>
#include <dpct/dpct.hpp>
#include "strata/sycl_queue.hpp"
#include "strata/prefill/gemm.hpp"
#include "strata/kernels/dequant_bf16.hpp"
#include <dpct/blas_utils.hpp>

#if defined(__HIPCC__) && defined(STRATA_HIPBLASLT_AVAILABLE)
// The HIP compatibility shim maps CUDA shuffle spellings to Strata helpers.
// hipBLASLt's public headers declare native HIP shuffle functions, so keep
// those declarations from being macro-expanded in this translation unit.
#undef __shfl_xor_sync
#undef __shfl_down_sync
#undef __shfl_up_sync
#undef __shfl_sync
#undef __ballot_sync
#endif

#include <climits>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <memory>
#include <algorithm>
#include <chrono>
#include <dpct/lib_common_utils.hpp>

#if defined(__HIPCC__) && defined(STRATA_HIPBLASLT_AVAILABLE)
#include "hipblaslt_tuning.hpp"
#include <hip/hip_runtime_api.h>
#include <hipblaslt/hipblaslt.h>
#include <hipblaslt/hipblaslt-ext.hpp>
#include <map>
#include <set>
#include <tuple>
#endif

namespace strata::prefill {
namespace {

void ck(int s, const char *what) {
    if (s != 0) {
        std::fprintf(stderr, "prefill gemm: %s: cuBLAS status %d\n", what, (int) s);
        std::exit(1);
    }
}

// #247/#325: on Windows (seen on gfx1201), hipBLAS can return success with the correct BF16/FP16 product for some
// shapes (hc up once T >= 96, the router) and still leave hipErrorInvalidValue set, which the next kernel's error
// check turns into an exit. The multiply has finished, so that one stale error is cleared after a GEMM that succeeded;
// any other error still stops the engine. Windows only: on Linux a stale hipErrorInvalidValue is a real error from
// an earlier call and keeps being reported. A no-op everywhere else (CUDA compiles none of it).
#if defined(__HIPCC__) && defined(_WIN32)
void absorb_hipblas_sticky(const char* what) {
    const hipError_t sticky = hipGetLastError();
    if (sticky == hipSuccess || sticky == hipErrorInvalidValue) return;
    std::fprintf(stderr, "prefill gemm: %s left %s\n", what, hipGetErrorString(sticky));
    std::exit(1);
}
#define STRATA_ABSORB_HIPBLAS_STICKY(what) absorb_hipblas_sticky(what)
#else
#define STRATA_ABSORB_HIPBLAS_STICKY(what) ((void) 0)
#endif

// A setup call whose failure the engine survives (the handle keeps its defaults), as before #240 - but said.
void note(int s, const char *what) {
    if (s != 0) std::fprintf(
        stderr, "prefill gemm: %s: cuBLAS status %d (continuing)\n", what,
        (int)s);
}

#if defined(__HIPCC__) && defined(STRATA_HIPBLASLT_AVAILABLE)
struct HipLtCallKey {
    strata::prefill::hipblaslt::InputType type;
    int t;
    int n;
    int k;
    int ldy;
    uint32_t beta_bits;

    bool operator<(const HipLtCallKey& other) const {
        return std::tie(type, n, k, ldy, t, beta_bits) <
               std::tie(other.type, other.n, other.k, other.ldy, other.t, other.beta_bits);
    }
};

struct HipLtCachedAlgo {
    bool supported = false;
    hipblasLtMatmulAlgo_t algo{};
    size_t workspace_bytes = 0;
};

struct HipLtState {
    hipblasLtHandle_t handle = nullptr;
    void* workspace = nullptr;
    size_t workspace_bytes = 0;
    strata::prefill::hipblaslt::TuningTable table;
    std::map<HipLtCallKey, HipLtCachedAlgo> cache;
    uint64_t lt_launches = 0;
    uint64_t fallbacks = 0;
    std::set<std::tuple<strata::prefill::hipblaslt::InputType, int, int, int, int>> fallback_shapes;

    ~HipLtState() {
        if (std::getenv("STRATA_HIPBLASLT_VERBOSE")) {
            std::fprintf(stderr, "prefill gemm: hipBLASLt summary launches=%llu fallbacks=%llu unique_fallback_shapes=%zu\n",
                         (unsigned long long) lt_launches, (unsigned long long) fallbacks, fallback_shapes.size());
            for (const auto& shape : fallback_shapes) {
                const auto type = std::get<0>(shape);
                std::fprintf(stderr, "prefill gemm: fallback shape dtype=%s T=%d N=%d K=%d ldy=%d\n",
                             type == strata::prefill::hipblaslt::InputType::bf16 ? "bf16" : "f16",
                             std::get<1>(shape), std::get<2>(shape), std::get<3>(shape), std::get<4>(shape));
            }
        }
        if (handle) hipblasLtDestroy(handle);
    }
};

struct HipLtDescriptors {
    hipblasLtMatmulDesc_t op = nullptr;
    hipblasLtMatrixLayout_t a = nullptr;
    hipblasLtMatrixLayout_t b = nullptr;
    hipblasLtMatrixLayout_t c = nullptr;

    ~HipLtDescriptors() {
        if (op) hipblasLtMatmulDescDestroy(op);
        if (a) hipblasLtMatrixLayoutDestroy(a);
        if (b) hipblasLtMatrixLayoutDestroy(b);
        if (c) hipblasLtMatrixLayoutDestroy(c);
    }

    bool init(hipDataType type, int t, int n, int k, int ldy) {
        const hipblasOperation_t trans_a = HIPBLAS_OP_T;
        const hipblasOperation_t trans_b = HIPBLAS_OP_N;
        if (hipblasLtMatmulDescCreate(&op, HIPBLAS_COMPUTE_32F, HIP_R_32F) != HIPBLAS_STATUS_SUCCESS ||
            hipblasLtMatmulDescSetAttribute(op, HIPBLASLT_MATMUL_DESC_TRANSA, &trans_a, sizeof(trans_a)) !=
                HIPBLAS_STATUS_SUCCESS ||
            hipblasLtMatmulDescSetAttribute(op, HIPBLASLT_MATMUL_DESC_TRANSB, &trans_b, sizeof(trans_b)) !=
                HIPBLAS_STATUS_SUCCESS ||
            hipblasLtMatrixLayoutCreate(&a, type, k, n, k) != HIPBLAS_STATUS_SUCCESS ||
            hipblasLtMatrixLayoutCreate(&b, type, k, t, k) != HIPBLAS_STATUS_SUCCESS ||
            hipblasLtMatrixLayoutCreate(&c, HIP_R_32F, n, t, ldy) != HIPBLAS_STATUS_SUCCESS) {
            return false;
        }
        return true;
    }
};

std::unique_ptr<HipLtState> create_hipblaslt_state(void* workspace, size_t workspace_bytes) {
    const char* path = std::getenv("STRATA_HIPBLASLT_TUNING");
    if (!path || !*path) return nullptr;

    auto state = std::make_unique<HipLtState>();
    state->workspace = workspace;
    state->workspace_bytes = workspace_bytes;
    if (hipblasLtCreate(&state->handle) != HIPBLAS_STATUS_SUCCESS) {
        std::fprintf(stderr, "prefill gemm: hipBLASLt handle creation failed; using hipBLASEx\n");
        return nullptr;
    }

    int version = 0;
    if (hipblasLtGetVersion(state->handle, &version) != HIPBLAS_STATUS_SUCCESS) {
        std::fprintf(stderr, "prefill gemm: hipBLASLt version query failed; using hipBLASEx\n");
        return nullptr;
    }
    int device = 0;
    hipDeviceProp_t properties{};
    if (hipGetDevice(&device) != hipSuccess || hipGetDeviceProperties(&properties, device) != hipSuccess) {
        std::fprintf(stderr, "prefill gemm: HIP device query failed; using hipBLASEx\n");
        return nullptr;
    }
    std::string arch(properties.gcnArchName);
    const auto suffix = arch.find(':');
    if (suffix != std::string::npos) arch.resize(suffix);

    std::string error;
    if (!state->table.load(path, arch, version, error)) {
        std::fprintf(stderr, "prefill gemm: %s; using hipBLASEx\n", error.c_str());
        return nullptr;
    }
    std::fprintf(stderr, "prefill gemm: hipBLASLt tuning enabled (%zu rows, %s, version %d)\n",
                 state->table.rows().size(), arch.c_str(), version);
    return state;
}

HipLtCachedAlgo resolve_hipblaslt_algo(HipLtState& state, strata::prefill::hipblaslt::InputType type, int t,
                                       int n, int k, int ldy, float beta) {
    uint32_t beta_bits = 0;
    static_assert(sizeof(beta_bits) == sizeof(beta));
    std::memcpy(&beta_bits, &beta, sizeof(beta));
    const HipLtCallKey key{type, t, n, k, ldy, beta_bits};
    const auto cached = state.cache.find(key);
    if (cached != state.cache.end()) return cached->second;

    HipLtCachedAlgo resolved;
    const bool verbose = std::getenv("STRATA_HIPBLASLT_VERBOSE") != nullptr;
    const auto* row = state.table.closest(type, n, k, ldy, t);
    if (!row) {
        if (verbose) {
            std::fprintf(stderr, "prefill gemm: Lt fallback; no calibration for dtype=%s T=%d N=%d K=%d ldy=%d\n",
                         type == strata::prefill::hipblaslt::InputType::bf16 ? "bf16" : "f16", t, n, k, ldy);
        }
        return state.cache.emplace(key, resolved).first->second;
    }

    HipLtDescriptors desc;
    const hipDataType input_type = type == strata::prefill::hipblaslt::InputType::bf16 ? HIP_R_16BF : HIP_R_16F;
    if (!desc.init(input_type, t, n, k, ldy)) {
        return state.cache.emplace(key, resolved).first->second;
    }

    std::vector<int> solution_ids{row->solution_id};
    std::vector<hipblasLtMatmulHeuristicResult_t> candidates;
    if (hipblaslt_ext::getAlgosFromIndex(state.handle, solution_ids, candidates) != HIPBLAS_STATUS_SUCCESS ||
        candidates.empty() || candidates.front().state != HIPBLAS_STATUS_SUCCESS ||
        hipblaslt_ext::getIndexFromAlgo(candidates.front().algo) != row->solution_id) {
        if (verbose) {
            std::fprintf(stderr, "prefill gemm: Lt fallback; solution %d unavailable for T=%d N=%d K=%d ldy=%d\n",
                         row->solution_id, t, n, k, ldy);
        }
        return state.cache.emplace(key, resolved).first->second;
    }

    const float alpha = 1.0f;
    size_t required_workspace = 0;
    auto algo = candidates.front().algo;
    if (hipblaslt_ext::matmulIsAlgoSupported(state.handle, desc.op, &alpha, desc.a, desc.b, &beta, desc.c, desc.c,
                                             algo, required_workspace) != HIPBLAS_STATUS_SUCCESS) {
        if (verbose) {
            std::fprintf(stderr, "prefill gemm: Lt fallback; solution %d rejects actual T=%d N=%d K=%d ldy=%d beta=%.9g\n",
                         row->solution_id, t, n, k, ldy, beta);
        }
        return state.cache.emplace(key, resolved).first->second;
    }

    resolved.supported = true;
    resolved.algo = algo;
    resolved.workspace_bytes = required_workspace;
    if (verbose) {
        std::fprintf(stderr,
                     "prefill gemm: Lt solution=%d dtype=%s T=%d N=%d K=%d ldy=%d beta=%.9g workspace=%zu\n",
                     row->solution_id, type == strata::prefill::hipblaslt::InputType::bf16 ? "bf16" : "f16", t, n,
                     k, ldy, beta, required_workspace);
    }
    return state.cache.emplace(key, resolved).first->second;
}

bool try_hipblaslt(void* opaque_state, strata::prefill::hipblaslt::InputType type, const uint16_t* x,
                   const uint16_t* w, float* y, int64_t t, int64_t n, int64_t k, int64_t ldy, float beta,
                   void* stream) {
    auto* state = static_cast<HipLtState*>(opaque_state);
    if (!state || t <= 0 || n <= 0 || k <= 0 || t > INT_MAX || n > INT_MAX || k > INT_MAX || ldy > INT_MAX ||
        ldy < n) {
        return false;
    }
    const auto resolved = resolve_hipblaslt_algo(*state, type, (int) t, (int) n, (int) k, (int) ldy, beta);
    if (!resolved.supported) {
        ++state->fallbacks;
        state->fallback_shapes.emplace(type, (int) t, (int) n, (int) k, (int) ldy);
        return false;
    }
    if (resolved.workspace_bytes > state->workspace_bytes) {
        ++state->fallbacks;
        state->fallback_shapes.emplace(type, (int) t, (int) n, (int) k, (int) ldy);
        if (std::getenv("STRATA_HIPBLASLT_VERBOSE")) {
            std::fprintf(stderr, "prefill gemm: Lt fallback; solution needs %zu workspace bytes, have %zu\n",
                         resolved.workspace_bytes, state->workspace_bytes);
        }
        return false;
    }

    HipLtDescriptors desc;
    const hipDataType input_type = type == strata::prefill::hipblaslt::InputType::bf16 ? HIP_R_16BF : HIP_R_16F;
    if (!desc.init(input_type, (int) t, (int) n, (int) k, (int) ldy)) return false;
    const float alpha = 1.0f;
    const hipblasStatus_t status = hipblasLtMatmul(state->handle, desc.op, &alpha, w, desc.a, x, desc.b, &beta, y,
                                                   desc.c, y, desc.c, &resolved.algo, state->workspace,
                                                   state->workspace_bytes, (hipStream_t) stream);
    if (status == HIPBLAS_STATUS_SUCCESS) {
        ++state->lt_launches;
        return true;
    }

    std::fprintf(stderr, "prefill gemm: hipBLASLt launch failed with status %d\n", (int) status);
    if (beta != 0.0f) {
        std::fprintf(stderr, "prefill gemm: refusing a fallback after hipBLASLt failed with nonzero beta\n");
        std::exit(1);
    }
    auto* mutable_state = static_cast<HipLtState*>(opaque_state);
    uint32_t beta_bits = 0;
    std::memcpy(&beta_bits, &beta, sizeof(beta_bits));
    auto cached = mutable_state->cache.find(HipLtCallKey{type, (int) t, (int) n, (int) k, (int) ldy, beta_bits});
    if (cached != mutable_state->cache.end()) cached->second.supported = false;
    ++mutable_state->fallbacks;
    mutable_state->fallback_shapes.emplace(type, (int) t, (int) n, (int) k, (int) ldy);
    return false;
}
#endif

}  // namespace

Gemm::~Gemm() {
#if defined(__HIPCC__) && defined(STRATA_HIPBLASLT_AVAILABLE)
    delete static_cast<HipLtState*>(hipblaslt_state_);
#endif
    if (handle_) delete ((dpct::blas::descriptor_ptr)handle_);
    if (!external_) {
        if (scratch_) sycl::free(scratch_, dpct::get_in_order_queue());
        if (workspace_) sycl::free(workspace_, dpct::get_in_order_queue());
    }
}

bool Gemm::init_external(void *stream, uint16_t *scratch, int64_t scratch_elems,
                         void *workspace, size_t ws_bytes,
                         std::string &err) try {
    dpct::blas::descriptor_ptr h = nullptr;
    if (const int s = DPCT_CHECK_ERROR(h = new dpct::blas::descriptor());
        s != 0) {
        err = "prefill gemm: cublasCreate: cuBLAS status " + std::to_string((int) s);
        return false;
    }
    handle_ = h;
    stream_ = stream;
    external_ = true;
    note(DPCT_CHECK_ERROR(h->set_queue(strata::q_of(stream))),
         "cublasSetStream");
    workspace_ = workspace;
    workspace_bytes_ = ws_bytes;
    /*
    DPCT1027: The call to cublasSetWorkspace was replaced with 0 because
    this functionality is redundant in SYCL.
    */
    note(0, "cublasSetWorkspace");
    note(DPCT_CHECK_ERROR(h->set_math_mode(dpct::blas::math_mode::mm_default)),
         "cublasSetMathMode");
    scratch_ = scratch;
    scratch_elems_ = scratch_elems;
#if defined(__HIPCC__) && defined(STRATA_HIPBLASLT_AVAILABLE)
    hipblaslt_state_ = create_hipblaslt_state(workspace_, ws_bytes).release();
#endif
    return true;
}
catch (sycl::exception const &exc) {
  std::cerr << exc.what() << "Exception caught at file:" << __FILE__
            << ", line:" << __LINE__ << std::endl;
  std::exit(1);
}

void Gemm::rebind(uint16_t* scratch, int64_t scratch_elems, void* workspace, size_t ws_bytes) {
    scratch_ = scratch;
    scratch_elems_ = scratch_elems;
    workspace_ = workspace;
    workspace_bytes_ = ws_bytes;
    /*
    DPCT1026: The call to cublasSetWorkspace was removed because this
    functionality is redundant in SYCL.
    */
#if defined(__HIPCC__) && defined(STRATA_HIPBLASLT_AVAILABLE)
    if (hipblaslt_state_) {
        auto* state = static_cast<HipLtState*>(hipblaslt_state_);
        state->workspace = workspace_;
        state->workspace_bytes = ws_bytes;
    }
#endif
}

bool Gemm::init(void *stream, int64_t scratch_elems, std::string &err) try {
    // #240: every failure names the call and the real status, so "no VRAM" can be told from a broken install
    dpct::blas::descriptor_ptr h = nullptr;
    if (const int s = DPCT_CHECK_ERROR(h = new dpct::blas::descriptor());
        s != 0) {
        err = "prefill gemm: cublasCreate: cuBLAS status " + std::to_string((int) s);
        return false;
    }
    handle_ = h;
    stream_ = stream;
    note(DPCT_CHECK_ERROR(h->set_queue(strata::q_of(stream))),
         "cublasSetStream");
    // A fixed workspace so the handle never allocates on the way (and graphs could capture it later).
    // ponytail: keep the old budget unless GPU widening is explicitly enabled.
    const size_t ws = [] {
        const char* f16 = std::getenv("STRATA_PREFILL_GPU_WIDEN");
        const char* bf16 = std::getenv("STRATA_PREFILL_BF16_GPU_WIDEN");
        return ((f16 && std::strcmp(f16, "1") == 0) || (bf16 && std::strcmp(bf16, "1") == 0))
                   ? 128u << 20 : 32u << 20;
    }();
    workspace_bytes_ = ws;
    /*
    DPCT1000: Error handling if-stmt was detected but could not be
    rewritten.
    */
    if (const dpct::err0 e =
            DPCT_CHECK_ERROR(workspace_ = (void *)sycl::malloc_device(
                                 ws, dpct::get_in_order_queue()));
        e != 0) {
        /*
        DPCT1009: SYCL reports errors using exceptions and does not use
        error codes. Please replace the "get_error_string_dummy(...)" with a
        real error-handling function.
        */
        /*
        DPCT1001: The statement could not be removed.
        */
        err = std::string("prefill gemm: workspace of ") + std::to_string(ws >> 20) + " MiB: " +
              dpct::get_error_string_dummy(e);
        return false;
    }
    /*
    DPCT1027: The call to cublasSetWorkspace was replaced with 0 because
    this functionality is redundant in SYCL.
    */
    note(0, "cublasSetWorkspace");
    note(DPCT_CHECK_ERROR(h->set_math_mode(dpct::blas::math_mode::mm_default)),
         "cublasSetMathMode");
#if defined(__HIPCC__) && defined(STRATA_HIPBLASLT_AVAILABLE)
    hipblaslt_state_ = create_hipblaslt_state(workspace_, ws).release();
#endif
    if (scratch_elems > 0) {
        /*
        DPCT1000: Error handling if-stmt was detected but could not be
        rewritten.
        */
        if (const dpct::err0 e = DPCT_CHECK_ERROR(
                scratch_ = (uint16_t *)sycl::malloc_device(
                    (size_t)scratch_elems * 2, dpct::get_in_order_queue()));
            e != 0) {
            /*
            DPCT1001: The statement could not be removed.
            */
            err = "prefill gemm: dequant scratch of " +
                  std::to_string(scratch_elems * 2 >> 20) + " MiB: " +
                  /*
                  DPCT1009: SYCL reports errors using exceptions and does
                  not use error codes. Please replace the
                  "get_error_string_dummy(...)" with a real error-handling
                  function.
                  */
                  dpct::get_error_string_dummy(e);
            return false;
        }
    }
    scratch_elems_ = scratch_elems;
    return true;
}
catch (sycl::exception const &exc) {
  std::cerr << exc.what() << "Exception caught at file:" << __FILE__
            << ", line:" << __LINE__ << std::endl;
  std::exit(1);
}

namespace {
// SYCL port (Windows/OpenCL bring-up): oneMKL SYCL BLAS answers "does not support this API" on the OpenCL
// backend, so the prompt GEMMs fall back to this plain kernel when oneMKL throws unimplemented. Correct, not
// fast (a B70 does low hundreds of GFLOPS here against XMX TFLOPS): prompts still read, at seconds per 2K
// tokens, until Level Zero (or a oneMKL with an OpenCL Xe2 backend) restores the fast path. oneMKL stays
// first wherever it runs, so Linux/Level Zero timings do not change; the fallback engages per process once
// logged. Y[T,N] fp32 = alpha * X[T,K] . W[N,K]^T + beta * Y[T,N]; X/W are bf16 (as_fp=0) or fp16 (as_fp=1)
// bits with row strides ldx/K.
inline float half_bits_to_float(uint16_t b, bool as_fp) {
    if (!as_fp) return sycl::bit_cast<float>((uint32_t) b << 16);   // bf16: mantissa-compatible shift
    // fp16 bits -> fp32 (subnormals flushed; activations/weights never need them)
    const uint32_t sign = (uint32_t) (b & 0x8000) << 16;
    int exp = (b >> 10) & 0x1F;
    uint32_t mant = (uint32_t) (b & 0x3FF);
    if (exp == 0) return sycl::bit_cast<float>(sign);   // zero/subnormal -> signed zero
    if (exp == 31) return sycl::bit_cast<float>(sign | 0x7F800000u | (mant << 13));   // inf/nan
    return sycl::bit_cast<float>(sign | ((uint32_t) (exp + 112) << 23) | (mant << 13));
}

void fallback_gemm(const uint16_t* X, const uint16_t* W, float* Y, int64_t T, int64_t N, int64_t K,
                   int64_t ldx, int64_t ldy, float alpha, float beta, bool as_fp, void* stream) {
    if (ldx <= 0) ldx = K;
    if (ldy <= 0) ldy = N;
    sycl::queue* q = strata::q_of(stream);
    constexpr int64_t TM = 4;   // outputs per work-item along N
    const size_t ncols = (size_t) ((N + TM - 1) / TM);
    q->parallel_for(sycl::range<2>((size_t) T, ncols), [=](sycl::id<2> id) {
        const int64_t t = (int64_t) id[0];
        const int64_t n0 = (int64_t) id[1] * TM;
        float acc[TM];
        for (int64_t m = 0; m < TM; ++m) acc[m] = 0.0f;
        for (int64_t k = 0; k < K; ++k) {
            const float x = half_bits_to_float(X[t * ldx + k], as_fp);
            for (int64_t m = 0; m < TM; ++m) {
                const int64_t n = n0 + m;
                if (n < N) acc[m] += x * half_bits_to_float(W[n * K + k], as_fp);
            }
        }
        for (int64_t m = 0; m < TM; ++m) {
            const int64_t n = n0 + m;
            // beta = 0 must not read Y (the caller's buffer is uninitialized there: 0 * NaN is NaN).
            if (n < N) Y[t * ldy + n] = beta == 0.0f ? alpha * acc[m] : alpha * acc[m] + beta * Y[t * ldy + n];
        }
    }).wait();
}

bool fallback_once_logged = false;
bool onemkl_broken = false;   // set after the first oneMKL failure: skip its (noisy) attempt afterwards
void fallback_note() {
    if (!fallback_once_logged) {
        fallback_once_logged = true;
        std::fprintf(stderr, "prefill gemm: oneMKL SYCL BLAS is not usable on this backend; "
                             "plain SYCL fallback (slow prompts, same answers)\n");
    }
    onemkl_broken = true;
}

// oneMKL SYCL BLAS has no OpenCL Xe2 backend: its attempt not only fails but poisons the queue
// (UR_RESULT_ERROR_IN_EVENT_LIST_EXEC_STATUS on what follows), so skip it outright there.
bool onemkl_usable(void* stream) {
    if (onemkl_broken) return false;
    try {
        if (strata::q_of(stream)->get_backend() == sycl::backend::opencl) {
            fallback_note();
            return false;
        }
    } catch (...) {}
    return true;
}

// Dense 16-bit input -> FP32 output, without an FP32 weight copy.
template <class Input>
void native_xmx_launch(const uint16_t* X, const uint16_t* W, float* Y,
                       int64_t T, int64_t N, int64_t K, int64_t ldx, sycl::queue* q) {
    namespace jm = sycl::ext::oneapi::experimental::matrix;
    constexpr int SG = 8, NSG = 8, TILE = 8;
    const auto* x = reinterpret_cast<const Input*>(X);
    const auto* w = reinterpret_cast<const Input*>(W);
    q->submit([&](sycl::handler& h) {
        sycl::local_accessor<float, 1> tail(sycl::range<1>(NSG * TILE * TILE), h);
        h.parallel_for(sycl::nd_range<3>(sycl::range<3>((T + TILE - 1) / TILE,
            (N + TILE * NSG - 1) / (TILE * NSG), SG * NSG), sycl::range<3>(1, 1, SG * NSG)),
            [=](sycl::nd_item<3> it) [[sycl::reqd_sub_group_size(SG)]] {
                const auto sg = it.get_sub_group();
                const int subgroup = int(sg.get_group_id()[0]);
                const int lane = int(sg.get_local_id()[0]);
                const int64_t row = int64_t(it.get_group(0)) * TILE;
                const int64_t col = (int64_t(it.get_group(1)) * NSG + subgroup) * TILE;
                if (col >= N) return;
                auto gptr = [](auto* p) { return sycl::address_space_cast<sycl::access::address_space::global_space,
                    sycl::access::decorated::no>(p); };
                jm::joint_matrix<sycl::sub_group, float, jm::use::accumulator, TILE, TILE> c;
                jm::joint_matrix_fill(sg, c, 0.f);
                for (int64_t k = 0; k < K; k += 16) {
                    jm::joint_matrix<sycl::sub_group, Input, jm::use::a, TILE, 16, jm::layout::row_major> a;
                    jm::joint_matrix<sycl::sub_group, Input, jm::use::b, 16, TILE, jm::layout::col_major> b;
                    jm::joint_matrix_load(sg, a, gptr(x + row * ldx + k), size_t(ldx));
                    jm::joint_matrix_load(sg, b, gptr(w + col * K + k), size_t(K));
                    jm::joint_matrix_mad(sg, c, a, b, c);
                }
                if (row + TILE <= T) {
                    jm::joint_matrix_store(sg, c, gptr(Y + row * N + col), size_t(N), jm::layout::row_major);
                } else {
                    auto* tmp = tail.get_multi_ptr<sycl::access::decorated::no>().get() + subgroup * TILE * TILE;
                    jm::joint_matrix_store(sg, c, sycl::address_space_cast<sycl::access::address_space::local_space,
                        sycl::access::decorated::no>(tmp), TILE, jm::layout::row_major);
                    sycl::group_barrier(sg);
                    for (int i = lane; i < TILE * TILE; i += SG)
                        if (row + i / TILE < T) Y[(row + i / TILE) * N + col + i % TILE] = tmp[i];
                }
            });
    });
}

bool native_xmx_gemm(const uint16_t* X, const uint16_t* W, float* Y,
                      int64_t T, int64_t N, int64_t K, int64_t ldx, int64_t ldy, float beta,
                      void* workspace, size_t bytes, void* stream, bool bf16 = false) {
    static const bool enabled = [] {
        const char* v = std::getenv("STRATA_PREFILL_NATIVE_XMX");
        return v && std::strcmp(v, "1") == 0;
    }();
    // ponytail: only aligned contiguous beta=0 products; retain the original path for other layouts.
    if (!enabled || T <= 0 || N <= 0 || K <= 0 || ldx < K || ldy != N || beta != 0.f || N % 8 || K % 16)
        return false;
    auto* q = strata::q_of(stream);
    if (!q->is_in_order()) return false;
    namespace jm = sycl::ext::oneapi::experimental::matrix;
    namespace qi = sycl::ext::oneapi::experimental::info;
    // This engine targets one Intel GPU; query the actual driver rather than assume DG2 support.
    static const auto supported = [q] {
        std::array<bool, 2> result{};
        try {
            const auto sizes = q->get_device().get_info<sycl::info::device::sub_group_sizes>();
            if (std::find(sizes.begin(), sizes.end(), size_t(8)) == sizes.end()) return result;
            for (const auto& c : q->get_device().get_info<qi::device::matrix_combinations>()) {
                if (c.ctype != jm::matrix_type::fp32 || c.dtype != jm::matrix_type::fp32 ||
                    !(c.msize == 8 || c.max_msize >= 8) || !(c.nsize == 8 || c.max_nsize >= 8) ||
                    !(c.ksize == 16 || c.max_ksize >= 16)) continue;
                if (c.atype == jm::matrix_type::fp16 && c.btype == jm::matrix_type::fp16) result[0] = true;
                if (c.atype == jm::matrix_type::bf16 && c.btype == jm::matrix_type::bf16) result[1] = true;
            }
        } catch (...) {}
        return result;
    }();
    if (!supported[bf16]) return false;
    const bool pad = T % 8 != 0 || ldx % 8 != 0;
    if (pad) {
        const size_t rows = size_t((T + 7) / 8) * 8;
        if (!workspace || rows > (bytes / sizeof(uint16_t)) / size_t(K)) return false;
        auto* padded = static_cast<uint16_t*>(workspace);
        q->parallel_for(sycl::range<1>(rows * size_t(K)), [=](sycl::id<1> p) {
            const size_t row = p[0] / K, col = p[0] % K;
            padded[p] = row < size_t(T) ? X[row * ldx + col] : uint16_t(0);
        });
        X = padded;
        ldx = K;
    }
    if (bf16) native_xmx_launch<sycl::ext::oneapi::bfloat16>(X, W, Y, T, N, K, ldx, q);
    else native_xmx_launch<sycl::half>(X, W, Y, T, N, K, ldx, q);
    q->wait_and_throw();
    static const bool profile = [] {
        const char* v = std::getenv("STRATA_PREFILL_XMX_PROFILE");
        return v && std::strcmp(v, "1") == 0;
    }();
    if (profile) std::fprintf(stderr, "NATIVE_XMX_CALL {\"bf16\":%s,\"T\":%lld,\"N\":%lld,\"K\":%lld,\"padded\":%s}\n",
        bf16 ? "true" : "false", (long long)T, (long long)N, (long long)K, pad ? "true" : "false");
    return true;
}


bool native_half_output_gemm(const uint16_t* X, const uint16_t* W, float* Y,
                            int64_t T, int64_t N, int64_t K, int64_t ldy, float beta, void* stream, bool bf16=false) {
    static const int mode=[] {
        const char* v=std::getenv("STRATA_PREFILL_HALF_OUTPUT");
        return !v ? 0 : std::strcmp(v,"1")==0 ? 1 : std::strcmp(v,"256")==0 ? 256 :
            std::strcmp(v,"512")==0 ? 512 : std::strcmp(v,"1024")==0 ? 1024 : 0;
    }();
    static const bool convert_bf16=[] {
        const char* v=std::getenv("STRATA_PREFILL_HALF_BF16");
        return v && std::strcmp(v,"1")==0;
    }();
    // ponytail: experimental, opt-in contiguous beta=0 only; existing paths handle padding and accumulation.
    if (!mode || (bf16 && !convert_bf16) || ldy!=N || beta!=0.f || K<=0 || (T==1 && N==1)) return false;
    auto* q=strata::q_of(stream);
    if (!q->is_in_order()) return false;
    static const bool profile=[] {
        const char* v=std::getenv("STRATA_PREFILL_HALF_PROFILE");
        return v && std::strcmp(v,"1")==0;
    }();
    const auto start=profile?std::chrono::steady_clock::now():std::chrono::steady_clock::time_point{};
    q->wait_and_throw();
    auto release=[q](auto* p) { sycl::free(p,*q); };
    std::unique_ptr<sycl::half,decltype(release)> partial(sycl::malloc_device<sycl::half>((size_t)T*N,*q),release);
    std::unique_ptr<int,decltype(release)> bad(sycl::malloc_shared<int>(1,*q),release);
    std::unique_ptr<sycl::half,decltype(release)> wh(bf16?sycl::malloc_device<sycl::half>((size_t)N*K,*q):nullptr,release);
    std::unique_ptr<sycl::half,decltype(release)> xh(bf16?sycl::malloc_device<sycl::half>((size_t)T*K,*q):nullptr,release);
    if (!partial || !bad || (bf16 && (!wh || !xh))) return false;
    *bad=0;
    auto* h=partial.get();
    auto* invalid=bad.get();
    const sycl::half* weights=reinterpret_cast<const sycl::half*>(W);
    const sycl::half* inputs=reinterpret_cast<const sycl::half*>(X);
    if (bf16) {
        auto* converted_w=wh.get();
        auto* converted_x=xh.get();
        const size_t nw=(size_t)N*K;
        q->parallel_for(sycl::range<1>(nw+(size_t)T*K),[=](sycl::id<1> p) {
            const size_t i=p[0];
            const uint16_t v=i<nw?W[i]:X[i-nw];
            const sycl::half value(sycl::bit_cast<float>(uint32_t(v)<<16));
            if (i<nw) converted_w[i]=value; else converted_x[i-nw]=value;
            if (!sycl::isfinite(float(value)))
                sycl::atomic_ref<int,sycl::memory_order::relaxed,sycl::memory_scope::device,
                    sycl::access::address_space::global_space>(*invalid).store(1);
        });
        // The in-order queue completes conversion before GEMM; a bad conversion still forces the final retry.
        weights=converted_w;inputs=converted_x;
    }
    const int64_t block=mode==1?K:mode;
    for (int64_t offset=0;offset<K;offset+=block) {
        oneapi::mkl::blas::column_major::gemm(*q,oneapi::mkl::transpose::trans,oneapi::mkl::transpose::nontrans,
            N,T,std::min(block,K-offset),sycl::half(1),weights+offset,K,inputs+offset,K,
            sycl::half(0),h,N,oneapi::mkl::blas::compute_mode::unset);
        q->parallel_for(sycl::range<1>((size_t)T*N),[=](sycl::id<1> i) {
            const float value=(offset?Y[i]:0.f)+float(h[i]);
            Y[i]=value;
            if (!sycl::isfinite(value))
                sycl::atomic_ref<int,sycl::memory_order::relaxed,sycl::memory_scope::device,
                    sycl::access::address_space::global_space>(*invalid).store(1);
        });
    }
    q->wait_and_throw();
    // Log before returning: serve uses _Exit, which skips atexit callbacks. Excludes buffer destruction/retry.
    if (profile) std::fprintf(stderr,"NATIVE_HALF_CALL {\"mode\":%d,\"bf16\":%s,\"T\":%lld,\"N\":%lld,\"K\":%lld,\"body_us\":%.3f,\"fallback\":%s}\n",
        mode,bf16?"true":"false",(long long)T,(long long)N,(long long)K,
        std::chrono::duration<double,std::micro>(std::chrono::steady_clock::now()-start).count(),*bad?"true":"false");
    return !*bad;
}

bool gpu_widen_gemm(const uint16_t* X, const uint16_t* W, float* Y, int64_t T, int64_t N, int64_t K,
                    int64_t ldx, int64_t ldy, float beta, void* workspace, size_t bytes, void* stream, bool bf16 = false) {
    static const bool enabled = [] {
        const char* v = std::getenv("STRATA_PREFILL_GPU_WIDEN");
        return v && std::strcmp(v, "1") == 0;
    }();
    static const bool bf16_enabled = [] {
        const char* v = std::getenv("STRATA_PREFILL_BF16_GPU_WIDEN");
        return v && std::strcmp(v, "1") == 0;
    }();
    // ponytail: retain oneMKL's padding writes for interleaved outputs and native row slices.
    if (!(bf16 ? bf16_enabled : enabled) || ldy != N || !workspace || K <= 0 || ldx < K) return false;
    // oneMKL's scalar dot product uses a different reduction order from FP32 GEMM.
    if (T == 1 && N == 1) return false;
    auto* q = strata::q_of(stream);
    if (!q->is_in_order()) return false;
    const size_t capacity = bytes / sizeof(float);
    if ((uint64_t) N > capacity / K) return false;
    const size_t nw = (size_t) N * K;
    if ((uint64_t) T > (capacity - nw) / ldx) return false;
    // oneMKL 2025.3/A770 corrupts a C slice above 4 GiB when A/B/C share one allocation.
    // ponytail: stage C in its own allocation; reuse this buffer only if allocation becomes a measured bottleneck.
    auto release = [q](float* p) { sycl::free(p, *q); };
    std::unique_ptr<float, decltype(release)> output(sycl::malloc_device<float>((size_t) T * ldy, *q), release);
    if (!output) return false;
    q->memcpy(output.get(), Y, (size_t) T * ldy * sizeof(float));
    auto* wf = static_cast<float*>(workspace);
    auto* xf = wf + nw;
    // ponytail: use the existing bounded workspace; large products keep the original path.
    // A770 oneMKL promotes 16-bit inputs to FP32 through host staging. Keep its FP32 GEMM and move only widening.
    q->parallel_for(sycl::range<1>(nw), [=](sycl::id<1> i) {
        const uint16_t v = W[i];
        wf[i] = bf16 ? sycl::bit_cast<float>(uint32_t(v) << 16) : float(sycl::bit_cast<sycl::half>(v));
    });
    q->parallel_for(sycl::range<1>((size_t) T * K), [=](sycl::id<1> i) {
        const size_t offset = (size_t(i) / K) * ldx + size_t(i) % K;
        const uint16_t v = X[offset];
        xf[offset] = bf16 ? sycl::bit_cast<float>(uint32_t(v) << 16) : float(sycl::bit_cast<sycl::half>(v));
    });
    // ponytail: the in-order queue orders conversion, GEMM and copy; the final wait protects all input lifetimes.
    oneapi::mkl::blas::column_major::gemm(*q, oneapi::mkl::transpose::trans,
        oneapi::mkl::transpose::nontrans, N, T, K, 1.f, wf, K, xf, ldx, beta, output.get(), ldy,
        oneapi::mkl::blas::compute_mode::unset);
    q->memcpy(Y, output.get(), (size_t) T * ldy * sizeof(float)).wait_and_throw();
    return true;
}
}  // namespace

void Gemm::bf16(const uint16_t* X, const uint16_t* W, float* Y, int64_t T, int64_t N, int64_t K, int64_t ldy,
                float beta, int64_t ldx) {
    if (T <= 0 || N <= 0) return;
    if (ldy <= 0) ldy = N;
    if (ldx <= K) ldx = 0;   // upstream: ldx (> K) is X's padded row stride, 0 = K
    const float alpha = 1.0f;
#if defined(__HIPCC__) && defined(STRATA_HIPBLASLT_AVAILABLE)
    if (try_hipblaslt(hipblaslt_state_, strata::prefill::hipblaslt::InputType::bf16, X, W, Y, T, N, K, ldy,
                      beta, stream_)) {
        STRATA_ABSORB_HIPBLAS_STICKY("hipBLASLt bf16");
        return;
    }
#endif
    // Column-major view: Y^T[N, T] = W[N, K] (stored K x N col-major, transposed) . X^T[K, T].
    // oneMKL SYCL BLAS has no OpenCL Xe2 backend ("does not support this API"): fall back to the plain
    // SYCL kernel there (slow prompts, same answers) instead of failing the prompt.
    if (!onemkl_usable(stream_) ||
        DPCT_CHECK_ERROR([&] {
           if (native_xmx_gemm(X,W,Y,T,N,K,ldx?ldx:K,ldy,beta,workspace_,workspace_bytes_,stream_,true)) return;
           if (!ldx && native_half_output_gemm(X,W,Y,T,N,K,ldy,beta,stream_,true)) return;
           if (gpu_widen_gemm(X, W, Y, T, N, K, ldx ? ldx : K, ldy, beta,
                             workspace_, workspace_bytes_, stream_, true)) return;
           dpct::blas::gemm(
           (dpct::blas::descriptor_ptr)handle_, oneapi::mkl::transpose::trans,
           oneapi::mkl::transpose::nontrans, (int)N, (int)T, (int)K, &alpha, W,
           dpct::library_data_t::real_bfloat16, (int)K, X,
           dpct::library_data_t::real_bfloat16, (int)(ldx ? ldx : K), &beta, Y,
           dpct::library_data_t::real_float, (int)ldy,
           dpct::compute_type::f32);
        }()) != 0) {
        fallback_note();
        fallback_gemm(X, W, Y, T, N, K, ldx ? ldx : K, ldy, alpha, beta, false, stream_);
        return;
    }
    STRATA_ABSORB_HIPBLAS_STICKY("cublasGemmEx");
}

void Gemm::f16(const uint16_t* X, const uint16_t* W, float* Y, int64_t T, int64_t N, int64_t K, int64_t ldy,
               float beta) {
    if (T <= 0 || N <= 0) return;
    if (ldy <= 0) ldy = N;
    const float alpha = 1.0f;
#if defined(__HIPCC__) && defined(STRATA_HIPBLASLT_AVAILABLE)
    if (try_hipblaslt(hipblaslt_state_, strata::prefill::hipblaslt::InputType::f16, X, W, Y, T, N, K, ldy,
                      beta, stream_)) {
        STRATA_ABSORB_HIPBLAS_STICKY("hipBLASLt f16");
        return;
    }
#endif
    if (!onemkl_usable(stream_) ||
        DPCT_CHECK_ERROR([&] {
           if (native_xmx_gemm(X,W,Y,T,N,K,K,ldy,beta,workspace_,workspace_bytes_,stream_)) return;
           if (native_half_output_gemm(X,W,Y,T,N,K,ldy,beta,stream_)) return;
           if (gpu_widen_gemm(X, W, Y, T, N, K, K, ldy, beta,
                             workspace_, workspace_bytes_, stream_)) return;
           dpct::blas::gemm(
           (dpct::blas::descriptor_ptr)handle_, oneapi::mkl::transpose::trans,
           oneapi::mkl::transpose::nontrans, (int)N, (int)T, (int)K, &alpha, W,
           dpct::library_data_t::real_half, (int)K, X,
           dpct::library_data_t::real_half, (int)K, &beta, Y,
           dpct::library_data_t::real_float, (int)ldy,
           dpct::compute_type::f32);
        }()) != 0) {
        fallback_note();
        fallback_gemm(X, W, Y, T, N, K, K, ldy, alpha, beta, true, stream_);
        return;
    }
    STRATA_ABSORB_HIPBLAS_STICKY("cublasGemmEx f16");
}

void Gemm::native(const uint16_t* X, int ggml_type, const void* W_blocks, float* Y, int64_t T, int64_t N, int64_t K,
                  int64_t ldy, float beta, int64_t ldx) {
    if (ldx > 0 && ldx != K) {   // upstream: a padded X needs STRATA_PF_PAD's HIP path; fatal elsewhere
        std::fprintf(stderr, "prefill gemm: a padded X (ldx %lld, K %lld) needs STRATA_PF_PAD's path\n",
                     (long long) ldx, (long long) K);
        std::exit(1);
    }
    if (N * K > scratch_elems_) {
        // Too large for the scratch at once: in row slices.
        const int64_t rows = scratch_elems_ / K;
        if (rows <= 0) { std::fprintf(stderr, "prefill gemm: scratch too small for K=%lld\n", (long long) K); std::exit(1); }
        if (ldy <= 0) ldy = N;
        for (int64_t r0 = 0; r0 < N; r0 += rows) {
            const int64_t n = (N - r0 < rows) ? N - r0 : rows;
            strata::kernels::dequant_f16(ggml_type, W_blocks, r0, n, K, scratch_, stream_);
            f16(X, scratch_, Y + r0, T, n, K, ldy, beta);
        }
        return;
    }
    strata::kernels::dequant_f16(ggml_type, W_blocks, 0, N, K, scratch_, stream_);
    f16(X, scratch_, Y, T, N, K, ldy, beta);
}

}  // namespace strata::prefill
