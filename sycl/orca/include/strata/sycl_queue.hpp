#pragma once
#include <dpct/dpct.hpp>
namespace strata {
inline l0::Queue* q_of(const void* stream) { return &l0::stream_of(stream); }
}
