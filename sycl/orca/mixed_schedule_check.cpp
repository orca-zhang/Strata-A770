// Manual native phased GEMV/router demo. Reuse model kernels and their SPIR-V.
// ponytail: only known-ready disjoint branches; model scheduling must supply its own read/write dependencies.
#include <dpct/dpct.hpp>
#include <strata/kernels/native_mmvq.hpp>
#include <strata/kernels/bf16_gemv.hpp>
#include <strata/kernels/bf16_bits.hpp>
#include <strata/kernels/f16_bits.hpp>
#include <strata/kernels/native_router.hpp>
#include <strata/kernels/elementwise.hpp>
#include "runtime_dependencies.hpp"
#include <array>
#include <bit>
#include <chrono>
#include <cmath>
#include <cstring>
#include <vector>

namespace {
void require(bool value, const char* message) { if (!value) throw std::runtime_error(message); }
constexpr int K = 2560, E = 512, guard = 16;

// Independent GGUF Q6_K decoding, used only for CPU64 samples outside timing.
float weight6(const uint8_t* block, int i) {
    const int half = i / 128, part = i % 128 / 32, lane = i % 32;
    const int low = block[half * 64 + (part & 1) * 32 + lane];
    const int high = block[128 + half * 32 + lane];
    const int q = ((low >> ((part / 2) * 4)) & 15) | (((high >> (part * 2)) & 3) << 4);
    uint16_t bits; std::memcpy(&bits, block + 208, 2);
    const auto scale = int8_t(block[192 + half * 8 + part * 2 + lane / 16]);
    return strata::kernels::f32_from_f16(bits) * float(scale) * float(q - 32);
}
}

