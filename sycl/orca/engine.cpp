#include "strata/orca_engine.hpp"
#include <level_zero/zes_api.h>
#include <fstream>
#include <cstring>
#include <cstdlib>
#ifdef _WIN32
#include <windows.h>
#include <dxgi1_4.h>
#endif

namespace strata::l0 {
namespace {
std::filesystem::path image_directory;
struct Engine {
    using Import = ze_result_t(ZE_APICALL*)(ze_driver_handle_t, void*, size_t);
    using Release = ze_result_t(ZE_APICALL*)(ze_driver_handle_t, void*);
    orca::l0::Runtime runtime;
    orca::l0::compiler::Program program;
    Queue queue;
    Import import = nullptr;
    Release release = nullptr;
    ze_result_t import_status = ZE_RESULT_ERROR_UNSUPPORTED_FEATURE;
    explicit Engine(const std::filesystem::path& images)
        : program(runtime, images / "elementwise"), queue(runtime, program) {
        std::ifstream manifest(images / "packs.txt");
        if (!manifest) throw std::runtime_error("missing Orca image manifest: " + images.string());
        std::string name;
        while (std::getline(manifest, name)) {
            if (name == "elementwise") continue;
            if (name.empty() || std::filesystem::path(name).has_parent_path())
                throw std::runtime_error("invalid Orca image pack name");
            program.add(images / name);
        }
        if (!manifest.eof()) throw std::runtime_error("cannot read Orca image manifest");
        void* function = nullptr;
        import_status = zeDriverGetExtensionFunctionAddress(runtime.native_driver(), "zexDriverReleaseImportedPointer", &function);
        if (import_status == ZE_RESULT_SUCCESS && function) {
            release = reinterpret_cast<Release>(function);
            function = nullptr;
            import_status = zeDriverGetExtensionFunctionAddress(runtime.native_driver(), "zexDriverImportExternalPointer", &function);
            if (import_status == ZE_RESULT_SUCCESS) import = reinterpret_cast<Import>(function);
        }
        if (import_status == ZE_RESULT_SUCCESS && !import) import_status = ZE_RESULT_ERROR_UNSUPPORTED_FEATURE;
    }
};
Engine& engine() {
    static Engine engine(image_directory);
    bind_default(engine.queue);
    return engine;
}
Queue& engine_queue() { return engine().queue; }

bool sysman_memory(orca::l0::Runtime& runtime, size_t& free_bytes, size_t& total_bytes) {
    static zes_device_handle_t device = [&] {
        if (zesInit(0) != ZE_RESULT_SUCCESS) return zes_device_handle_t{};
        uint32_t count = 0;
        if (zesDriverGet(&count, nullptr) != ZE_RESULT_SUCCESS) return zes_device_handle_t{};
        std::vector<zes_driver_handle_t> drivers(count);
        if (zesDriverGet(&count, drivers.data()) != ZE_RESULT_SUCCESS) return zes_device_handle_t{};
        for (auto driver : drivers) {
            uint32_t devices_count = 0;
            if (zesDeviceGet(driver, &devices_count, nullptr) != ZE_RESULT_SUCCESS) continue;
            std::vector<zes_device_handle_t> devices(devices_count);
            if (zesDeviceGet(driver, &devices_count, devices.data()) != ZE_RESULT_SUCCESS) continue;
            for (auto candidate : devices) {
                zes_device_properties_t props{};
                props.stype = ZES_STRUCTURE_TYPE_DEVICE_PROPERTIES;
                if (zesDeviceGetProperties(candidate, &props) == ZE_RESULT_SUCCESS &&
                    !std::memcmp(props.core.uuid.id, runtime.properties().uuid.id, ZE_MAX_DEVICE_UUID_SIZE))
                    return candidate;
            }
        }
        return zes_device_handle_t{};
    }();
    if (!device) return false;
    // ponytail: the installed engine owns one device for the process lifetime.
    // Cache module identity/location; free bytes must still be queried each time.
    static const auto memory = [device = device] {
        uint32_t count = 0;
        if (zesDeviceEnumMemoryModules(device, &count, nullptr) != ZE_RESULT_SUCCESS)
            return std::vector<zes_mem_handle_t>{};
        std::vector<zes_mem_handle_t> modules(count);
        if (count && zesDeviceEnumMemoryModules(device, &count, modules.data()) != ZE_RESULT_SUCCESS)
            return std::vector<zes_mem_handle_t>{};
        modules.resize(count);
        std::erase_if(modules, [](auto handle) {
            zes_mem_properties_t props{};
            props.stype = ZES_STRUCTURE_TYPE_MEM_PROPERTIES;
            return zesMemoryGetProperties(handle, &props) != ZE_RESULT_SUCCESS || props.location != ZES_MEM_LOC_DEVICE;
        });
        return modules;
    }();
    size_t available = 0, total = 0;
    for (auto handle : memory) {
        zes_mem_state_t state{};
        state.stype = ZES_STRUCTURE_TYPE_MEM_STATE;
        if (zesMemoryGetState(handle, &state) != ZE_RESULT_SUCCESS || state.free > state.size ||
            state.size > SIZE_MAX - total || state.free > SIZE_MAX - available) return false;
        total += state.size;
        available += state.free;
    }
    if (!total) return false;
    free_bytes = available;
    total_bytes = total;
    return true;
}
}

void install_engine(const std::filesystem::path& images) {
    if (default_factory) throw std::logic_error("Orca engine bootstrap already installed");
    const char* override_path = std::getenv("STRATA_ORCA_IMAGES");
    image_directory = !images.empty() ? images : override_path && *override_path
        ? std::filesystem::path(override_path) : std::filesystem::path(STRATA_ORCA_IMAGE_DIR);
    default_factory = &engine_queue;
}

Queue* create_copy_queue() {
    auto& owner = stream_of(nullptr);
    auto queue = std::make_unique<Queue>(owner.runtime(), owner.program(), true);
    dpct::get_current_device().queues.push_back(queue.get());
    return queue.release();
}

void memory_info(size_t& free_bytes, size_t& total_bytes) {
    auto& queue = stream_of(nullptr);
    size_t device_free = 0, device_total = 0;
    const bool have_sysman = sysman_memory(queue.runtime(), device_free, device_total);
#ifdef _WIN32
    // WDDM's process budget includes desktop pressure and allocations retained
    // by the driver. Match the selected L0 device by LUID, never adapter order.
    static const auto source = [&] {
        std::pair<std::shared_ptr<IDXGIAdapter3>, unsigned> cached;
        ze_device_luid_ext_properties_t luid{};
        luid.stype = ZE_STRUCTURE_TYPE_DEVICE_LUID_EXT_PROPERTIES;
        auto properties = queue.runtime().properties();
        properties.pNext = &luid;
        if (zeDeviceGetProperties(queue.runtime().native_device(), &properties) != ZE_RESULT_SUCCESS || !luid.nodeMask)
            return cached;
        using Factory = HRESULT(WINAPI*)(REFIID, void**);
        static HMODULE dxgi = LoadLibraryW(L"dxgi.dll");
        const auto create = dxgi ? reinterpret_cast<Factory>(GetProcAddress(dxgi, "CreateDXGIFactory1")) : nullptr;
        IDXGIFactory4* factory = nullptr;
        if (create && SUCCEEDED(create(__uuidof(IDXGIFactory4), reinterpret_cast<void**>(&factory)))) {
            LUID id{};
            static_assert(sizeof(id) == sizeof(luid.luid));
            std::memcpy(&id, &luid.luid, sizeof(id));
            IDXGIAdapter3* adapter = nullptr;
            const auto found = factory->EnumAdapterByLuid(id, __uuidof(IDXGIAdapter3), reinterpret_cast<void**>(&adapter));
            factory->Release();
            if (SUCCEEDED(found)) {
                cached.first = std::shared_ptr<IDXGIAdapter3>(adapter, [](auto* value) { value->Release(); });
                cached.second = std::countr_zero(luid.nodeMask);
            }
        }
        return cached;
    }();
    DXGI_QUERY_VIDEO_MEMORY_INFO budget{};
    if (source.first && SUCCEEDED(source.first->QueryVideoMemoryInfo(source.second, DXGI_MEMORY_SEGMENT_GROUP_LOCAL, &budget)) && budget.Budget) {
        total_bytes = queue.get_device().get_info<sycl::info::device::global_mem_size>();
        const auto ceiling = std::min<uint64_t>(budget.Budget, total_bytes);
        free_bytes = budget.CurrentUsage < ceiling ? ceiling - budget.CurrentUsage : 0;
        // Some L0 allocations are absent from DXGI's process usage.
        // The global device query also accounts for those allocations.
        if (have_sysman) free_bytes = std::min(free_bytes, device_free);
        return;
    }
#endif
    if (!have_sysman)
        throw std::runtime_error("Orca cannot query available device memory (DXGI/Sysman unavailable)");
    free_bytes = device_free;
    total_bytes = device_total;
}

int import_host(void* pointer, size_t bytes) {
    if (!pointer || !bytes) return int(ZE_RESULT_ERROR_INVALID_ARGUMENT);
    auto& owner = engine();
    if (!owner.import) return int(owner.import_status);
    return int(owner.import(owner.runtime.native_driver(), pointer, bytes));
}

void release_host(void* pointer) {
    if (!pointer) return;
    dpct::get_current_device().queues_wait_and_throw();
    auto& owner = engine();
    if (!owner.release) throw std::runtime_error("native host release extension unavailable");
    orca::l0::check(owner.release(owner.runtime.native_driver(), pointer), "native host release");
}
}
