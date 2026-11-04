// Experimental DG2 i915 transport: no Level Zero, SYCL, UR or libdrm linkage.
// MI_STORE_DWORD_IMM proves submission only; this is not a zebin compute executor.
#include <i915_drm.h>
#include <sys/ioctl.h>
#include <sys/mman.h>
#include <fcntl.h>
#include <unistd.h>
#include <algorithm>
#include <array>
#include <cerrno>
#include <chrono>
#include <cstdio>
#include <cstring>
#include <stdexcept>
#include <string>
#include <vector>

namespace {
constexpr uint64_t data_va = 0x1000000, batch_va = 0x2000000;
constexpr uint32_t store_dword = (0x20u << 23) | 2u, batch_end = 0xau << 23;
std::array<uint32_t, 6> packet(uint64_t address, uint32_t value) {
    if ((address & 3) || address >= (uint64_t{1} << 47))
        throw std::invalid_argument("store address must be aligned and in the low canonical GPU range");
    return {store_dword, uint32_t(address), uint32_t(address >> 32), value, batch_end, 0};
}
void require(bool value, const char* message) { if (!value) throw std::runtime_error(message); }
void call(int fd, unsigned long request, void* argument, const char* label) {
    int result;
    do { result = ioctl(fd, request, argument); } while (result < 0 && errno == EINTR);
    if (result < 0) throw std::runtime_error(std::string(label) + ": " + std::strerror(errno));
}
uint64_t pointer(const void* value) { return reinterpret_cast<uintptr_t>(value); }
struct File {
    int fd;
    explicit File(const char* path) : fd(open(path, O_RDWR | O_CLOEXEC)) {
        if (fd < 0) throw std::runtime_error(std::string("open render node: ") + std::strerror(errno));
    }
    ~File() { close(fd); }
    File(const File&) = delete;
};
struct Buffer {
    int fd;
    uint32_t handle = 0;
    void* map = MAP_FAILED;
    explicit Buffer(int descriptor) : fd(descriptor) {
        drm_i915_gem_create create{};
        create.size = 4096;
        call(fd, DRM_IOCTL_I915_GEM_CREATE, &create, "GEM_CREATE(system memory)");
        handle = create.handle;
        try {
            drm_i915_gem_mmap_offset offset{};
            offset.handle = handle;
            offset.flags = I915_MMAP_OFFSET_WB;
            call(fd, DRM_IOCTL_I915_GEM_MMAP_OFFSET, &offset, "GEM_MMAP_OFFSET(WB)");
            map = mmap(nullptr, 4096, PROT_READ | PROT_WRITE, MAP_SHARED, fd, offset.offset);
            if (map == MAP_FAILED) throw std::runtime_error(std::string("mmap: ") + std::strerror(errno));
        } catch (...) { release(); throw; }
    }
    void cpu(bool write) {
        drm_i915_gem_set_domain domain{};
        domain.handle = handle;
        domain.read_domains = I915_GEM_DOMAIN_CPU;
        domain.write_domain = write ? I915_GEM_DOMAIN_CPU : 0;
        call(fd, DRM_IOCTL_I915_GEM_SET_DOMAIN, &domain, "GEM_SET_DOMAIN(CPU)");
    }
    void release() noexcept {
        if (map != MAP_FAILED) munmap(map, 4096);
        if (handle) { drm_gem_close close{}; close.handle = handle; ioctl(fd, DRM_IOCTL_GEM_CLOSE, &close); }
    }
    ~Buffer() { release(); }
    Buffer(const Buffer&) = delete;
};
struct Context {
    int fd;
    uint32_t id = 0;
    Context(int descriptor, i915_engine_class_instance engine) : fd(descriptor) {
        drm_i915_gem_context_create create{};
        call(fd, DRM_IOCTL_I915_GEM_CONTEXT_CREATE, &create, "GEM_CONTEXT_CREATE");
        id = create.ctx_id;
        try {
            struct { uint64_t extensions; i915_engine_class_instance engines[1]; } map{};
            static_assert(sizeof(map) == 16);
            map.engines[0] = engine;
            drm_i915_gem_context_param parameter{};
            parameter.ctx_id = id;
            parameter.param = I915_CONTEXT_PARAM_ENGINES;
            // UAPI uses an 8-byte prefix plus packed 4-byte engine entries.
            parameter.size = 8 + sizeof(i915_engine_class_instance);
            parameter.value = pointer(&map);
            call(fd, DRM_IOCTL_I915_GEM_CONTEXT_SETPARAM, &parameter, "context compute engine map");
        } catch (...) { release(); throw; }
    }
    void release() noexcept {
        if (id) { drm_i915_gem_context_destroy destroy{}; destroy.ctx_id = id; ioctl(fd, DRM_IOCTL_I915_GEM_CONTEXT_DESTROY, &destroy); }
    }
    ~Context() { release(); }
    Context(const Context&) = delete;
};
int parameter(int fd, int name) {
    int value = 0;
    drm_i915_getparam get{};
    get.param = name;
    get.value = &value;
    call(fd, DRM_IOCTL_I915_GETPARAM, &get, "I915_GETPARAM");
    return value;
}
i915_engine_class_instance compute_engine(int fd) {
    drm_i915_query_item item{};
    item.query_id = DRM_I915_QUERY_ENGINE_INFO;
    drm_i915_query query{};
    query.num_items = 1;
    query.items_ptr = pointer(&item);
    call(fd, DRM_IOCTL_I915_QUERY, &query, "query compute engine size");
    require(item.length >= int(sizeof(drm_i915_query_engine_info)) && item.length <= 1024*1024, "invalid engine query size");
    std::vector<uint64_t> storage((item.length + 7) / 8);
    const auto capacity = storage.size() * sizeof(uint64_t);
    item.data_ptr = pointer(storage.data());
    call(fd, DRM_IOCTL_I915_QUERY, &query, "query compute engines");
    require(item.length >= int(sizeof(drm_i915_query_engine_info)) && size_t(item.length) <= capacity, "incomplete engine query");
    const auto* info = reinterpret_cast<const drm_i915_query_engine_info*>(storage.data());
    require(info->num_engines <= (item.length - sizeof(*info)) / sizeof(info->engines[0]), "invalid engine count");
    for (uint32_t i = 0; i < info->num_engines; ++i)
        if (info->engines[i].engine.engine_class == I915_ENGINE_CLASS_COMPUTE) return info->engines[i].engine;
    throw std::runtime_error("i915 exposes no compute engine");
}
void self_check() {
    const auto words = packet(0x123456780ull, 0x31415926);
    require(words == std::array<uint32_t, 6>{0x10000002, 0x23456780, 1, 0x31415926, 0x05000000, 0}, "incorrect DG2 MI encoding");
    for (auto address : {uint64_t{3}, uint64_t{1} << 47}) {
        bool rejected = false;
        try { (void)packet(address, 1); } catch (const std::invalid_argument&) { rejected = true; }
        require(rejected, "invalid GPU address accepted");
    }
    require(data_va + 4096 <= batch_va && batch_va < (uint64_t{1} << 47), "overlapping/noncanonical GPU virtual ranges");
    std::puts("PASS packet encoding, canonical addresses and rejection cases; CPU only, no GPU claim");
}
} // namespace

