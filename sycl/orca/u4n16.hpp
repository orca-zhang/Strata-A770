#pragma once
// A770 opt-in U4 N16: the checked U4 math and cache, combined without numerical changes.

// Direct FP16/BF16 -> U4 weights x S4 activations; opt-in Orca prefill only.
#include <sycl/sycl.hpp>
#include <sycl/ext/intel/esimd.hpp>
#include <sycl/ext/intel/esimd/xmx/dpas.hpp>
#include <sycl/ext/intel/experimental/esimd/math.hpp>
#include <sycl/ext/oneapi/bfloat16.hpp>
#include <sycl/ext/oneapi/matrix/matrix.hpp>
#include <algorithm>
#include <cmath>
#include <cstdio>
#include <cstring>
#include <fstream>
#include <future>
#include <limits>
#include <memory>
#include <stdexcept>
#include <string>
#include <type_traits>
#include <vector>

namespace u4s4 {
namespace es=sycl::ext::intel::esimd;
using P=es::xmx::dpas_argument_type;
constexpr int guard=32;
inline void require(bool ok,const char* message) {if (!ok) throw std::runtime_error(message);}
template<class T> std::vector<T> read(strata::l0::Queue& q,const T* p,size_t n) {
    std::vector<T> v(n);q.memcpy(v.data(),p,n*sizeof(T)).wait_and_throw();return v;
}
template<class T> T* alloc(strata::l0::Queue& q,size_t n) {
    auto* p=strata::l0::malloc_device<T>(n,q);if (!p) throw std::bad_alloc();return p;
}

struct Packed {
    strata::l0::Queue& q;
    const int rows,k,groups,words,metas;
    uint32_t* bits;float* scales;int32_t* sums;int* invalid;
    Packed(strata::l0::Queue& queue,int maxrows,int kk):q(queue),rows((maxrows+7)/8*8),k(kk),
        groups((kk+63)/64),words(rows*groups*8),metas(rows*groups),
        bits(alloc<uint32_t>(q,2*words+2*guard)),scales(alloc<float>(q,2*metas+2*guard)),
        sums(alloc<int32_t>(q,2*metas+2*guard)),invalid(alloc<int>(q,1)) {
        q.fill(bits,0x55555555u,2*words+2*guard);q.fill(scales,-12345.f,2*metas+2*guard);
        q.fill(sums,1234567,2*metas+2*guard);q.fill(invalid,0,1);
    }
    ~Packed() {q.wait();strata::l0::free(invalid,q);strata::l0::free(sums,q);strata::l0::free(scales,q);strata::l0::free(bits,q);}
    void check() {
        require(read(q,invalid,1)[0]==0,"nonfinite quantization input");
        for (size_t off:{size_t(0),size_t(2*words+guard)})
            for (auto v:read(q,bits+off,guard)) require(v==0x55555555u,"packed guard");
        for (size_t off:{size_t(0),size_t(2*metas+guard)}) {
            for (auto v:read(q,scales+off,guard)) require(v==-12345.f,"scale guard");
            for (auto v:read(q,sums+off,guard)) require(v==1234567,"sum guard");
        }
    }
};

template<bool Weight,int Digits,class Input=sycl::half>
void pack(strata::l0::Queue& q,const uint16_t* input,int active,Packed& packed) {
    static_assert(Digits==1 || Digits==2);
    require(active>0 && active<=packed.rows,"pack dimensions");
    const int kk=packed.k,ng=packed.groups,words=packed.words,metas=packed.metas;
    auto* bits=packed.bits+guard;auto* scales=packed.scales+guard;auto* sums=packed.sums+guard;
    auto* invalid=packed.invalid;
    q.parallel_for(sycl::nd_range<1>(size_t((active+7)/8*8)*ng*32,32),
        [=](sycl::nd_item<1> it) [[sycl::reqd_sub_group_size(32)]] {
            const int row=int(it.get_group_linear_id())/ng,g=int(it.get_group_linear_id())%ng;
            const int lane=int(it.get_local_linear_id());
            // Each lane holds two values; each group has its own scale and zero-point correction.
            float values[2];
            for (int j=0;j<2;++j) {
                const int pos=g*64+lane*2+j;
                float v=row<active && pos<kk?float(reinterpret_cast<const Input*>(input)[row*kk+pos]):0.f;
                if (!sycl::isfinite(v)) {
                    sycl::atomic_ref<int,sycl::memory_order::relaxed,sycl::memory_scope::device,
                        sycl::access::address_space::global_space>(*invalid).store(1);v=0;
                }
                values[j]=v;
            }
            const auto sg=it.get_sub_group();
            for (int digit=0;digit<Digits;++digit) {
                constexpr int limit=7;
                float maximum=sycl::fmax(sycl::fabs(values[0]),sycl::fabs(values[1]));
                maximum=sycl::reduce_over_group(sg,maximum,sycl::maximum<float>());
                // BF16 reaches FP32's subnormal range: never divide by an underflowed zero scale.
                const float scale=maximum>0?sycl::fmax(maximum/float(limit),0x1p-126f):1.f;
                int quant[2];
                for (int j=0;j<2;++j) {
                    quant[j]=int(sycl::clamp(sycl::rint(values[j]/scale),-float(limit),float(limit)));
                    values[j]=sycl::fma(-float(quant[j]),scale,values[j]);
                }
                const int sum=sycl::reduce_over_group(sg,quant[0]+quant[1],sycl::plus<int>());
                if (lane==0) {
                    const int meta=(row/8*ng+g)*8+row%8+digit*metas;
                    scales[meta]=scale;sums[meta]=sum;
                }
                const uint32_t pair=uint32_t((quant[0]+(Weight?8:0))&15) |
                    (uint32_t((quant[1]+(Weight?8:0))&15)<<4);
                // Four adjacent lanes own one DWORD: no byte stores or read/modify/write races.
                uint32_t word=0;
                for (int j=0;j<4;++j) word|=sycl::select_from_group(sg,pair,(lane/4)*4+j)<<(j*8);
                if (lane%4==0) {
                    const int within=Weight?(lane/4)*8+row%8:(row%8)*8+lane/4;
                    bits[(row/8*ng+g)*64+within+digit*words]=word;
                }
            }
        });
}

}

