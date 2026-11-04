#include <dpct/dpct.hpp>
#include <cstdio>
#include <cstring>
#include <chrono>
#include <sycl/ext/intel/esimd.hpp>
#include <sycl/ext/intel/experimental/grf_size_properties.hpp>
#include "../orca-l0/tests/runtime_dependencies.hpp"

namespace {
void queue_dependencies() {
#ifdef STRATA_ORCA_DG2
    check_runtime_dependencies(true);
#else
    check_runtime_dependencies();
#endif
}
#ifdef STRATA_ORCA_DG2
void copy_check(bool value, const char* message) { if (!value) throw std::runtime_error(message); }
template<class F> void copy_reject(F body) {
    bool rejected = false;
    try { body(); } catch (const std::exception&) { rejected = true; }
    copy_check(rejected, "invalid batched copy accepted");
}
void copy_batch_check(strata::l0::Runtime& runtime, strata::l0::Program& program) {
    namespace a = strata::l0::dg2_backend;
    a::Stream stream(runtime, program);
    constexpr size_t count = 137;
    auto* device = static_cast<uint32_t*>(runtime.allocate(count * 4));
    auto* host = static_cast<uint32_t*>(runtime.allocate(count * 4, a::Memory::host));
    for (size_t i = 0; i < count; ++i) host[i] = uint32_t(i * 97 + 11);
    for (size_t i = 0; i < count; ++i) {
        stream.copy(device + i, host + i, 4);
        if (i == 0) {
            if (runtime.batch_ops > 1) {
                copy_check(runtime.pending_copy_batches == 1, "copy did not retain its pending registry pin");
                copy_reject([&] { runtime.deallocate(host); });
            }
            copy_reject([&] { stream.copy(device + count - 1, host, 8); });
        }
    }
    a::Event completion; completion.runtime = &runtime;
    std::array<uint32_t, count> output{};
    stream.copy(output.data(), device, sizeof(output)); stream.barrier(&completion);
    copy_check(runtime.pending_copy_batches == 0, "command boundary retained pending copy pin");
    copy_check(completion.point->gpu.wait(5000000000ull), "batch completion timeout");
    std::array<std::thread, 8> workers;
    for (auto& worker : workers) worker = std::thread([&] {
        for (int i = 0; i < 200; ++i) completion.point->finish();
    });
    for (auto& worker : workers) worker.join();
    copy_check(stream.wait(), "batch tail timeout");
    for (size_t i = 0; i < count; ++i) copy_check(output[i] == host[i], "batch lost data or order");
    {
        // Adding an allocation is allowed while an old registry snapshot is
        // pinned. Its new device pointer must never be read as a Host address.
        stream.copy(device, host, sizeof(uint32_t));
        auto* later = static_cast<uint32_t*>(runtime.allocate(sizeof(uint32_t)));
        a::Stream uploader(runtime, program);
        const uint32_t value = 0x76543210;
        uploader.copy(later, &value, sizeof(value)); copy_check(uploader.wait(), "new allocation upload timeout");
        stream.copy(device + 1, later, sizeof(value));
        stream.copy(host, device, 2 * sizeof(value)); copy_check(stream.wait(), "new registry source timeout");
        copy_check(host[0] == 11 && host[1] == value, "stale registry misclassified a new device source");
        runtime.deallocate(later);
    }
    // Ordinary stack inputs must stage before their storage is changed or destroyed.
    for (size_t i = 0; i < count; ++i) {
        uint32_t temporary = uint32_t(i + 12345);
        stream.copy(device + i, &temporary, 4); temporary = 0;
    }
    stream.copy(output.data(), device, sizeof(output)); copy_check(stream.wait(), "stack staging timeout");
    for (size_t i = 0; i < count; ++i) copy_check(output[i] == i + 12345, "delayed ordinary Host staging");
    {
        // Registered D2D regions use one dispatch with the four-entry module;
        // old modules must keep the same row data and untouched padding.
        constexpr size_t bytes = count * sizeof(uint32_t);
        auto* rectangular = static_cast<uint8_t*>(runtime.allocate(bytes));
        auto* input = reinterpret_cast<uint8_t*>(host);
        const uint8_t guard = 0xcd;
        for (const auto shape : {std::array<size_t,4>{7,11,13,9}, {32,5,48,64}, {33,7,64,48}}) {
            const auto [width,height,dp,sp] = shape;
            for (size_t i = 0; i < bytes; ++i) input[i] = uint8_t(i * 31 + width);
            stream.copy(device, host, bytes); copy_check(stream.wait(), "pitched input upload timeout");
            stream.fill(rectangular, &guard, 1, bytes);
            stream.copy_2d(rectangular, dp, device, sp, width, height);
            stream.copy(host, rectangular, bytes); copy_check(stream.wait(), "registered pitched copy timeout");
            std::array<uint8_t,bytes> expected;
            expected.fill(guard);
            for (size_t row = 0; row < height; ++row)
                for (size_t col = 0; col < width; ++col)
                    expected[row * dp + col] = uint8_t((row * sp + col) * 31 + width);
            copy_check(!std::memcmp(host,expected.data(),bytes), "registered pitched copy lost data or padding");
            copy_reject([&] { stream.copy_2d(rectangular, 1, device, sp, width, height); });
        }
        runtime.deallocate(rectangular);
        for (size_t i = 0; i < count; ++i) host[i] = uint32_t(i * 97 + 11);
    }
    {
        // Recorded ordinary Host sources keep their address across replays;
        // only eager tiny uploads may snapshot the value in producer storage.
        uint32_t value = 0x2468ace0;
        stream.begin_recording(a::Ordering::in_order);
        stream.copy(device, &value, sizeof(value));
        auto& graph = stream.end_recording();
        for (unsigned pass = 0; pass < 3; ++pass) {
            value += 17;
            stream.replay(graph); copy_check(stream.wait(), "tiny recorded upload timeout");
            stream.copy(host, device, sizeof(value)); copy_check(stream.wait(), "tiny recorded upload readback timeout");
            copy_check(host[0] == value, "recorded tiny upload stopped refreshing its original source");
        }
        runtime.discard(graph);
        host[0] = 11;
    }
    {
        // Independent queue switches must not race the copy producer's pending
        // array. No device kernel is needed: use distinct destinations and guards.
        a::Stream other(runtime, program);
        auto* scratch = static_cast<uint32_t*>(runtime.allocate(8));
        std::atomic<int> phase{0};
        std::exception_ptr producer_error;
        std::thread producer([&] {
            try {
                for (unsigned pass = 0; pass < 24; ++pass) {
                    const uint32_t zero = 0;
                    stream.fill(device, &zero, 4, count * 4);
                    phase.store(1, std::memory_order_release); phase.notify_one();
                    for (size_t i = 0; i < count; ++i) stream.copy(device + i, host + i, 4);
                    copy_check(stream.wait(), "concurrent copy producer timeout");
                    stream.copy(output.data(), device, sizeof(output)); copy_check(stream.wait(), "concurrent readback timeout");
                    for (size_t i = 0; i < count; ++i) copy_check(output[i] == host[i], "queue switch lost a pending copy");
                    phase.store(2, std::memory_order_release); phase.notify_one();
                    while (phase.load(std::memory_order_acquire) == 2) phase.wait(2);
                }
            } catch (...) { producer_error = std::current_exception(); phase.store(3); phase.notify_one(); }
        });
        for (unsigned pass = 0; pass < 24; ++pass) {
            while (phase.load(std::memory_order_acquire) == 0) phase.wait(0);
            if (phase.load() == 3) break;
            for (unsigned i = 0; i < 8; ++i) other.fill(scratch, &i, 4, 8);
            copy_check(other.wait(), "concurrent independent queue timeout");
            while (phase.load(std::memory_order_acquire) == 1) phase.wait(1);
            if (phase.load() == 3) break;
            phase.store(0, std::memory_order_release); phase.notify_one();
        }
        producer.join();
        if (producer_error) std::rethrow_exception(producer_error);
        runtime.deallocate(scratch);
    }
    {
        // Producer snapshots cover both inline and cached Host storage, odd
        // tails and the cache ceiling. Mutating or destroying the caller's
        // storage must not change an accepted upload.
        auto* target = static_cast<std::byte*>(runtime.allocate(131073));
        std::vector<std::byte> actual(131073);
        for (size_t bytes : {size_t(17),size_t(65),size_t(4097),size_t(65536),size_t(98304),size_t(131072),size_t(131073),size_t(33)}) {
            {
                std::vector<std::byte> temporary(bytes,std::byte{0x5a});
                stream.copy(target,temporary.data(),bytes);
                if (runtime.batch_ops > 1 && bytes <= 131072)
                    copy_check(runtime.pending_copy_batches != 0, "cached Host input was not producer-batched");
                std::fill(temporary.begin(),temporary.end(),std::byte{0xa5});
            }
            stream.copy(actual.data(),target,bytes);
            copy_check(stream.wait(), "cached Host input readback timeout");
            for (size_t i=0;i<bytes;++i) copy_check(actual[i]==std::byte{0x5a}, "cached Host input changed after acceptance");
        }
        // A growing Host snapshot slot must not disturb an earlier queued copy.
        for (size_t pass=0;pass<3;++pass) {
            for (size_t i=0;i<67;++i) {
                std::array<std::byte,65> temporary;
                temporary.fill(std::byte((i+pass)&255));
                stream.copy(target+65*i,temporary.data(),temporary.size());
            }
            stream.copy(actual.data(),target,67*65);
            copy_check(stream.wait(), "cached Host partial batch timeout");
            for (size_t i=0;i<67*65;++i) copy_check(actual[i]==std::byte((i/65+pass)&255), "cached Host slot reuse lost order");
        }
        runtime.deallocate(target);
    }
    {
        // Mixed sizes and D2D consumers share a flush and cross its submission cap.
        // Keep guards between spans; accepted Host snapshots must survive slot reuse.
        constexpr size_t capacity = 4 * 1024 * 1024;
        auto* target = static_cast<std::byte*>(runtime.allocate(capacity));
        std::vector<std::byte> actual(capacity), expected(capacity);
        const std::array<size_t, 6> sizes{1, 16, 17, 65, 4097, 131072};
        for (unsigned pass = 0; pass < 3; ++pass) {
            const uint8_t guard = 0xcd;
            stream.fill(target, &guard, 1, capacity);
            std::fill(expected.begin(), expected.end(), std::byte{guard});
            size_t offset = 1;
            for (size_t i = 0; i < 70; ++i) {
                const size_t bytes = sizes[i % sizes.size()];
                std::vector<std::byte> temporary(bytes, std::byte((i + pass * 71) & 255));
                stream.copy(target + offset, temporary.data(), bytes);
                std::copy(temporary.begin(), temporary.end(), expected.begin() + offset);
                std::fill(temporary.begin(), temporary.end(), std::byte{0xa5});
                const size_t source = offset;
                offset += bytes + 17;
                if (i % 7 == 0) {
                    stream.copy(target + offset, target + source, bytes);
                    std::copy_n(expected.begin() + source, bytes, expected.begin() + offset);
                    offset += bytes + 17;
                }
            }
            stream.copy(actual.data(), target, capacity);
            copy_check(stream.wait() && actual == expected, "mixed upload slab lost ordering, snapshots or guards");
        }
        runtime.deallocate(target);
    }
    // A registered Host destination needs fence visibility but no staged CPU
    // readback ledger. Several observers can publish this completion together.
    uint32_t final_value = 0x13579bdf;
    stream.copy(device, &final_value, 4); stream.copy(host, device, 4);
    a::Event direct; direct.runtime = &runtime;
    stream.barrier(&direct); copy_check(direct.point->gpu.wait(5000000000ull), "direct Host completion timeout");
    for (auto& worker : workers) worker = std::thread([&] {
        for (int i = 0; i < 200; ++i) direct.point->finish();
    });
    for (auto& worker : workers) worker.join();
    copy_check(stream.wait() && host[0] == final_value, "direct Host completion lost visibility");
    {
        // A graph retains its original native queue. A second consumer must be
        // able to replay, then discard it after immutable waits on both streams.
        auto recorder = std::make_unique<a::Stream>(runtime, program);
        recorder->begin_recording(a::Ordering::in_order);
        recorder->copy(device, host, 4);
        auto& graph = recorder->end_recording(); recorder.reset();
        a::Stream first(runtime, program), second(runtime, program);
        first.replay(graph); copy_check(first.wait(), "first graph consumer timeout");
        second.replay(graph); copy_check(second.wait(), "second graph consumer timeout");
        runtime.discard(graph);
        stream.copy(host, device, 4); copy_check(stream.wait() && host[0] == final_value, "retained graph consumer lost data");
    }
    // Releasing after the successful wait remains valid even if native command
    // storage collection is intentionally deferred to the next submission.
    runtime.deallocate(host); runtime.deallocate(device);
    std::puts("PASS 137 registered copies, partial tail, bounds/free guards, both 8-finisher paths, ordinary Host lifetime, 24 cross-thread queue-switch waves, retained graph across two consumers and release after immutable completion");
}
void inline_event_check(strata::l0::Runtime& runtime, strata::l0::Program& program, bool benchmark) {
    constexpr size_t count = 64, launches = 200;
    constexpr uint32_t guard = 0xa5a5a5a5;
    strata::l0::Queue setup(runtime, program);
    strata::l0::dg2_backend::Stream recorder(runtime, program);
    std::vector<std::shared_ptr<strata::l0::EventPool>> pools;
    for (size_t i = 0; i < (launches + 63) / 64; ++i)
        pools.push_back(std::make_shared<strata::l0::EventPool>(runtime));
    auto* data = static_cast<uint32_t*>(runtime.allocate((count + 2) * sizeof(uint32_t)));
    setup.fill(data, guard, count + 2).wait_and_throw();
    recorder.begin_recording(strata::l0::Ordering::in_order);
    for (size_t i = 0; i < launches; ++i) {
        recorder.parallel_for(sycl::nd_range<1>(count, 32), [=](sycl::nd_item<1> item) {
            data[1 + item.get_global_id(0)] += 1;
        });
        recorder.barrier(pools[i / 64]->handle(uint32_t(i % 64)));
    }
    auto& graph = recorder.end_recording();
    const size_t expected = runtime.inline_events ? 1 : launches;
    if (graph.segments.size() != expected) throw std::runtime_error("recorded GPU signals did not preserve bounded graph segments");
    const int passes = benchmark ? 32 : 3, warmups = benchmark ? 8 : 0;
    std::array<uint32_t, count + 2> output{};
    std::vector<double> samples;
    std::shared_ptr<strata::l0::dg2_backend::Point> old;
    uint64_t submissions = 0;
    for (int pass = 0; pass < passes; ++pass) {
        setup.fill(data + 1, uint32_t(pass * 17), count).wait_and_throw();
        const auto before = runtime.invoke([&] { return runtime.gpu.completed(graph.queue); });
        const auto start = std::chrono::steady_clock::now();
        recorder.replay(graph);
        if (!recorder.wait(5000000000ull)) throw std::runtime_error("inline event replay timeout");
        const auto ms = std::chrono::duration<double, std::milli>(std::chrono::steady_clock::now() - start).count();
        submissions = runtime.invoke([&] { return runtime.gpu.completed(graph.queue); }) - before;
        setup.memcpy(output.data(), data, sizeof(output)).wait_and_throw();
        if (submissions != expected || output.front() != guard || output.back() != guard ||
            !std::all_of(output.begin() + 1, output.end() - 1, [&](uint32_t value) { return value == uint32_t(pass * 17 + launches); }))
            throw std::runtime_error("inline event replay lost values, guards or its submission count");
        for (size_t i = 0; i < launches; ++i)
            if (!strata::l0::event_ready(pools[i / 64]->handle(uint32_t(i % 64))))
                throw std::runtime_error("recorded GPU event failed to complete");
        if (!old) old = pools[0]->handle(0)->point;
        if (!old->ready()) throw std::runtime_error("rearming a graph changed an immutable old event point");
        if (pass >= warmups) samples.push_back(ms);
    }
    runtime.discard(graph);
    if (!benchmark && runtime.inline_events) {
        strata::l0::dg2_backend::Stream producer(runtime, program), consumer(runtime, program);
        strata::l0::EventPool points(runtime);
        const auto gate = runtime.staging(64);
        runtime.invoke([&] { return runtime.gpu.cpu_access(*gate, true); });
        auto* word = new (gate->data) std::atomic<uint64_t>(0);
        struct Release {
            std::atomic<uint64_t>* word;
            void open() { word->store(1, std::memory_order_release); _mm_clflush(word); _mm_mfence(); }
            ~Release() { open(); }
        } release{word};
        producer.begin_recording(strata::l0::Ordering::in_order);
        producer.parallel_for(sycl::nd_range<1>(count, 32), [=](sycl::nd_item<1> item) { data[1 + item.get_global_id(0)] += 1; });
        producer.barrier(points.handle(0));
        runtime.invoke([&] { return runtime.gpu.wait_value(gate->gpu, 1); }); // Hold the same native list after its prefix signal.
        producer.parallel_for(sycl::nd_range<1>(count, 32), [=](sycl::nd_item<1> item) { data[1 + item.get_global_id(0)] += 7; });
        producer.barrier(points.handle(1));
        auto& held = producer.end_recording();
        if (held.segments.size() != 1) throw std::runtime_error("inline event fixture still splits its producer list");
        for (int pass = 0; pass < 16; ++pass) {
            setup.fill(data + 1, uint32_t(pass * 17), count).wait_and_throw();
            word->store(0, std::memory_order_release); _mm_clflush(word); _mm_mfence();
            const auto before = runtime.invoke([&] { return runtime.gpu.completed(held.queue); });
            producer.replay(held);
            const bool prefix_ready = strata::l0::event_wait(points.handle(0), 300000000ull);
            consumer.barrier(nullptr, {points.handle(0)});
            consumer.copy(output.data(), data, sizeof(output));
            const bool consumed = consumer.wait(100000000ull);
            const bool tail_held = !strata::l0::event_ready(points.handle(1)) && !held.segments[0].last->ready() &&
                runtime.invoke([&] { return runtime.gpu.completed(held.queue); }) == before;
            release.open();
            if (!producer.wait(5000000000ull) || !consumer.wait(5000000000ull)) throw std::runtime_error("inline event fixture did not drain");
            if (!prefix_ready || !consumed || !tail_held || runtime.invoke([&] { return runtime.gpu.completed(held.queue); }) != before + 1 ||
                output.front() != guard || output.back() != guard ||
                !std::all_of(output.begin() + 1, output.end() - 1, [&](uint32_t value) { return value == uint32_t(pass * 17 + 1); }))
                throw std::runtime_error("inline event prefix/consumer depended on the gated tail or retired it early");
        }
        runtime.discard(held);
        std::puts("PASS inline event adapter: 16 one-submission graph replays, cross-queue prefix reads before gated tail, queue fence/resource ownership preserved and full guards");
    }
    runtime.deallocate(data);
    if (benchmark) {
        std::printf("EVENT_BENCH {\"inline\":%s,\"kernels\":200,\"signals\":200,\"submissions\":%llu,\"warmups\":8,\"samples_ms\":[",
            runtime.inline_events ? "true" : "false", (unsigned long long)submissions);
        for (size_t i = 0; i < samples.size(); ++i) std::printf("%s%.6f", i ? "," : "", samples[i]);
        std::puts("]}");
    } else std::printf("PASS recorded GPU events: 200 kernels/signals, %llu submissions, three replays, changing inputs, immutable old points and full guards\n", (unsigned long long)submissions);
}
void batch_check(strata::l0::Runtime& runtime, strata::l0::Program& program, bool benchmark) {
    constexpr size_t count = 64, iterations = 200;
    constexpr uint32_t guard = 0xa5a5a5a5;
    strata::l0::Queue setup(runtime, program);
    const auto queue = runtime.invoke([&] { return runtime.gpu.create_queue(); });
    strata::l0::dg2_backend::Stream batch(runtime, program, queue);
    auto* a = static_cast<uint32_t*>(runtime.allocate((count + 2) * sizeof(uint32_t)));
    auto* b = static_cast<uint32_t*>(runtime.allocate((count + 2) * sizeof(uint32_t)));
    setup.fill(a, guard, count + 2); setup.fill(b, guard, count + 2); setup.wait_and_throw();
    std::vector<double> samples;
    uint64_t submissions = 0;
    const int passes = benchmark ? 32 : 2, warmups = benchmark ? 8 : 0;
    std::array<uint32_t, count + 2> output{};
    for (int pass = 0; pass < passes; ++pass) {
        output.fill(0);
        const auto before = runtime.invoke([&] { return runtime.gpu.completed(queue); });
        const auto start = std::chrono::steady_clock::now();
        for (size_t i = 0; i < iterations; ++i) {
            const uint32_t value = uint32_t(pass * 1024 + i);
            batch.fill(a + 1, &value, sizeof(value), count * sizeof(value));
            batch.copy(b + 1, a + 1, count * sizeof(value));
            batch.parallel_for(sycl::nd_range<1>(count, 32), [=](sycl::nd_item<1> item) {
                b[1 + item.get_global_id(0)] += 1;
            });
        }
        batch.copy(output.data(), b, sizeof(output));
        if (!batch.wait(5000000000ull)) throw std::runtime_error("mixed batch timeout");
        const double ms = std::chrono::duration<double, std::milli>(std::chrono::steady_clock::now() - start).count();
        submissions = runtime.invoke([&] { return runtime.gpu.completed(queue); }) - before;
        const auto expected = (3 * iterations + 1 + runtime.batch_ops - 1) / runtime.batch_ops;
        if (submissions != expected || output.front() != guard || output.back() != guard ||
            !std::all_of(output.begin() + 1, output.end() - 1,
                [&](uint32_t value) { return value == uint32_t(pass * 1024 + iterations); }))
            throw std::runtime_error("mixed batch lost data, guards or its bounded submission count");
        if (pass >= warmups) samples.push_back(ms);
    }
    batch.copy(output.data(), a, sizeof(output));
    if (!batch.wait(5000000000ull) || output.front() != guard || output.back() != guard ||
        !std::all_of(output.begin() + 1, output.end() - 1,
            [&](uint32_t value) { return value == uint32_t((passes - 1) * 1024 + iterations - 1); }))
        throw std::runtime_error("mixed batch modified its input guards");
    runtime.deallocate(a); runtime.deallocate(b);
    if (benchmark) {
        std::printf("BATCH_BENCH {\"limit\":%u,\"operations\":601,\"submissions\":%llu,\"warmups\":8,\"samples_ms\":[",
            runtime.batch_ops, (unsigned long long)submissions);
        for (size_t i = 0; i < samples.size(); ++i) std::printf("%s%.6f", i ? "," : "", samples[i]);
        std::puts("]}");
        return;
    }
    constexpr size_t pitch = 64, width = 31, rows = 7;
    std::array<uint8_t, rows * pitch + 2> input{}, result{};
    for (size_t i = 0; i < input.size(); ++i) input[i] = uint8_t(i * 17 + 3);
    auto* data = static_cast<uint8_t*>(runtime.allocate(input.size()));
    setup.memcpy(data, input.data(), input.size()).wait_and_throw();
    auto before = runtime.invoke([&] { return runtime.gpu.completed(queue); });
    result.fill(0xfe);
    batch.copy_2d(result.data() + 1, pitch, data + 1, pitch, width, rows);
    if (!batch.wait(5000000000ull) || runtime.invoke([&] { return runtime.gpu.completed(queue); }) - before != (rows + runtime.batch_ops - 1) / runtime.batch_ops)
        throw std::runtime_error("padded 2D readback did not batch rows or flush its tail");
    for (size_t i = 0; i < result.size(); ++i) {
        const bool copied = i >= 1 && i <= rows * pitch && (i - 1) % pitch < width;
        if (result[i] != (copied ? input[i] : uint8_t(0xfe)))
            throw std::runtime_error("padded 2D readback changed padding or guards");
    }
    before = runtime.invoke([&] { return runtime.gpu.completed(queue); });
    result.fill(0xfe);
    batch.copy_2d(result.data() + 1, pitch, data + 1, pitch, pitch, rows);
    if (!batch.wait(5000000000ull) || runtime.invoke([&] { return runtime.gpu.completed(queue); }) - before != 1 ||
        result.front() != 0xfe || result.back() != 0xfe ||
        !std::equal(input.begin() + 1, input.end() - 1, result.begin() + 1))
        throw std::runtime_error("contiguous 2D copy was not one guarded transfer");
    runtime.deallocate(data);
    std::printf("PASS eager batching: 601 mixed fill/copy/kernel operations, %llu submissions at limit %u, changing inputs, full guards, padded/contiguous 2D readbacks and immediate tail flush\n",
        (unsigned long long)submissions, runtime.batch_ops);
}
#endif
}

