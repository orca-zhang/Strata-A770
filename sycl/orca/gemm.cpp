// Strata's prefill GEMM API on native XMX kernels and a caller-reusable workspace.
#include <dpct/dpct.hpp>
#include <strata/prefill/gemm.hpp>
#include <strata/prefill/fp16_pair.hpp>
#include <strata/prefill/kernels.hpp>
#include <strata/kernels/dequant_bf16.hpp>
#include <climits>
#include <array>

namespace strata::prefill {
void orca_xmx_gemm(const uint16_t*, const uint16_t*, float*, int64_t, int64_t, int64_t,
                   int64_t, const uint16_t*, bool, void*);
namespace {
size_t bytes(size_t rows, size_t columns, size_t element) {
    if (columns > SIZE_MAX / element || rows > SIZE_MAX / (columns * element))
        throw std::invalid_argument("native GEMM size overflow");
    return rows * columns * element;
}
size_t aligned(size_t size) {
    if (size > SIZE_MAX - 63) throw std::invalid_argument("native GEMM workspace overflow");
    return (size + 63) & ~size_t(63);
}

struct MatrixRequest {
    const uint16_t* x;
    const uint16_t* w;
    float* y;
    int64_t t, n, k, ldx, ldy;
    float beta;
    bool bf16;
};
MatrixRequest validated(const uint16_t* x, const uint16_t* w, float* y, int64_t t, int64_t n, int64_t k,
                        int64_t ldx, int64_t ldy, float beta, bool bf16) {
    if (t < 0 || n < 0 || k <= 0 || t > INT_MAX || n > INT_MAX - 7 || k > INT_MAX - 15)
        throw std::invalid_argument("invalid native GEMM dimensions");
    if (ldx <= 0) ldx = k;
    if (ldy <= 0) ldy = n;
    if (t && n) {
        if (!x || !w || !y || ldx < k || ldy < n || ldx > INT_MAX)
            throw std::invalid_argument("invalid native GEMM pointers/strides");
        (void)bytes(size_t(t), size_t(ldx), 2);
        (void)bytes(size_t(n), size_t(k), 2);
        (void)bytes(size_t(t), size_t(ldy), 4);
    }
    return {x, w, y, t, n, k, ldx, ldy, beta, bf16};
}

void product(const uint16_t* x, const uint16_t* w, float* y, int64_t t, int64_t n, int64_t k,
             int64_t ldx, int64_t ldy, float beta, bool bf16, void* workspace, size_t capacity,
             void* stream, bool finish) {
    const auto request = validated(x, w, y, t, n, k, ldx, ldy, beta, bf16);
    if (!t || !n) return;
    ldx = request.ldx;
    ldy = request.ldy;
    const int64_t nk = (k + 15) / 16 * 16, nn = (n + 7) / 8 * 8;
    const bool weights_padded = nk != k || nn != n;
    const bool input_padded = nk != k || ldx % 8;
    const bool output_temporary = nn != n || ldy != n || beta != 0.f;
    const size_t weight_bytes = weights_padded ? aligned(bytes(size_t(nn), size_t(nk), 2)) : 0;
    const size_t tail_bytes = !input_padded && t % 8 ? aligned(bytes(8, size_t(nk), 2)) : 0;
    const size_t row_bytes = (input_padded ? size_t(nk) * 2 : 0) + (output_temporary ? size_t(nn) * 4 : 0);
    if (weight_bytes > capacity || tail_bytes > capacity - weight_bytes)
        throw std::runtime_error("native GEMM workspace cannot hold padding");
    const size_t fixed = weight_bytes + tail_bytes;
    const size_t room = capacity - fixed;
    // Chunk rows only when an unusual layout needs staging. The common aligned,
    // contiguous beta=0 product writes directly to Y, with at most eight tail rows.
    size_t chunk = row_bytes ? std::min<size_t>(size_t(t + 7) / 8 * 8, room / row_bytes / 8 * 8) : size_t(t);
    if (!chunk || ((fixed || row_bytes) && (!workspace || uintptr_t(workspace) % 64)))
        throw std::runtime_error("native GEMM needs an aligned, sufficiently large workspace");
    auto& queue = strata::l0::stream_of(stream);
    auto* base = static_cast<std::byte*>(workspace);
    const uint16_t* matrix = w;
    if (weights_padded) {
        auto* padded = reinterpret_cast<uint16_t*>(base);
        queue.parallel_for(sycl::range<1>(size_t(nn) * size_t(nk)), [=](sycl::id<1> item) {
            const int64_t row = int64_t(item[0] / nk), col = int64_t(item[0] % nk);
            padded[item] = row < n && col < k ? w[row * k + col] : uint16_t(0);
        });
        matrix = padded;
    }
    for (int64_t first = 0; first < t; first += int64_t(chunk)) {
        const int64_t rows = std::min<int64_t>(t - first, int64_t(chunk));
        const int64_t padded_rows = (rows + 7) / 8 * 8;
        const uint16_t* source = x + first * ldx;
        const uint16_t* tail = nullptr;
        int64_t stride = ldx;
        size_t offset = fixed;
        if (input_padded) {
            auto* padded = reinterpret_cast<uint16_t*>(base + offset);
            queue.parallel_for(sycl::range<1>(size_t(padded_rows) * size_t(nk)), [=](sycl::id<1> item) {
                const int64_t row = int64_t(item[0] / nk), col = int64_t(item[0] % nk);
                padded[item] = row < rows && col < k ? source[row * ldx + col] : uint16_t(0);
            });
            source = padded;
            stride = nk;
            offset += bytes(size_t(padded_rows), size_t(nk), 2);
            if (rows % 8) tail = padded + (rows - rows % 8) * nk;
        } else if (rows % 8) {
            auto* padded = reinterpret_cast<uint16_t*>(base + weight_bytes);
            const int64_t last = rows - rows % 8;
            queue.parallel_for(sycl::range<1>(8 * size_t(nk)), [=](sycl::id<1> item) {
                const int64_t row = last + int64_t(item[0] / nk), col = int64_t(item[0] % nk);
                padded[item] = row < rows ? source[row * ldx + col] : uint16_t(0);
            });
            tail = padded;
        }
        auto* destination = output_temporary ? reinterpret_cast<float*>(base + offset) : y + first * ldy;
        orca_xmx_gemm(source, matrix, destination, rows, nn, nk, stride, tail, bf16, &queue);
        if (output_temporary) {
            queue.parallel_for(sycl::range<1>(size_t(rows) * size_t(n)), [=](sycl::id<1> item) {
                const int64_t row = int64_t(item[0] / n), col = int64_t(item[0] % n);
                auto& value = y[(first + row) * ldy + col];
                const float dot = destination[row * nn + col];
                value = beta == 0.f ? dot : dot + beta * value;
            });
        }
    }
    if (finish) queue.wait_and_throw();
}

struct Span {
    uintptr_t begin, end;
    Span(const void* pointer, size_t size) : begin(uintptr_t(pointer)), end(begin + size) {
        if (size > UINTPTR_MAX - begin) throw std::invalid_argument("native GEMM address overflow");
    }
    bool overlaps(const Span& other) const { return begin < other.end && other.begin < end; }
};
Span input_span(const MatrixRequest& r) { return {r.x, bytes(size_t(r.t), size_t(r.ldx), 2)}; }
Span output_span(const MatrixRequest& r) { return {r.y, bytes(size_t(r.t), size_t(r.ldy), 4)}; }
Span weight_span(const MatrixRequest& r) { return {r.w, bytes(size_t(r.n), size_t(r.k), 2)}; }

struct ProductBatch {
    strata::l0::Queue& queue;
    void* workspace;
    size_t capacity;
    // ponytail: bounded scan over 16 ready requests, no worker or timer. Larger
    // scheduling windows belong to the model scheduler if measurements need it.
    std::array<MatrixRequest, 16> pending{};
    size_t count = 0;
    int64_t rows = 0;
    bool enabled = true;
    Gemm::BatchStats stats{};