namespace expert_u4 {
using namespace u4s4;

struct Weight {
    const uint32_t* bits;
    const float* scales;
};

inline Weight weight(const uint8_t* blob,int n,int k) {
    return {reinterpret_cast<const uint32_t*>(blob),reinterpret_cast<const float*>(blob+size_t(n)*k/2)};
}

}

#include <array>

namespace grouped_u4 {
using namespace u4s4;
constexpr int N=2560,F=640,GU=1280,EXPERTS=8;
constexpr size_t GU_BYTES=1843200,BYTES=2764800;
struct Entry {const uint8_t* weight=nullptr;int source=0,rows=0,packed=0;};
struct Batch {
    std::array<Entry,EXPERTS> entries{};
    int count=0,rows=0;
    void add(const uint8_t* weight,int source,int count_rows,int capacity) {
        require(weight && source>=0 && count_rows>0 && count<EXPERTS,"grouped U4 entry");
        const int padded=(count_rows+7)/8*8;
        require(rows+padded<=capacity,"grouped U4 scratch capacity");
        entries[count++]={weight,source,count_rows,rows};rows+=padded;
    }
};
inline Entry entry(const Batch& batch,int row) {
    int chosen=0;
    for(int i=1;i<batch.count;++i)if(row>=batch.entries[i].packed)chosen=i;
    return batch.entries[chosen];
}

// ponytail: descriptors are captured by value; no host metadata lifetime or shared-memory rewrite race.
template<bool Swiglu>
void pack_batch(strata::l0::Queue& q,const void* input,const Batch& batch,Packed& packed) {
    require(batch.count>0 && batch.count<=EXPERTS && batch.rows<=packed.rows,"grouped U4 pack shape");
    const int kk=packed.k,ng=packed.groups,words=packed.words,metas=packed.metas;
    auto* bits=packed.bits+guard;auto* scales=packed.scales+guard;auto* sums=packed.sums+guard;auto* invalid=packed.invalid;
    q.parallel_for(sycl::nd_range<1>(size_t(batch.rows)*ng*32,32),[=](sycl::nd_item<1> it) [[sycl::reqd_sub_group_size(32)]] {
        const int row=int(it.get_group_linear_id())/ng,g=int(it.get_group_linear_id())%ng,lane=int(it.get_local_linear_id());
        const auto e=entry(batch,row);const int local=row-e.packed,source=e.source+local;
        float values[2];
        for(int j=0;j<2;++j) {
            const int pos=g*64+lane*2+j;float v=0.f;
            if(local<e.rows && pos<kk) {
                if constexpr(Swiglu) {
                    const auto* gu=static_cast<const float*>(input);
                    const float gate=gu[source*GU+2*pos],up=gu[source*GU+2*pos+1];
                    float h=gate/(1.f+sycl::native::exp(-gate))*up;
                    if(h>65504.f)h=65504.f;else if(h< -65504.f)h=-65504.f;
                    v=float(sycl::half(h));
                } else v=float(static_cast<const sycl::half*>(input)[source*kk+pos]);
            }
            if(!sycl::isfinite(v)) {
                sycl::atomic_ref<int,sycl::memory_order::relaxed,sycl::memory_scope::device,
                    sycl::access::address_space::global_space>(*invalid).store(1);v=0;
            }
            values[j]=v;
        }
        const auto sg=it.get_sub_group();
        for(int digit=0;digit<2;++digit) {
            float maximum=sycl::fmax(sycl::fabs(values[0]),sycl::fabs(values[1]));
            maximum=sycl::reduce_over_group(sg,maximum,sycl::maximum<float>());
            const float scale=maximum>0?sycl::fmax(maximum/7.f,0x1p-126f):1.f;
            int quant[2];
            for(int j=0;j<2;++j) {
                quant[j]=int(sycl::clamp(sycl::rint(values[j]/scale),-7.f,7.f));
                values[j]=sycl::fma(-float(quant[j]),scale,values[j]);
            }
            const int sum=sycl::reduce_over_group(sg,quant[0]+quant[1],sycl::plus<int>());
            if(lane==0) {const int meta=(row/8*ng+g)*8+row%8+digit*metas;scales[meta]=scale;sums[meta]=sum;}
            const uint32_t pair=uint32_t(quant[0]&15)|(uint32_t(quant[1]&15)<<4);
            uint32_t word=0;
            for(int j=0;j<4;++j)word|=sycl::select_from_group(sg,pair,(lane/4)*4+j)<<(j*8);
            if(lane%4==0)bits[(row/8*ng+g)*64+row%8*8+lane/4+digit*words]=word;
        }
    });
}

template<bool Down,int NC=2>
strata::l0::Event product(strata::l0::Queue& q,const Packed& x,const Batch& batch,float* out) {
    constexpr int n=Down?N:GU;
    const int ng=x.groups,nt=(n+8*NC-1)/(8*NC),tiles=batch.rows/8*nt;
    const int words=x.words,metas=x.metas;
    auto* xb=x.bits+guard;auto* xs=x.scales+guard;auto* sums=x.sums+guard;auto* invalid=x.invalid;
    return q.parallel_for(sycl::nd_range<1>(size_t((tiles+1)/2*2),2),[=](sycl::nd_item<1> it) SYCL_ESIMD_KERNEL {
        const int tile=int(it.get_global_id(0));if(tile>=tiles)return;
        const int row=tile/nt*8,col=tile%nt*(8*NC);
        const auto e=entry(batch,row);const int local=row-e.packed,source=e.source+local;
        const auto w=expert_u4::weight(e.weight+(Down?GU_BYTES:0),n,Down?F:N);
        es::simd<float,64> accum[NC];
        #pragma unroll
        for(int c=0;c<NC;++c)accum[c]=0;
        for(int g=0;g<ng;++g) {
            es::simd<uint32_t,64> av[2];es::simd<float,64> sa[2];es::simd<int32_t,64> correction[2];
            #pragma unroll
            for(int a=0;a<2;++a) {
                av[a].copy_from(xb+(row/8*ng+g)*64+a*words);
                es::simd<float,8> scale;scale.copy_from(xs+(row/8*ng+g)*8+a*metas);
                es::simd<int32_t,8> sum;sum.copy_from(sums+(row/8*ng+g)*8+a*metas);
                #pragma unroll
                for(int r=0;r<8;++r) {sa[a].template select<8,1>(r*8)=float(scale[r]);correction[a].template select<8,1>(r*8)=int(sum[r])*8;}
            }
            #pragma unroll
            for(int c=0;c<NC;++c) {
                if(col+c*8>=n)continue;
                const int at=(col/8+c)*ng+g;
                es::simd<uint32_t,64> bw;bw.copy_from(w.bits+at*64);
                es::simd<float,8> sw;sw.copy_from(w.scales+at*8);
                #pragma unroll
                for(int a=0;a<2;++a) {
                    es::simd<int32_t,64> zero=0;
                    const auto dot=es::xmx::dpas<8,8,int32_t,int32_t,uint32_t,uint32_t,P::u4,P::s4>(zero,bw,av[a]);
                    es::simd<float,64> fp=es::convert<float>(dot-correction[a]);
                    es::simd<float,64> scale=sa[a]*sw.template replicate<8>();
                    accum[c]=es::simd<float,64>(__builtin_elementwise_fma(fp.data(),scale.data(),accum[c].data()));
                }
            }
        }
        if(*invalid)for(int c=0;c<NC;++c)accum[c]=std::numeric_limits<float>::quiet_NaN();
        es::simd<uint32_t,64> i(0,1);
        #pragma unroll
        for(int c=0;c<NC;++c) {
            const auto offsets=((source+i/8)*n+col+c*8+i%8)*4;
            es::scatter(out,offsets,accum[c],(local+i/8<uint32_t(e.rows))&(col+c*8+i%8<uint32_t(n)));
        }
    });
}

}

