// Orca-owned C ABI forwarding to raw vendor handles. This does not use ze_loader.
#define ZE_APIEXPORT
#define ZE_DLLEXPORT
#include <level_zero/ze_ddi.h>
#include <level_zero/zes_ddi.h>
#include <orca_l0/direct_driver.hpp>
#include <cstdlib>
#include <cstring>
#include <filesystem>
#include <mutex>
#include <stdexcept>
#include <string>
#ifdef _WIN32
#define WIN32_LEAN_AND_MEAN
#define NOMINMAX
#include <windows.h>
#else
#include <dlfcn.h>
#endif

namespace {
std::once_flag once;
std::string path, error;
bool loaded = false;
#ifdef _WIN32
HMODULE library = nullptr;
void* symbol(const char* name) { return reinterpret_cast<void*>(GetProcAddress(library, name)); }
#else
void* library = nullptr;
void* symbol(const char* name) { return dlsym(library, name); }
#endif

template<class Table> void table(const char* name, Table& target) {
    using Getter = ze_result_t (ZE_APICALL*)(ze_api_version_t, Table*);
    const auto get = reinterpret_cast<Getter>(symbol(name));
    if (!get) throw std::runtime_error(std::string("missing vendor DDI export: ") + name);
    for (uint32_t minor = ZE_MINOR_VERSION(ZE_API_VERSION_CURRENT) + 1; minor-- > 0;) {
        target = {};
        const auto result = get(static_cast<ze_api_version_t>(ZE_MAKE_VERSION(1, minor)), &target);
        if (result == ZE_RESULT_SUCCESS) return;
        if (result != ZE_RESULT_ERROR_UNSUPPORTED_VERSION)
            throw std::runtime_error(std::string("vendor DDI table rejected: ") + name);
    }
    throw std::runtime_error(std::string("no compatible vendor DDI version: ") + name);
}

// The included bridges also declare the exact DDI tables used by this runtime.
#include "direct_tables.inc"

void initialize() {
    std::call_once(once, [] {
        try {
            const char* configured = std::getenv("ORCA_L0_DRIVER");
#ifdef _WIN32
            if (!configured || !*configured || !std::filesystem::path(configured).is_absolute())
                throw std::runtime_error("set ORCA_L0_DRIVER to the installed ze_intel_gpu64.dll absolute path");
            path = configured;
            library = LoadLibraryExW(std::filesystem::path(path).c_str(), nullptr,
                LOAD_LIBRARY_SEARCH_DLL_LOAD_DIR | LOAD_LIBRARY_SEARCH_DEFAULT_DIRS);
            if (!library) throw std::runtime_error("cannot load vendor driver, Windows error " + std::to_string(GetLastError()));
#else
            path = configured && *configured ? configured : "libze_intel_gpu.so.1";
            library = dlopen(path.c_str(), RTLD_NOW | RTLD_LOCAL);
            if (!library) throw std::runtime_error(dlerror());
#endif
            // ponytail: retain the vendor library until process exit; native handles can outlive a Runtime.
            load_tables();
            loaded = true;
        } catch (const std::exception& e) {
            error = e.what();
        }
    });
}
ze_result_t unavailable() { return loaded ? ZE_RESULT_ERROR_UNSUPPORTED_FEATURE : ZE_RESULT_ERROR_UNINITIALIZED; }
} // namespace

namespace orca::l0 {
const char* direct_driver_path() noexcept { return path.c_str(); }
const char* direct_driver_error() noexcept { return error.c_str(); }
}

// Fixed C ABI tail calls. No per-dispatch locks, allocation, TLS or handle maps.
#include "direct_api.inc"