int main(int argc, char** argv) try {
    if (argc < 3 || argc > 4 || (argc == 4 && std::string(argv[3]) != "--bench"))
        throw std::invalid_argument("usage: strata-orca-mixed-check images new-output.bin [--bench]");
    require(!std::filesystem::exists(argv[2]), "output already exists");
    std::setvbuf(stdout, nullptr, _IONBF, 0);
    check_runtime_dependencies();
    namespace k = strata::kernels;
    orca::l0::Runtime runtime;
    const std::filesystem::path images(argv[1]);
    orca::l0::compiler::Program program(runtime, images / "native_mmvq");
    program.add(images / "native_bf16"); program.add(images / "native_router");
    program.add(images / "elementwise");
    strata::l0::Queue main(runtime, program);
    strata::l0::bind_default(main);
    uint32_t count = 0;
    orca::l0::check(zeDeviceGetCommandQueueGroupProperties(runtime.native_device(), &count, nullptr), "queue group count");
    std::vector<ze_command_queue_group_properties_t> groups(count);
    for (auto& g : groups) g.stype = ZE_STRUCTURE_TYPE_COMMAND_QUEUE_GROUP_PROPERTIES;
    orca::l0::check(zeDeviceGetCommandQueueGroupProperties(runtime.native_device(), &count, groups.data()), "queue groups");
    for (uint32_t i = 0; i < count; ++i)
        std::printf("QUEUE_GROUP {\"ordinal\":%u,\"flags\":%u,\"queues\":%u}\n", i, groups[i].flags, groups[i].numQueues);
    std::ofstream binary(argv[2], std::ios::binary);
    require(bool(binary), "cannot create output");
    int checks = 0;
    size_t replays = 0;
    for (int n : {640, 10240}) for (int banks : {1, 8}) for (int tokens : {1, 4, 8}) {
        const size_t wb = k::native_mmvq_weight_bytes(14, K, n), xb = size_t(tokens) * K;
        const size_t qb = k::native_q8_1_bytes(K, tokens), rb = size_t(E) * K;
        std::vector<uint8_t> hw(wb * banks);
        uint32_t rng = 1234567;
        for (auto& byte : hw) { rng ^= rng << 13; rng ^= rng >> 17; rng ^= rng << 5; byte = uint8_t(rng) & 0x3b; }
        std::vector<uint16_t> hr(rb * banks);
        for (size_t i = 0; i < hr.size(); ++i) hr[i] = k::bf16_from_f32(float(int(i * 7919 % 63) - 31) / 256.f);
        auto* w = static_cast<uint8_t*>(runtime.allocate(hw.size()));
        auto* r = static_cast<uint16_t*>(runtime.allocate(hr.size() * 2));
        auto* x = static_cast<float*>(runtime.allocate(xb * banks * 4));
        auto* xq = static_cast<uint8_t*>(runtime.allocate(qb * banks));
        auto* progress = static_cast<volatile uint32_t*>(runtime.allocate(3 * sizeof(uint32_t), orca::l0::Memory::host));
        progress[0] = 17; progress[1] = 0; progress[2] = 29;
        uint32_t expected_progress = 0;
        const std::array<size_t, 5> sizes{size_t(tokens) * n, size_t(tokens) * E, size_t(tokens) * 10, size_t(tokens) * 10, size_t(tokens) * E};
        std::array<float*, 5> outputs;
        for (int i = 0; i < 5; ++i) outputs[i] = static_cast<float*>(runtime.allocate((sizes[i] * banks + 2 * guard) * 4));
        auto clear = [&] { for (int i = 0; i < 5; ++i) main.fill(outputs[i], -12345.f, sizes[i] * banks + 2 * guard); main.wait_and_throw(); };
        auto snapshot = [&] {
            std::vector<uint32_t> result;
            for (int i = 0; i < 5; ++i) {
                const size_t at = result.size(), length = sizes[i] * banks + 2 * guard;
                result.resize(at + length);
                main.memcpy(result.data() + at, outputs[i], length * 4).wait();
                for (int j = 0; j < guard; ++j) require(result[at+j] == std::bit_cast<uint32_t>(-12345.f) &&
                    result[at+length-1-j] == std::bit_cast<uint32_t>(-12345.f), "output guard");
                for (size_t j = guard; j < length - guard; ++j)
                    if (i == 2) require(result[at+j] < E, "router index");
                    else require(std::isfinite(std::bit_cast<float>(result[at+j])), "non-finite output");
            }
            return result;
        };
        main.memcpy(w, hw.data(), hw.size()); main.memcpy(r, hr.data(), hr.size() * 2); main.wait_and_throw();
        auto projection = [&] {
            for (int bank = 0; bank < banks; ++bank) {
                k::native_quantize_q8_1(x + bank * xb, xq + bank * qb, K, tokens, &main);
                k::native_mmvq(14, w + bank * wb, xq + bank * qb, outputs[0] + guard + bank * sizes[0], K, n, tokens, &main);
            }
        };
        auto router = [&](strata::l0::Queue& queue) {
            for (int bank = 0; bank < banks; ++bank) {
                auto* logits = outputs[1] + guard + bank * sizes[1];
                k::bf16_gemv_fp32_mmvf_multi(x + bank * xb, K, r + bank * rb, logits, E, K, E, tokens, &queue);
                k::native_router_top10_multi_ne(logits, reinterpret_cast<int32_t*>(outputs[2] + guard) + bank * sizes[2],
                    outputs[3] + guard + bank * sizes[3], tokens, E, &queue);
            }
        };
        auto consume = [&] {
            // A real GPU consumer after the join, rather than only host readback.
            k::copy_from_mapped(outputs[4] + guard, outputs[1] + guard, sizes[1] * banks, &main);
            main.ext_oneapi_submit_barrier();
            k::doorbell_ring(const_cast<uint32_t*>(progress + 1), &main);
        };
        main.begin_recording(); projection(); router(main); consume(); auto serial = main.end_recording();
        main.begin_recording(); projection(); k::doorbell_ring(const_cast<uint32_t*>(progress + 1), &main); auto gemv = main.end_recording();
        main.begin_recording(true);
        main.independent([&] {
            for (int bank = 0; bank < banks; ++bank) {
                k::native_quantize_q8_1(x + bank * xb, xq + bank * qb, K, tokens, &main);
                k::bf16_gemv_fp32_mmvf_multi(x + bank * xb, K, r + bank * rb,
                    outputs[1] + guard + bank * sizes[1], E, K, E, tokens, &main);
            }
        });
        main.independent([&] {
            for (int bank = 0; bank < banks; ++bank) {
                k::native_mmvq(14, w + bank * wb, xq + bank * qb,
                    outputs[0] + guard + bank * sizes[0], K, n, tokens, &main);
                k::native_router_top10_multi_ne(outputs[1] + guard + bank * sizes[1],
                    reinterpret_cast<int32_t*>(outputs[2] + guard) + bank * sizes[2],
                    outputs[3] + guard + bank * sizes[3], tokens, E, &main);
            }
        });
        consume(); auto joined = main.end_recording();
        auto run = [&](int mode) {
            if (mode == 0) main.replay(serial);
            else if (mode == 1) main.replay(joined);
            else if (mode == 2) main.replay(gemv);
            else { projection(); router(main); consume(); }
            main.wait_and_throw();
            require(progress[0] == 17 && progress[2] == 29 && progress[1] == ++expected_progress,
                    "GPU tail did not complete exactly once before wait returned");
            ++replays;
        };
        std::vector<float> hx(xb * banks);
        std::vector<uint32_t> reference;
        for (int profile = 0; profile < 3; ++profile) {
            for (size_t i = 0; i < hx.size(); ++i)
                hx[i] = float(int((i * 7919 + profile * 101) % 1000) - 500) / 512.f;
            main.memcpy(x, hx.data(), hx.size() * 4).wait();
            clear(); run(0); reference = snapshot();
            for (int mode : {1, 3}) { clear(); run(mode); require(snapshot() == reference, "schedule changed output bits"); ++checks; }
            std::vector<uint8_t> hq(qb * banks); main.memcpy(hq.data(), xq, hq.size()).wait();
            std::vector<float> ya(sizes[0] * banks), yr(sizes[1] * banks);
            main.memcpy(ya.data(), outputs[0] + guard, ya.size() * 4);
            main.memcpy(yr.data(), outputs[1] + guard, yr.size() * 4); main.wait_and_throw();
            for (int bank = 0; bank < banks; ++bank) for (int token : {0, tokens - 1}) {
                for (int row : {0, n / 2, n - 1}) {
                    double expected = 0, magnitude = 0;
                    for (int j = 0; j < K; ++j) {
                        const auto* block = hq.data() + bank * qb + (size_t(token) * K / 32 + j / 32) * 36;
                        uint16_t scale; std::memcpy(&scale, block, 2);
                        const double input = double(k::f32_from_f16(scale)) * int8_t(block[4 + j % 32]);
                        const double term = input * weight6(hw.data() + bank * wb + (size_t(row) * K / 256 + j / 256) * 210, j % 256);
                        expected += term; magnitude += std::fabs(term);
                    }
                    require(std::fabs(ya[bank * sizes[0] + token * n + row] - expected) <= 2e-5 * (1 + magnitude), "Q6_K CPU64 sample");
                }
                for (int row : {0, E / 2, E - 1}) {
                    double expected = 0, magnitude = 0;
                    for (int j = 0; j < K; ++j) {
                        const double term = double(hx[bank * xb + token * K + j]) * k::f32_from_bf16(hr[bank * rb + row * K + j]);
                        expected += term; magnitude += std::fabs(term);
                    }
                    require(std::fabs(yr[bank * sizes[1] + token * E + row] - expected) <= 2e-5 * (1 + magnitude), "BF16 CPU64 sample");
                }
            }
            binary.write(reinterpret_cast<const char*>(reference.data()), std::streamsize(reference.size() * 4));
            require(bool(binary), "output write failed");
        }
        if (argc == 4) {
            for (int repeat = 0; repeat < 8; ++repeat) { run(0); run(1); run(2); run(3); }
            for (int repetition = 0; repetition < 7; ++repetition) for (int mode : {0, 1, 1, 0, 2, 3}) {
                const auto start = std::chrono::steady_clock::now();
                for (int window = 0; window < 8; ++window) run(mode);
                const double us = std::chrono::duration<double, std::micro>(std::chrono::steady_clock::now() - start).count() / 8;
                std::printf("SAMPLE {\"N\":%d,\"K\":%d,\"T\":%d,\"banks\":%d,\"repetition\":%d,\"mode\":%d,\"window_us\":%.6f}\n", n, K, tokens, banks, repetition, mode, us);
            }
            run(0); require(snapshot() == reference, "serial replay changed output");
            run(1); require(snapshot() == reference, "parallel replay changed output");
        }
        std::printf("PASS N=%d T=%d banks=%d: three inputs, full bitwise outputs, CPU64 samples, guards\n", n, tokens, banks);
        main.wait_and_throw();
        runtime.deallocate(w); runtime.deallocate(r); runtime.deallocate(x); runtime.deallocate(xq);
        runtime.deallocate(const_cast<uint32_t*>(progress));
        for (auto* p : outputs) runtime.deallocate(p);
    }
    check_runtime_dependencies();
    std::printf("PASS mixed schedule: %d complete output comparisons\n", checks);
    std::printf("PASS replay epochs: %llu checked GPU tails\n", static_cast<unsigned long long>(replays));
    return 0;
} catch (const std::exception& e) { std::fprintf(stderr, "%s\n", e.what()); return 1; }
