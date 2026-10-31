#pragma once
// Compile-time SYCL support only. Host submissions go directly to Level Zero.
#include <sycl/sycl.hpp>
#include <sycl/detail/kernel_launch_helper.hpp>
#include <orca_l0/compiler/program.hpp>
#include <cstring>

namespace orca::l0::compiler {

// Strata uses one-dimensional scratch accessors. The compiler lowers __init's
// local pointer to a Level Zero dynamic shared-memory argument.
template<class T, int Dimensions = 1>
class __SYCL_SPECIAL_CLASS __SYCL_TYPE(local_accessor) local_accessor {
    static_assert(Dimensions == 1, "only linear scratch memory is supported");
#ifdef __SYCL_DEVICE_ONLY__
    using pointer = typename sycl::detail::DecoratedType<T,
        sycl::access::address_space::local_space>::type*;
    pointer data_ = nullptr;
    void __init(pointer value) { data_ = value; }
#else
    std::size_t bytes_ = 0;
#endif
public:
    template<class Handler>
    local_accessor(sycl::range<1> size, Handler&) {
#ifndef __SYCL_DEVICE_ONLY__
        if (size[0] > SIZE_MAX / sizeof(T))
            throw std::invalid_argument("invalid local memory size");
        // Empty scratch is a legal SYCL placeholder on paths that never access
        // it. Level Zero local arguments still require a nonzero allocation.
        bytes_ = size[0] ? size[0] * sizeof(T) : sizeof(T);
#endif
    }
    local_accessor() = default;
    T& operator[](std::size_t i) const {
#ifdef __SYCL_DEVICE_ONLY__
        return data_[i];
#else
        throw std::logic_error("local scratch cannot be accessed on the host");
#endif
    }
    template<sycl::access::decorated Decorated = sycl::access::decorated::no>
    auto get_multi_ptr() const {
        using Pointer = sycl::multi_ptr<T, sycl::access::address_space::local_space, Decorated>;
#ifdef __SYCL_DEVICE_ONLY__
        return Pointer(data_);
#else
        return Pointer(nullptr);
#endif
    }
};

// The compiler emits both the SPIR-V entry point and its capture descriptors.
// Keep the generated ABI; handwritten capture layouts would break on compiler upgrades.
template<class Name, class F>
void bind(ze_kernel_handle_t kernel, const F& function) {
    namespace d = sycl::detail;
    if (d::getKernelSize<Name>() != sizeof(F))
        throw std::runtime_error("compiler kernel ABI does not match host closure size");
    const auto* closure = reinterpret_cast<const std::byte*>(&function);
    for (unsigned i = 0; i < d::getKernelNumParams<Name>(); ++i) {
        const auto arg = d::getKernelParamDesc<Name>(i);
        if (arg.offset < 0 || static_cast<std::size_t>(arg.offset) >= sizeof(F))
            throw std::runtime_error("invalid compiler kernel capture offset");
        std::size_t size;
        switch (arg.kind) {
        case d::kernel_param_kind_t::kind_pointer: size = sizeof(void*); break;
        case d::kernel_param_kind_t::kind_std_layout:
            if (arg.info <= 0) throw std::runtime_error("invalid compiler kernel argument size");
            size = static_cast<std::size_t>(arg.info);
            break;
        case d::kernel_param_kind_t::kind_accessor:
            if ((arg.info & 0x7ff) != static_cast<int>(sycl::access::target::local) ||
                sizeof(std::size_t) > sizeof(F) - static_cast<std::size_t>(arg.offset))
                throw std::runtime_error("unsupported compiler accessor capture");
            std::memcpy(&size, closure + arg.offset, sizeof(size));
            if (!size) throw std::runtime_error("empty local memory capture");
            check(zeKernelSetArgumentValue(kernel, i, size, nullptr),
                  "zeKernelSetArgumentValue(local capture)");
            continue;
        default:
            throw std::runtime_error("unsupported compiler kernel capture kind");
        }
        if (size > sizeof(F) - static_cast<std::size_t>(arg.offset))
            throw std::runtime_error("compiler kernel capture exceeds closure");
        check(zeKernelSetArgumentValue(kernel, i, size, closure + arg.offset),
              "zeKernelSetArgumentValue(compiler capture)");
    }
}

template<class KernelName, int Dimensions, class F>
void append(Program& program, Commands& commands, sycl::nd_range<Dimensions> range, const F& function) {
#ifndef __SYCL_DEVICE_ONLY__
    const auto* name = sycl::detail::getKernelName<KernelName>();
    if (!name || !*name) throw std::runtime_error("missing compiler kernel ABI metadata");
    auto kernel = program.kernel(name, sycl::detail::getKernelNumParams<KernelName>());
    bind<KernelName>(kernel, function);
    ze_group_count_t groups{1, 1, 1}, local{1, 1, 1};
    uint32_t* g[] = {&groups.groupCountX, &groups.groupCountY, &groups.groupCountZ};
    uint32_t* l[] = {&local.groupCountX, &local.groupCountY, &local.groupCountZ};
    for (int i = 0; i < Dimensions; ++i) {
        const auto n = range.get_global_range()[Dimensions - 1 - i];
        const auto block = range.get_local_range()[Dimensions - 1 - i];
        if (!n || !block || n % block || block > UINT32_MAX || n / block > UINT32_MAX)
            throw std::invalid_argument("invalid native kernel launch range");
        *g[i] = static_cast<uint32_t>(n / block);
        *l[i] = static_cast<uint32_t>(block);
    }
    commands.launch(kernel, groups, local);
#endif
}

template<class Name = void, int Dimensions, class F, class... Properties>
void parallel_for(Program& program, Commands& commands, sycl::nd_range<Dimensions> range,
                  sycl::ext::oneapi::experimental::detail::properties_t<Properties...>, F function) {
    using KernelName = std::conditional_t<std::is_void_v<Name>, F, Name>;
    sycl::detail::KernelWrapperHelperFuncs::kernel_parallel_for<KernelName,
        sycl::nd_item<Dimensions>, F, Properties...>(function);
    append<KernelName>(program, commands, range, function);
}

template<class Name = void, int Dimensions, class F>
void parallel_for(Program& program, Commands& commands, sycl::nd_range<Dimensions> range, F function) {
    parallel_for<Name>(program, commands, range,
        sycl::ext::oneapi::experimental::properties{}, function);
}

template<class Name = void, class F, class... Properties>
void parallel_for(Program& program, Commands& commands, sycl::range<1> range,
                  sycl::ext::oneapi::experimental::detail::properties_t<Properties...>, F function) {
    using KernelName = std::conditional_t<std::is_void_v<Name>, F, Name>;
    // Keep the original callable: wrapping an ESIMD lambda in an ordinary
    // nd_item lambda would lose its explicit-SIMD annotation.
    sycl::detail::KernelWrapperHelperFuncs::kernel_parallel_for<KernelName,
        sycl::id<1>, F, Properties...>(function);
#ifndef __SYCL_DEVICE_ONLY__
    if (!range[0] || range[0] > UINT32_MAX) throw std::invalid_argument("invalid native range");
    auto kernel = program.kernel(sycl::detail::getKernelName<KernelName>(),
                                  sycl::detail::getKernelNumParams<KernelName>());
    uint32_t x = 1, y = 1, z = 1;
    check(zeKernelSuggestGroupSize(kernel, uint32_t(range[0]), 1, 1, &x, &y, &z),
          "zeKernelSuggestGroupSize(range)");
    if (!x || y != 1 || z != 1 || range[0] % x)
        throw std::runtime_error("invalid native range group suggestion");
    append<KernelName>(program, commands, sycl::nd_range<1>(range, sycl::range<1>(x)), function);
#endif
}

} // namespace orca::l0::compiler