#include <array>
#include <filesystem>
#include <string_view>

namespace model_u4 {
using u4s4::require;
constexpr size_t GU_BYTES=1843200,DOWN_BYTES=921600,BYTES=GU_BYTES+DOWN_BYTES;
constexpr int SLOTS=16,MIN_ROWS=128;

// ponytail: measured experiment choices only; arbitrary thresholds need their own calibration.
constexpr int minimum_rows(std::string_view value) {
    return value=="1"?1:value=="8"?8:value=="16"?16:value=="32"?32:value=="64"?64:value=="128"?128:0;
}
static_assert(minimum_rows("1")==1 && minimum_rows("8")==8 && minimum_rows("16")==16 &&
              minimum_rows("32")==32 && minimum_rows("64")==64 && minimum_rows("128")==128);
static_assert(minimum_rows("")==0 && minimum_rows("0")==0 && minimum_rows("-1")==0 &&
              minimum_rows(" 32")==0 && minimum_rows("32x")==0 && minimum_rows("999999999999999999999")==0);
inline int configured_minimum_rows() {
    const char* value=std::getenv("STRATA_U4_MIN_ROWS");
    if(!value)return MIN_ROWS;
    const int result=minimum_rows(value);
    require(result>0,"U4 min rows must be exactly1/8/16/32/64/128");
    return result;
}

inline bool enabled() {
    const char* flag=std::getenv("STRATA_PREFILL_U4_EXPERTS");
    return flag && std::strcmp(flag,"1")==0;
}

struct Cache {
    strata::l0::Queue& q;
    strata::l0::Queue& copy;
    const int min_rows=configured_minimum_rows();
    u4s4::Packed ax,ah;
    std::array<int,48*512> index;
    std::vector<std::pair<int,int>> experts;
    std::array<uint8_t*,48> host{};
    std::array<size_t,48> host_count{};
    std::vector<size_t> host_offset;
    uint8_t* storage=nullptr;
    std::array<int,SLOTS> pending;
    uint64_t copy_ops=0;
    std::array<uint8_t*,SLOTS> device{};
    std::array<strata::l0::Event,SLOTS> copied,used;
    std::array<bool,SLOTS> live{};
    uint64_t calls=0,rows=0,dma_bytes=0,batches=0;

