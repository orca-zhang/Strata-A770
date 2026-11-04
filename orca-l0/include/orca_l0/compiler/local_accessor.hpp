#pragma once
#include <sycl/sycl.hpp>
#include <stdexcept>

namespace orca::l0::compiler {
// Strata uses one-dimensional scratch accessors. The compiler lowers __init's
// local pointer to a dynamic shared-memory argument for either runtime.
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
        // it. Native local arguments still require a nonzero allocation.
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

} // namespace orca::l0::compiler