int main(int argc, char** argv) try {
    if (argc != 2) throw std::invalid_argument("usage: orca-l0-drm-submit-check <--self-check|/dev/dri/renderD128>");
    if (std::string(argv[1]) == "--self-check") { self_check(); return 0; }
    File file(argv[1]);
    char name[16]{};
    drm_version version{};
    version.name_len = sizeof(name);
    version.name = name;
    call(file.fd, DRM_IOCTL_VERSION, &version, "DRM driver name");
    require(version.name_len == 4 && std::memcmp(name, "i915", 4) == 0, "this experiment requires the i915 DRM driver");
    const auto device = parameter(file.fd, I915_PARAM_CHIPSET_ID);
    require(device >= 0x5690 && device <= 0x56bf, "this experiment supports DG2 only");
    require(parameter(file.fd, I915_PARAM_HAS_EXEC_SOFTPIN) != 0, "i915 softpin is required");
    const auto engine = compute_engine(file.fd);
    Context context(file.fd, engine);
    Buffer data(file.fd), batch(file.fd);
    std::array<drm_i915_gem_exec_object2, 2> objects{};
    objects[0].handle = data.handle;
    objects[0].offset = data_va;
    objects[0].flags = EXEC_OBJECT_PINNED | EXEC_OBJECT_SUPPORTS_48B_ADDRESS | EXEC_OBJECT_WRITE;
    objects[1].handle = batch.handle;
    objects[1].offset = batch_va;
    objects[1].flags = EXEC_OBJECT_PINNED | EXEC_OBJECT_SUPPORTS_48B_ADDRESS;
    drm_i915_gem_execbuffer2 execute{};
    execute.buffers_ptr = pointer(objects.data());
    execute.buffer_count = objects.size();
    execute.batch_len = sizeof(packet(0, 0));
    execute.flags = I915_EXEC_NO_RELOC;
    execute.rsvd1 = context.id;
    using Clock = std::chrono::steady_clock;
    std::vector<double> submit, complete;
    for (uint32_t pass = 0; pass < 72; ++pass) {
        // Mutation/domain transfer are preparation, outside submission timing.
        batch.cpu(true);
        const auto words = packet(data_va, 0x31410000u + pass);
        std::memcpy(batch.map, words.data(), sizeof(words));
        const auto start = Clock::now();
        call(file.fd, DRM_IOCTL_I915_GEM_EXECBUFFER2, &execute, "GEM_EXECBUFFER2(compute)");
        const auto submitted = Clock::now();
        drm_i915_gem_wait wait{};
        wait.bo_handle = batch.handle;
        wait.timeout_ns = 5000000000ll;
        call(file.fd, DRM_IOCTL_I915_GEM_WAIT, &wait, "GEM_WAIT");
        data.cpu(false);
        const auto done = Clock::now();
        require(*static_cast<volatile uint32_t*>(data.map) == words[3], "GPU store/replay output mismatch");
        if (pass >= 8) {
            submit.push_back(std::chrono::duration<double, std::micro>(submitted - start).count());
            complete.push_back(std::chrono::duration<double, std::micro>(done - start).count());
            std::printf("SAMPLE %u %.3f %.3f\n", pass - 8, submit.back(), complete.back());
        }
    }
    std::sort(submit.begin(), submit.end());
    std::sort(complete.begin(), complete.end());
    std::printf("PASS DG2 0x%x i915 compute instance %u, 72 exact GPU stores; median submit %.3f us, complete %.3f us\n",
        device, engine.engine_instance, (submit[31]+submit[32])/2, (complete[31]+complete[32])/2);
    return 0;
} catch (const std::exception& e) { std::fprintf(stderr, "%s\n", e.what()); return 1; }
