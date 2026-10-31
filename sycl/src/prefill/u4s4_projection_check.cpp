// Experimental direct FP16/BF16 -> U4 weights x S4 activations. No inference dispatch uses this file.
#include <sycl/sycl.hpp>
#include <sycl/ext/intel/esimd.hpp>
#include <sycl/ext/intel/esimd/xmx/dpas.hpp>
#include <sycl/ext/intel/experimental/esimd/math.hpp>
#include <sycl/ext/oneapi/bfloat16.hpp>
#include <sycl/ext/oneapi/matrix/matrix.hpp>
#include <algorithm>
#include <chrono>
#include <cmath>
#include <cstdio>
#include <cstring>
#include <fstream>
#include <limits>
#include <memory>
#include <random>
#include <stdexcept>
#include <string>
#include <type_traits>
#include <vector>

namespace u4s4 {
namespace es=sycl::ext::intel::esimd;
using P=es::xmx::dpas_argument_type;
constexpr int guard=32;
int checks=0;
void require(bool ok,const char* message) {if (!ok) throw std::runtime_error(message);++checks;}
template<class T> std::vector<T> read(sycl::queue& q,const T* p,size_t n) {
    std::vector<T> v(n);q.memcpy(v.data(),p,n*sizeof(T)).wait_and_throw();return v;
}
template<class T> T* alloc(sycl::queue& q,size_t n) {
    auto* p=sycl::malloc_device<T>(n,q);if (!p) throw std::bad_alloc();return p;
}

struct Packed {
    sycl::queue& q;
    const int rows,k,groups,words,metas;
    uint32_t* bits;float* scales;int32_t* sums;int* invalid;
    Packed(sycl::queue& queue,int maxrows,int kk):q(queue),rows((maxrows+7)/8*8),k(kk),
        groups((kk+63)/64),words(rows*groups*8),metas(rows*groups),
        bits(alloc<uint32_t>(q,2*words+2*guard)),scales(alloc<float>(q,2*metas+2*guard)),
        sums(alloc<int32_t>(q,2*metas+2*guard)),invalid(alloc<int>(q,1)) {
        q.fill(bits,0x55555555u,2*words+2*guard);q.fill(scales,-12345.f,2*metas+2*guard);
        q.fill(sums,1234567,2*metas+2*guard);q.fill(invalid,0,1);
    }
    ~Packed() {q.wait();sycl::free(invalid,q);sycl::free(sums,q);sycl::free(scales,q);sycl::free(bits,q);}
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
void pack(sycl::queue& q,const uint16_t* input,int active,Packed& packed) {
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

template<int XD,int WD>
sycl::event product(sycl::queue& q,const Packed& x,const Packed& w,float* y,int t,int n) {
    require(t>0 && t<=x.rows && n>0 && n<=w.rows && x.k==w.k,"product dimensions");
    const int ng=x.groups,nt=(n+7)/8,tiles=(t+7)/8*nt;
    const int xwords=x.words,wwords=w.words,xmetas=x.metas,wmetas=w.metas;
    const auto* xb=x.bits+guard;const auto* wb=w.bits+guard;
    const auto* xs=x.scales+guard;const auto* ws=w.scales+guard;const auto* sums=x.sums+guard;
    return q.parallel_for(sycl::nd_range<1>(size_t((tiles+1)/2*2),2),
        [=](sycl::nd_item<1> it) SYCL_ESIMD_KERNEL {
            const int tile=int(it.get_global_id(0));if (tile>=tiles) return;
            const int row=tile/nt*8,col=tile%nt*8;
            es::simd<float,64> result=0;
            for (int g=0;g<ng;++g) {
                const int xt=row/8*ng+g,wt=col/8*ng+g;
                #pragma unroll
                for (int a=0;a<XD;++a) {
                    es::simd<uint32_t,64> av;av.copy_from(xb+xt*64+a*xwords);
                    es::simd<float,8> scalea;scalea.copy_from(xs+xt*8+a*xmetas);
                    es::simd<int32_t,8> suma;suma.copy_from(sums+xt*8+a*xmetas);
                    es::simd<float,64> sa;es::simd<int32_t,64> sum;
                    #pragma unroll
                    for (int r=0;r<8;++r) {sa.select<8,1>(r*8)=float(scalea[r]);sum.select<8,1>(r*8)=int32_t(suma[r]);}
                    #pragma unroll
                    for (int b=0;b<WD;++b) {
                        es::simd<uint32_t,64> bv;bv.copy_from(wb+wt*64+b*wwords);
                        es::simd<float,8> scaleb;scaleb.copy_from(ws+wt*8+b*wmetas);
                        es::simd<int32_t,64> zero=0;
                        auto dot=es::xmx::dpas<8,8,int32_t,int32_t,uint32_t,uint32_t,P::u4,P::s4>(zero,bv,av);
                        // Stored weights are q_w+8. Correct separately for every group and residual digit.
                        result+=es::convert<float>(dot-8*sum)*(sa*scaleb.replicate<8>());
                    }
                }
            }
            es::simd<uint32_t,64> index(0,1);
            auto offsets=((row+index/8)*n+col+index%8)*uint32_t(sizeof(float));
            es::simd_mask<64> valid=(row+index/8<uint32_t(t)) & (col+index%8<uint32_t(n));
            es::scatter(y,offsets,result,valid);
        });
}

template<int XD,int WD,class Input=sycl::half>
sycl::event fused_product(sycl::queue& q,const uint16_t* input,Packed& x,const Packed& w,float* y,int t,int n) {
    require(t>0 && t<=x.rows && n>0 && n<=w.rows && x.k==w.k,"fused dimensions");
    const int kk=x.k,ng=x.groups,nt=(n+7)/8,tiles=(t+7)/8*nt,wwords=w.words,wmetas=w.metas;
    const auto* wb=w.bits+guard;const auto* ws=w.scales+guard;auto* invalid=x.invalid;
    return q.parallel_for(sycl::nd_range<1>(size_t((tiles+1)/2*2),2),
        [=](sycl::nd_item<1> it) SYCL_ESIMD_KERNEL {
            const int tile=int(it.get_global_id(0));if (tile>=tiles) return;
            const int row=tile/nt*8,col=tile%nt*8;
            es::simd<float,64> result=0;es::simd<uint32_t,64> lane(0,1);
            for (int g=0;g<ng;++g) {
                es::simd<uint32_t,64> av0=0,av1=0;
                es::simd<float,8> sa0=1,sa1=1;es::simd<int32_t,8> sum0=0,sum1=0;
                // ponytail: quantization repeats per N tile; this experiment must beat the separate pack before dispatch.
                #pragma unroll
                for (int r=0;r<8;++r) {
                    if (row+r>=t) continue;
                    es::simd<uint16_t,64> raw=0;
                    if (row+r<t) {
                        if (g*64+64<=kk) raw.copy_from(input+(row+r)*kk+g*64);
                        else raw=es::gather<uint16_t,64>(input,((row+r)*kk+g*64+lane)*2,lane+g*64<uint32_t(kk));
                    }
                    es::simd<float,64> values;
                    if constexpr (std::is_same_v<Input,sycl::half>) {es::simd<sycl::half,64> half=raw.template bit_cast_view<sycl::half>();values=es::convert<float>(half);}
                    else {es::simd<uint32_t,64> bits=es::convert<uint32_t>(raw)<<16;values=bits.template bit_cast_view<float>();}
                    auto bad=es::abs(values)>std::numeric_limits<float>::max() | (values!=values);
                    if (bad.any()) {
                        es::atomic_update<es::atomic_op::bit_or>(invalid,es::simd<uint32_t,1>(0),es::simd<int,1>(1));
                        values.merge(0.f,bad);
                    }
                    #pragma unroll
                    for (int d=0;d<XD;++d) {
                        const float maximum=es::hmax<float>(es::abs(values));
                        const float scale=maximum>0?es::max<float>(es::div_ieee(maximum,7.f),0x1p-126f):1.f;
                        es::simd<float,64> rounded=es::rnde<float>(es::div_ieee(values,scale));
                        es::simd<int32_t,64> quant=es::convert<int32_t>(es::min<float>(es::max<float>(rounded,-7.f),7.f));
                        es::simd<uint32_t,64> nibble=es::convert<uint32_t>(quant)&15u;
                        es::simd<uint32_t,8> words=0;
                        #pragma unroll
                        for (int j=0;j<8;++j) words|=nibble.template select<8,8>(j)<<(j*4);
                        const int32_t sum=es::reduce<int32_t>(quant,std::plus<>());
                        if (d==0) {av0.template select<8,1>(r*8)=words;sa0[r]=scale;sum0[r]=sum;}
                        else {av1.template select<8,1>(r*8)=words;sa1[r]=scale;sum1[r]=sum;}
                        values=sycl::ext::intel::experimental::esimd::fma<float,64>(-es::convert<float>(quant),es::simd<float,64>(scale),values);
                    }
                }
                #pragma unroll
                for (int a=0;a<XD;++a) {
                    es::simd<uint32_t,64> av=a==0?av0:av1;
                    es::simd<float,8> scalea=a==0?sa0:sa1;
                    es::simd<int32_t,8> suma=a==0?sum0:sum1;
                    es::simd<float,64> sa;es::simd<int32_t,64> sum;
                    #pragma unroll
                    for (int r=0;r<8;++r) {sa.template select<8,1>(r*8)=float(scalea[r]);sum.template select<8,1>(r*8)=int32_t(suma[r]);}
                    #pragma unroll
                    for (int b=0;b<WD;++b) {
                        es::simd<uint32_t,64> bv;bv.copy_from(wb+(col/8*ng+g)*64+b*wwords);
                        es::simd<float,8> scaleb;scaleb.copy_from(ws+(col/8*ng+g)*8+b*wmetas);
                        es::simd<int32_t,64> zero=0;
                        auto dot=es::xmx::dpas<8,8,int32_t,int32_t,uint32_t,uint32_t,P::u4,P::s4>(zero,bv,av);
                        result+=es::convert<float>(dot-8*sum)*(sa*scaleb.replicate<8>());
                    }
                }
            }
            auto offsets=((row+lane/8)*n+col+lane%8)*uint32_t(sizeof(float));
            es::scatter(y,offsets,result,(row+lane/8<uint32_t(t)) & (col+lane%8<uint32_t(n)));
        });
}

template<int XD,int WD,class Input=sycl::half>
void oracle(sycl::queue& q,int t,int n,int kk) {
    const int xp=(t+7)/8*8,wp=(n+7)/8*8,ng=(kk+63)/64;
    Packed x(q,t,kk),w(q,n,kk);
    std::vector<Input> a(t*kk),b(n*kk);
    for (int i=0;i<t*kk;++i) a[i]=Input(i<kk?0.f:float(i%37-18)*.13f);
    for (int i=0;i<n*kk;++i) b[i]=Input(float(i%29-14)*.07f);
    if (t>1) {a[kk]=Input(65504.f);a[kk+1]=Input(-65504.f);}
    if constexpr (std::is_same_v<Input,sycl::ext::oneapi::bfloat16>)
        if (t>1 && kk>=64) for (int i=0;i<kk;++i) a[(t-1)*kk+i]=Input((i%3-1)*0x1p-126f);
    auto* da=alloc<uint16_t>(q,a.size());auto* db=alloc<uint16_t>(q,b.size());auto* y=alloc<float>(q,t*n+2*guard);
    q.memcpy(da,a.data(),a.size()*2);q.memcpy(db,b.data(),b.size()*2);q.fill(y,-12345.f,t*n+2*guard);
    pack<false,XD,Input>(q,da,t,x);pack<true,WD,Input>(q,db,n,w);product<XD,WD>(q,x,w,y+guard,t,n);
    const auto ax=read(q,x.bits+guard,2*x.words),bw=read(q,w.bits+guard,2*w.words);
    const auto sx=read(q,x.scales+guard,2*x.metas),sw=read(q,w.scales+guard,2*w.metas);
    const auto sums=read(q,x.sums+guard,2*x.metas);
    const auto out=read(q,y,t*n+2*guard);
    fused_product<XD,WD,Input>(q,da,x,w,y+guard,t,n);
    const auto fused=read(q,y,t*n+2*guard);
    for (size_t i=0;i<fused.size();++i) {
        if (fused[i]!=out[i]) std::fprintf(stderr,"DIFF T%d N%d K%d XD%d WD%d bf%d i%zu fused %.9g separate %.9g\n",t,n,kk,XD,WD,int(!std::is_same_v<Input,sycl::half>),i,fused[i],out[i]);
        require(fused[i]==out[i],"fused/unfused exact dot and guards");
    }
    auto code=[&](bool weight,int digit,int row,int g,int k) {
        const auto& data=weight?bw:ax;const int plane=weight?w.words:x.words;
        const int offset=(row/8*ng+g)*64+(weight?(k/8)*8+row%8:row%8*8+k/8)+digit*plane;
        const int v=(data[offset]>>((k%8)*4))&15;
        return weight?v-8:(v>=8?v-16:v);
    };
    // Independent CPU quantizer checks every packed nibble, scale, residual and sum, including padding.
    for (bool weight:{false,true}) {
        const auto& input=weight?b:a;const auto& scales=weight?sw:sx;
        const int rows=weight?wp:xp,active=weight?n:t,digits=weight?WD:XD,metas=weight?w.metas:x.metas;
        for (int r=0;r<rows;++r) for (int g=0;g<ng;++g) {
            float v[64]{};
            for (int i=0;i<64;++i) if (r<active && g*64+i<kk) v[i]=float(input[r*kk+g*64+i]);
            for (int d=0;d<digits;++d) {
                float maximum=0;for (float z:v) maximum=std::max(maximum,std::fabs(z));
                constexpr int limit=7;
                const float scale=maximum>0?std::max(maximum/float(limit),0x1p-126f):1.f;
                const int meta=(r/8*ng+g)*8+r%8+d*metas;
                require(scales[meta]==scale,"CPU group scale");int sum=0;
                for (int i=0;i<64;++i) {
                    const int quant=int(std::nearbyint(v[i]/scale));
                    require(code(weight,d,r,g,i)==quant,"CPU packed/residual quantizer");
                    v[i]=std::fma(-float(quant),scale,v[i]);sum+=quant;
                }
                if (!weight) require(sums[meta]==sum,"CPU activation sum");
            }
        }
    }
    for (int r=0;r<t;++r) for (int c=0;c<n;++c) {
        double expected=0,magnitude=0;
        for (int g=0;g<ng;++g) for (int ad=0;ad<XD;++ad) for (int wd=0;wd<WD;++wd) {
            int dot=0;for (int k=0;k<64;++k) dot+=code(false,ad,r,g,k)*code(true,wd,c,g,k);
            const double term=double(dot)*sx[(r/8*ng+g)*8+r%8+ad*x.metas]*sw[(c/8*ng+g)*8+c%8+wd*w.metas];
            expected+=term;magnitude+=std::fabs(term);
        }
        require(std::fabs(out[guard+r*n+c]-expected)<=2e-6*(1+magnitude),"CPU corrected/scaled dot");
    }
    for (int i=0;i<guard;++i) require(out[i]==-12345.f && out[out.size()-1-i]==-12345.f,"output guard");
    x.check();w.check();
    const bool bf=std::is_same_v<Input,sycl::ext::oneapi::bfloat16>;
    for (uint16_t bad:bf?std::vector<uint16_t>{0x7fc0,0x7f80,0xff80}:std::vector<uint16_t>{0x7e00,0x7c00,0xfc00}) {
        q.memcpy(da,&bad,2);q.fill(x.invalid,0,1);pack<false,XD,Input>(q,da,t,x);
        require(read(q,x.invalid,1)[0]==1,"nonfinite flag");
        q.fill(x.invalid,0,1);fused_product<XD,WD,Input>(q,da,x,w,y+guard,t,n);
        require(read(q,x.invalid,1)[0]==1,"fused nonfinite flag");
    }
    sycl::free(y,q);sycl::free(db,q);sycl::free(da,q);
}

void selftest(sycl::queue& q) {
    // ponytail: keep this prototype in one runnable checker until operator precision/speed justify a dispatch.
    for (int k:{1,63,64,65,128}) {
        oracle<1,1>(q,1,1,k);oracle<2,1>(q,9,9,k);oracle<2,2>(q,9,9,k);
        using BF=sycl::ext::oneapi::bfloat16;
        oracle<1,1,BF>(q,1,1,k);oracle<2,1,BF>(q,9,9,k);oracle<2,2,BF>(q,9,9,k);
    }
    std::printf("SELFTEST {\"checks\":%d}\n",checks);
}

template<class Input>
void original(sycl::queue& q,const uint16_t* x,const uint16_t* w,float* y,int t,int n,int kk) {
    namespace jm=sycl::ext::oneapi::experimental::matrix;
    namespace ji=sycl::ext::intel::experimental::matrix;
    q.parallel_for(sycl::nd_range<3>({size_t((t+7)/8),size_t((n+63)/64),64},{1,1,64}),
        [=](sycl::nd_item<3> it) [[sycl::reqd_sub_group_size(8)]] {
            auto sg=it.get_sub_group();
            const int row=int(it.get_group(0))*8,col=(int(it.get_group(1))*8+int(sg.get_group_id()[0]))*8;
            if (col>=n) return;
            auto gp=[](auto* p) {return sycl::address_space_cast<sycl::access::address_space::global_space,
                sycl::access::decorated::no>(p);};
            jm::joint_matrix<sycl::sub_group,float,jm::use::accumulator,8,8> c;
            jm::joint_matrix_fill(sg,c,0.f);
            for (int k=0;k<kk;k+=16) {
                jm::joint_matrix<sycl::sub_group,Input,jm::use::a,8,16,jm::layout::row_major> a;
                jm::joint_matrix<sycl::sub_group,Input,jm::use::b,16,8,jm::layout::col_major> b;
                jm::joint_matrix_load(sg,a,gp(reinterpret_cast<const Input*>(x)+row*kk+k),kk);
                jm::joint_matrix_load(sg,b,gp(reinterpret_cast<const Input*>(w)+col*kk+k),kk);
                jm::joint_matrix_mad(sg,c,a,b,c);
            }
            ji::joint_matrix_apply(sg,c,[=](float& v,size_t r,size_t c) {
                if (row+int(r)<t) y[(row+r)*n+col+c]=v;
            });
        });
}

template<class Input>
void projection(sycl::queue& q,const char* path,int n,int kk,int bench_mode,int bench_t,const std::string& pipeline,const char* native) {
    bool repack=pipeline=="repack" || pipeline=="fused-repack";
    const bool fused=pipeline=="fused" || pipeline=="fused-repack" || pipeline=="fused-vs-cached";
    const bool compare_cached=pipeline=="fused-vs-cached";
    const bool compare_repack=pipeline=="cached-vs-repack";
    constexpr int MAXT=256,BANKS=8;
    const char* type=std::is_same_v<Input,sycl::half>?"fp16":"bf16";
    const size_t count=size_t(n)*kk,bytes=count*2;
    std::ifstream file(path,std::ios::binary|std::ios::ate);
    require(file && file.tellg()==std::streamoff(bytes),"matrix file size");
    std::vector<uint16_t> host_weights(count);file.seekg(0);
    file.read(reinterpret_cast<char*>(host_weights.data()),bytes);require(bool(file),"matrix file read");
    for (auto bits:host_weights) require(std::isfinite(float(sycl::bit_cast<Input>(bits))),"finite original weights");
    auto* w=alloc<uint16_t>(q,count*BANKS);auto* x=alloc<uint16_t>(q,size_t(MAXT)*kk);
    auto* y=alloc<float>(q,size_t(MAXT)*n+2*guard);
    for (int bank=0;bank<BANKS;++bank) q.memcpy(w+bank*count,host_weights.data(),bytes);
    Packed ax(q,MAXT,kk),temporary(q,n,kk);
    std::vector<std::unique_ptr<Packed>> weights;
    for (int bank=0;bank<BANKS;++bank) {
        weights.push_back(std::make_unique<Packed>(q,n,kk));
        pack<true,2,Input>(q,w+bank*count,n,*weights.back());
    }
    q.wait_and_throw();
    const auto codes=read(q,weights[0]->bits+guard,2*weights[0]->words);
    const auto scales=read(q,weights[0]->scales+guard,2*weights[0]->metas);
    if (native) {
        std::ifstream packed(native,std::ios::binary|std::ios::ate);uint32_t header[8]{};
        require(bool(packed),"native file open");const auto size=packed.tellg();packed.seekg(0);
        packed.read(reinterpret_cast<char*>(header),sizeof(header));
        require(bool(packed) && std::memcmp(header,"U4S4v001",8)==0,"native header");
        const int digits=int(header[4]);
        require(header[2]==uint32_t(n) && header[3]==uint32_t(kk) && (digits==1 || digits==2) &&
            header[5]==uint32_t(std::is_same_v<Input,sycl::ext::oneapi::bfloat16>) && header[6]==64 && header[7]==0,"native dimensions/type/version");
        require(digits==2 || bench_mode==1 || bench_mode==2,"two U4 planes required for weight residual");
        const size_t nw=size_t(weights[0]->words)*digits,ns=size_t(weights[0]->metas)*digits;
        require(size==std::streamoff(32+4*(nw+ns)),"native file size");
        std::vector<uint32_t> bits(nw);std::vector<float> scale(ns);
        packed.read(reinterpret_cast<char*>(bits.data()),nw*4);packed.read(reinterpret_cast<char*>(scale.data()),ns*4);
        require(bool(packed),"native payload read");
        require(std::equal(bits.begin(),bits.end(),codes.begin()) && std::equal(scale.begin(),scale.end(),scales.begin()),"offline/GPU native packing mismatch");
        for (auto& bank:weights) {q.memcpy(bank->bits+guard,bits.data(),nw*4);q.memcpy(bank->scales+guard,scale.data(),ns*4);}
        q.wait_and_throw();
        std::printf("NATIVE {\"digits\":%d,\"bytes\":%lld,\"exact_gpu_match\":true}\n",digits,(long long)size);
    }
    for (int digits:{1,2}) {
        double se=0,ss=0,maximum=0;
        for (int row=0;row<n;++row) for (int k=0;k<kk;++k) {
            double value=0;
            for (int d=0;d<digits;++d) {
                const int tile=row/8*weights[0]->groups+k/64;
                const auto bits=codes[tile*64+(k%64/8)*8+row%8+d*weights[0]->words];
                const int code=int((bits>>((k%8)*4))&15)-8;
                value+=code*double(scales[tile*8+row%8+d*weights[0]->metas]);
            }
            const double a=float(sycl::bit_cast<Input>(host_weights[size_t(row)*kk+k])),e=value-a;
            se+=e*e;ss+=a*a;maximum=std::max(maximum,std::fabs(e));
        }
        std::printf("WEIGHT {\"type\":\"%s\",\"digits\":%d,\"relative_l2\":%.9g,\"max_abs\":%.9g}\n",type,digits,std::sqrt(se/(ss+1e-300)),maximum);
    }
    auto launch=[&](int mode,int t,int bank,bool use_fused=false,bool use_repack=false) {
        bank%=BANKS;
        if (!mode) {original<Input>(q,x,w+bank*count,y+guard,t,n,kk);return;}
        const Packed* packed=weights[bank].get();
        if (use_repack) {
            if (mode==3) pack<true,2,Input>(q,w+bank*count,n,temporary);
            else pack<true,1,Input>(q,w+bank*count,n,temporary);
            packed=&temporary;
        }
        if (use_fused) {
            if (mode==1) fused_product<1,1,Input>(q,x,ax,*packed,y+guard,t,n);
            else if (mode==2) fused_product<2,1,Input>(q,x,ax,*packed,y+guard,t,n);
            else fused_product<2,2,Input>(q,x,ax,*packed,y+guard,t,n);
        }
        else if (mode==1) {pack<false,1,Input>(q,x,t,ax);product<1,1>(q,ax,*packed,y+guard,t,n);}
        else if (mode==2) {pack<false,2,Input>(q,x,t,ax);product<2,1>(q,ax,*packed,y+guard,t,n);}
        else {pack<false,2,Input>(q,x,t,ax);product<2,2>(q,ax,*packed,y+guard,t,n);}
    };
    std::mt19937 rng(251008);std::normal_distribution<float> normal;
    const std::vector<int> shapes=bench_mode>0?std::vector<int>{bench_t}:std::vector<int>{1,6,8,16,40,64,128,256};
    bool quality=true;bool valid[257][4]{};int pair_count=0;
    for (int profile=0;profile<2;++profile) {
        std::vector<Input> input(size_t(MAXT)*kk);
        for (size_t i=0;i<input.size();++i) input[i]=Input(normal(rng)*((profile && i%256==0)?16.f:1.f));
        q.memcpy(x,input.data(),input.size()*2);
        for (int t:shapes) {
            launch(0,t,0);const auto reference=read(q,y+guard,size_t(t)*n);
            for (int row:{0,t-1}) for (int col:{0,n-1}) {
                double expected=0,magnitude=0;
                for (int k=0;k<kk;++k) {
                    const double term=double(float(input[size_t(row)*kk+k]))*float(sycl::bit_cast<Input>(host_weights[size_t(col)*kk+k]));
                    expected+=term;magnitude+=std::fabs(term);
                }
                require(std::fabs(reference[size_t(row)*n+col]-expected)<=2e-5*(1+magnitude),"original projection CPU64 spot check");
            }
            const std::vector<int> modes=bench_mode>0?std::vector<int>{bench_mode}:std::vector<int>{1,2,3};
            for (int mode:modes) {
                const size_t ny=size_t(t)*n;
                q.fill(y,-12345.f,ny+2*guard);launch(mode,t,0,fused,repack);const auto out=read(q,y,ny+2*guard);
                ax.check();weights[0]->check();temporary.check();
                for (int i=0;i<guard;++i) require(out[i]==-12345.f && out[ny+guard+i]==-12345.f,"projection guard");
                double se=0,ss=0,oo=0,dot=0,maximum=0;
                for (size_t i=0;i<ny;++i) {
                    const double a=reference[i],b=out[guard+i],e=b-a;
                    require(std::isfinite(a) && std::isfinite(b),"finite projection");
                    se+=e*e;ss+=a*a;oo+=b*b;dot+=a*b;maximum=std::max(maximum,std::fabs(e));
                }
                const double nrmse=std::sqrt(se/(ss+1e-300));
                const double cosine=ss==0?(oo==0?1:0):oo==0?0:dot/std::sqrt(ss)/std::sqrt(oo);
                const bool passed=nrmse<=(profile?.10:.05) && cosine>=(profile?.98:.995);quality &= passed;
                if (!profile) valid[t][mode]=passed;else valid[t][mode] &= passed;
                std::printf("ERROR {\"type\":\"%s\",\"T\":%d,\"mode\":%d,\"profile\":%d,\"nrmse\":%.9g,\"cosine\":%.9g,\"max_abs\":%.9g,\"passed\":%s}\n",
                    type,t,mode,profile,nrmse,cosine,maximum,passed?"true":"false");
            }
        }
    }
    auto timing=[&](int selected_mode,int selected_t,bool selected_repack) {
        repack=selected_repack;
        std::mt19937 timing_rng(251008);normal.reset();std::vector<Input> input(size_t(MAXT)*kk);
        for (auto& v:input) v=Input(normal(timing_rng));q.memcpy(x,input.data(),input.size()*2);
        for (int i=0;i<128;++i) {launch(compare_cached||compare_repack?selected_mode:0,selected_t,i,false,compare_repack);launch(selected_mode,selected_t,i,fused,repack);if (i%32==31) q.wait_and_throw();}
        for (int cycle=0;cycle<8;++cycle) {
            double us[4];
            for (int arm=0;arm<4;++arm) {
                const auto start=std::chrono::steady_clock::now();
                const bool candidate=arm==1||arm==2;
                for (int i=0;i<256;++i) {launch(candidate||compare_cached||compare_repack?selected_mode:0,selected_t,i,candidate&&fused,candidate?repack:compare_repack);if (i%32==31) q.wait_and_throw();}
                q.wait_and_throw();us[arm]=std::chrono::duration<double,std::micro>(std::chrono::steady_clock::now()-start).count()/256;
            }
            std::printf("PAIR {\"type\":\"%s\",\"T\":%d,\"mode\":%d,\"cycle\":%d,\"repack\":%s,\"fused\":%s,\"baseline_cached\":%s,\"baseline_repack\":%s,\"a0_us\":%.9g,\"b0_us\":%.9g,\"b1_us\":%.9g,\"a1_us\":%.9g}\n",
                type,selected_t,selected_mode,cycle,repack?"true":"false",fused?"true":"false",compare_cached?"true":"false",compare_repack?"true":"false",us[0],us[1],us[2],us[3]);
            ++pair_count;
        }
    };
    if (bench_mode>0 && quality) timing(bench_mode,bench_t,repack);
    if (bench_mode==-1) for (int t:{1,8,64,256}) for (int mode=1;mode<=3;++mode)
        if (valid[t][mode]) for (bool cached_repack:{false,true}) timing(mode,t,cached_repack);
    std::printf("SUMMARY {\"checks\":%d,\"timed\":%s,\"repack\":%s}\n",checks,pair_count?"true":"false",repack?"true":"false");
    q.wait_and_throw();sycl::free(y,q);sycl::free(x,q);sycl::free(w,q);
}
} // namespace u4s4

int main(int argc,char** argv) try {
    std::setvbuf(stdout,nullptr,_IONBF,0);
    const bool self=argc==2 && std::string(argv[1])=="--selftest";
    u4s4::require(self || (argc>=6 && std::string(argv[1])=="--matrix"),
        "usage: --selftest | --matrix file fp16|bf16 N K [--native file] [--check-fused | --screen | --bench mode T cached|repack|fused|fused-repack|fused-vs-cached|cached-vs-repack]");
    int mode=0,t=0,n=0,k=0,option=6;bool bf=false;std::string pipeline="cached";const char* native=nullptr;
    auto integer=[](const char* s) {size_t end=0;const int v=std::stoi(s,&end);if (s[end]) throw std::invalid_argument("invalid integer");return v;};
    if (!self) {
        bf=std::string(argv[3])=="bf16";n=integer(argv[4]);k=integer(argv[5]);
        u4s4::require(bf || std::string(argv[3])=="fp16","invalid input type");
        u4s4::require(n>0 && n<=16384 && n%8==0 && k>0 && k<=16384 && k%64==0 && int64_t(n)*k<=16777216,"invalid N/K");
    }
    if (argc>option && std::string(argv[option])=="--native") {
        u4s4::require(argc>option+1,"missing native file");native=argv[option+1];option+=2;
    }
    if (!self && argc!=option) {
        if (argc==option+1 && std::string(argv[option])=="--screen") mode=-1;
        else if (argc==option+1 && std::string(argv[option])=="--check-fused") pipeline="fused";
        else {
            u4s4::require(argc==option+4 && std::string(argv[option])=="--bench","invalid bench arguments");
            mode=integer(argv[option+1]);t=integer(argv[option+2]);pipeline=argv[option+3];
            u4s4::require(mode>=1 && mode<=3 && t>=1 && t<=256 && (pipeline=="cached" || pipeline=="repack" ||
                pipeline=="fused" || pipeline=="fused-repack" || pipeline=="fused-vs-cached" || pipeline=="cached-vs-repack"),"invalid bench mode/T/cache");
        }
    }
    sycl::queue q(sycl::gpu_selector_v,{sycl::property::queue::in_order{},sycl::property::queue::enable_profiling{}});
    const auto name=q.get_device().get_info<sycl::info::device::name>();
    std::printf("DEVICE %s DRIVER %s PROFILING %d\n",name.c_str(),q.get_device().get_info<sycl::info::device::driver_version>().c_str(),
        int(q.has_property<sycl::property::queue::enable_profiling>()));
    if (name.find("A770")==std::string::npos) {std::puts("SKIP A770 U4S4 prototype");return 77;}
    u4s4::selftest(q);
    if (!self) {
        if (bf) u4s4::projection<sycl::ext::oneapi::bfloat16>(q,argv[2],n,k,mode,t,pipeline,native);
        else u4s4::projection<sycl::half>(q,argv[2],n,k,mode,t,pipeline,native);
    }
} catch (const std::exception& e) {std::fprintf(stderr,"FAIL %s\n",e.what());return 1;}
