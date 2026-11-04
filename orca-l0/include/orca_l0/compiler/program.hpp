#pragma once
#include <orca_l0/runtime.hpp>
#include <fstream>
#include <map>
#include <sstream>
#include <set>

namespace orca::l0::compiler {

// Single submission owner, like Runtime. Modules and kernels are loaded lazily;
// their handles stay owned by Runtime until all queues have drained.
class Program {
public:
    Program(Runtime& runtime, const std::filesystem::path& directory) : runtime_(runtime) { add(directory); }

    // One pack per translation unit; all packs share this program and native stream.
    void add(const std::filesystem::path& directory) {
        std::ifstream index(directory / "index.txt");
        if (!index) throw std::runtime_error("cannot read native kernel index");
        std::string line;
        bool native_identity_checked = false;
        while (std::getline(index, line)) {
            if (!line.empty() && line.back() == '\r') line.pop_back();
            const auto tab = line.find('\t');
            if (!tab || tab == std::string::npos || tab == line.size() - 1)
                throw std::runtime_error("invalid native kernel index row");
            const std::filesystem::path file(line.substr(0, tab));
            const bool native = file.extension() == ".zebin";
            if (file.has_parent_path() || file.is_absolute() || (!native && file.extension() != ".spv"))
                throw std::runtime_error("kernel image must be a local SPIR-V or zebin filename");
            const auto flags_tab = line.find('\t', tab + 1);
            const auto name = line.substr(tab + 1, flags_tab == std::string::npos ? flags_tab : flags_tab - tab - 1);
            const auto flags = flags_tab == std::string::npos ? std::string{} : line.substr(flags_tab + 1);
            if (native) {
                if (!flags.empty()) throw std::runtime_error("native pack flags must already be baked");
                if (!native_identity_checked) {
                    std::ifstream identity_file(directory / "native-device.txt");
                    std::string identity;
                    if (!std::getline(identity_file, identity)) throw std::runtime_error("native pack requires device identity");
                    if (!identity.empty() && identity.back() == '\r') identity.pop_back();
                    if (identity != runtime_.binary_identity())
                        throw std::runtime_error("native pack device/driver mismatch; rebuild from SPIR-V");
                    native_identity_checked = true;
                }
            }
            if (name.empty() || name.find_first_of("\t\r\n ") != std::string::npos ||
                !valid_flags(flags) ||
                !kernels_.emplace(name, Kernel{directory / file, flags}).second)
                throw std::runtime_error("invalid or duplicate native kernel name");
        }
        if (!index.eof() || kernels_.empty()) throw std::runtime_error("incomplete native kernel index");
    }

    ze_kernel_handle_t kernel(const char* name, uint32_t argument_count) {
        if (!name || !*name) throw std::invalid_argument("empty compiler kernel name");
        const auto it = kernels_.find(name);
        if (it == kernels_.end()) throw std::runtime_error(std::string("native kernel not found: ") + name);
        auto& entry = it->second;
        if (!entry.handle) {
            auto& module = modules_[{entry.file, entry.flags}];
            if (!module) module = runtime_.module(entry.file, entry.file.extension() == ".zebin"
                ? ZE_MODULE_FORMAT_NATIVE : ZE_MODULE_FORMAT_IL_SPIRV, entry.flags.c_str());
            auto kernel = runtime_.kernel(module, name);
            check(zeKernelSetIndirectAccess(kernel, ZE_KERNEL_INDIRECT_ACCESS_FLAG_DEVICE |
                  ZE_KERNEL_INDIRECT_ACCESS_FLAG_HOST | ZE_KERNEL_INDIRECT_ACCESS_FLAG_SHARED),
                  "zeKernelSetIndirectAccess(compiler captures)");
            ze_kernel_properties_t properties{};
            properties.stype = ZE_STRUCTURE_TYPE_KERNEL_PROPERTIES;
            check(zeKernelGetProperties(kernel, &properties), "zeKernelGetProperties(compiler ABI)");
            if (properties.numKernelArgs != argument_count)
                throw std::runtime_error(std::string("native kernel ABI argument count mismatch: ") + name);
            entry.handle = kernel;
            entry.argument_count = argument_count;
        }
        if (entry.argument_count != argument_count) throw std::runtime_error("conflicting compiler kernel ABI");
        return entry.handle;
    }
private:
    static bool valid_flags(const std::string& flags) {
        std::istringstream input(flags);
        std::set<std::string> seen;
        std::string flag;
        while (input >> flag)
            if ((flag != "-vc-codegen" && flag != "-cl-fp32-correctly-rounded-divide-sqrt" &&
                 flag != "-doubleGRF" && flag != "-ze-opt-large-register-file" &&
                 flag != "-ze-opt-register-file-size=512" &&
                 flag != "-ze-intel-enable-auto-large-GRF-mode") ||
                !seen.insert(flag).second) return false;
        const auto modes = seen.count("-doubleGRF") + seen.count("-ze-opt-large-register-file") +
            seen.count("-ze-opt-register-file-size=512") + seen.count("-ze-intel-enable-auto-large-GRF-mode");
        return modes <= 1 && (!seen.count("-doubleGRF") || seen.count("-vc-codegen"));
    }
    struct Kernel {
        std::filesystem::path file;
        std::string flags;
        ze_kernel_handle_t handle = nullptr;
        uint32_t argument_count = 0;
    };
    Runtime& runtime_;
    std::map<std::string, Kernel, std::less<>> kernels_;
    std::map<std::pair<std::filesystem::path, std::string>, ze_module_handle_t> modules_;
};

} // namespace orca::l0::compiler