    void single(const MatrixRequest& r) {
        product(r.x, r.w, r.y, r.t, r.n, r.k, r.ldx, r.ldy, r.beta, r.bf16,
                workspace, capacity, &queue, false);
        ++stats.products;
    }
    size_t packing_bytes(int64_t t, int64_t n, int64_t k) const {
        const size_t padded = size_t(t + 7) / 8 * 8;
        const size_t x = aligned(bytes(padded, size_t(k), 2));
        const size_t y = bytes(padded, size_t(n), 4);
        return x <= capacity && y <= capacity - x ? x + y : SIZE_MAX;
    }
    void enqueue(MatrixRequest r) {
        if (!r.t || !r.n) return;
        const auto out = output_span(r);
        if (out.overlaps(input_span(r)) || out.overlaps(weight_span(r)))
            throw std::invalid_argument("native GEMM output aliases an input");
        bool compatible = count < pending.size() && rows <= INT_MAX - 7 - r.t;
        if (count) {
            const auto& first = pending[0];
            compatible &= first.w == r.w && first.n == r.n && first.k == r.k && first.bf16 == r.bf16;
            for (size_t i = 0; compatible && i < count; ++i) {
                const auto previous = output_span(pending[i]);
                compatible = !previous.overlaps(out) && !previous.overlaps(input_span(r)) &&
                    !previous.overlaps(weight_span(r)) && !out.overlaps(input_span(pending[i])) &&
                    !out.overlaps(weight_span(pending[i]));
            }
        }
        if (!compatible || (count && packing_bytes(rows + r.t, r.n, r.k) > capacity)) queue.flush();
        const bool packable = enabled && workspace && uintptr_t(workspace) % 64 == 0 &&
            r.n % 8 == 0 && r.k % 16 == 0 && r.t <= INT_MAX - 7 && packing_bytes(r.t, r.n, r.k) <= capacity;
        if (!packable) {
            queue.flush();
            single(r);
        } else {
            queue.defer(this, [](void* owner) { static_cast<ProductBatch*>(owner)->flush(); });
            pending[count++] = r;
            rows += r.t;
        }
        ++stats.requests;
    }
    void flush() {
        if (!count) return;
        const auto requests = pending; // Kernel arguments own these descriptors; no in-flight host buffer reuse.
        const size_t total = std::exchange(count, 0);
        const int64_t t = std::exchange(rows, 0);
        const auto& first = requests[0];
        if (total == 1) { single(first); return; }
        bool contiguous = first.ldx == first.k && first.ldy == first.n;
        for (size_t i = 1; contiguous && i < total; ++i) {
            const auto& a = requests[i - 1];
            const auto& b = requests[i];
            contiguous = b.ldx == first.k && b.ldy == first.n && b.beta == first.beta &&
                b.x == a.x + a.t * a.ldx && b.y == a.y + a.t * a.ldy;
        }
        if (contiguous) {
            auto combined = first;
            combined.t = t;
            single(combined);
        } else {
            const int64_t n = first.n, k = first.k, padded = (t + 7) / 8 * 8;
            auto* x = static_cast<uint16_t*>(workspace);
            auto* y = reinterpret_cast<float*>(static_cast<std::byte*>(workspace) + aligned(bytes(size_t(padded), size_t(k), 2)));
            queue.parallel_for(sycl::range<1>(size_t(padded) * size_t(k)), [=](sycl::id<1> item) {
                int64_t row = int64_t(item[0] / k);
                const int64_t col = int64_t(item[0] % k);
                uint16_t value = 0;
                for (size_t i = 0; i < total; ++i) {
                    if (row < requests[i].t) { value = requests[i].x[row * requests[i].ldx + col]; break; }
                    row -= requests[i].t;
                }
                x[item] = value;
            });
            product(x, first.w, y, padded, n, k, k, n, 0.f, first.bf16, nullptr, 0, &queue, false);
            ++stats.products;
            queue.parallel_for(sycl::range<1>(size_t(t) * size_t(n)), [=](sycl::id<1> item) {
                int64_t row = int64_t(item[0] / n);
                const int64_t col = int64_t(item[0] % n);
                for (size_t i = 0; i < total; ++i) {
                    const auto& r = requests[i];
                    if (row < r.t) {
                        auto& value = r.y[row * r.ldy + col];
                        value = r.beta == 0.f ? y[item] : y[item] + r.beta * value;
                        break;
                    }
                    row -= r.t;
                }
            });
        }
        stats.merged_requests += total;
    }
};
}

Gemm::~Gemm() {
    if (!handle_) return;
    auto* batch = static_cast<ProductBatch*>(handle_);
    try {
        auto& queue = strata::l0::stream_of(stream_);
        queue.wait_and_throw();
        if (!external_) {
            strata::l0::free(scratch_, queue);
            strata::l0::free(workspace_, queue);
        }
    } catch (...) {} // Runtime retains the resources if the driver cannot drain.
    batch->queue.cancel_deferred(batch);
    delete batch;
}

bool Gemm::init(void* stream, int64_t count, std::string& error) try {
    if (handle_ || count < 0 || uint64_t(count) > SIZE_MAX / 2)
        throw std::invalid_argument("invalid/repeated native GEMM initialization");
    auto& queue = strata::l0::stream_of(stream);
    constexpr size_t workspace_bytes = 32u << 20;
    auto* scratch = strata::l0::malloc_device<uint16_t>(size_t(count), queue);
    void* workspace = nullptr;
    try { workspace = strata::l0::malloc_device(workspace_bytes, queue); }
    catch (...) { strata::l0::free(scratch, queue); throw; }
    std::unique_ptr<ProductBatch> batch;
    try { batch = std::make_unique<ProductBatch>(ProductBatch{queue, workspace, workspace_bytes}); }
    catch (...) { strata::l0::free(scratch, queue); strata::l0::free(workspace, queue); throw; }
    stream_ = &queue;
    scratch_ = scratch;
    scratch_elems_ = count;
    workspace_ = workspace;
    workspace_bytes_ = workspace_bytes;
    external_ = false;
    handle_ = batch.release();
    return true;
} catch (const std::exception& e) { error = e.what(); return false; }

bool Gemm::init_external(void* stream, uint16_t* scratch, int64_t count, void* workspace,
                         size_t capacity, std::string& error) try {
    if (handle_ || count < 0 || uint64_t(count) > SIZE_MAX / 2 ||
        (count && !scratch) || (capacity && !workspace))
        throw std::invalid_argument("invalid/repeated native GEMM initialization");
    auto& queue = strata::l0::stream_of(stream);
    auto batch = std::make_unique<ProductBatch>(ProductBatch{queue, workspace, capacity});
    stream_ = &queue;
    scratch_ = scratch;
    scratch_elems_ = count;
    workspace_ = workspace;
    workspace_bytes_ = capacity;
    external_ = true;
    handle_ = batch.release();
    return true;
} catch (const std::exception& e) { error = e.what(); return false; }

void Gemm::rebind(uint16_t* scratch, int64_t count, void* workspace, size_t capacity) {
    if (!handle_ || !external_ || count < 0 || uint64_t(count) > SIZE_MAX / 2 ||
        (count && !scratch) || (capacity && !workspace))
        throw std::invalid_argument("native GEMM rebind needs initialized external buffers");
    strata::l0::stream_of(stream_).wait_and_throw();
    scratch_ = scratch;
    scratch_elems_ = count;
    workspace_ = workspace;
    workspace_bytes_ = capacity;
    auto& batch = *static_cast<ProductBatch*>(handle_);
    batch.workspace = workspace;
    batch.capacity = capacity;
}

void Gemm::bf16(const uint16_t* x, const uint16_t* w, float* y, int64_t t, int64_t n, int64_t k,
                int64_t ldy, float beta, int64_t ldx) {
    if (!handle_) throw std::logic_error("native GEMM is not initialized");
    static_cast<ProductBatch*>(handle_)->enqueue(validated(x, w, y, t, n, k, ldx, ldy, beta, true));
}
void Gemm::f16(const uint16_t* x, const uint16_t* w, float* y, int64_t t, int64_t n, int64_t k,
               int64_t ldy, float beta) {
    if (!handle_) throw std::logic_error("native GEMM is not initialized");
    static_cast<ProductBatch*>(handle_)->enqueue(validated(x, w, y, t, n, k, k, ldy, beta, false));
}
void Gemm::set_auto_batch(bool enabled) {
    if (!handle_) throw std::logic_error("native GEMM is not initialized");
    strata::l0::stream_of(stream_).flush();
    static_cast<ProductBatch*>(handle_)->enabled = enabled;
}
Gemm::BatchStats Gemm::batch_stats() const {
    if (!handle_) throw std::logic_error("native GEMM is not initialized");
    return static_cast<ProductBatch*>(handle_)->stats;
}
bool Gemm::f16_expert(const uint16_t* x, const uint16_t* gu, const uint16_t* down,
                      float* intermediate, uint16_t* hidden, float* y, int64_t t, bool finish) {
    if (!handle_) throw std::logic_error("native GEMM is not initialized");
    product(x, gu, intermediate, t, 1280, 2560, 2560, 1280, 0.f, false,
            workspace_, workspace_bytes_, stream_, false);
    swiglu_interleaved(intermediate, hidden, t, stream_);
    product(hidden, down, y, t, 2560, 640, 640, 2560, 0.f, false,
            workspace_, workspace_bytes_, stream_, finish);
    return true;
}
void Gemm::native(const uint16_t* x, int type, const void* weights, float* y, int64_t t,
                  int64_t n, int64_t k, int64_t ldy, float beta, int64_t ldx) {
    if (!handle_ || k <= 0 || n < 0 || t < 0 || k > INT_MAX - 15 || n > INT_MAX - 7 || t > INT_MAX ||
        scratch_elems_ / k <= 0 || (t && n && (!x || !weights || !y)))
        throw std::invalid_argument("native dequant GEMM has invalid dimensions/scratch");
    if (ldy <= 0) ldy = n;
    const int64_t step = scratch_elems_ / k;
    for (int64_t row = 0; row < n; row += step) {
        const int64_t count = std::min(step, n - row);
        strata::kernels::dequant_f16(type, weights, row, count, k, scratch_, stream_);
        product(x, scratch_, y + row, t, count, k, ldx, ldy, beta, false,
                workspace_, workspace_bytes_, stream_, false);
    }
}
bool Gemm::bf16_hcd_exact(const uint16_t*, int64_t, const uint16_t*, float*, int64_t, int64_t, int64_t) { return false; }
bool prompt_f16() { return false; }
}

