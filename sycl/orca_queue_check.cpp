#include "orca_queue.hpp"
#include <cstdio>
#include <cstring>
#include <chrono>
#include <sycl/ext/intel/esimd.hpp>
#include <sycl/ext/intel/experimental/grf_size_properties.hpp>
#include "../orca-l0/tests/runtime_dependencies.hpp"

int main(int argc, char** argv) try {
    if (argc != 2) throw std::invalid_argument("usage: native-queue-check image-directory");
    check_runtime_dependencies();
    orca::l0::Runtime runtime;
    orca::l0::compiler::Program program(runtime, argv[1]);
    strata::l0::Queue compute(runtime, program), copy(runtime, program, true);
    const auto& device = compute.get_device();
    if (device.get_info<sycl::info::device::name>() != runtime.properties().name ||
        device.get_info<sycl::info::device::vendor_id>() != 0x8086 ||
        !device.has(sycl::aspect::fp16) || !device.has(sycl::aspect::usm_host_allocations) ||
        device.get_info<sycl::info::device::global_mem_size>() == 0)
        throw std::runtime_error("native device properties mismatch");
    auto* data = static_cast<float*>(runtime.allocate(256 * sizeof(float)));
    auto* result = static_cast<float*>(runtime.allocate(256 * sizeof(float), orca::l0::Memory::host));
    auto* flag = static_cast<int*>(runtime.allocate(sizeof(int), orca::l0::Memory::host));
    *flag = 0;
    auto gate = std::make_shared<strata::l0::NativeEvent>(runtime);
    struct ReleaseGate {
        std::shared_ptr<strata::l0::NativeEvent> event;
        bool released = false;
        void release() {
            orca::l0::check(zeEventHostSignal(event->handle), "release native test gate");
            released = true;
        }
        ~ReleaseGate() { if (!released) zeEventHostSignal(event->handle); }
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
            captured = compute.parallel_for(sycl::nd_range<1>(256, 32), [=](sycl::nd_item<1> item) {
                const auto i = item.get_global_id(0);
                data[i] = float(i + *flag);
            });
        });
        compute.memcpy(result, data, 256 * sizeof(float));
        auto graph = compute.end_recording();
        for (int pass = 0; pass < 3; ++pass) {
            *flag = pass;
            compute.replay(graph).wait_and_throw();
            captured.wait_and_throw();
            for (int i = 0; i < 256; ++i)
                if (result[i] != float(i + pass)) throw std::runtime_error("native queue graph mismatch");
        }
        compute.wait_and_throw();
        graph.reset();
        captured.wait_and_throw(); // An event may outlive the recorded list after completion.
    }

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
    auto* source = static_cast<float*>(runtime.allocate(jobs * width * sizeof(float), orca::l0::Memory::host));
    auto* output = static_cast<float*>(runtime.allocate(jobs * width * sizeof(float)));
    auto* checked = static_cast<float*>(runtime.allocate(jobs * width * sizeof(float), orca::l0::Memory::host));
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

    // PLE reuses each host buffer as soon as a later in-order fill publishes
    // its upload sequence. Check that protocol with real 1024-token payloads.
    constexpr size_t upload_words = 1024 * 2560, uploads = 64;
    auto* upload_host = static_cast<uint32_t*>(runtime.allocate(2 * upload_words * 4, orca::l0::Memory::host));
    auto* upload_device = static_cast<uint32_t*>(runtime.allocate(upload_words * 4));
    auto* upload_sequence = static_cast<uint64_t*>(runtime.allocate(2 * sizeof(uint64_t), orca::l0::Memory::host));
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

    strata::l0::Queue failing(runtime, program, true);
    auto failed_completion = failing.submit([](strata::l0::Queue& handler) {
        handler.host_task([] { throw std::runtime_error("expected callback failure"); });
    });
    bool propagated = false;
    try { failed_completion.wait_and_throw(); }
    catch (const std::runtime_error& error) { propagated = std::strcmp(error.what(), "expected callback failure") == 0; }
    if (!propagated) throw std::runtime_error("native callback exception was lost");
    check_runtime_dependencies();
    std::puts("PASS native engine queues: exact completion points, compute/copy dependencies, SPSC callbacks, graph replay, callback error release");
    return 0;
} catch (const std::exception& error) {
    std::fprintf(stderr, "%s\n", error.what());
    return 1;
}
