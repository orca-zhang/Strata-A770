#include <dpct/dpct.hpp>
#include "u4n16.hpp"
void orca_u4_kernels(strata::l0::Queue& q, const uint16_t* x, const int32_t* ids,
    float* gu, float* y, int t, const grouped_u4::Batch& batch, u4s4::Packed& ax, u4s4::Packed& ah) {
    u4s4::pack<false,2>(q,x,t,ax);
    shared_u4::gu<2>(q,ax,ids,batch,gu);
    grouped_u4::pack_batch<true>(q,gu,batch,ah);
    grouped_u4::product<true,2>(q,ah,batch,y);
}