    Cache(strata::l0::Queue& compute,strata::l0::Queue& copying,int maxrows,int readers=4):q(compute),copy(copying),ax(q,maxrows,2560),ah(q,maxrows,640) {
        require(q.is_in_order() && copy.is_in_order(),"U4 expert queue ordering");
        require(q.get_device().get_info<sycl::info::device::name>().find("A770")!=std::string::npos,"U4 expert A770 gate");
        const char* directory=std::getenv("STRATA_PREFILL_U4_EXPERT_DIR");
        require(directory && *directory,"U4 expert asset directory");
        const auto root=std::filesystem::path(directory);
        std::ifstream list(root/"experts.list");require(bool(list),"U4 expert list open");
        index.fill(-1);pending.fill(-1);int layer,expert;
        while(list>>layer) {
            require(bool(list>>expert),"U4 expert list pair");
            require(layer>=0 && layer<48 && expert>=0 && expert<512,"U4 expert list index");
            require(index[layer*512+expert]<0,"U4 expert list duplicate");
            index[layer*512+expert]=int(experts.size());experts.emplace_back(layer,expert);
            host_offset.push_back(host_count[layer]++);
        }
        require(list.eof() && !experts.empty() && experts.size()<=48*512,"U4 expert list syntax");
        require(readers==1 || readers==2 || readers==4,"U4 loader reader count");
        try {
            // ponytail: same total payload, one contiguous allocation per layer; single8.94GB returned null on A770.
            for(int l=0;l<48;++l)if(host_count[l]) {
                host[l]=strata::l0::malloc_host<uint8_t>(host_count[l]*BYTES,q);
                require(host[l],"U4 per-layer pinned weight allocation");
            }
            auto load=[&](size_t first,size_t last) {
                for(size_t i=first;i<last;++i) {
                    const auto [l,e]=experts[i];const auto name="layer"+std::to_string(l)+"-expert"+std::to_string(e);
                    for(int part=0;part<2;++part) {
                        const size_t bytes=part?DOWN_BYTES:GU_BYTES;
                        auto* target=payload(i)+(part?GU_BYTES:0);
                        const auto path=root/"weights"/(name+(part?"-down.u4":"-gate_up.u4"));
                        std::ifstream input(path,std::ios::binary|std::ios::ate);
                        require(input && input.tellg()==std::streamoff(32+bytes),"U4 expert file length");
                        input.seekg(0);uint32_t header[8];input.read((char*)header,32);
                        require(input && !std::memcmp(header,"U4S4v001",8) && header[2]==unsigned(part?2560:1280) &&
                                header[3]==unsigned(part?640:2560) && header[4]==1 && header[5]==1 && header[6]==64 && header[7]==0,"U4 expert file header");
                        input.read((char*)target,bytes);require(bool(input),"U4 expert file payload");
                        const size_t elements=size_t(part?2560:1280)*(part?640:2560);
                        const float* scale=reinterpret_cast<const float*>(target+elements/2);
                        for(size_t j=0;j<elements/64;++j)require(std::isfinite(scale[j]) && scale[j]>0.f,"U4 expert finite positive scale");
                    }
                }
            };
            if(readers==1)load(0,experts.size());
            else {
                // ponytail: bounded disjoint reads; async future destruction joins before allocation cleanup on error.
                std::vector<std::future<void>> jobs;
                for(int i=0;i<readers;++i)jobs.push_back(std::async(std::launch::async,load,
                    experts.size()*i/readers,experts.size()*(i+1)/readers));
                for(auto& job:jobs)job.get();
            }
            storage=u4s4::alloc<uint8_t>(q,SLOTS*BYTES);
            for(int i=0;i<SLOTS;++i)device[i]=storage+size_t(i)*BYTES;
        } catch(...) {
            if(storage)strata::l0::free(storage,q);
            for(auto*& p:host)if(p) {strata::l0::free(p,q);p=nullptr;}throw;
        }
        std::fprintf(stderr,"PREFILL_EXPERT_U4 loaded=%zu host_bytes=%zu device_bytes=%zu min_rows=%d decode_original=1\n",
                     experts.size(),experts.size()*BYTES,SLOTS*BYTES,min_rows);
    }
    ~Cache() {
        copy.wait();q.wait();if(storage)strata::l0::free(storage,q);for(auto* p:host)if(p)strata::l0::free(p,q);
    }
    uint8_t* payload(size_t i)const {return host[experts[i].first]+host_offset[i]*BYTES;}
    bool eligible(int64_t layer,int expert,int64_t count)const {
        return layer>=0 && layer<48 && expert>=0 && expert<512 && count>=min_rows && count<=ax.rows && index[layer*512+expert]>=0;
    }
    void begin() {
        q.fill(ax.invalid,0,1);q.fill(ah.invalid,0,1);calls=rows=dma_bytes=batches=copy_ops=0;
    }
    void stage(int64_t layer,int expert,int slot) {
        require(layer>=0 && layer<48 && expert>=0 && expert<512 && slot>=0 && slot<SLOTS,"U4 stage bounds");
        require(index[layer*512+expert]>=0 && pending[slot]<0,"U4 pending slot identity");
        pending[slot]=index[layer*512+expert];
    }
    void flush(int bank,int count) {
        require((bank==0 || bank==8) && count>0 && count<=8,"U4 copy bank");
        for(int i=0;i<count;++i)require(pending[bank+i]>=0 && size_t(pending[bank+i])<experts.size(),"U4 unstaged copy");
        // ponytail: one consumer fence covers this bank; the shared kernel marks every occupied slot together.
        if(live[bank])copy.ext_oneapi_submit_barrier({used[bank]});
        for(int i=0;i<count;) {
            int end=i+1;
            while(end<count && experts[pending[bank+end]].first==experts[pending[bank+i]].first && pending[bank+end]==pending[bank+i]+end-i)++end;
            const size_t bytes=size_t(end-i)*BYTES;
            copied[bank+count-1]=copy.memcpy(device[bank+i],payload(size_t(pending[bank+i])),bytes);
            dma_bytes+=bytes;++copy_ops;i=end;
        }
        for(int i=0;i<count;++i)pending[bank+i]=-1;
    }

