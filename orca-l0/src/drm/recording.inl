// Shared Runtime::append implementation; included after the platform's State definition.
size_t Runtime::allocation_index(const Allocation& memory) const {
    require(bool(memory.identity), "invalid allocation identity");
    const auto& allocations = state_->allocations;
    const auto found = std::find_if(allocations.begin(), allocations.end(), [&](const Allocation& own) {
        return own.identity == memory.identity && own.data == memory.data && own.gpu == memory.gpu &&
            own.bytes == memory.bytes && own.handle == memory.handle && own.imported == memory.imported &&
            own.device_local == memory.device_local;
    });
    require(found != allocations.end(), "stale, foreign or modified allocation");
    return size_t(found - allocations.begin());
}
Allocation Runtime::allocate_storage(size_t bytes, bool device_local) {
    auto memory = device_local ? allocate_device(bytes) : allocate(bytes);
    try {
        auto entry = state_->storage.emplace(memory.handle, detail::RecordingStorage{memory, std::vector<uint64_t>(state_->queues.size() + 1)}).first;
        state_->storage_by_gpu.emplace(memory.gpu, &entry->second);
    } catch (...) { state_->failed = true; throw; }
    return memory;
}
Allocation Runtime::allocate_staging(size_t bytes) { return allocate_storage(bytes); }
void Runtime::barrier() {
    require(!state_->recording.closed && !state_->failed, "barrier requires an open healthy recording");
    // Match dispatch dependencies; completion tags and queue tails still flush to the host.
    detail::emit(state_->recording.commands, detail::barrier(false, state_->host_visibility));
}
void Runtime::signal_value(uint64_t address, uint64_t value) {
    require(!state_->recording.closed && !state_->failed, "signal requires an open healthy recording");
    const auto signal = detail::signal_value(address, value);
    detail::validate_gpu_range(address, 8, state_->allocations);
    use_address(address); detail::emit(state_->recording.commands, signal);
}
void Runtime::write_global_timestamp(uint64_t address) {
    require(!state_->recording.closed && !state_->failed, "timestamp requires an open healthy recording");
    const auto timestamp = detail::timestamp_value(address);
    detail::validate_gpu_range(address, 8, state_->allocations);
    use_address(address); detail::emit(state_->recording.commands, timestamp);
}
void Runtime::wait_value(uint64_t address, uint32_t value) {
    require(!state_->recording.closed && !state_->failed, "memory wait requires an open healthy recording");
    const auto wait = detail::wait_value(address, value);
    detail::validate_gpu_range(address, 4, state_->allocations);
    use_address(address); detail::emit(state_->recording.commands, wait);
}
bool Runtime::storage_idle(const Allocation& memory) const {
    const auto& s = *state_;
    const auto& use = s.storage.at(memory.handle).last_use;
    for (size_t i = 0; i < use.size(); ++i)
        if (use[i] > s.queue(i).done && !s.queue(i).observer->ready(use[i])) return false;
    return true;
}
void Runtime::use_storage(const Allocation& memory) {
    if (!memory.handle) return;
    auto* storage = &state_->storage.at(memory.handle);
    detail::retain_storage(state_->recording.storage, storage);
}
void Runtime::use_address(uint64_t address) {
    auto& ranges = state_->storage_by_gpu;
    auto found = ranges.upper_bound(address);
    if (found == ranges.begin()) return;
    const auto& memory = (--found)->second->allocation;
    if (address - memory.gpu < memory.bytes) detail::retain_storage(state_->recording.storage, found->second);
}
std::pair<Allocation, uint32_t> Runtime::capture(detail::RecordingHeap& heap, size_t bytes, bool device_local) {
    bytes = detail::align(bytes, 64);
    if (bytes > heap.allocation.bytes - heap.used || heap.allocation.device_local != device_local || !storage_idle(heap.allocation)) {
        // Captures remain immutable across replay/reset; old slabs stay owned by the runtime.
        heap.allocation = allocate_storage(std::max(bytes, size_t{1024 * 1024}), device_local); heap.used = 0;
    }
    const auto offset = uint32_t(heap.used); heap.used += bytes;
    if (!device_local) storage_access(heap.allocation);
    return std::make_pair(heap.allocation, offset);
}
std::pair<std::shared_ptr<const Allocation>, uint32_t> Runtime::capture(
        detail::CaptureHeap& heap, detail::AllocationPool& pool, size_t bytes) {
    bytes = detail::align(bytes, 64);
    if (!heap.allocation || bytes > heap.allocation->bytes - heap.used || !storage_idle(*heap.allocation)) {
        heap.allocation.reset();
        // Bootstrap transfers use separate pools so they cannot expand the mirrored normal slab set.
        auto& selected = state_->recording.queue == state_->storage_queue ? state_->transfer_parameter_pool : pool;
        heap.allocation = selected.take([&](size_t n) { return allocate_storage(n); }, std::max(bytes, size_t{1024 * 1024})); heap.used = 0;
    }
    const auto offset = uint32_t(heap.used); heap.used += bytes;
    storage_access(*heap.allocation);
    return std::make_pair(heap.allocation, offset);
}
namespace {
void retain_capture(std::vector<std::shared_ptr<const Allocation>>& captures, const std::shared_ptr<const Allocation>& memory) {
    if (!captures.empty() && captures.back() == memory) return;
    if (std::find(captures.begin(), captures.end(), memory) == captures.end()) captures.push_back(memory);
}
} // namespace
uint64_t Runtime::allocated_bytes() const {
    uint64_t bytes = 0;
    for (const auto& a : state_->allocations) {
        require(a.bytes <= UINT64_MAX - bytes, "allocation byte count overflow");
        bytes += a.bytes;
    }
    return bytes;
}
void Runtime::reset_gate(const HostGate& gate) {
    auto& s = *state_;
    require(gate.state_ && gate.state_->owner.get() == s.identity.get() && gate.state_->reusable,
            "reset requires this Runtime's reusable host gate");
    require(!s.failed && storage_idle(gate.state_->memory), "host gate reset before owner-observed completion");
    gate.state_->reset();
}
uint64_t Runtime::gpu_address(const void* pointer) const {
    require(!state_->failed, "address translation after runtime failure");
    return detail::gpu_address(uint64_t(reinterpret_cast<uintptr_t>(pointer)), state_->allocations, nullptr, &state_->cpu_bounds);
}
uint32_t Runtime::suggest_group_size(const std::string& path, const std::string& name, uint32_t global) {
    auto& s = *state_;
    require(!s.recording.closed && !s.failed, "range suggestion requires an open, healthy recording");
    return detail::suggest_group_size(detail::recorded_image(s.images, path, name).kernel, global);
}
void Runtime::set_memory_module(const std::string& path, bool local_thread_memory, bool local_isa) {
    auto& s = *state_;
    require(!s.recording.closed && s.idle() && !s.failed && s.memory_module.empty() && !path.empty(), "memory module requires an open, healthy, unbound recording");
    // Validate all entry points before accepting the binding; replay never opens this file.
    for (const char* name : {"orca_copy", "orca_fill", "orca_fill_wide"}) {
        const auto& k = detail::recorded_image(s.images, path, name).kernel;
        require(!(local_thread_memory || local_isa) || (!k.scratch_bytes[0] && !k.scratch_bytes[1] && !k.private_bytes),
                "internal transfers require scratch/private-free memory kernels");
    }
    const auto module = detail::read_zebin(path);
    bool copy_2d = false;
    for (size_t i = 1; i < module.sections.size(); ++i)
        if (module.name(i) == ".text.orca_copy_2d") copy_2d = true;
    if (copy_2d) {
        const auto& k = detail::recorded_image(s.images, path, "orca_copy_2d").kernel;
        require(!(local_thread_memory || local_isa) || (!k.scratch_bytes[0] && !k.scratch_bytes[1] && !k.private_bytes),
                "internal transfers require scratch/private-free memory kernels");
    }
    bool copy_coaligned = false;
    for (size_t i = 1; i < module.sections.size(); ++i)
        if (module.name(i) == ".text.orca_copy_coaligned") copy_coaligned = true;
    if (copy_coaligned) {
        const auto& k = detail::recorded_image(s.images, path, "orca_copy_coaligned").kernel;
        require(!(local_thread_memory || local_isa) || (!k.scratch_bytes[0] && !k.scratch_bytes[1] && !k.private_bytes),
                "internal transfers require scratch/private-free memory kernels");
    }
    s.memory_copy_coaligned = copy_coaligned;
    s.memory_copy_2d = copy_2d; s.memory_module = path;
    s.local_thread_memory = local_thread_memory; s.local_isa = local_isa;
#ifdef _WIN32
    if (local_isa && !s.isa_heap.allocation.handle && !s.memory_isa_heap.allocation.handle) {
        // Keep bootstrap and mathematical code near each other before large tensor allocations.
        s.memory_isa_heap.allocation = allocate_storage(1024 * 1024);
        s.isa_heap.allocation = allocate_storage(4 * 1024 * 1024, true);
    }
#endif
    if (local_thread_memory || local_isa) {
        s.storage_queue = queue_index(create_queue());
        if (local_thread_memory) s.thread_readback = allocate_storage(3 * detail::thread_memory_guard);
    }
}
void Runtime::transfer_storage(uint64_t destination, uint64_t source, size_t bytes, uint8_t pattern) {
    auto& s = *state_;
    require(s.storage_queue != SIZE_MAX && !s.failed, "internal transfer queue is unavailable");
    auto saved = std::move(s.recording);
    auto parameters = std::move(s.indirect_heap);
    s.indirect_heap = {};
    s.recording = {}; s.recording.queue = s.storage_queue;
    try {
        // A separate queue must progress even when the caller's list or another
        // queue has an unsignaled host dependency. Transfer kernels cannot recurse
        // into scratch/private initialization (validated at module binding).
        if (source) copy(destination, source, bytes);
        else fill(destination, &pattern, sizeof(pattern), bytes);
        close(); submit_recording(SIZE_MAX, s.storage_queue); wait(); reset();
        s.recording = std::move(saved); s.indirect_heap = std::move(parameters);
    } catch (...) {
        s.recording = std::move(saved); s.indirect_heap = std::move(parameters); s.failed = true; throw;
    }
}
void Runtime::copy(uint64_t destination, uint64_t source, size_t bytes) {
    auto& s = *state_;
    require(!s.recording.closed && !s.failed, "copy requires an open, healthy recording");
    if (!bytes) return;
    detail::validate_gpu_range(destination, bytes, s.allocations);
    detail::validate_gpu_range(source, bytes, s.allocations);
    use_address(destination); use_address(source);
    if (destination == source) return;
    require(destination > source ? destination - source >= bytes : source - destination >= bytes,
            "partially overlapping GPU copy");
    require(!s.memory_module.empty(), "copy requires set_memory_module");
    const uint64_t count = bytes;
    const auto groups = uint32_t(std::min<size_t>(256, (bytes - 1) / 4096 + 1));
    const char* kernel = s.memory_copy_coaligned && ((destination | source) & 15) && !((destination ^ source) & 15)
        ? "orca_copy_coaligned" : "orca_copy";
    append(s.memory_module, kernel, 256, groups,
           {Argument::value(destination), Argument::value(source), Argument::value(count)});
}
void Runtime::copy_2d(uint64_t destination, size_t destination_pitch, uint64_t source, size_t source_pitch,
                      size_t width, size_t height) {
    auto& s = *state_;
    require(!s.recording.closed && !s.failed, "2D copy requires an open, healthy recording");
    if (!width || !height) return;
    require(width <= destination_pitch && width <= source_pitch &&
            height - 1 <= (SIZE_MAX - width) / std::max(destination_pitch, source_pitch),
            "invalid or overflowing pitched GPU copy");
    const auto destination_span = (height - 1) * destination_pitch + width;
    const auto source_span = (height - 1) * source_pitch + width;
    detail::validate_gpu_range(destination, destination_span, s.allocations);
    detail::validate_gpu_range(source, source_span, s.allocations);
    use_address(destination); use_address(source);
    if (destination == source && (height == 1 || destination_pitch == source_pitch)) return;
    require(destination > source ? destination - source >= source_span : source - destination >= destination_span,
            "overlapping pitched GPU copy spans");
    if (width == destination_pitch && width == source_pitch) { copy(destination, source, width * height); return; }
    require(!s.memory_module.empty(), "2D copy requires set_memory_module");
    // Cache the optional entry point once at binding. Existing three-entry
    // memory modules keep their previous row semantics without repeated queries.
    if (!s.memory_copy_2d) {
        for (size_t row = 0; row < height; ++row)
            copy(destination + row * destination_pitch, source + row * source_pitch, width);
        return;
    }
    const uint64_t dp = destination_pitch, sp = source_pitch, w = width, h = height;
    const auto groups = uint32_t(std::min<size_t>(256, (width * height - 1) / 4096 + 1));
    append(s.memory_module, "orca_copy_2d", 256, groups,
           {Argument::value(destination), Argument::value(source), Argument::value(dp), Argument::value(sp),
            Argument::value(w), Argument::value(h)});
}
void Runtime::fill(uint64_t destination, const void* pattern, size_t pattern_bytes, size_t bytes) {
    auto& s = *state_;
    require(!s.recording.closed && !s.failed, "fill requires an open, healthy recording");
    if (!bytes) return;
    detail::validate_gpu_range(destination, bytes, s.allocations);
    use_address(destination);
    require(pattern && pattern_bytes && pattern_bytes <= 128 && !(pattern_bytes & (pattern_bytes - 1)), "fill needs a power-of-two pattern of 1..128 bytes");
    require(!s.memory_module.empty(), "fill requires set_memory_module");
    const uint64_t count = bytes;
    const auto groups = uint32_t(std::min<size_t>(256, (bytes - 1) / 4096 + 1));
    if (pattern_bytes <= 16) {
        std::array<uint8_t, 16> captured;
        for (size_t i = 0; i < captured.size(); i += pattern_bytes) std::memcpy(captured.data() + i, pattern, pattern_bytes);
        append(s.memory_module, "orca_fill", 256, groups,
               {Argument::value(destination), Argument::value(count), Argument::value(captured)});
    } else try {
        // Capture larger patterns in the existing immutable heap, avoiding per-lane private-memory spills.
        const auto [memory, offset] = capture(s.indirect_heap, s.parameter_pool, pattern_bytes);
        retain_capture(s.recording.captures, memory);
        use_storage(*memory);
        std::memcpy(static_cast<uint8_t*>(memory->data) + offset, pattern, pattern_bytes);
        const auto address = memory->gpu + offset;
        const uint32_t mask = uint32_t(pattern_bytes - 1);
        append(s.memory_module, "orca_fill_wide", 256, groups,
               {Argument::value(destination), Argument::value(count), Argument::value(address), Argument::value(mask)});
    } catch (...) { s.failed = true; throw; }
}
void Runtime::append(const std::string& path, const std::string& name, uint32_t local, uint32_t groups,
                     const std::vector<Argument>& args) {
    append(path, name, {local, 1, 1}, {groups, 1, 1}, args);
}
void Runtime::append(const std::string& path, const std::string& name, std::array<uint32_t, 3> local,
                     std::array<uint32_t, 3> groups, const std::vector<Argument>& args) {
    auto& s = *state_;
    require(!s.recording.closed && !s.failed, "recording closed/failed");
    auto& entry = detail::recorded_image(s.images, path, name);
    // Validate user arguments before changing the recording or allocating resources.
    auto cross = detail::pack_arguments(entry.kernel, local, groups, args, s.allocations, 0, 0, 0, &entry.resident_hints, &s.cpu_bounds);
    try {
        auto write_storage = [&](const Allocation& memory, size_t offset, const void* source, size_t bytes, size_t padded) {
            require(source && bytes && bytes <= padded && offset <= memory.bytes && padded <= memory.bytes - offset,
                    "invalid internal upload range");
            auto staging = memory;
            if (memory.device_local) {
                // ponytail: reuse the first-use ISA staging slab for module data
                // and descriptors too; replay never stages or uploads these bytes.
                if (s.isa_staging.bytes < padded)
                    s.isa_staging = allocate_storage(std::max(padded, size_t{1024 * 1024}));
                staging = s.isa_staging;
            }
            storage_access(staging);
            auto* destination = static_cast<uint8_t*>(staging.data) + (memory.device_local ? 0 : offset);
            std::memcpy(destination, source, bytes);
            std::memset(destination + bytes, 0, padded - bytes);
            if (memory.device_local) transfer_storage(memory.gpu + offset, staging.gpu, padded);
        };
        // Keep one valid parameter base when the first kernel uses only inline arguments.
        if (!s.indirect_heap.allocation) (void)capture(s.indirect_heap, s.parameter_pool, 64);
        if (!entry.isa.handle) {
            const auto& loaded = entry.kernel;
            const bool local_isa = s.local_isa && path != s.memory_module;
#ifdef _WIN32
            // DG2 default in pinned NEO; calibrated on Windows A770 C0.
            const auto bytes = detail::align(loaded.isa.size(), 64) + 512;
#else
            // ponytail: retain conservative Linux padding until GPU validation.
            const auto bytes = detail::align(loaded.isa.size(), 64) + 4096;
#endif
            auto& heap = s.local_isa && path == s.memory_module ? s.memory_isa_heap : s.isa_heap;
            std::tie(entry.isa, entry.isa_offset) = capture(heap, bytes, local_isa);
            write_storage(entry.isa, entry.isa_offset, loaded.isa.data(), loaded.isa.size(), bytes);
            auto data = s.module_data.find(path);
            if (data == s.module_data.end()) {
                detail::ModuleData storage;
                // Keep the bound memory module host-backed to bootstrap its own uploads.
                const bool local_data = s.storage_queue != SIZE_MAX && path != s.memory_module;
                if (!loaded.global_data.empty()) {
                    storage.globals = allocate_storage(loaded.global_data.size(), local_data);
                    write_storage(storage.globals, 0, loaded.global_data.data(), loaded.global_data.size(), loaded.global_data.size());
                }
                if (!loaded.constant_data.empty()) {
                    storage.constants = allocate_storage(loaded.constant_data.size(), local_data);
                    write_storage(storage.constants, 0, loaded.constant_data.data(), loaded.constant_data.size(), loaded.constant_data.size());
                }
                // All kernels in this module share one data segment, including across list resets.
                s.module_data.emplace(path, storage);
            }
        }
        const auto& k = entry.kernel;
        auto thread_pool = [&](uint32_t bytes, size_t& current, bool retain_unused = false) {
            // Reuse only this queue's immutable descriptor/pools; private memory stays demand-bound.
            if (!bytes && (!retain_unused || current == SIZE_MAX)) return detail::ThreadMemory{};
            if (current == SIZE_MAX || s.thread_memory.at(current).bytes_per_thread < bytes) {
                const auto payload = uint64_t(bytes) * detail::thread_memory_slots;
                require(payload <= 256 * 1024 * 1024 - 2 * detail::thread_memory_guard, "per-thread memory exceeds the scratch/private pool limit");
                const auto memory = allocate_storage(size_t(payload) + 2 * detail::thread_memory_guard,
                                                     s.local_thread_memory);
                if (memory.device_local) {
                    transfer_storage(memory.gpu, 0, memory.bytes, detail::thread_memory_sentinel);
                    transfer_storage(memory.gpu + detail::thread_memory_guard, 0, size_t(payload));
                } else {
                    storage_access(memory);
                    std::memset(memory.data, detail::thread_memory_sentinel, memory.bytes);
                    std::memset(static_cast<uint8_t*>(memory.data) + detail::thread_memory_guard, 0, size_t(payload));
                }
                s.thread_memory.push_back({memory, size_t(payload), bytes});
                current = s.thread_memory.size() - 1;
            }
            return s.thread_memory.at(current);
        };
        auto& queue = s.queue(s.recording.queue);
        const auto scratch0 = thread_pool(k.scratch_bytes[0], queue.current_scratch[0], true);
        const auto scratch1 = thread_pool(k.scratch_bytes[1], queue.current_scratch[1], true);
        const auto private_memory = thread_pool(k.private_bytes, queue.current_private);
        uint64_t scratch_heap = 0;
        if (scratch0.allocation.handle || scratch1.allocation.handle) {
            const std::array<size_t, 2> key{scratch0.allocation.handle ? queue.current_scratch[0] : SIZE_MAX,
                                          scratch1.allocation.handle ? queue.current_scratch[1] : SIZE_MAX};
            auto descriptor = s.scratch_descriptors.find(key);
            if (descriptor == s.scratch_descriptors.end()) {
                const auto memory = allocate_storage(4096, s.local_thread_memory);
                std::array<uint8_t, 256> contents{};
                for (size_t slot = 0; slot < 2; ++slot) {
                    const auto& pool = slot ? scratch1 : scratch0;
                    if (!pool.allocation.handle) continue;
                    const auto surface = detail::scratch_surface(pool.allocation.gpu + detail::thread_memory_guard,
                                         pool.bytes_per_thread, detail::thread_memory_slots, s.mocs, s.local_scratch_cache);
                    std::memcpy(contents.data() + 128 + slot * sizeof(surface), &surface, sizeof(surface));
                }
                write_storage(memory, 0, contents.data(), contents.size(), 4096);
                descriptor = s.scratch_descriptors.emplace(key, memory).first;
            }
            scratch_heap = descriptor->second.gpu;
            use_storage(descriptor->second);
        }
        const auto& data = s.module_data.at(path);
        const auto private_va = private_memory.allocation.handle ? private_memory.allocation.gpu + detail::thread_memory_guard : 0;
        detail::bind_base_arguments(k, cross, data.globals.gpu, data.constants.gpu, private_va);
        auto payload = detail::indirect_payload(entry, cross, local);
        auto indirect = entry.indirect.lock();
        bool captured_payload = false;
        if (!payload.empty() && (entry.last_indirect != payload || !indirect ||
                ((indirect->gpu ^ s.indirect_heap.allocation->gpu) >> 32))) {
            // Keep identical cached arguments near this recording's current heap.
            // Copy once during capture when an older list lives in another SBA window.
            // The weak cache cannot keep a discarded list's captures alive or alias a recycled lease.
            std::tie(indirect, entry.indirect_offset) = capture(s.indirect_heap, s.parameter_pool, payload.size());
            entry.indirect = indirect;
            captured_payload = true;
            std::memcpy(static_cast<uint8_t*>(indirect->data) + entry.indirect_offset,
                        payload.data(), payload.size());
        }
        entry.last_indirect = std::move(payload);
        entry.last_local = local;
        if (entry.last_indirect.empty()) indirect = s.indirect_heap.allocation;
        retain_capture(s.recording.captures, indirect);
        // ponytail: borrow owned allocations here; use_storage never changes their containers.
        for (const auto* memory : std::array<const Allocation*, 7>{&entry.isa, indirect.get(), &data.globals, &data.constants,
                                  &scratch0.allocation, &scratch1.allocation, &private_memory.allocation}) use_storage(*memory);
        auto parameter_gpu = indirect->gpu;
        if (s.local_parameters && s.storage_queue != SIZE_MAX && path != s.memory_module) {
            auto [mirror, created] = s.parameter_mirrors.try_emplace(indirect->handle);
            if (created) mirror->second = {*indirect, allocate_storage(indirect->bytes, true)};
            parameter_gpu = mirror->second[1].gpu;
            use_storage(mirror->second[1]);
            if (created || captured_payload) {
                // A new mirror includes earlier cached payloads in this host slab.
                // Reused mirrors copy only newly written ranges; original source-use
                // ledgers prevent overwriting any prefix still read by another list.
                const size_t begin = created ? 0 : entry.indirect_offset;
                const size_t end = created ? indirect->bytes : begin + entry.last_indirect.size();
                auto [range, fresh] = s.recording.parameter_uploads.try_emplace(indirect->handle,
                    std::array<size_t, 2>{begin, end});
                if (!fresh) {
                    range->second[0] = std::min(range->second[0], begin);
                    range->second[1] = std::max(range->second[1], end);
                }
            }
        }
        detail::record_dispatch(s.recording.commands, k, cross, local, groups, entry.isa.gpu, entry.isa.bytes,
                                parameter_gpu, s.max_threads, &s.recording.encoder_state, s.mocs, s.host_visibility,
                                detail::local_memory_bytes(k, args) - k.slm, scratch_heap, entry.isa_offset, entry.indirect_offset, s.disable_overdispatch);
    } catch (...) { s.failed = true; throw; }
}
void Runtime::check_scratch_guards() {
    auto& s = *state_;
    require(s.idle() && !s.failed, "scratch guard check requires idle healthy queues");
    for (const auto& memory : s.thread_memory) {
        const auto guard_end = detail::thread_memory_guard + memory.payload_bytes;
        if (memory.allocation.device_local) {
            const auto tail = memory.allocation.bytes - guard_end;
            require(tail <= s.thread_readback.bytes - detail::thread_memory_guard, "scratch readback exceeds staging");
            transfer_storage(s.thread_readback.gpu, memory.allocation.gpu, detail::thread_memory_guard);
            transfer_storage(s.thread_readback.gpu + detail::thread_memory_guard, memory.allocation.gpu + guard_end, tail);
            storage_access(s.thread_readback);
            const auto* p = static_cast<const uint8_t*>(s.thread_readback.data);
            require(std::all_of(p, p + detail::thread_memory_guard + tail,
                    [](uint8_t v) { return v == detail::thread_memory_sentinel; }), "GPU scratch/private memory guard corrupted");
        } else {
            cpu_access(memory.allocation, false);
            const auto* p = static_cast<const uint8_t*>(memory.allocation.data);
            require(std::all_of(p, p + detail::thread_memory_guard, [](uint8_t v) { return v == detail::thread_memory_sentinel; }) &&
                    std::all_of(p + guard_end, p + memory.allocation.bytes, [](uint8_t v) { return v == detail::thread_memory_sentinel; }),
                    "GPU scratch/private memory guard corrupted");
        }
    }
}
Commands Runtime::finish() {
    auto& s = *state_;
    require(!s.failed && s.recording.sequence <= s.queue(s.recording.queue).done, "finish requires a completed healthy recording");
    if (!s.recording.closed) close();
    Commands result;
    result.owner_ = s.identity;
    if (s.free_recordings.empty()) {
        result.index_ = s.recordings.size(); s.recordings.emplace_back();
    } else {
        result.index_ = s.free_recordings.back(); s.free_recordings.pop_back();
    }
    auto& slot = s.recordings[result.index_];
    result.generation_ = slot.generation;
    slot = std::move(s.recording); slot.generation = result.generation_;
    s.recording = {};
    return result;
}
void Runtime::discard(Commands& commands) {
    auto& s = *state_;
    const auto index = command_index(commands);
    auto& list = s.list(index);
    auto& queue = s.queue(list.queue);
    if (!s.failed && list.sequence > queue.done && queue.observer->observed.load(std::memory_order_acquire) >= list.sequence) queue.done = list.sequence;
    require(!s.failed && list.sequence <= queue.done && list.generation != UINT64_MAX,
            "discard requires a completed healthy list and available generation");
    s.free_recordings.push_back(index); // Allocate before changing ownership, so failure leaves the list usable.
    const auto generation = list.generation + 1;
    list = {}; list.generation = generation;
    if (s.indirect_heap.allocation.use_count() == 1) s.indirect_heap = {};
    commands = {};
}
size_t Runtime::command_index(const Commands& commands) const {
    const auto& s = *state_;
    require(commands.owner_.get() == s.identity.get() && commands.index_ < s.recordings.size(),
            "command list belongs to another or destroyed runtime");
    require(s.recordings[commands.index_].closed && commands.generation_ == s.recordings[commands.index_].generation,
            "command list was discarded or its slot was reused");
    return commands.index_;
}
Queue Runtime::default_queue() const {
    require(!state_->failed, "queue lookup after runtime failure");
    Queue queue; queue.owner_ = state_->identity; queue.index_ = 0; return queue;
}
Queue Runtime::create_queue() {
    auto& s = *state_;
    require(!s.recording.closed && s.idle() && !s.failed, "queue creation requires idle healthy queues");
    s.queues.emplace_back();
    try { s.create_queue(s.queues.back()); }
    catch (...) { s.failed = true; throw; }
    try { for (auto& storage : s.storage) storage.second.last_use.push_back(0); }
    catch (...) { s.failed = true; throw; }
    Queue queue; queue.owner_ = s.identity; queue.index_ = s.queues.size(); return queue;
}
size_t Runtime::queue_index(const Queue& queue) const {
    require(queue.owner_.get() == state_->identity.get() && queue.index_ <= state_->queues.size(),
            "queue belongs to another or destroyed runtime");
    return queue.index_;
}
size_t Runtime::completion_index(const Completion& completion) const {
    require(completion.owner_.get() == state_->identity.get() && completion.queue_ <= state_->queues.size() &&
            completion.sequence_ && completion.sequence_ <= state_->queue(completion.queue_).sequence,
            "invalid, foreign or destroyed-runtime completion");
    return completion.queue_;
}
void Runtime::begin(const Queue& queue, bool in_order) {
    const auto index = queue_index(queue);
    auto& s = *state_;
    require(!s.recording.closed && s.recording.commands.empty() && s.recording.gates.empty() && !s.failed,
            "queue selection requires an empty open healthy recording");
    s.recording.queue = index;
    s.recording.encoder_state.in_order = in_order;
}
uint64_t Runtime::submit() { return submit_recording(SIZE_MAX, 0); }
uint64_t Runtime::submit(const Commands& commands) { return submit_recording(command_index(commands), 0); }
Completion Runtime::submit(const Commands& commands, const Queue& queue, std::initializer_list<Completion> waits) {
    const auto index = queue_index(queue);
    const auto list = command_index(commands);
    Completion completion; completion.owner_ = state_->identity; completion.queue_ = index;
    completion.observer_ = state_->queue(index).observer;
    completion.sequence_ = submit_recording(list, index, waits.begin(), waits.size());
    return completion;
}
Completion Runtime::submit(const Commands& commands, const Queue& queue, const std::vector<Completion>& waits) {
    const auto index = queue_index(queue), list = command_index(commands);
    Completion completion; completion.owner_ = state_->identity; completion.queue_ = index;
    completion.observer_ = state_->queue(index).observer;
    completion.sequence_ = submit_recording(list, index, waits.data(), waits.size());
    return completion;
}
uint64_t Runtime::completed() { return completed_queue(0); }
uint64_t Runtime::completed(const Queue& queue) { return completed_queue(queue_index(queue)); }
void Runtime::completed(const Queue* queues, uint64_t* sequences, size_t count) {
    require(!state_->failed && count <= 16 && (!count || (queues && sequences)), "invalid completion batch or failed runtime");
    std::array<size_t, 16> indices{}, mapping{};
    std::array<uint64_t, 16> values{};
    size_t unique = 0;
    for (size_t i = 0; i < count; ++i) {
        const auto index = queue_index(queues[i]);
        const auto found = std::find(indices.begin(), indices.begin() + unique, index);
        mapping[i] = size_t(found - indices.begin());
        if (found == indices.begin() + unique) indices[unique++] = index;
    }
    // Validate every handle before querying or modifying the caller's output.
    completed_queues(indices.data(), values.data(), unique);
    for (size_t i = 0; i < count; ++i) sequences[i] = values[mapping[i]];
}
bool Runtime::ready(const Completion& completion) {
    const auto index = completion_index(completion);
    require(!state_->failed, "completion query after runtime failure");
    auto& done = state_->queue(index).done;
    if (done >= completion.sequence_) return true;
    if (!completion.ready()) return false;
    done = std::max(done, completion.sequence_);
    return true;
}
bool Runtime::ready() {
    const auto& list = state_->recording;
    require(list.closed && !state_->failed, "query requires a closed healthy list");
    return list.sequence <= state_->queue(list.queue).done || completed_queue(list.queue) >= list.sequence;
}
bool Runtime::ready(const Commands& commands) {
    const auto& list = state_->list(command_index(commands));
    require(!state_->failed, "query after runtime failure");
    return list.sequence <= state_->queue(list.queue).done || completed_queue(list.queue) >= list.sequence;
}
void Runtime::wait(uint64_t timeout_ns) {
    require(state_->recording.closed, "wait requires a closed list");
    wait_sequence(state_->recording.queue, state_->recording.sequence, timeout_ns);
}
void Runtime::wait(const Commands& commands, uint64_t timeout_ns) {
    const auto& list = state_->list(command_index(commands));
    wait_sequence(list.queue, list.sequence, timeout_ns);
}
void Runtime::wait(const Completion& completion, uint64_t timeout_ns) {
    wait_sequence(completion_index(completion), completion.sequence_, timeout_ns);
}
