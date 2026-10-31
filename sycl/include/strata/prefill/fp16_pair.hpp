#pragma once
#include <sycl/sycl.hpp>
#include <sycl/ext/oneapi/matrix/matrix.hpp>
#include "strata/prefill/kernels.hpp"
#include <algorithm>
#include <climits>

namespace fp16_pair {
struct Expert {
    const uint16_t *x, *gu_weight, *down_weight;
    float *gu;
    uint16_t *hidden;
    float *output;
    int rows;
};

#ifdef STRATA_ORCA_NATIVE
// The native implementation retains Orca's checked M4 tile and owns its kernel images.
bool execute(sycl::queue& q, Expert a, Expert b, void* workspace, size_t bytes, bool finish = true);
#else
inline void multiply(sycl::queue& q, Expert a, Expert b, uint16_t* padding, bool down) {
    namespace jm = sycl::ext::oneapi::experimental::matrix;
    constexpr int SG=8, NSG=8, TILE=8;
    const int n=down?2560:1280, k=down?640:2560;
    const int tiles_a=(a.rows+7)/8, tiles_b=(b.rows+7)/8;
    if (a.rows%8 || b.rows%8) {
        q.parallel_for(sycl::range<1>(2*8*size_t(k)),[=](sycl::id<1> id) {
            const size_t i=id[0];const int which=int(i/(8*k)),offset=int(i%(8*k));
            const Expert e=which?b:a;
            const int row=e.rows-e.rows%8+offset/k,col=offset%k;
            const auto* x=down?e.hidden:e.x;
            padding[size_t(which)*8*2560+offset]=row<e.rows?x[size_t(row)*k+col]:uint16_t(0);
        });
    }
    q.submit([&](sycl::handler& h) {
        sycl::local_accessor<float,1> tail(sycl::range<1>(NSG*TILE*TILE),h);
        h.parallel_for(sycl::nd_range<3>({size_t(tiles_a+tiles_b),size_t((n+63)/64),SG*NSG},{1,1,SG*NSG}),
            [=](sycl::nd_item<3> it) [[sycl::reqd_sub_group_size(SG)]] {
                const auto sg=it.get_sub_group();
                const int subgroup=int(sg.get_group_id()[0]),lane=int(sg.get_local_id()[0]);
                const int tile=int(it.get_group(0)),which=tile>=tiles_a;
                const Expert e=which?b:a;
                const int row=(tile-(which?tiles_a:0))*TILE,col=(int(it.get_group(1))*NSG+subgroup)*TILE;
                if (col>=n) return;
                const bool partial=row+TILE>e.rows;
                const auto* x=reinterpret_cast<const sycl::half*>(down?e.hidden:e.x);
                const auto* w=reinterpret_cast<const sycl::half*>(down?e.down_weight:e.gu_weight);
                const auto* source=partial?reinterpret_cast<const sycl::half*>(padding)+size_t(which)*8*2560:x+size_t(row)*k;
                float* y=down?e.output:e.gu;
                auto gp=[](auto* p) {return sycl::address_space_cast<sycl::access::address_space::global_space,
                    sycl::access::decorated::no>(p);};
                jm::joint_matrix<sycl::sub_group,float,jm::use::accumulator,TILE,TILE> c;
                jm::joint_matrix_fill(sg,c,0.f);
                for (int i=0;i<k;i+=16) {
                    jm::joint_matrix<sycl::sub_group,sycl::half,jm::use::a,TILE,16,jm::layout::row_major> ax;
                    jm::joint_matrix<sycl::sub_group,sycl::half,jm::use::b,16,TILE,jm::layout::col_major> bw;
                    jm::joint_matrix_load(sg,ax,gp(source+i),size_t(k));
                    jm::joint_matrix_load(sg,bw,gp(w+size_t(col)*k+i),size_t(k));
                    jm::joint_matrix_mad(sg,c,ax,bw,c);
                }
                if (!partial) jm::joint_matrix_store(sg,c,gp(y+size_t(row)*n+col),size_t(n),jm::layout::row_major);
                else {
                    auto* tmp=tail.get_multi_ptr<sycl::access::decorated::no>().get()+subgroup*TILE*TILE;
                    jm::joint_matrix_store(sg,c,sycl::address_space_cast<sycl::access::address_space::local_space,
                        sycl::access::decorated::no>(tmp),TILE,jm::layout::row_major);
                    sycl::group_barrier(sg);
                    for (int i=lane;i<TILE*TILE;i+=SG)
                        if(row+i/TILE<e.rows)y[size_t(row+i/TILE)*n+col+i%TILE]=tmp[i];
                }
            });
    });
}

inline bool execute(sycl::queue& q,Expert a,Expert b,void* workspace,size_t bytes,bool finish=true) {
    // ponytail: exactly two fixed-dimension experts reuse the existing DQ ring; measure before increasing the group.
    if (!q.has_property<sycl::property::queue::in_order>()) return false;
    for (auto e:{a,b})
        if(e.rows<=0 || e.rows>INT_MAX/2-7 || !e.x || !e.gu_weight || !e.down_weight || !e.gu || !e.hidden || !e.output)
            return false;
    if ((a.rows%8 || b.rows%8) && (!workspace || bytes<2*8*2560*sizeof(uint16_t))) return false;
    auto* padding=static_cast<uint16_t*>(workspace);
    multiply(q,a,b,padding,false);
    strata::prefill::swiglu_interleaved(a.gu,a.hidden,a.rows,&q);
    strata::prefill::swiglu_interleaved(b.gu,b.hidden,b.rows,&q);
    multiply(q,a,b,padding,true);
    if(finish)q.wait_and_throw();
    return true;
}
#endif
}
