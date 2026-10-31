#include <dpct/dpct.hpp>
#include <strata/kernels/elementwise.hpp>
#include <strata/kernels/dequant_bf16.hpp>
#include <strata/kernels/bf16_bits.hpp>
#include <strata/kernels/f16_bits.hpp>
#include <strata/prefill/gemm.hpp>
#include <strata/prefill/kernels.hpp>
#include <strata/prefill/fp16_pair.hpp>
#include <array>
#include <cmath>
#include <cstdio>
#include <limits>
#include <vector>
#include <chrono>
#include <cstring>
#include "../../orca-l0/tests/runtime_dependencies.hpp"

namespace strata::prefill {
void finite_count_async(const float*, int64_t, uint32_t*, void*);
void finite_count16_async(const uint16_t*, int64_t, bool, uint32_t*, void*);
void capture_down_async(const uint16_t*, const uint16_t*, const float*, int64_t, int64_t,
                        int64_t, int64_t, int64_t, int64_t*, uint16_t*, uint16_t*, float*, void*);
void gr_write_finite(float*, const float*, const float*, int64_t, int64_t, void*, uint32_t*, bool = false);
void gr_write_norm_rs_finite(float*, const float*, const float*, int64_t, const float*, float,
                             float*, uint16_t*, int64_t, void*, uint16_t*, uint32_t*, bool = false);
void orca_xmx_gemm(const uint16_t*, const uint16_t*, float*, int64_t, int64_t, int64_t,
                   int64_t, const uint16_t*, bool, void*);
}