namespace fp16_pair {
class OrcaFp16ExpertPair;
namespace {

void multiply(strata::l0::Queue& q, Expert a, Expert b, uint16_t* padding, bool down) {
    namespace jm = sycl::ext::oneapi::experimental::matrix;
    constexpr int SG = 8, NSG = 8, ROWS = 4, TILE = 8;
    const int n = down ? 2560 : 1280, k = down ? 640 : 2560;
    const int tiles_a = (a.rows + ROWS - 1) / ROWS, tiles_b = (b.rows + ROWS - 1) / ROWS;
    if (a.rows % TILE || b.rows % TILE) {
        q.parallel_for(sycl::range<1>(2 * TILE * size_t(k)), [=](sycl::id<1> id) {
            const size_t i = id[0];
            const int which = int(i / (TILE * k)), offset = int(i % (TILE * k));
            const Expert e = which ? b : a;
            const int row = e.rows - e.rows % TILE + offset / k, col = offset % k;
            const auto* x = down ? e.hidden : e.x;
            padding[size_t(which) * TILE * 2560 + offset] = row < e.rows ? x[size_t(row) * k + col] : uint16_t(0);
        });
    }
    q.submit([&](strata::l0::Queue& h) {
        orca::l0::compiler::local_accessor<float, 1> tail(sycl::range<1>(NSG * ROWS * TILE), h);
        h.parallel_for<OrcaFp16ExpertPair>(sycl::nd_range<3>(
            sycl::range<3>(size_t(tiles_a + tiles_b), size_t(n / (NSG * TILE)), SG * NSG),
            sycl::range<3>(1, 1, SG * NSG)),
            [=](sycl::nd_item<3> it) [[sycl::reqd_sub_group_size(SG)]] {
                const auto sg = it.get_sub_group();
                const int subgroup = int(sg.get_group_id()[0]), lane = int(sg.get_local_id()[0]);
                const int tile = int(it.get_group(0)), which = tile >= tiles_a;
                const Expert e = which ? b : a;
                const int row = (tile - (which ? tiles_a : 0)) * ROWS;
                const int col = (int(it.get_group(1)) * NSG + subgroup) * TILE;
                const bool partial = row + ROWS > e.rows;
                const auto* x = reinterpret_cast<const sycl::half*>(down ? e.hidden : e.x);
                const auto* w = reinterpret_cast<const sycl::half*>(down ? e.down_weight : e.gu_weight);
                const auto* source = partial ? reinterpret_cast<const sycl::half*>(padding) +
                    size_t(which) * TILE * 2560 + (row % TILE) * k : x + size_t(row) * k;
                float* y = down ? e.output : e.gu;
                auto gp = [](auto* p) { return sycl::address_space_cast<sycl::access::address_space::global_space,
                    sycl::access::decorated::no>(p); };
                jm::joint_matrix<sycl::sub_group, float, jm::use::accumulator, ROWS, TILE> c;
                jm::joint_matrix_fill(sg, c, 0.f);
                for (int i = 0; i < k; i += 16) {
                    jm::joint_matrix<sycl::sub_group, sycl::half, jm::use::a, ROWS, 16, jm::layout::row_major> ax;
                    jm::joint_matrix<sycl::sub_group, sycl::half, jm::use::b, 16, TILE, jm::layout::col_major> bw;
                    jm::joint_matrix_load(sg, ax, gp(source + i), size_t(k));
                    jm::joint_matrix_load(sg, bw, gp(w + size_t(col) * k + i), size_t(k));
                    jm::joint_matrix_mad(sg, c, ax, bw, c);
                }
                if (!partial) jm::joint_matrix_store(sg, c, gp(y + size_t(row) * n + col), size_t(n), jm::layout::row_major);
                else {
                    auto* tmp = tail.get_multi_ptr<sycl::access::decorated::no>().get() + subgroup * ROWS * TILE;
                    jm::joint_matrix_store(sg, c, sycl::address_space_cast<sycl::access::address_space::local_space,
                        sycl::access::decorated::no>(tmp), TILE, jm::layout::row_major);
                    sycl::group_barrier(sg);
                    for (int i = lane; i < ROWS * TILE; i += SG)
                        if (row + i / TILE < e.rows) y[size_t(row + i / TILE) * n + col + i % TILE] = tmp[i];
                }
            });
    });
}
}

bool execute(strata::l0::Queue& q, Expert a, Expert b, void* workspace, size_t bytes, bool finish) {
    // ponytail: two existing DQ slots and the GEMM workspace; keep Orca's M4 K-order and 256-GRF matrix kernel.
    if (!q.is_in_order()) return false;
    for (auto e : {a, b})
        if (e.rows <= 0 || e.rows > INT_MAX / 2 - 7 || !e.x || !e.gu_weight || !e.down_weight ||
            !e.gu || !e.hidden || !e.output) return false;
    if ((a.rows % 8 || b.rows % 8) && (!workspace || bytes < 2 * 8 * 2560 * sizeof(uint16_t))) return false;
    auto* padding = static_cast<uint16_t*>(workspace);
    multiply(q, a, b, padding, false);
    strata::prefill::swiglu_interleaved(a.gu, a.hidden, a.rows, &q);
    strata::prefill::swiglu_interleaved(b.gu, b.hidden, b.rows, &q);
    multiply(q, a, b, padding, true);
    if (finish) q.wait_and_throw();
    return true;
}
}