    void end() {
        // ponytail: invalid is sticky within one request and cleared at begin; fail the request before logits are consumed.
        const int input_bad=u4s4::read(q,ax.invalid,1)[0],hidden_bad=u4s4::read(q,ah.invalid,1)[0];
        if(input_bad || hidden_bad)throw std::runtime_error("U4 expert nonfinite activation: input="+
            std::to_string(input_bad)+" hidden="+std::to_string(hidden_bad));
        std::fprintf(stderr,"PREFILL_EXPERT_U4 calls=%llu rows=%llu dma_bytes=%llu batches=%llu\n",
                     (unsigned long long)calls,(unsigned long long)rows,(unsigned long long)dma_bytes,(unsigned long long)batches);
    }
};
}

#include <algorithm>

namespace shared_u4 {
using namespace u4s4;

// ponytail: keep the existing eight-expert descriptors and down kernel. Only GU reads shared token rows.
template<int NC=2>
strata::l0::Event gu(strata::l0::Queue& q,const Packed& x,const int32_t* source_ids,
               const grouped_u4::Batch& batch,float* out) {
    constexpr int n=1280;
    const int ng=x.groups,nt=n/(8*NC),tiles=batch.rows/8*nt,words=x.words,metas=x.metas;
    auto* xb=x.bits+guard;auto* xs=x.scales+guard;auto* sums=x.sums+guard;auto* invalid=x.invalid;
    return q.parallel_for(sycl::nd_range<1>(size_t((tiles+1)/2*2),2),[=](sycl::nd_item<1> it) SYCL_ESIMD_KERNEL {
        const int tile=int(it.get_global_id(0));if(tile>=tiles)return;
        const int row=tile/nt*8,col=tile%nt*(8*NC);
        const auto e=grouped_u4::entry(batch,row);const int local=row-e.packed,source=e.source+local;
        const auto w=expert_u4::weight(e.weight,n,2560);
        es::simd<uint32_t,8> r(0,1);es::simd<uint32_t,64> i(0,1);
        const auto valid8=uint32_t(local)+r<uint32_t(e.rows);
        const auto valid64=uint32_t(local)+i/8<uint32_t(e.rows);
        auto ids=es::gather<int32_t,8>(source_ids,(uint32_t(source)+r)*4,valid8);
        ids.merge(0,!valid8);
        es::simd<uint32_t,64> bit_base;
        #pragma unroll
        for(int j=0;j<8;++j)bit_base.template select<8,1>(j*8)=uint32_t(ids[j]/8*ng*64+ids[j]%8*8)*4+r*4;
        const auto meta_base=(es::convert<uint32_t>(ids)/8*ng*8+es::convert<uint32_t>(ids)%8)*4;
        es::simd<float,64> accum[NC];
        #pragma unroll
        for(int c=0;c<NC;++c)accum[c]=0;
        for(int g=0;g<ng;++g) {
            es::simd<uint32_t,64> av[2];es::simd<float,64> sa[2];es::simd<int32_t,64> correction[2];
            #pragma unroll
            for(int a=0;a<2;++a) {
                // ponytail: padded IDs already point to valid token0; the output store remains masked.
                av[a]=es::gather<uint32_t,64>(xb,bit_base+uint32_t(g*64+a*words)*4);
                auto scale=es::gather<float,8>(xs,meta_base+uint32_t(g*8+a*metas)*4);
                auto sum=es::gather<int32_t,8>(sums,meta_base+uint32_t(g*8+a*metas)*4);
                #pragma unroll
                for(int j=0;j<8;++j) {sa[a].template select<8,1>(j*8)=float(scale[j]);correction[a].template select<8,1>(j*8)=int(sum[j])*8;}
            }
            #pragma unroll
            for(int c=0;c<NC;++c) {
                const int at=(col/8+c)*ng+g;
                es::simd<uint32_t,64> bw;bw.copy_from(w.bits+at*64);
                es::simd<float,8> sw;sw.copy_from(w.scales+at*8);
                #pragma unroll
                for(int a=0;a<2;++a) {
                    es::simd<int32_t,64> zero=0;
                    const auto dot=es::xmx::dpas<8,8,int32_t,int32_t,uint32_t,uint32_t,P::u4,P::s4>(zero,bw,av[a]);
                    es::simd<float,64> fp=es::convert<float>(dot-correction[a]);
                    es::simd<float,64> scale=sa[a]*sw.template replicate<8>();
                    accum[c]=es::simd<float,64>(__builtin_elementwise_fma(fp.data(),scale.data(),accum[c].data()));
                }
            }
        }
        if(*invalid)for(int c=0;c<NC;++c)accum[c]=std::numeric_limits<float>::quiet_NaN();
        #pragma unroll
        for(int c=0;c<NC;++c)es::scatter(out,((source+i/8)*n+col+c*8+i%8)*4,accum[c],valid64);
    });
}

inline void run(model_u4::Cache& cache,const int32_t* ids,float* gu_out,float* down,
                const grouped_u4::Batch& batch,int bank) {
    require(ids && (bank==0 || bank==8) && batch.count>0 && batch.count<=8,"shared U4 group");
    for(int i=0;i<batch.count;++i)require(batch.entries[i].weight==cache.device[bank+i],"shared U4 slot identity");
    cache.q.ext_oneapi_submit_barrier({cache.copied[bank+batch.count-1]});
    const char* tile=std::getenv("STRATA_U4_TILE_N8");
    require(!tile || std::strcmp(tile,"0")==0,"Orca U4 supports N16 only");
    gu<2>(cache.q,cache.ax,ids,batch,gu_out);
    grouped_u4::pack_batch<true>(cache.q,gu_out,batch,cache.ah);
    grouped_u4::product<true,2>(cache.q,cache.ah,batch,down);
    const auto consumed=cache.q.ext_oneapi_submit_barrier();
    for(int i=0;i<batch.count;++i) {
        cache.used[bank+i]=consumed;cache.live[bank+i]=true;cache.rows+=batch.entries[i].rows;
    }
    cache.calls+=batch.count;++cache.batches;
}
}
