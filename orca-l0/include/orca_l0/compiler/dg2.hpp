#pragma once
// SYCL supplies compile-time entry points/capture descriptors only; execution is Orca::DG2.
#include <orca_l0/dg2.hpp>
#include <orca_l0/compiler/local_accessor.hpp>
#include <sycl/detail/kernel_launch_helper.hpp>
#include <filesystem>
#include <fstream>
#include <map>
#include <cstring>

namespace orca::l0::dg2 {
class Program {
    struct Entry { std::string file; bool native; };
public:
    explicit Program(const std::filesystem::path& directory) { add(directory); }
    void add(const std::filesystem::path& directory) {
        std::ifstream input(directory / "index.txt");
        if (!input) throw std::runtime_error("cannot read DG2 kernel index");
        std::map<std::string, Entry, std::less<>> additions;
        for (std::string line; std::getline(input, line);) {
            if (!line.empty() && line.back() == '\r') line.pop_back();
            const auto tab = line.find('\t'), flags = line.find('\t', tab == line.npos ? tab : tab + 1);
            if (!tab || tab == line.npos || line.size() > 16384 || line.find('\0') != line.npos)
                throw std::runtime_error("invalid DG2 kernel index row");
            const std::filesystem::path file(line.substr(0, tab));
            const auto name = line.substr(tab + 1, flags == line.npos ? flags : flags - tab - 1);
            const bool native = file.extension() == ".zebin";
            if (file != file.filename() || (!native && file.extension() != ".spv") ||
                (native && flags != line.npos && flags + 1 != line.size()) ||
                name.empty() || name.find_first_of(" \t\r\n") != name.npos ||
                kernels_.count(name) || !additions.emplace(name, Entry{(directory / file).string(), native}).second)
                throw std::runtime_error("invalid, duplicate or unbaked DG2 kernel index entry");
        }
        if (!input.eof() || additions.empty()) throw std::runtime_error("empty or incomplete DG2 kernel index");
        kernels_.merge(additions);
    }
    const std::string& kernel_file(const char* name) const {
        if (!name || !*name) throw std::invalid_argument("empty DG2 compiler kernel name");
        const auto found = kernels_.find(name);
        if (found == kernels_.end()) throw std::runtime_error(std::string("DG2 compiler kernel not found: ") + name);
        if (!found->second.native)
            throw std::runtime_error("DG2 kernel requires an offline native bake; SPIR-V fallback is unavailable");
        return found->second.file;
    }
private:
    std::map<std::string, Entry, std::less<>> kernels_;
};
} // namespace orca::l0::dg2

namespace orca::l0::compiler {
template<class Name, int Dimensions, class F>
void append(dg2::Program& program, dg2::Runtime& runtime, sycl::nd_range<Dimensions> range, const F& function) {
#ifndef __SYCL_DEVICE_ONLY__
    namespace d = sycl::detail;
    if (d::getKernelSize<Name>() != sizeof(F)) throw std::runtime_error("DG2 compiler capture size mismatch");
    const char* name = d::getKernelName<Name>();
    const auto& file = program.kernel_file(name);
    const auto* closure = reinterpret_cast<const std::byte*>(&function);
    std::vector<dg2::Argument> arguments;
    arguments.reserve(d::getKernelNumParams<Name>());
    for (unsigned i = 0; i < d::getKernelNumParams<Name>(); ++i) {
        const auto descriptor = d::getKernelParamDesc<Name>(i);
        if (descriptor.offset < 0 || size_t(descriptor.offset) >= sizeof(F)) throw std::runtime_error("invalid DG2 capture offset");
        size_t bytes = 0;
        if (descriptor.kind == d::kernel_param_kind_t::kind_pointer) bytes = sizeof(void*);
        else if (descriptor.kind == d::kernel_param_kind_t::kind_std_layout && descriptor.info > 0) bytes = size_t(descriptor.info);
        else if (descriptor.kind == d::kernel_param_kind_t::kind_accessor) {
            if ((descriptor.info & 0x7ff) != int(sycl::access::target::local) || sizeof(bytes) > sizeof(F) - size_t(descriptor.offset))
                throw std::runtime_error("unsupported DG2 accessor capture");
            std::memcpy(&bytes, closure + descriptor.offset, sizeof(bytes));
            if (!bytes) throw std::runtime_error("empty DG2 local-memory capture");
            arguments.push_back(dg2::Argument::local(bytes));
            continue;
        } else throw std::runtime_error("unsupported DG2 compiler capture kind");
        if (bytes > sizeof(F) - size_t(descriptor.offset)) throw std::runtime_error("DG2 capture exceeds closure");
        arguments.push_back(dg2::Argument::capture(closure + descriptor.offset, bytes));
    }
    std::array<uint32_t, 3> local{1,1,1}, groups{1,1,1};
    for (int i = 0; i < Dimensions; ++i) {
        const auto total = range.get_global_range()[Dimensions - 1 - i], block = range.get_local_range()[Dimensions - 1 - i];
        if (!total || !block || total % block || block > UINT32_MAX || total / block > UINT32_MAX)
            throw std::invalid_argument("invalid DG2 compiler launch range");
        local[i] = uint32_t(block); groups[i] = uint32_t(total / block);
    }
    runtime.append(file, name, local, groups, arguments);
#endif
}

template<class Name = void, int Dimensions, class F, class... Properties>
void parallel_for(dg2::Program& program, dg2::Runtime& runtime, sycl::nd_range<Dimensions> range,
                  sycl::ext::oneapi::experimental::detail::properties_t<Properties...>, F function) {
    using KernelName = std::conditional_t<std::is_void_v<Name>, F, Name>;
    sycl::detail::KernelWrapperHelperFuncs::kernel_parallel_for<KernelName, sycl::nd_item<Dimensions>, F, Properties...>(function);
    append<KernelName>(program, runtime, range, function);
}
template<class Name = void, int Dimensions, class F>
void parallel_for(dg2::Program& program, dg2::Runtime& runtime, sycl::nd_range<Dimensions> range, F function) {
    parallel_for<Name>(program, runtime, range, sycl::ext::oneapi::experimental::properties{}, function);
}
template<class Name = void, class F, class... Properties>
void parallel_for(dg2::Program& program, dg2::Runtime& runtime, sycl::range<1> range,
                  sycl::ext::oneapi::experimental::detail::properties_t<Properties...>, F function) {
    using KernelName = std::conditional_t<std::is_void_v<Name>, F, Name>;
    sycl::detail::KernelWrapperHelperFuncs::kernel_parallel_for<KernelName, sycl::id<1>, F, Properties...>(function);
#ifndef __SYCL_DEVICE_ONLY__
    if (!range[0] || range[0] > UINT32_MAX) throw std::invalid_argument("invalid DG2 compiler range");
    const auto& file = program.kernel_file(sycl::detail::getKernelName<KernelName>());
    const auto local = runtime.suggest_group_size(file, sycl::detail::getKernelName<KernelName>(), uint32_t(range[0]));
    append<KernelName>(program, runtime, sycl::nd_range<1>(range, sycl::range<1>(local)), function);
#endif
}
template<class Name = void, class F>
void parallel_for(dg2::Program& program, dg2::Runtime& runtime, sycl::range<1> range, F function) {
    parallel_for<Name>(program, runtime, range, sycl::ext::oneapi::experimental::properties{}, function);
}
} // namespace orca::l0::compiler