int main(int argc, char** argv) try {
    if (argc != 2 && (argc != 3 || (std::strcmp(argv[2], "--batch-bench") && std::strcmp(argv[2], "--event-bench"))))
        throw std::invalid_argument("usage: native-queue-check image-directory [--batch-bench|--event-bench]");
    queue_dependencies();
    strata::l0::Runtime runtime;
    strata::l0::Program program(runtime, argv[1]);
#ifdef STRATA_ORCA_DG2
    if (argc == 3 && !std::strcmp(argv[2], "--event-bench")) {
        inline_event_check(runtime, program, true); queue_dependencies(); return 0;
    }
    inline_event_check(runtime, program, false);
    batch_check(runtime, program, argc == 3);
    if (argc == 3) { queue_dependencies(); return 0; }
    copy_batch_check(runtime, program);
#else
    if (argc == 3) throw std::invalid_argument("batch benchmark requires the native DG2 backend");
#endif
    strata::l0::Queue compute(runtime, program), copy(runtime, program, true);
    {
        constexpr size_t count = 64;
        auto* host = static_cast<float*>(runtime.allocate(count * sizeof(float), strata::l0::Memory::host));
        auto* table = static_cast<uint64_t*>(runtime.allocate(4 * sizeof(uint64_t)));
        auto* values = static_cast<float*>(runtime.allocate(4 * sizeof(float)));
        const std::array<size_t,3> offsets{0,7,63};
        std::array<uint64_t,4> pointers{};
        for (size_t i = 0; i < offsets.size(); ++i) {
            const auto* address = strata::l0::device_pointer(host + offsets[i], compute);
            pointers[i] = reinterpret_cast<uint64_t>(address);
            if (strata::l0::device_pointer(address, compute) != address)
                throw std::runtime_error("GPU alias was translated twice");
#ifdef STRATA_ORCA_DG2
            if (address == host + offsets[i]) throw std::runtime_error("DG2 serialized host pointer was not translated");
#endif
        }
        if (strata::l0::device_pointer(static_cast<const float*>(nullptr), compute))
            throw std::runtime_error("null GPU alias changed");
#ifdef STRATA_ORCA_DG2
        float foreign = 0;
        bool rejected = false;
        try { (void)strata::l0::device_pointer(&foreign, compute); } catch (const std::runtime_error&) { rejected = true; }
        if (!rejected) throw std::runtime_error("GPU alias accepted unregistered host storage");
#endif
        compute.memcpy(table, pointers.data(), sizeof(pointers)).wait_and_throw();
        compute.begin_recording();
        compute.parallel_for(sycl::nd_range<1>(32, 32), [=](sycl::nd_item<1> item) {
            const auto i = item.get_local_id(0);
            if (i < 4) values[i] = table[i] ? *reinterpret_cast<const float*>(table[i]) : -17.f;
        });
        auto graph = compute.end_recording();
        for (int replay = 0; replay < 32; ++replay) {
            for (size_t i = 0; i < count; ++i) host[i] = float(100 * replay + i);
            compute.replay(graph);
            std::array<float,4> output{};
            compute.memcpy(output.data(), values, sizeof(output)).wait_and_throw();
            for (size_t i = 0; i < output.size(); ++i)
                if (output[i] != (i < offsets.size() ? host[offsets[i]] : -17.f))
                    throw std::runtime_error("serialized GPU pointer-table read mismatch");
            for (size_t i = 0; i < count; ++i)
                if (host[i] != float(100 * replay + i)) throw std::runtime_error("GPU pointer-table read modified host input");
        }
        graph.reset();
        compute.wait_and_throw();
        runtime.deallocate(values); runtime.deallocate(table); runtime.deallocate(host);
        std::puts("PASS serialized GPU pointer table: 32 graph replays, host/interior/GPU/null aliases, fresh host values and full input guards");
    }
    const auto& device = compute.get_device();
    if (device.get_info<sycl::info::device::name>() != runtime.properties().name ||
        device.get_info<sycl::info::device::vendor_id>() != 0x8086 ||
        !device.has(sycl::aspect::fp16) || !device.has(sycl::aspect::usm_host_allocations) ||
        device.get_info<sycl::info::device::global_mem_size>() == 0)
        throw std::runtime_error("native device properties mismatch");
    auto* data = static_cast<float*>(runtime.allocate(256 * sizeof(float)));
    auto* result = static_cast<float*>(runtime.allocate(256 * sizeof(float), strata::l0::Memory::host));
    auto* flag = static_cast<int*>(runtime.allocate(sizeof(int), strata::l0::Memory::host));
    *flag = 0;
    auto gate = std::make_shared<strata::l0::NativeEvent>(runtime);
    struct ReleaseGate {
        std::shared_ptr<strata::l0::NativeEvent> event;
        bool released = false;
        void release() {
            strata::l0::signal_event(event->handle);
            released = true;
        }
        ~ReleaseGate() { if (!released) try { strata::l0::signal_event(event->handle); } catch (...) {} }
    } release_gate{gate};
    auto first = compute.parallel_for(sycl::nd_range<1>(256, 32), [=](sycl::nd_item<1> item) {
        data[item.get_global_id(0)] = float(item.get_global_id(0));
    });
    compute.submit([&](auto& handler) {
        orca::l0::compiler::local_accessor<float, 1> empty(sycl::range<1>(0), handler);
        handler.parallel_for(sycl::nd_range<1>(32, 32), [=](sycl::nd_item<1> item) {
            // Keep the local pointer in the ABI without accessing empty scratch.
            if (item.get_global_id(0) == 0 && *flag != 0) empty[0] = 1.f;
        });
    });
    compute.ext_oneapi_submit_barrier({strata::l0::Event::borrow(gate->handle)});
    struct DeferredFill {
        strata::l0::Queue& queue;
        float* data;
        int flushed = 0;
        ~DeferredFill() { queue.cancel_deferred(this); }
    } deferred{compute, data};
    compute.defer(&deferred, [](void* owner) {
        auto& work = *static_cast<DeferredFill*>(owner);
        ++work.flushed;
        work.queue.fill(work.data, -77.f, 256);
    });
    bool watchdog_empty = true, watchdog_waited = true;
    std::exception_ptr watchdog_error;
    std::thread watchdog([&] {
        try {
            watchdog_empty = compute.ext_oneapi_empty();
            watchdog_waited = compute.wait(0);
        } catch (...) { watchdog_error = std::current_exception(); }
    });
    watchdog.join();
    if (watchdog_error) std::rethrow_exception(watchdog_error);
    if (watchdog_empty || watchdog_waited) throw std::runtime_error("watchdog missed the native queue gate");
    copy.depends_on(first);
    copy.memcpy(result, data, 256 * sizeof(float));
    // The copy depends on the earlier kernel, not the later blocked queue tail.
    // Always release the gate before reporting a failure or unwinding queues.
    bool copied = false;
    std::exception_ptr copy_error;
    try { copied = copy.wait(5000000000ULL); } catch (...) { copy_error = std::current_exception(); }
    release_gate.release();
    compute.wait_and_throw();
    if (copy_error) std::rethrow_exception(copy_error);
    if (!copied) throw std::runtime_error("cross-queue snapshot waited for the later queue tail");
    if (deferred.flushed != 1) throw std::runtime_error("deferred queue work did not flush exactly once");
    for (int i = 0; i < 256; ++i)
        if (result[i] != float(i)) throw std::runtime_error("cross-queue copy mismatch");

    std::atomic<int> callbacks{0};
    auto deferred_done = compute.submit([&](strata::l0::Queue& handler) {
        handler.defer(&deferred, [](void* owner) {
            auto& work = *static_cast<DeferredFill*>(owner);
            ++work.flushed;
            work.queue.fill(work.data, -55.f, 256);
        });
    });
    copy.depends_on(deferred_done);
    copy.memcpy(result, data, 256 * sizeof(float)).wait_and_throw();
    if (deferred.flushed != 2 || result[255] != -55.f)
        throw std::runtime_error("completion marker excluded deferred work");
    // Restore the earlier snapshot expected by the callback checks below.
    copy.fill(result, 255.f, 256).wait_and_throw();
    for (int i = 1; i <= 8; ++i) {
        auto done = copy.submit([&](strata::l0::Queue& handler) {
            handler.host_task([&, i] {
                if (callbacks != i - 1 || result[255] != 255.f)
                    throw std::runtime_error("native callback ordering mismatch");
                ++callbacks;
                *flag = i;
            });
        });
        compute.depends_on(done);
    }
    compute.parallel_for(sycl::nd_range<1>(256, 32), [=](sycl::nd_item<1> item) {
        const auto i = item.get_global_id(0);
        data[i] = float(i + *flag);
    });
    compute.memcpy(result, data, 256 * sizeof(float)).wait_and_throw();
    copy.wait_and_throw();
    if (callbacks != 8) throw std::runtime_error("native callback dropped");
    for (int i = 0; i < 256; ++i)
        if (result[i] != float(i + 8)) throw std::runtime_error("callback-to-kernel dependency mismatch");

    const auto properties = sycl::ext::oneapi::experimental::properties{
        sycl::ext::intel::experimental::grf_size<256>};
    compute.parallel_for(sycl::range<1>(16), properties, [=](sycl::id<1> i) SYCL_ESIMD_KERNEL {
        sycl::ext::intel::esimd::simd<float, 16> values{float(i[0])};
        values.copy_to(data + i[0] * 16);
    });
    compute.memcpy(result, data, 256 * sizeof(float)).wait_and_throw();
    for (int i = 0; i < 256; ++i)
        if (result[i] != float(i / 16)) throw std::runtime_error("native ESIMD range/property mismatch");

    for (bool allow_independent : {false, true}) {
        compute.begin_recording(allow_independent);
        if (allow_independent) {
            bool caught = false;
            try { compute.independent([] { throw std::runtime_error("phase test"); }); }
            catch (const std::runtime_error&) { caught = true; }
            if (!caught || !compute.is_in_order()) throw std::runtime_error("native phase exception lost ordering");
            compute.independent([&] {
                int rejected = 0;
                try { compute.independent([] {}); } catch (const std::logic_error&) { ++rejected; }
                try { compute.end_recording(); } catch (const std::logic_error&) { ++rejected; }
                if (rejected != 2 || compute.is_in_order()) throw std::runtime_error("invalid native phase state accepted");
            });
        }
        strata::l0::Event captured;
        compute.independent([&] {
            compute.parallel_for(sycl::nd_range<1>(128, 32), [=](sycl::nd_item<1> item) {
                const auto i = item.get_global_id(0);
                data[i] = float(i + *flag);
            });
            compute.parallel_for(sycl::nd_range<1>(128, 32), [=](sycl::nd_item<1> item) {
                const auto i = item.get_global_id(0);
                data[128 + i] = float(2 * i + *flag);
            });
        });
        captured = compute.parallel_for(sycl::nd_range<1>(128, 32), [=](sycl::nd_item<1> item) {
            const auto i = item.get_global_id(0);
            data[i] += data[128 + i];
        });
        compute.memcpy(result, data, 256 * sizeof(float));
        auto graph = compute.end_recording();
#ifdef STRATA_ORCA_DG2
        if (graph->commands->in_order != (!allow_independent || !runtime.relaxed_recording))
            throw std::runtime_error("DG2 graph discarded its requested ordering");
#endif
        for (int pass = 0; pass < 16; ++pass) {
            *flag = pass;
            compute.replay(graph).wait_and_throw();
            captured.wait_and_throw();
            for (int i = 0; i < 256; ++i)
                if (result[i] != float(i < 128 ? 3 * i + 2 * pass : 2 * (i - 128) + pass))
                    throw std::runtime_error("native graph lost an independent producer or its dependent join");
        }
        compute.wait_and_throw();
        graph.reset();
        captured.wait_and_throw(); // An event may outlive the recorded list after completion.
    }
#ifdef STRATA_ORCA_DG2
    // A rejected transfer activates the empty recorder before range validation.
    // Ending it must release ownership before later eager/ordered work starts.
    compute.begin_recording(true);
    bool invalid_capture = false;
    try { compute.memcpy(data + 256, data, sizeof(float)); }
    catch (const std::invalid_argument&) { invalid_capture = true; }
    auto empty_capture = compute.end_recording();
    if (!invalid_capture || !empty_capture->commands->segments.empty() || runtime.active != nullptr)
        throw std::runtime_error("empty DG2 capture retained its recorder or leaked relaxed ordering");
    compute.replay(empty_capture).wait_and_throw();
    empty_capture.reset();
    compute.parallel_for(sycl::nd_range<1>(256, 32), [=](sycl::nd_item<1> item) {
        data[item.get_global_id(0)] = 3.f;
    });
    compute.parallel_for(sycl::nd_range<1>(256, 32), [=](sycl::nd_item<1> item) {
        data[item.get_global_id(0)] += 7.f;
    });
    compute.memcpy(result, data, 256 * sizeof(float)).wait_and_throw();
    for (int i = 0; i < 256; ++i)
        if (result[i] != 10.f) throw std::runtime_error("empty capture broke later eager dependencies");
    std::puts("PASS empty captured transfer rejection: recorder released, empty replay and eager dependencies preserved");
#endif

    // A non-multiple of the block size must leave the guard element untouched.
    compute.fill(data, -3.f, 256);
    compute.parallel_for(sycl::range<1>(255), [=](sycl::id<1> i) { data[i] = float(i[0]); });
    compute.memcpy(result, data, 256 * sizeof(float)).wait_and_throw();
    for (int i = 0; i < 256; ++i)
        if (result[i] != (i == 255 ? -3.f : float(i)))
            throw std::runtime_error("native range/fill guard mismatch");
    std::vector<strata::l0::Event> held;
    for (int i = 0; i < 80; ++i) held.push_back(compute.memcpy(result, data, sizeof(float)));
    compute.wait_and_throw();
    for (const auto& event : held) if (!event.ready()) throw std::runtime_error("native event pool growth lost a marker");
    held.clear();
    for (int pass = 0; pass < 128; ++pass) {
        compute.parallel_for(sycl::nd_range<1>(32, 32), [=](sycl::nd_item<1> item) {
            if (item.get_global_id(0) == 0) data[0] = float(pass);
        });
        compute.memcpy(result, data, sizeof(float)).wait_and_throw();
        if (result[0] != float(pass)) throw std::runtime_error("native completion slot reuse mismatch");
    }

    // An unpublished lazy marker cannot let a consumer mutate its producer's
    // queue. Explicit barriers must already be usable across host threads.
    auto unpublished = compute.fill(data, 123.f, 1);
    bool rejected = false;
    std::thread invalid_reader([&] {
        try { (void)unpublished.native(); }
        catch (const std::logic_error&) { rejected = true; }
    });
    invalid_reader.join();
    if (!rejected) throw std::runtime_error("foreign thread materialized a lazy completion");
    unpublished.wait_and_throw();

    strata::l0::Event published;
    std::atomic<int> issued{0}, consumed{0};
    std::atomic<bool> handoff_failed{false};
    std::exception_ptr handoff_error;
    std::jthread reader([&](std::stop_token stop) {
        for (int pass = 1; pass <= 128; ++pass) {
            while (issued.load(std::memory_order_acquire) < pass) {
                if (stop.stop_requested()) return;
                std::this_thread::yield();
            }
            try {
                copy.depends_on(published);
                copy.memcpy(result, data, 256 * sizeof(float)).wait_and_throw();
                for (int i = 0; i < 256; ++i)
                    if (result[i] != float(pass)) throw std::runtime_error("threaded compute/copy dependency mismatch");
            } catch (...) {
                handoff_error = std::current_exception();
                handoff_failed.store(true, std::memory_order_release);
            }
            consumed.store(pass, std::memory_order_release);
            if (handoff_failed.load(std::memory_order_acquire)) return;
        }
    });
    for (int pass = 1; pass <= 128; ++pass) {
        compute.fill(data, float(pass), 256);
        published = compute.ext_oneapi_submit_barrier();
        issued.store(pass, std::memory_order_release);
        // First pass deliberately leaves the marker as the producer's tail.
        // Later passes also overlap an independent producer submission.
        if (pass > 1) compute.fill(flag, pass, 1);
        while (consumed.load(std::memory_order_acquire) < pass) std::this_thread::yield();
        if (handoff_failed.load(std::memory_order_acquire)) break;
    }
    reader.join();
    if (handoff_error) std::rethrow_exception(handoff_error);
    compute.wait_and_throw();
    copy.wait_and_throw();

    // Match routed Prefill's eight staging slots without a host wait per
    // expert. Hold compute until each burst is submitted so slot reuse must
    // wait on the correct completion, even on a fast or otherwise idle GPU.
    constexpr size_t slots = 8, width = 256, jobs = 128, bursts = 16;
    auto* staged = static_cast<float*>(runtime.allocate(slots * width * sizeof(float)));
    auto* source = static_cast<float*>(runtime.allocate(jobs * width * sizeof(float), strata::l0::Memory::host));
    auto* output = static_cast<float*>(runtime.allocate(jobs * width * sizeof(float)));
    auto* checked = static_cast<float*>(runtime.allocate(jobs * width * sizeof(float), strata::l0::Memory::host));
    std::array<strata::l0::Event, slots> copied_slots, used_slots;
    for (size_t burst = 0; burst < bursts; ++burst) {
        for (size_t i = 0; i < jobs * width; ++i) source[i] = float(burst * jobs * width + i);
        auto paused = std::make_shared<strata::l0::NativeEvent>(runtime);
        ReleaseGate release{paused};
        compute.ext_oneapi_submit_barrier({strata::l0::Event::borrow(paused->handle)});
        for (size_t job = 0; job < jobs; ++job) {
            const size_t slot = job % slots;
            if (burst || job >= slots) copy.depends_on(used_slots[slot]);
            copy.memcpy(staged + slot * width, source + job * width, width * sizeof(float));
            copied_slots[slot] = copy.ext_oneapi_submit_barrier();
            compute.depends_on(copied_slots[slot]);
            compute.parallel_for(sycl::nd_range<1>(width, 32), [=](sycl::nd_item<1> item) {
                const size_t i = item.get_global_id(0);
                output[job * width + i] = staged[slot * width + i] * 2.f + 1.f;
            });
            used_slots[slot] = compute.ext_oneapi_submit_barrier();
        }
        if (compute.ext_oneapi_empty() || copy.ext_oneapi_empty())
            throw std::runtime_error("staging ring ignored its pending reuse dependency");
        release.release();
        compute.memcpy(checked, output, jobs * width * sizeof(float)).wait_and_throw();
        copy.wait_and_throw();
        for (size_t i = 0; i < jobs * width; ++i)
            if (checked[i] != source[i] * 2.f + 1.f)
                throw std::runtime_error("asynchronous eight-slot staging ring overwrote live data");
    }
    std::puts("PASS asynchronous staging: eight slots, 2048 jobs, forced overlap and completion-pool reuse");
    for (int pass = 0; pass < 64; ++pass) {
        std::array<float, 32> input, output;
        for (size_t i = 0; i < input.size(); ++i) input[i] = float(pass * 100 + i);
        output.fill(-999.f);
        compute.memcpy(data, input.data(), sizeof(input));
        compute.memcpy(output.data(), data, sizeof(output)).wait_and_throw();
        if (output != input) throw std::runtime_error("event wait did not publish earlier staged readbacks");
    }
    std::puts("PASS event-only staged readbacks: 64 fresh destinations, no stream wait or later submission");

    {
        // Engine graphs survive their recorder. Replay must join both sides of
        // the caller's stream and publish readbacks from the retained queue.
        std::array<float, 32> input{}, before{}, after{};
        std::shared_ptr<strata::l0::NativeGraph> graph;
        {
            strata::l0::Queue recorder(runtime, program);
            recorder.begin_recording();
            recorder.memcpy(before.data(), data, sizeof(before));
            recorder.memcpy(data, input.data(), sizeof(input));
            graph = recorder.end_recording();
        }
        for (int pass = 0; pass < 32; ++pass) {
            for (size_t i = 0; i < input.size(); ++i) input[i] = float(pass * 100 + i);
            before.fill(-999.f); after.fill(-999.f);
            const float sentinel = float(-1000 - pass);
            compute.fill(data, sentinel, input.size());
            compute.replay(graph);
            compute.memcpy(after.data(), data, sizeof(after)).wait_and_throw();
            if (after != input || std::any_of(before.begin(), before.end(), [=](float value) { return value != sentinel; }))
                throw std::runtime_error("cross-stream graph lost ordering, source refresh or staged readbacks");
        }
        compute.wait_and_throw();
    }
    std::puts("PASS retained-queue graph: recorder destroyed, 32 replays, refreshed uploads and both stream joins");

    // PLE reuses each host buffer as soon as a later in-order fill publishes
    // its upload sequence. Check that protocol with real 1024-token payloads.
    constexpr size_t upload_words = 1024 * 2560, uploads = 64;
    auto* upload_host = static_cast<uint32_t*>(runtime.allocate(2 * upload_words * 4, strata::l0::Memory::host));
    auto* upload_device = static_cast<uint32_t*>(runtime.allocate(upload_words * 4));
    auto* upload_sequence = static_cast<uint64_t*>(runtime.allocate(2 * sizeof(uint64_t), strata::l0::Memory::host));
    auto* upload_bad = static_cast<uint32_t*>(runtime.allocate(sizeof(uint32_t)));
    upload_sequence[0] = upload_sequence[1] = 0;
    uint64_t wanted[2]{};
    compute.fill(upload_bad, uint32_t(0), 1);
    auto upload_gate = std::make_shared<strata::l0::NativeEvent>(runtime);
    ReleaseGate upload_release{upload_gate};
    compute.ext_oneapi_submit_barrier({strata::l0::Event::borrow(upload_gate->handle)});
    for (uint32_t pass = 1; pass <= uploads; ++pass) {
        const auto slot = (pass - 1) % 2;
        const auto deadline = std::chrono::steady_clock::now() + std::chrono::seconds(10);
        while (static_cast<volatile const uint64_t*>(upload_sequence)[slot] < wanted[slot]) {
            if (std::chrono::steady_clock::now() >= deadline)
                throw std::runtime_error("upload sequence did not reach the host");
            std::this_thread::yield();
        }
        auto* source = upload_host + slot * upload_words;
        for (size_t i = 0; i < upload_words; ++i) source[i] = uint32_t(i) * 2654435761u ^ pass;
        compute.memcpy(upload_device, source, upload_words * 4);
        wanted[slot] = pass;
        compute.fill(upload_sequence + slot, wanted[slot], 1);
        if (pass == 1) {
            if (static_cast<volatile const uint64_t*>(upload_sequence)[slot] != 0)
                throw std::runtime_error("upload sequence fill overtook its blocked copy");
            upload_release.release();
        }
        compute.parallel_for(sycl::range<1>(upload_words), [=](sycl::id<1> item) {
            if (upload_device[item] != (uint32_t(item[0]) * 2654435761u ^ pass))
                sycl::atomic_ref<uint32_t, sycl::memory_order::relaxed, sycl::memory_scope::device,
                    sycl::access::address_space::global_space>(*upload_bad).store(1);
        });
    }
    uint32_t bad_upload = 0;
    compute.memcpy(&bad_upload, upload_bad, sizeof(bad_upload)).wait_and_throw();
    if (bad_upload) throw std::runtime_error("upload completion sequence allowed early host-buffer reuse");
    std::puts("PASS upload sequences: 64 ten-MiB uploads, two host buffers, no host wait for a consuming kernel");

#ifdef STRATA_ORCA_DG2
    compute.wait_and_throw(); copy.wait_and_throw();
    {
        std::array<float, 32> input{}, output{};
        strata::l0::dg2_backend::HostEvent reusable(runtime);
        struct Drain {
            strata::l0::dg2_backend::HostEvent& event;
            strata::l0::Queue& queue;
            ~Drain() { try { event.signal(); queue.wait_and_throw(); } catch (...) {} }
        } drain{reusable, compute};
        reusable.signal(); // A captured wait must survive a later reset even if signaled now.
        std::shared_ptr<strata::l0::NativeGraph> graph;
        {
            strata::l0::Queue recorder(runtime, program);
            recorder.begin_recording();
            recorder.ext_oneapi_submit_barrier({strata::l0::Event::borrow(reusable.handle())});
            recorder.memcpy(data, input.data(), sizeof(input));
            recorder.memcpy(output.data(), data, sizeof(output));
            graph = recorder.end_recording();
        }
        for (int pass = 0; pass < 32; ++pass) {
            reusable.reset();
            for (size_t i = 0; i < input.size(); ++i) input[i] = float(pass * 100 + i);
            output.fill(-999.f);
            auto done = compute.replay(graph);
            const bool escaped = done.wait(1000000ull);
            bool rejected = false;
            try { reusable.reset(); } catch (const std::runtime_error&) { rejected = true; }
            reusable.signal();
            done.wait_and_throw();
            if (escaped || !rejected || output != input)
                throw std::runtime_error("reusable event lost its captured wait, in-flight reset guard or readback");
        }
    }
    std::puts("PASS reusable host event: signaled capture, recorder destroyed, 32 reset/replays, timed waits and in-flight reset rejection");
    {
        strata::l0::dg2_backend::Stream producer(runtime, program), consumer(runtime, program);
        auto gate = std::make_shared<strata::l0::NativeEvent>(runtime);
        ReleaseGate release{gate};
        strata::l0::dg2_backend::Event first, last;
        first.runtime = last.runtime = &runtime;
        const uint32_t early = 11, final = 77;
        producer.barrier(nullptr, {gate->handle});
        producer.fill(upload_bad, &early, sizeof(early), sizeof(early));
        producer.barrier(&first);
        producer.fill(upload_bad, &final, sizeof(final), sizeof(final));
        producer.barrier(&last);
        // More than 16 events still need only one native predecessor: retain
        // the newest point even when an older point appears later in the list.
        consumer.barrier(nullptr, {&first, &last, &first, &last, &first, &last,
            &first, &last, &first, &last, &first, &last, &first, &last, &first, &last, &last, &first});
        uint32_t value = 0;
        consumer.copy(&value, upload_bad, sizeof(value));
        if (consumer.wait(0)) throw std::runtime_error("coalesced dependency ignored its producer gate");
        release.release();
        if (!consumer.wait(5000000000ull) || value != final || !producer.wait(5000000000ull))
            throw std::runtime_error("coalesced dependencies did not retain the latest producer point");
        compute.fill(upload_bad, uint32_t(0), 1).wait_and_throw();
    }
    std::puts("PASS dependency coalescing: 18 events, one gated predecessor queue, latest point retained");
    {
        // Freeze a completed prefix behind one gate, with a separately gated
        // tail. A nonblocking collection must publish every earlier readback.
        strata::l0::dg2_backend::Stream batch(runtime, program);
        auto start = std::make_shared<strata::l0::NativeEvent>(runtime);
        auto tail = std::make_shared<strata::l0::NativeEvent>(runtime);
        ReleaseGate release_start{start}, release_tail{tail};
        strata::l0::dg2_backend::Event prefix;
        prefix.runtime = &runtime;
        std::array<uint32_t, 16> readbacks;
        readbacks.fill(UINT32_MAX);
        batch.barrier(nullptr, {start->handle});
        for (auto& value : readbacks) batch.copy(&value, upload_bad, sizeof(value));
        batch.barrier(&prefix);
        batch.barrier(nullptr, {tail->handle});
        batch.barrier();
        // Publish the blocked tail before opening the first gate.
        if (batch.wait(0)) throw std::runtime_error("batch ignored its first gate");
        release_start.release();
        if (!prefix.point->gpu.wait(5000000000ull)) throw std::runtime_error("batch prefix GPU timeout");
        for (auto value : readbacks)
            if (value != UINT32_MAX) throw std::runtime_error("unobserved staged readback was published early");
        if (batch.wait(0)) throw std::runtime_error("batch collection waited past the gated tail");
        for (auto value : readbacks)
            if (value != 0) throw std::runtime_error("batch collection failed to publish a completed prefix");
        release_tail.release();
        if (!batch.wait(5000000000ull)) throw std::runtime_error("batch tail timeout");
    }
    std::puts("PASS batch collection: 16 staged readbacks published without waiting for a gated tail");
    {
        // More retained graph queues than one native query batch can hold.
        // Keep every graph pending, then collect its staged results while a
        // later stream tail is still blocked on another host gate.
        constexpr size_t graph_count = 18;
        std::array<std::array<uint32_t, 4>, graph_count> output;
        std::array<strata::l0::dg2_backend::Commands*, graph_count> graphs{};
        strata::l0::dg2_backend::Stream batch(runtime, program);
        for (size_t i = 0; i < graph_count; ++i) {
            strata::l0::dg2_backend::Stream recorder(runtime, program);
            recorder.begin_recording(strata::l0::dg2_backend::Ordering::in_order);
            const uint32_t value = uint32_t(i + 17);
            recorder.fill(upload_bad, &value, sizeof(value), sizeof(value));
            recorder.copy(&output[i][1], upload_bad, sizeof(value));
            recorder.copy(&output[i][2], upload_bad, sizeof(value));
            graphs[i] = &recorder.end_recording();
        }
        for (int pass = 0; pass < 2; ++pass) {
            for (auto& row : output) row.fill(UINT32_MAX);
            auto start = std::make_shared<strata::l0::NativeEvent>(runtime);
            auto tail = std::make_shared<strata::l0::NativeEvent>(runtime);
            ReleaseGate release_start{start}, release_tail{tail};
            strata::l0::dg2_backend::Event prefix;
            prefix.runtime = &runtime;
            batch.barrier(nullptr, {start->handle});
            for (auto* graph : graphs) batch.replay(*graph);
            batch.barrier(&prefix);
            batch.barrier(nullptr, {tail->handle});
            if (batch.wait(0)) throw std::runtime_error("cross-queue batch escaped its start gate");
            release_start.release();
            if (!prefix.point->gpu.wait(5000000000ull)) throw std::runtime_error("cross-queue batch prefix timeout");
            for (const auto& row : output)
                if (std::any_of(row.begin(), row.end(), [](uint32_t value) { return value != UINT32_MAX; }))
                    throw std::runtime_error("cross-queue staged readback was published before collection");
            if (batch.wait(0)) throw std::runtime_error("cross-queue collection waited for the gated tail");
            for (size_t i = 0; i < graph_count; ++i)
                if (output[i][0] != UINT32_MAX || output[i][3] != UINT32_MAX ||
                    output[i][1] != i + 17 || output[i][2] != i + 17)
                    throw std::runtime_error("cross-queue batch lost a completed readback, queue order or output guard");
            release_tail.release();
            if (!batch.wait(5000000000ull)) throw std::runtime_error("cross-queue batch tail timeout");
        }
        for (auto* graph : graphs) runtime.discard(*graph);
        compute.fill(upload_bad, uint32_t(0), 1).wait_and_throw();
    }
    std::puts("PASS cross-queue batch collection: 18 retained graph queues, two waves, 72 staged readbacks, bounded lookahead, fresh guards and collection before the gated tail");
    {
        auto* tiny = static_cast<uint8_t*>(runtime.allocate(1));
        bool bounded = false, base_only = false;
        try { (void)runtime.address(tiny, 2); } catch (const std::invalid_argument&) { bounded = true; }
        try { runtime.deallocate(tiny + 1); } catch (const std::invalid_argument&) { base_only = true; }
        if (!bounded || !base_only) throw std::runtime_error("native allocation padding/base validation failed");
        compute.fill(tiny, uint8_t(0x5a), 1);
        uint8_t value = 0;
        compute.memcpy(&value, tiny, 1).wait_and_throw();
        compute.wait_and_throw(); copy.wait_and_throw();
        const auto physical = runtime.blocks.back().memory.bytes;
        const auto before = runtime.invoke([&] { return runtime.gpu.allocated_bytes(); });
        runtime.deallocate(tiny);
        if (value != 0x5a || runtime.invoke([&] { return runtime.gpu.allocated_bytes(); }) != before - physical)
            throw std::runtime_error("native adapter did not release its backing allocation");
    }
    std::puts("PASS adapter allocation release: logical bounds, base pointer and physical backing reclamation");
#endif

    strata::l0::Queue failing(runtime, program, true);
    auto failed_completion = failing.submit([](strata::l0::Queue& handler) {
        handler.host_task([] { throw std::runtime_error("expected callback failure"); });
    });
    bool propagated = false;
    try { failed_completion.wait_and_throw(); }
    catch (const std::runtime_error& error) { propagated = std::strcmp(error.what(), "expected callback failure") == 0; }
    if (!propagated) throw std::runtime_error("native callback exception was lost");
    queue_dependencies();
    std::puts("PASS native engine queues: exact completion points, compute/copy dependencies, SPSC callbacks, graph replay, callback error release");
    return 0;
} catch (const std::exception& error) {
    std::fprintf(stderr, "%s\n", error.what());
    return 1;
}