int main(int argc, char** argv) try {
    if (argc < 2 || argc > 3 || (argc == 3 && std::string(argv[2]) != "--bench-batch"))
        throw std::invalid_argument("usage: strata-orca-kernel-check image-directory [--bench-batch]");
    check_runtime_dependencies();
    namespace k = strata::kernels;
    orca::l0::Runtime runtime;
    const std::filesystem::path images(argv[1]);
    orca::l0::compiler::Program program(runtime, images / "elementwise");
    std::ifstream packs(images / "packs.txt");
    if (!packs) throw std::runtime_error("missing native pack manifest");
    std::string pack;
    while (std::getline(packs, pack)) {
        if (pack == "elementwise") continue;
        if (pack.empty() || std::filesystem::path(pack).has_parent_path())
            throw std::runtime_error("invalid native pack name");
        program.add(images / pack);
    }
    strata::l0::Queue stream(runtime, program);
    strata::l0::bind_default(stream);
    {
        constexpr int rows = 9, n = 2560, k = 640;
        std::vector<void*> allocations;
        auto host = [&](size_t bytes) {
            auto* p = runtime.allocate(bytes, orca::l0::Memory::host);
            allocations.push_back(p); return p;
        };
        auto* x = static_cast<uint16_t*>(host(rows * k * 2));
        auto* w = static_cast<uint16_t*>(host(n * k * 2));
        auto* y = static_cast<float*>(host(rows * n * 4));
        auto* sx = static_cast<uint16_t*>(host((rows * k + 2) * 2));
        auto* sw = static_cast<uint16_t*>(host((n * k + 2) * 2));
        auto* sy = static_cast<float*>(host((rows * n + 2) * 4));
        auto* meta = static_cast<int64_t*>(host(8 * sizeof(int64_t)));
        std::fill_n(meta, 8, int64_t(0)); meta[0] = 17; meta[7] = 29;
        for (int i = 0; i < rows * k; ++i) x[i] = uint16_t(i);
        for (int i = 0; i < n * k; ++i) w[i] = uint16_t(i * 7);
        std::fill_n(y, rows * n, 1.f);
        std::fill_n(sx, rows * k + 2, uint16_t(123));
        std::fill_n(sw, n * k + 2, uint16_t(123));
        std::fill_n(sy, rows * n + 2, -123.f);
        auto capture = [&](int layer) { strata::prefill::capture_down_async(x, w, y, rows, rows,
            layer, 23, 1024, meta + 1, sx + 1, sw + 1, sy + 1, &stream); };
        capture(3); stream.wait_and_throw();
        if (meta[1] || sx[1] != 123 || sw[1] != 123 || sy[1] != -123.f)
            throw std::runtime_error("Down capture accepted finite output");
        // All mutations stay queued: capture must finish before source/DQ reuse.
        stream.fill(y + rows * n - 1, std::numeric_limits<float>::quiet_NaN(), 1);
        capture(7);
        stream.fill(x, uint16_t(0), rows * k);
        stream.fill(w, uint16_t(0), n * k);
        stream.fill(y, std::numeric_limits<float>::infinity(), rows * n);
        capture(8);
        stream.wait_and_throw();
        const int64_t expected[] = {17, rows, n, k, 7, 23, 1024, 29};
        if (!std::equal(meta, meta + 8, expected)) throw std::runtime_error("Down capture winner/metadata mismatch");
        for (int i = 0; i < rows * k; ++i)
            if (sx[i + 1] != uint16_t(i)) throw std::runtime_error("Down capture input mismatch");
        for (int i = 0; i < n * k; ++i)
            if (sw[i + 1] != uint16_t(i * 7)) throw std::runtime_error("Down capture weight mismatch");
        for (int i = 0; i < rows * n; ++i)
            if (i == rows * n - 1 ? !std::isnan(sy[i + 1]) : sy[i + 1] != 1.f)
                throw std::runtime_error("Down capture output mismatch");
        if (sx[0] != 123 || sx[rows * k + 1] != 123 || sw[0] != 123 || sw[n * k + 1] != 123 ||
            sy[0] != -123.f || sy[rows * n + 1] != -123.f)
            throw std::runtime_error("Down capture guard overwrite");
        for (auto* p : allocations) runtime.deallocate(p);
        std::puts("PASS Down capture: finite skip, queued NaN, exact X/W/Y, first winner, reuse and guards");
    }
    {
        constexpr int length = 4111;
        auto* values = static_cast<float*>(runtime.allocate(length * sizeof(float), orca::l0::Memory::host));
        auto* counts = static_cast<uint32_t*>(runtime.allocate(3 * sizeof(uint32_t), orca::l0::Memory::host));
        for (int i = 0; i < length; ++i) values[i] = float(i);
        counts[0] = 17; counts[1] = 0; counts[2] = 29;
        strata::prefill::finite_count_async(values, length, counts + 1, &stream);
        stream.wait_and_throw();
        if (counts[1]) throw std::runtime_error("finite reduction rejected finite values");
        values[0] = std::numeric_limits<float>::quiet_NaN();
        values[2048] = std::numeric_limits<float>::infinity();
        values[length - 1] = -std::numeric_limits<float>::infinity();
        strata::prefill::finite_count_async(values, length, counts + 1, &stream);
        strata::prefill::finite_count_async(nullptr, 0, nullptr, &stream);
        stream.wait_and_throw();
        if (counts[0] != 17 || counts[1] != 3 || counts[2] != 29)
            throw std::runtime_error("finite reduction count/guard mismatch");
        runtime.deallocate(values); runtime.deallocate(counts);
    }
    {
        constexpr int length = 65536; // Exhaust every FP16/BF16 bit pattern, including signed NaNs and subnormals.
        auto* values = static_cast<uint16_t*>(runtime.allocate(length * sizeof(uint16_t), orca::l0::Memory::host));
        auto* counts = static_cast<uint32_t*>(runtime.allocate(3 * sizeof(uint32_t), orca::l0::Memory::host));
        for (int i = 0; i < length; ++i) values[i] = uint16_t(i);
        for (bool brain : {false, true}) {
            counts[0] = 17; counts[1] = 0; counts[2] = 29;
            strata::prefill::finite_count16_async(values, length, brain, counts + 1, &stream);
            strata::prefill::finite_count16_async(nullptr, 0, brain, nullptr, &stream);
            stream.wait_and_throw();
            if (counts[0] != 17 || counts[1] != (brain ? 256u : 2048u) || counts[2] != 29)
                throw std::runtime_error("16-bit finite reduction count/guard mismatch");
        }
        runtime.deallocate(values); runtime.deallocate(counts);
        std::puts("PASS finite16 diagnostic: every FP16/BF16 bit pattern, strided scan and guards");
    }
    {
        namespace pf = strata::prefill;
        constexpr int tokens = 2, n = 2560, hc = 4, dim = n * hc;
        std::vector<void*> allocations;
        auto host = [&](size_t size) {
            auto* p = runtime.allocate(size, orca::l0::Memory::host);
            allocations.push_back(p);
            return p;
        };
        auto* r = static_cast<float*>(host(tokens * dim * 4));
        auto* reference = static_cast<float*>(host(tokens * dim * 4));
        auto* bo = static_cast<float*>(host(tokens * n * 4));
        auto* inj = static_cast<float*>(host(tokens * hc * 4));
        auto* wn = static_cast<float*>(host(dim * 4));
        auto* rs = static_cast<float*>(host(tokens * hc * 4));
        auto* image = static_cast<uint16_t*>(host(tokens * dim * 2));
        auto* counts = static_cast<uint32_t*>(host(3 * sizeof(uint32_t)));
        std::fill_n(bo, tokens * n, 0.25f);
        std::fill_n(inj, tokens * hc, 0.f);
        std::fill_n(wn, dim, 1.f);
        for (bool norm : {false, true}) for (bool poison : {false, true}) for (bool all : {false, true}) {
            std::fill_n(r, tokens * dim, 1.f);
            if (poison) {
                r[3] = std::numeric_limits<float>::quiet_NaN(); // counted only in full-row mode
                r[dim + 1] = std::numeric_limits<float>::infinity();
                r[2 * dim - 1] = std::numeric_limits<float>::quiet_NaN();
            }
            std::copy_n(r, tokens * dim, reference);
            counts[0] = 17; counts[1] = 0; counts[2] = 29;
            if (norm) {
                pf::gr_write_norm_rs(reference, bo, inj, hc, wn, 1e-6f, rs, image, tokens, &stream);
                pf::gr_write_norm_rs_finite(r, bo, inj, hc, wn, 1e-6f, rs, image, tokens, &stream,
                                             nullptr, counts + 1, all);
            } else {
                pf::gr_write(reference, bo, inj, hc, tokens, &stream);
                pf::gr_write_finite(r, bo, inj, hc, tokens, &stream, counts + 1, all);
            }
            stream.wait_and_throw();
            if (counts[0] != 17 || counts[1] != (poison ? (all ? 3u : 2u) : 0u) || counts[2] != 29)
                throw std::runtime_error("fused residual diagnostic count/guard mismatch");
            for (int i = 0; i < tokens * dim; ++i)
                if (std::isnan(reference[i]) ? !std::isnan(r[i]) : r[i] != reference[i])
                    throw std::runtime_error("fused residual diagnostic changed residual arithmetic");
        }
        for (auto* p : allocations) runtime.deallocate(p);
        std::puts("PASS fused residual diagnostics: finite/NaN/Inf, last/all-row scope, guards and normal-kernel parity");
    }
    constexpr int count = 512, width = 256;
    auto device = [&](size_t bytes) { return runtime.allocate(bytes); };
    auto* x = static_cast<float*>(device(count * sizeof(float)));
    auto* bias = static_cast<float*>(device(count * sizeof(float)));
    auto* weight = static_cast<float*>(device(width * sizeof(float)));
    auto* gate = static_cast<float*>(device(count * sizeof(float)));
    auto* f16 = static_cast<uint16_t*>(device(count * sizeof(uint16_t)));
    auto* bf16 = static_cast<uint16_t*>(device(count * sizeof(uint16_t)));
    auto* q8 = static_cast<uint8_t*>(device(34));
    auto* dequant = static_cast<uint16_t*>(device(32 * sizeof(uint16_t)));
    auto* codes = static_cast<uint8_t*>(device(count / 4));
    auto* embedded = static_cast<float*>(device(count * sizeof(float)));
    auto* scales = static_cast<float*>(device(count / 32 * sizeof(float)));
    auto* seq = static_cast<uint32_t*>(runtime.allocate(sizeof(uint32_t), orca::l0::Memory::host));
    auto* flag = static_cast<uint32_t*>(runtime.allocate(sizeof(uint32_t), orca::l0::Memory::host));
    *seq = 0;
    *flag = 1;
    std::vector<float> input(count), add(count), weights(width), scale(count / 32, 0.5f);
    std::vector<float> result(count), gates(count), embeddings(count);
    std::vector<uint16_t> halves(count), bhalves(count);
    std::array<uint8_t, 34> raw{};
    std::array<uint8_t, count / 4> packed{};
    std::array<uint16_t, 32> unquantized{};
    raw[0] = 0; raw[1] = 0x38; // Q8_0 scale = fp16(0.5).
    for (int i = 0; i < 32; ++i) raw[2 + i] = uint8_t(i - 16);
    for (int i = 0; i < count; ++i) add[i] = float(i % 5 - 2) * 0.125f;
    for (int i = 0; i < width; ++i) weights[i] = -0.25f * float(i % 4 + 1);
    for (int i = 0; i < count / 4; ++i) packed[i] = uint8_t(i);
    stream.memcpy(bias, add.data(), count * sizeof(float));
    stream.memcpy(weight, weights.data(), width * sizeof(float));
    stream.memcpy(q8, raw.data(), raw.size());
    stream.memcpy(codes, packed.data(), packed.size());
    stream.memcpy(scales, scale.data(), scale.size() * sizeof(float));
    if (!stream.wait()) throw std::runtime_error("initial transfer timeout");

    stream.begin_recording();
    stream.memcpy(x, input.data(), count * sizeof(float));
    k::scale_inplace(x, count, 2.0f, &stream);
    k::add_inplace(x, bias, count, &stream);
    k::gdn_gate(x, bias, weight, gate, 2, width, &stream);
    k::rms_norm_weighted(x, weight, 2, width, 1e-5f, &stream);
    k::f32_to_f16_bulk(x, f16, count, &stream);
    k::f32_to_bf16_bulk(x, bf16, count, &stream);
    k::embedding_gather(codes, scales, nullptr, count, 2, -2, 32, embedded, &stream);
    k::dequant_bf16(8, q8, 0, 1, 32, dequant, &stream);
    k::doorbell_ring(seq, &stream);
    k::doorbell_wait(flag, seq, &stream);
    stream.memcpy(result.data(), x, count * sizeof(float));
    stream.memcpy(gates.data(), gate, count * sizeof(float));
    stream.memcpy(halves.data(), f16, count * sizeof(uint16_t));
    stream.memcpy(bhalves.data(), bf16, count * sizeof(uint16_t));
    stream.memcpy(unquantized.data(), dequant, sizeof(unquantized));
    stream.memcpy(embeddings.data(), embedded, count * sizeof(float));
    auto graph = stream.end_recording();
    for (int pass = 0; pass < 3; ++pass) {
        *flag = pass + 1;
        for (int i = 0; i < count; ++i) input[i] = (i % 31 - 15) * float(pass + 1);
        stream.replay(graph);
        if (!stream.wait(10000000000ULL)) throw std::runtime_error("Strata native graph timeout");
        if (*seq != uint32_t(pass + 1)) throw std::runtime_error("native doorbell did not advance");
        for (int row = 0; row < 2; ++row) {
            double sum = 0;
            for (int col = 0; col < width; ++col) {
                const int i = row * width + col;
                const double v = input[i] * 2.0f + add[i];
                sum += v * v;
            }
            const double inv = 1.0 / std::sqrt(sum / width + 1e-5f);
            for (int col = 0; col < width; ++col) {
                const int i = row * width + col;
                const float v = input[i] * 2.0f + add[i];
                const double expected = v * weights[col] * inv;
                const double a = v + add[col];
                const double softplus = a > 20 ? a : std::log1p(std::exp(a));
                if (!std::isfinite(result[i]) || !std::isfinite(gates[i]) ||
                    std::abs(result[i] - expected) > 3e-5 ||
                    std::abs(gates[i] - softplus * weights[col]) > 3e-5) {
                    std::fprintf(stderr, "pass %d row %d col %d: norm %.9g / %.9g, gate %.9g / %.9g\n",
                        pass, row, col, result[i], expected, gates[i], softplus * weights[col]);
                    throw std::runtime_error("Strata gate/RMSNorm output mismatch");
                }
                // Conversion follows the actual FP32 GPU result, independently of reduction rounding.
                if (halves[i] != k::f16_from_f32(result[i]) || bhalves[i] != k::bf16_from_f32(result[i]))
                    throw std::runtime_error("Strata FP16/BF16 conversion mismatch");
                const int code = (packed[i / 4] >> (2 * (i % 4))) & 3;
                if (embeddings[i] != (code - 2) * 0.5f)
                    throw std::runtime_error("Strata packed embedding mismatch");
            }
        }
        for (int i = 0; i < 32; ++i)
            if (unquantized[i] != k::bf16_from_f32((i - 16) * 0.5f))
                throw std::runtime_error("Strata Q8_0 dequantization mismatch");
    }
    for (bool brain : {false, true}) for (int t : {1, 8, 13}) for (int n : {8, 48, 64}) for (int inner : {16, 64}) {
        const int stride = inner + 8;
        const int first_tail_row = t - t % 8;
        std::vector<uint16_t> hx(size_t(t) * stride, 0), hw(size_t(n) * inner), ht(8 * inner, 0);
        std::vector<float> got(size_t(t) * n + 16, -1234.f);
        auto encode = [brain](float value) { return brain ? k::bf16_from_f32(value) : k::f16_from_f32(value); };
        auto input_value = [](int row, int col) { return float((row * 13 + col) % 23 - 11) * 0.125f; };
        auto weight_value = [](int row, int col) { return float((row * 7 + col * 3) % 17 - 8) * 0.125f; };
        for (int row = 0; row < t; ++row) for (int col = 0; col < inner; ++col) {
            hx[size_t(row) * stride + col] = encode(input_value(row, col));
            if (row >= first_tail_row) ht[size_t(row - first_tail_row) * inner + col] = hx[size_t(row) * stride + col];
        }
        for (int row = 0; row < n; ++row) for (int col = 0; col < inner; ++col)
            hw[size_t(row) * inner + col] = encode(weight_value(row, col));
        auto* dx = static_cast<uint16_t*>(device(hx.size() * 2));
        auto* dw = static_cast<uint16_t*>(device(hw.size() * 2));
        auto* dt = static_cast<uint16_t*>(device(ht.size() * 2));
        auto* dy = static_cast<float*>(device(got.size() * 4));
        stream.memcpy(dx, hx.data(), hx.size() * 2);
        stream.memcpy(dw, hw.data(), hw.size() * 2);
        stream.memcpy(dt, ht.data(), ht.size() * 2);
        stream.memcpy(dy, got.data(), got.size() * sizeof(float));
        strata::prefill::orca_xmx_gemm(dx, dw, dy, t, n, inner, stride, t % 8 ? dt : nullptr, brain, &stream);
        stream.memcpy(got.data(), dy, got.size() * 4).wait_and_throw();
        for (int row = 0; row < t; ++row) for (int col = 0; col < n; ++col) {
            float expected = 0;
            for (int i = 0; i < inner; ++i) expected += input_value(row, i) * weight_value(col, i);
            if (got[size_t(row) * n + col] != expected)
                throw std::runtime_error("Strata native XMX output mismatch");
        }
        for (size_t i = size_t(t) * n; i < got.size(); ++i)
            if (got[i] != -1234.f) throw std::runtime_error("Strata native XMX guard overwritten");
        runtime.deallocate(dx); runtime.deallocate(dw); runtime.deallocate(dt); runtime.deallocate(dy);
    }
    // Exercise the engine's actual API with row padding, beta, XMX input/weight
    // padding, output guards, and a small workspace forcing multiple chunks.
    auto* workspace = device(4096);
    auto* scratch = static_cast<uint16_t*>(device(64 * sizeof(uint16_t)));
    strata::prefill::Gemm gemm;
    std::string error;
    if (!gemm.init_external(&stream, scratch, 64, workspace, 4096, error))
        throw std::runtime_error(error);
    int gemm_cases = 0;
    for (bool brain : {false, true}) for (int t : {1, 8, 37}) for (int n : {4, 13, 24})
        for (int inner : {15, 32}) for (float beta : {0.f, 1.f}) {
        const int stride = inner + (brain ? 3 : 0), output_stride = n + 5;
        std::vector<uint16_t> hx(size_t(t) * stride, 0), hw(size_t(n) * inner);
        // NaNs require beta=0 to skip the previous output, and every row has guards.
        std::vector<float> hy(size_t(t) * output_stride + 16, -1234.f);
        for (int row = 0; row < t; ++row) for (int col = 0; col < n; ++col)
            hy[size_t(row) * output_stride + col] = beta == 0.f ? NAN : 0.25f;
        auto input = [](int row, int col) { return float((row * 3 + col) % 11 - 5) * 0.125f; };
        auto weight = [](int row, int col) { return float((row + col * 2) % 9 - 4) * 0.125f; };
        auto encode = [brain](float v) { return brain ? k::bf16_from_f32(v) : k::f16_from_f32(v); };
        for (int row = 0; row < t; ++row) for (int col = 0; col < inner; ++col)
            hx[size_t(row) * stride + col] = encode(input(row, col));
        for (int row = 0; row < n; ++row) for (int col = 0; col < inner; ++col)
            hw[size_t(row) * inner + col] = encode(weight(row, col));
        auto* dx = static_cast<uint16_t*>(device(hx.size() * 2));
        auto* dw = static_cast<uint16_t*>(device(hw.size() * 2));
        auto* dy = static_cast<float*>(device(hy.size() * 4));
        stream.memcpy(dx, hx.data(), hx.size() * 2);
        stream.memcpy(dw, hw.data(), hw.size() * 2);
        stream.memcpy(dy, hy.data(), hy.size() * 4);
        if (brain) gemm.bf16(dx, dw, dy, t, n, inner, output_stride, beta, stride);
        else gemm.f16(dx, dw, dy, t, n, inner, output_stride, beta);
        stream.memcpy(hy.data(), dy, hy.size() * 4).wait_and_throw();
        for (int row = 0; row < t; ++row) for (int col = 0; col < output_stride; ++col) {
            float expected = -1234.f;
            if (col < n) {
                expected = beta * 0.25f;
                for (int i = 0; i < inner; ++i) expected += input(row, i) * weight(col, i);
            }
            if (hy[size_t(row) * output_stride + col] != expected)
                throw std::runtime_error("Strata GEMM API value/row guard mismatch");
        }
        for (size_t i = size_t(t) * output_stride; i < hy.size(); ++i)
            if (hy[i] != -1234.f) throw std::runtime_error("Strata GEMM API end guard overwritten");
        runtime.deallocate(dx); runtime.deallocate(dw); runtime.deallocate(dy);
        ++gemm_cases;
    }
    // Actual long-prefill tails and projection layouts, with stale NaNs in
    // workspace/output. Periodic rows keep the independent CPU reference small.
    {
        constexpr size_t real_bytes = 1u << 20;
        auto* real_workspace = device(real_bytes);
        {
            strata::prefill::Gemm real;
            if (!real.init_external(&stream, scratch, 64, real_workspace, real_bytes, error))
                throw std::runtime_error(error);
            for (const auto shape : {std::array<int, 3>{48, 2560, 96}, {4, 10240, 4}, {1, 2560, 1}})
                for (int t : {1024, 2069, 2047, 2048}) {
                const auto [n, inner, output_stride] = shape;
                const int stride = inner + (n == 4 && t != 1024 ? 3 : 0);
                auto input = [](int row, int col) { return float((row * 3 + col) % 11 - 5) * 0.125f; };
                auto weight = [](int row, int col) { return float((row + col * 2) % 9 - 4) * 0.125f; };
                std::vector<uint16_t> hx(size_t(t) * stride, 0), hw(size_t(n) * inner);
                std::vector<float> hy(size_t(t) * output_stride + 16), expected(11 * n, 0.f);
                for (int row = 0; row < t; ++row) for (int col = 0; col < inner; ++col)
                    hx[size_t(row) * stride + col] = k::bf16_from_f32(input(row, col));
                for (int row = 0; row < n; ++row) for (int col = 0; col < inner; ++col)
                    hw[size_t(row) * inner + col] = k::bf16_from_f32(weight(row, col));
                for (int row = 0; row < 11; ++row) for (int col = 0; col < n; ++col)
                    for (int i = 0; i < inner; ++i) expected[row * n + col] += input(row, i) * weight(col, i);
                auto* dx = static_cast<uint16_t*>(device(hx.size() * 2));
                auto* dw = static_cast<uint16_t*>(device(hw.size() * 2));
                auto* dy = static_cast<float*>(device(hy.size() * 4));
                stream.memcpy(dx, hx.data(), hx.size() * 2);
                stream.memcpy(dw, hw.data(), hw.size() * 2);
                for (float beta : {0.f, 1.f}) {
                    std::fill(hy.begin(), hy.end(), -1234.f);
                    for (int row = 0; row < t; ++row) for (int col = 0; col < n; ++col)
                        hy[size_t(row) * output_stride + col] = beta == 0.f ? NAN : 0.25f;
                    stream.memcpy(dy, hy.data(), hy.size() * 4);
                    stream.fill(static_cast<uint16_t*>(real_workspace), uint16_t(0x7fc0), real_bytes / 2);
                    real.bf16(dx, dw, dy, t, n, inner, output_stride, beta, stride);
                    stream.memcpy(hy.data(), dy, hy.size() * 4).wait_and_throw();
                    for (size_t i = 0; i < hy.size(); ++i) {
                        const size_t row = i / output_stride, col = i % output_stride;
                        const float value = row < size_t(t) && col < size_t(n)
                            ? expected[(row % 11) * n + col] + beta * 0.25f : -1234.f;
                        if (hy[i] != value)
                            throw std::runtime_error("long-prefill GEMM value/guard mismatch");
                    }
                    ++gemm_cases;
                }
                runtime.deallocate(dx); runtime.deallocate(dw); runtime.deallocate(dy);
            }
        }
        runtime.deallocate(real_workspace);
    }
    {
        constexpr int t = 3, n = 5, inner = 32, stride = 35, output_stride = 8;
        std::vector<uint16_t> hx(t * stride, 0);
        std::vector<uint8_t> hw(n * 34, 0);
        std::vector<float> hy(t * output_stride + 8, -1234.f);
        for (int row = 0; row < t; ++row) for (int i = 0; i < inner; ++i)
            hx[row * stride + i] = k::f16_from_f32(float((row + i) % 7 - 3) * 0.125f);
        for (int row = 0; row < n; ++row) {
            hw[row * 34 + 1] = 0x38; // fp16(0.5)
            for (int i = 0; i < inner; ++i) hw[row * 34 + 2 + i] = uint8_t((row * 3 + i) % 11 - 5);
        }
        auto* dx = static_cast<uint16_t*>(device(hx.size() * 2));
        auto* dw = static_cast<uint8_t*>(device(hw.size()));
        auto* dy = static_cast<float*>(device(hy.size() * 4));
        stream.memcpy(dx, hx.data(), hx.size() * 2);
        stream.memcpy(dw, hw.data(), hw.size());
        for (float beta : {0.f, 1.f}) {
            for (int row = 0; row < t; ++row) for (int col = 0; col < n; ++col)
                hy[row * output_stride + col] = beta == 0.f ? NAN : 0.25f;
            stream.memcpy(dy, hy.data(), hy.size() * 4);
            // Only two dequantized rows fit, so the last slice is one row.
            gemm.native(dx, 8, dw, dy, t, n, inner, output_stride, beta, stride);
            stream.memcpy(hy.data(), dy, hy.size() * 4).wait_and_throw();
            for (int row = 0; row < t; ++row) for (int col = 0; col < output_stride; ++col) {
                float expected = -1234.f;
                if (col < n) {
                    expected = beta * 0.25f;
                    for (int i = 0; i < inner; ++i)
                        expected += float((row + i) % 7 - 3) * 0.125f * float((col * 3 + i) % 11 - 5) * 0.5f;
                }
                if (hy[row * output_stride + col] != expected)
                    throw std::runtime_error("native dequant GEMM slice/stride/value mismatch");
            }
            for (size_t i = t * output_stride; i < hy.size(); ++i)
                if (hy[i] != -1234.f) throw std::runtime_error("native dequant GEMM end guard overwritten");
        }
        runtime.deallocate(dx); runtime.deallocate(dw); runtime.deallocate(dy);
    }
    // Four independent products must become one product, for both contiguous
    // rows and gathered/scattered layouts, including beta and graph replay.
    for (bool brain : {false, true}) for (bool contiguous : {false, true}) for (bool capture : {false, true}) {
        constexpr int inner = 16, n = 16, rows = 7;
        const int stride = brain && !contiguous ? 19 : inner;
        const int output_stride = contiguous ? n : n + 5;
        std::vector<uint16_t> hx(rows * stride), hw(n * inner);
        std::vector<float> initial(rows * output_stride + 8, -1234.f), got(initial.size());
        auto input = [](int r, int c) { return float((r * 3 + c) % 11 - 5) * 0.125f; };
        auto weight = [](int r, int c) { return float((r + c * 2) % 9 - 4) * 0.125f; };
        auto encode = [brain](float v) { return brain ? k::bf16_from_f32(v) : k::f16_from_f32(v); };
        for (int r = 0; r < rows; ++r) for (int c = 0; c < inner; ++c) hx[r * stride + c] = encode(input(r, c));
        for (int r = 0; r < n; ++r) for (int c = 0; c < inner; ++c) hw[r * inner + c] = encode(weight(r, c));
        for (int r = 0; r < rows; ++r) for (int c = 0; c < n; ++c) initial[r * output_stride + c] = 0.25f;
        auto* dx = static_cast<uint16_t*>(device(hx.size() * 2));
        auto* dw = static_cast<uint16_t*>(device(hw.size() * 2));
        auto* dy = static_cast<float*>(device(initial.size() * 4));
        stream.memcpy(dx, hx.data(), hx.size() * 2);
        stream.memcpy(dw, hw.data(), hw.size() * 2);
        stream.wait_and_throw();
        const auto before = gemm.batch_stats();
        if (capture) stream.begin_recording();
        stream.memcpy(dy, initial.data(), initial.size() * 4);
        int first = 0, request = 0;
        for (int count : {1, 2, 1, 3}) {
            const int source = contiguous ? first : rows - first - count;
            const float beta = !contiguous && request % 2 ? 1.f : 0.f;
            if (brain) gemm.bf16(dx + source * stride, dw, dy + first * output_stride,
                                  count, n, inner, output_stride, beta, stride);
            else gemm.f16(dx + source * stride, dw, dy + first * output_stride,
                          count, n, inner, output_stride, beta);
            first += count;
            ++request;
        }
        bool empty = true, waited = true;
        std::thread watchdog([&] { empty = stream.ext_oneapi_empty(); waited = stream.wait(0); });
        watchdog.join();
        if (empty || waited) throw std::runtime_error("watchdog ignored deferred matrix work");
        std::shared_ptr<strata::l0::NativeGraph> batch_graph;
        if (capture) batch_graph = stream.end_recording();
        for (int pass = 0; pass < (capture ? 2 : 1); ++pass) {
            if (capture) stream.replay(batch_graph);
            // Copying the result is a dependency boundary that flushes eager work.
            stream.memcpy(got.data(), dy, got.size() * 4).wait_and_throw();
            first = request = 0;
            for (int count : {1, 2, 1, 3}) {
                const int source = contiguous ? first : rows - first - count;
                const float beta = !contiguous && request % 2 ? 1.f : 0.f;
                for (int r = 0; r < count; ++r) for (int c = 0; c < output_stride; ++c) {
                    float expected = -1234.f;
                    if (c < n) {
                        expected = beta * 0.25f;
                        for (int i = 0; i < inner; ++i) expected += input(source + r, i) * weight(c, i);
                    }
                    if (got[(first + r) * output_stride + c] != expected)
                        throw std::runtime_error("automatic matrix batch output/row guard mismatch");
                }
                first += count;
                ++request;
            }
            for (size_t i = rows * output_stride; i < got.size(); ++i)
                if (got[i] != -1234.f) throw std::runtime_error("automatic matrix batch end guard overwritten");
        }
        const auto after = gemm.batch_stats();
        if (after.requests - before.requests != 4 || after.products - before.products != 1 ||
            after.merged_requests - before.merged_requests != 4)
            throw std::runtime_error("ready matrices were not combined into one product");
        batch_graph.reset();
        // Repeated output is a dependency: the second product accumulates the
        // first. Disabling batching must preserve the same ordered result.
        for (bool enabled : {true, false}) {
            gemm.set_auto_batch(enabled);
            const auto start = gemm.batch_stats();
            if (brain) {
                gemm.bf16(dx, dw, dy, 1, n, inner, n, 0.f, stride);
                gemm.bf16(dx, dw, dy, 1, n, inner, n, 1.f, stride);
            } else {
                gemm.f16(dx, dw, dy, 1, n, inner);
                gemm.f16(dx, dw, dy, 1, n, inner, n, 1.f);
            }
            stream.memcpy(got.data(), dy, n * 4).wait_and_throw();
            for (int c = 0; c < n; ++c) {
                float expected = 0;
                for (int i = 0; i < inner; ++i) expected += 2.f * input(0, i) * weight(c, i);
                if (got[c] != expected) throw std::runtime_error("dependent GEMMs were combined or reordered");
            }
            if (gemm.batch_stats().products - start.products != 2)
                throw std::runtime_error("GEMM dependency/disable boundary was lost");
        }
        gemm.set_auto_batch(true);
        runtime.deallocate(dx); runtime.deallocate(dw); runtime.deallocate(dy);
    }
    check_runtime_dependencies();
    std::printf("PASS Strata numerical sources on %s: embedding, scale/add, gate, RMSNorm, FP16/BF16, Q8_0, doorbells; 3 native graph replays\n",
                runtime.properties().name);
    std::puts("PASS Strata native XMX FP16/BF16 GEMM: 36 shapes, padded strides and partial tiles, exact outputs and guards");
    {
        constexpr int n = 2560, hidden = 640, gu_cols = 1280, max_rows = 129, guard = 32;
        const size_t gx = size_t(max_rows) * n, gw = size_t(gu_cols) * n, dw = size_t(n) * hidden;
        const size_t gs = size_t(max_rows) * gu_cols + 2 * guard;
        const size_t hs = size_t(max_rows) * hidden + 2 * guard, ys = gx + 2 * guard;
        constexpr size_t pad_bytes = 2 * 8 * n * sizeof(uint16_t);
        auto* px = static_cast<uint16_t*>(device(2 * gx * 2));
        auto* pgw = static_cast<uint16_t*>(device(2 * gw * 2));
        auto* pdw = static_cast<uint16_t*>(device(2 * dw * 2));
        auto* pgu = static_cast<float*>(device(2 * gs * 4));
        auto* ph = static_cast<uint16_t*>(device(2 * hs * 2));
        auto* py = static_cast<float*>(device(2 * ys * 4));
        auto* pad = static_cast<uint16_t*>(device(pad_bytes + 4 * guard));
        std::vector<uint16_t> x(2 * gx), wg(2 * gw), wd(2 * dw);
        for (size_t i = 0; i < x.size(); ++i) x[i] = k::f16_from_f32(float(int(i % 23) - 11) / 32.f);
        for (size_t i = 0; i < wg.size(); ++i) wg[i] = k::f16_from_f32(float(int(i % 29) - 14) / 64.f);
        for (size_t i = 0; i < wd.size(); ++i) wd[i] = k::f16_from_f32(float(int(i % 31) - 15) / 64.f);
        stream.memcpy(px, x.data(), x.size() * 2);
        stream.memcpy(pgw, wg.data(), wg.size() * 2);
        stream.memcpy(pdw, wd.data(), wd.size() * 2);
        strata::prefill::Gemm reference;
        if (!reference.init_external(&stream, nullptr, 0, pad + guard, pad_bytes, error))
            throw std::runtime_error(error);
        auto reset = [&] {
            stream.fill(pgu, -12345.f, 2 * gs);
            stream.fill(ph, uint16_t(0x55aa), 2 * hs);
            stream.fill(py, -12345.f, 2 * ys);
            stream.fill(pad, uint16_t(0x55aa), pad_bytes / 2 + 2 * guard);
        };
        std::vector<float> expected_g, expected_y;
        std::vector<uint16_t> expected_h;
        int checks = 0;
        auto outputs = [&](bool save) {
            std::vector<float> g(2 * gs), y(2 * ys);
            std::vector<uint16_t> h(2 * hs);
            stream.memcpy(g.data(), pgu, g.size() * 4);
            stream.memcpy(h.data(), ph, h.size() * 2);
            stream.memcpy(y.data(), py, y.size() * 4).wait_and_throw();
            if (save) { expected_g = g; expected_y = y; expected_h = h; }
            else {
                if (std::memcmp(g.data(), expected_g.data(), g.size() * 4) ||
                    std::memcmp(y.data(), expected_y.data(), y.size() * 4) || h != expected_h)
                    throw std::runtime_error("native FP16 expert pair complete GU/H/Y or guard mismatch");
                checks += 3;
            }
        };
        for (auto shape : {std::pair{1, 1}, {3, 5}, {4, 4}, {4, 5}, {7, 9}, {8, 16}, {127, 129}}) {
            const fp16_pair::Expert first{px, pgw, pdw, pgu + guard, ph + guard, py + guard, shape.first};
            const fp16_pair::Expert second{px + gx, pgw + gw, pdw + dw,
                pgu + gs + guard, ph + hs + guard, py + ys + guard, shape.second};
            reset();
            for (auto e : {first, second})
                reference.f16_expert(e.x, e.gu_weight, e.down_weight, e.gu, e.hidden, e.output, e.rows);
            outputs(true);
            for (int bank = 0; bank < 2; ++bank) {
                const int rows = bank ? shape.second : shape.first;
                for (size_t i = 0; i < gs; ++i)
                    if (i < guard || i >= guard + size_t(rows) * gu_cols) {
                        if (expected_g[bank * gs + i] != -12345.f) throw std::runtime_error("native pair GU reference guard");
                    } else if (!std::isfinite(expected_g[bank * gs + i])) throw std::runtime_error("native pair GU nonfinite");
                for (size_t i = 0; i < ys; ++i)
                    if (i < guard || i >= guard + size_t(rows) * n) {
                        if (expected_y[bank * ys + i] != -12345.f) throw std::runtime_error("native pair Down reference guard");
                    } else if (!std::isfinite(expected_y[bank * ys + i])) throw std::runtime_error("native pair Down nonfinite");
                for (size_t i = 0; i < hs; ++i)
                    if ((i < guard || i >= guard + size_t(rows) * hidden) && expected_h[bank * hs + i] != 0x55aa)
                        throw std::runtime_error("native pair hidden reference guard");
            }
            for (int mode : {0, 1, 2, 3}) {
                reset();
                if (mode >= 2) stream.begin_recording(mode == 3);
                if (!fp16_pair::execute(stream, first, second, pad + guard, pad_bytes, mode == 0))
                    throw std::runtime_error("native FP16 pair dispatch refused");
                if (mode >= 2) {
                    auto graph = stream.end_recording();
                    stream.replay(graph);
                    stream.wait_and_throw();
                }
                outputs(false);
                std::vector<uint16_t> padding(pad_bytes / 2 + 2 * guard);
                stream.memcpy(padding.data(), pad, padding.size() * 2).wait_and_throw();
                for (int i = 0; i < guard; ++i)
                    if (padding[i] != 0x55aa || padding[padding.size() - guard + i] != 0x55aa)
                        throw std::runtime_error("native FP16 pair workspace guard");
            }
            for (int invalid = 0; invalid < 10; ++invalid) {
                reset(); outputs(true);
                auto bad = first;
                if (invalid == 0) bad.rows = 0;
                if (invalid == 1) bad.rows = -1;
                if (invalid == 2) bad.rows = INT_MAX;
                if (invalid == 3) bad.x = nullptr;
                if (invalid == 4) bad.gu_weight = nullptr;
                if (invalid == 5) bad.down_weight = nullptr;
                if (invalid == 6) bad.hidden = nullptr;
                if (invalid == 7) bad.output = nullptr;
                auto tail_case = second;
                if (invalid >= 8) tail_case.rows = 1;
                if (fp16_pair::execute(stream, bad, tail_case, invalid == 8 ? nullptr : pad + guard,
                                      invalid == 9 ? pad_bytes - 1 : pad_bytes, false))
                    throw std::runtime_error("invalid native FP16 pair accepted");
                outputs(false);
            }
        }
        std::printf("PASS native FP16 expert pair: %d full-output checks, distinct weights, M4/tail guards, async/capture and rejected inputs\n", checks);
    }
    std::printf("PASS Strata Gemm API: %d cases, strides/beta/unaligned dimensions/chunked workspace\n", gemm_cases);
    std::puts("PASS Strata native dequant GEMM: sliced scratch, padded strides, beta and output guards");
    std::puts("PASS default matrix batching: FP16/BF16, four products to one, contiguous/gather/scatter, beta, dependencies, disable, watchdog and graph replay");
    if (argc == 3) {
        strata::prefill::Gemm bench;
        if (!bench.init(&stream, 0, error)) throw std::runtime_error(error);
        // Isolated operator demo: host enqueue + GPU + completion, same native
        // kernels in both modes. These timings are not full-model throughput.
        for (const auto shape : {std::array<int, 2>{1280, 2560}, std::array<int, 2>{2560, 640}})
            for (int t : {1, 4}) for (int requests : {1, 8}) for (bool packed : {false, true}) {
            const int n = shape[0], inner = shape[1];
            const int xstep = t * inner + (packed ? 32 : 0), ystep = t * n + (packed ? 16 : 0);
            auto* bx = static_cast<uint16_t*>(device(size_t(xstep) * requests * 2));
            auto* bw = static_cast<uint16_t*>(device(size_t(n) * inner * 2));
            auto* by = static_cast<float*>(device(size_t(ystep) * requests * 4));
            stream.fill(bx, k::f16_from_f32(0.5f), size_t(xstep) * requests);
            stream.fill(bw, k::f16_from_f32(0.25f), size_t(n) * inner);
            stream.fill(by, -1234.f, size_t(ystep) * requests);
            stream.wait_and_throw();
            auto window = [&] {
                for (int i = 0; i < requests; ++i)
                    bench.f16(bx + size_t(i) * xstep, bw, by + size_t(i) * ystep, t, n, inner);
                stream.wait_and_throw();
            };
            for (bool enabled : {false, true}) {
                bench.set_auto_batch(enabled);
                for (int i = 0; i < 3; ++i) window();
            }
            std::vector<double> baseline, batched, improvements;
            uint64_t single_products = 0, batch_products = 0;
            for (int repeat = 0; repeat < 5; ++repeat) {
                double a = 0, b = 0;
                for (bool enabled : {false, true, true, false}) {
                    bench.set_auto_batch(enabled);
                    const auto stats = bench.batch_stats();
                    const auto start = std::chrono::steady_clock::now();
                    for (int i = 0; i < 5; ++i) window();
                    const double us = std::chrono::duration<double, std::micro>(std::chrono::steady_clock::now() - start).count() / 5;
                    const auto products = bench.batch_stats().products - stats.products;
                    if (enabled) { b += us / 2; batch_products = products / 5; }
                    else { a += us / 2; single_products = products / 5; }
                }
                baseline.push_back(a); batched.push_back(b); improvements.push_back(100 * (a / b - 1));
            }
            std::vector<float> result(size_t(ystep) * requests);
            stream.memcpy(result.data(), by, result.size() * 4).wait_and_throw();
            for (int i = 0; i < requests; ++i) for (int j = 0; j < ystep; ++j)
                if (result[size_t(i) * ystep + j] != (j < t * n ? float(inner) * 0.125f : -1234.f))
                    throw std::runtime_error("matrix batching benchmark output/guard mismatch");
            std::sort(baseline.begin(), baseline.end()); std::sort(batched.begin(), batched.end());
            std::sort(improvements.begin(), improvements.end());
            std::printf("BATCH_BENCH {\"N\":%d,\"K\":%d,\"T\":%d,\"requests\":%d,\"layout\":\"%s\","
                        "\"baseline_us\":%.3f,\"batched_us\":%.3f,\"median_change_pct\":%.3f,"
                        "\"min_change_pct\":%.3f,\"max_change_pct\":%.3f,\"baseline_products\":%llu,\"batched_products\":%llu}\n",
                n, inner, t, requests, packed ? "gather_scatter" : "contiguous", baseline[2], batched[2],
                improvements[2], improvements.front(), improvements.back(),
                (unsigned long long)single_products, (unsigned long long)batch_products);
            runtime.deallocate(bx); runtime.deallocate(bw); runtime.deallocate(by);
        }
    }
    return 0;
} catch (const std::exception& e) {
    std::fprintf(stderr, "%s\n", e.what());
    return 1;
}
