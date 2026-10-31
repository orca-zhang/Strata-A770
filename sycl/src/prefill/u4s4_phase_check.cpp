// A770 phase experiment: U4/S4 GEMM for prefill; exact-weight vector GEMV for decode.
// ponytail: reuse the existing standalone checker's pack/reference helpers until a model gate justifies dispatch.
#define main u4s4_projection_checker_main
#include "u4s4_projection_check.cpp"
#undef main
#include "strata/kernels/bf16_gemv.hpp"

namespace phase_check {
using namespace u4s4;
using BF = sycl::ext::oneapi::bfloat16;

// Two output tiles share each A load. Combine radix16 integer terms before scaling.
template<int NC>
void prefill(sycl::queue& q, const Packed& x, const Packed& w, float* out, int t, int n) {
    const int ng=x.groups, nt=(n+8*NC-1)/(8*NC), tiles=(t+7)/8*nt;
    const int xwords=x.words, wwords=w.words, xmetas=x.metas;
    auto* xb=x.bits+guard;auto* wb=w.bits+guard;
    auto* xs=x.scales+guard;auto* ws=w.scales+guard;auto* sums=x.sums+guard;
    q.parallel_for(sycl::nd_range<1>((tiles+1)/2*2,2), [=](sycl::nd_item<1> it) SYCL_ESIMD_KERNEL {
        const int tile=int(it.get_global_id(0));if(tile>=tiles)return;
        const int row=tile/nt*8,col=tile%nt*(8*NC);
        es::simd<float,64> accum[NC];
        #pragma unroll
        for(int c=0;c<NC;++c)accum[c]=0;
        for(int g=0;g<ng;++g) {
            es::simd<uint32_t,64> av[2];
            es::simd<float,64> sa[2];es::simd<int32_t,64> correction[2];
            #pragma unroll
            for(int a=0;a<2;++a) {
                av[a].copy_from(xb+(row/8*ng+g)*64+a*xwords);
                es::simd<float,8> sx;sx.copy_from(xs+(row/8*ng+g)*8+a*xmetas);
                es::simd<int32_t,8> sum;sum.copy_from(sums+(row/8*ng+g)*8+a*xmetas);
                #pragma unroll
                for(int r=0;r<8;++r) {
                    sa[a].select<8,1>(r*8)=float(sx[r]);
                    correction[a].select<8,1>(r*8)=int(sum[r])*136;
                }
            }
            #pragma unroll
            for(int c=0;c<NC;++c) {
                if(col+c*8>=n)continue;
                const int at=((col/8+c)*ng+g);
                es::simd<uint32_t,64> bh,bl;bh.copy_from(wb+at*64);bl.copy_from(wb+at*64+wwords);
                es::simd<float,8> highscale;highscale.copy_from(ws+at*8);
                const es::simd<float,64> sw=highscale.replicate<8>()*.0625f;
                #pragma unroll
                for(int a=0;a<2;++a) {
                    es::simd<int32_t,64> zero=0;
                    auto dot=es::xmx::dpas<8,8,int32_t,int32_t,uint32_t,uint32_t,P::u4,P::s4>(zero,bh,av[a]);
                    dot*=16;
                    dot=es::xmx::dpas<8,8,int32_t,int32_t,uint32_t,uint32_t,P::u4,P::s4>(dot,bl,av[a]);
                    accum[c]+=es::convert<float>(dot-correction[a])*(sa[a]*sw);
                }
            }
        }
        es::simd<uint32_t,64> i(0,1);
        #pragma unroll
        for(int c=0;c<NC;++c) {
            const auto offset=((row+i/8)*n+col+c*8+i%8)*4;
            es::scatter(out,offset,accum[c],(row+i/8<uint32_t(t))&(col+c*8+i%8<uint32_t(n)));
        }
    });
}

// No activation quantization, DPAS or weight requantization on this decode path.
template<int NT>
void gemv(sycl::queue& q,const float* x,const uint16_t* w,float* out,int t,int n,int k) {
    q.parallel_for(sycl::nd_range<1>((n+7)/8*128,128),[=](sycl::nd_item<1> it) [[sycl::reqd_sub_group_size(16)]] {
        const int row=int(it.get_group(0))*8+int(it.get_local_id(0))/16;
        if(row>=n)return;
        const int lane=int(it.get_local_id(0))%16;
        float accum[NT]{};
        for(int off=lane*8;off<k;off+=128) {
            sycl::vec<uint16_t,8> bits;bits.load((size_t(row)*k+off)/8,w);
            #pragma unroll
            for(int token=0;token<NT;++token)if(token<t) {
                sycl::vec<float,8> input;input.load((size_t(token)*k+off)/8,x);
                #pragma unroll
                for(int j=0;j<8;++j)accum[token]=sycl::fma(sycl::bit_cast<float>(uint32_t(bits[j])<<16),input[j],accum[token]);
            }
        }
        const auto sg=it.get_sub_group();
        #pragma unroll
        for(int token=0;token<NT;++token)if(token<t) {
            #pragma unroll
            for(int offset=8;offset;offset/=2)accum[token]+=sycl::permute_group_by_xor(sg,accum[token],offset);
            if(lane==0)out[token*n+row]=accum[token];
        }
    });
}

void compare(const std::vector<float>& ref,const std::vector<float>& got,double bound,const char* phase,int t,int variant,int profile) {
    double error=0,power=0,maximum=0;
    require(ref.size()==got.size(),"output size");
    for(size_t i=0;i<ref.size();++i) {
        require(std::isfinite(got[i])&&std::isfinite(ref[i]),"nonfinite output");
        const double d=double(got[i])-ref[i];error+=d*d;power+=double(ref[i])*ref[i];maximum=std::max(maximum,std::fabs(d));
    }
    const double rel=std::sqrt(error/(power+1e-300));
    std::printf("ERROR {\"phase\":\"%s\",\"T\":%d,\"variant\":%d,\"profile\":%d,\"nrmse\":%.12g,\"max_abs\":%.12g,\"passed\":%s}\n",phase,t,variant,profile,rel,maximum,rel<=bound?"true":"false");
    require(rel<=bound,"output accuracy gate");
}

void run(sycl::queue& q,const char* rawpath,const char* packedpath,int n,int k,const std::string& phase,bool bench) {
    constexpr int MAXT=256,BANKS=8;
    const size_t count=size_t(n)*k;
    std::ifstream f(rawpath,std::ios::binary|std::ios::ate);
    require(f&&f.tellg()==std::streamoff(count*2),"raw BF16 size");f.seekg(0);
    std::vector<uint16_t> raw(count);f.read(reinterpret_cast<char*>(raw.data()),count*2);require(bool(f),"raw BF16 read");
    for(auto bits:raw)require(std::isfinite(float(sycl::bit_cast<BF>(bits))),"finite BF16 weights");
    auto* original_w=alloc<uint16_t>(q,count*BANKS);
    for(int bank=0;bank<BANKS;++bank)q.memcpy(original_w+bank*count,raw.data(),count*2);
    auto* x=alloc<uint16_t>(q,size_t(MAXT)*k);auto* xf=alloc<float>(q,size_t(MAXT)*k);
    auto* y=alloc<float>(q,size_t(MAXT)*n+2*guard);
    Packed activation(q,MAXT,k);
    std::vector<std::unique_ptr<Packed>> weights;
    std::ifstream pf(packedpath,std::ios::binary|std::ios::ate);const auto bytes=pf.tellg();pf.seekg(0);
    uint32_t header[8]{};pf.read(reinterpret_cast<char*>(header),32);
    require(pf&&std::memcmp(header,"U4S4v002",8)==0&&header[2]==uint32_t(n)&&header[3]==uint32_t(k)&&header[4]==2&&header[5]==1&&header[6]==64&&header[7]==0,"native BF16 radix16 header");
    require(bytes==std::streamoff(32+count+count/64*4),"native payload size");
    std::vector<uint32_t> words(count/4);std::vector<float> scales(count/64);
    pf.read(reinterpret_cast<char*>(words.data()),words.size()*4);pf.read(reinterpret_cast<char*>(scales.data()),scales.size()*4);require(bool(pf),"native read");
    for(float s:scales)require(std::isfinite(s)&&s>0,"positive radix scale");
    for(int bank=0;bank<BANKS;++bank) {
        weights.push_back(std::make_unique<Packed>(q,n,k));
        auto& w=*weights.back();q.memcpy(w.bits+guard,words.data(),count);
        q.memcpy(w.scales+guard,scales.data(),scales.size()*4);
        auto low=scales;for(float& s:low)s*=.0625f;
        q.memcpy(w.scales+guard+w.metas,low.data(),low.size()*4).wait_and_throw();
    }
    std::mt19937 rng(271027);std::normal_distribution<float> normal;
    const std::vector<int> shapes=phase=="prefill"?std::vector<int>{8,9,16,40,64,128,256}:std::vector<int>{1,4,6};
    auto launch=[&](int variant,int t,int bank) {
        if(phase=="decode") {
            if(!variant)strata::kernels::bf16_gemv_fp32_mmvf_multi(xf,k,original_w+bank*count,y+guard,n,k,n,t,&q);
            else if(t==1)gemv<1>(q,xf,original_w+bank*count,y+guard,t,n,k);
            else if(t<=4)gemv<4>(q,xf,original_w+bank*count,y+guard,t,n,k);
            else gemv<8>(q,xf,original_w+bank*count,y+guard,t,n,k);
        } else {
            if(!variant)original<BF>(q,x,original_w+bank*count,y+guard,t,n,k);
            else {
                pack<false,2,BF>(q,x,t,activation);
                if(variant==1)product<2,2>(q,activation,*weights[bank],y+guard,t,n);
                else if(variant==2)prefill<1>(q,activation,*weights[bank],y+guard,t,n);
                else prefill<2>(q,activation,*weights[bank],y+guard,t,n);
            }
        }
    };
    for(int profile=0;profile<2;++profile) {
        std::vector<uint16_t> input(size_t(MAXT)*k);std::vector<float> float_input(input.size());
        for(size_t i=0;i<input.size();++i) {
            const BF v(normal(rng)*((profile&&i%256==0)?16.f:1.f));
            input[i]=sycl::bit_cast<uint16_t>(v);float_input[i]=float(v);
        }
        q.memcpy(x,input.data(),input.size()*2);q.memcpy(xf,float_input.data(),float_input.size()*4);
        for(int t:shapes) {
            q.fill(y,-12345.f,size_t(MAXT)*n+2*guard);
            launch(0,t,0);const auto ref=read(q,y+guard,size_t(t)*n);
            for(int row:{0,t-1})for(int col:{0,n-1}) {
                double expected=0,magnitude=0;
                for(int j=0;j<k;++j) {
                    const double v=double(float_input[row*k+j])*float(sycl::bit_cast<BF>(raw[col*k+j]));expected+=v;magnitude+=std::fabs(v);
                }
                require(std::fabs(ref[row*n+col]-expected)<=2e-5*(1+magnitude),"CPU64 reference");
            }
            std::vector<float> reference_packed;
            for(int variant=1;variant<=(phase=="prefill"?3:1);++variant) {
                launch(variant,t,0);const auto got=read(q,y+guard,size_t(t)*n);
                compare(ref,got,phase=="decode"?2e-6:(profile?.10:.05),phase.c_str(),t,variant,profile);
                if(phase=="prefill") {
                    if(variant==1)reference_packed=got;
                    else compare(reference_packed,got,2e-6,"same-quantized",t,variant,profile);
                }
                const auto guarded=read(q,y,size_t(MAXT)*n+2*guard);
                for(int i=0;i<guard;++i)require(guarded[i]==-12345.f,"front output guard");
                for(size_t i=size_t(t)*n+guard;i<guarded.size();++i)require(guarded[i]==-12345.f,"tail/unused output guard");
            }
        }
    }
    activation.check();for(auto& w:weights)w->check();
    if(bench)for(int t:shapes)for(int variant=(phase=="prefill"?2:1);variant<=(phase=="prefill"?3:1);++variant) {
        for(int i=0;i<64;++i){launch(0,t,i%BANKS);launch(variant,t,i%BANKS);}q.wait_and_throw();
        for(int cycle=0;cycle<8;++cycle) {
            double us[4]{};const int order[4]={0,variant,variant,0};
            for(int arm=0;arm<4;++arm) {
                const auto start=std::chrono::steady_clock::now();
                for(int i=0;i<64;++i)launch(order[arm],t,i%BANKS);
                q.wait_and_throw();us[arm]=std::chrono::duration<double,std::micro>(std::chrono::steady_clock::now()-start).count()/64;
            }
            std::printf("PAIR {\"phase\":\"%s\",\"T\":%d,\"variant\":%d,\"cycle\":%d,\"a0_us\":%.9g,\"b0_us\":%.9g,\"b1_us\":%.9g,\"a1_us\":%.9g}\n",phase.c_str(),t,variant,cycle,us[0],us[1],us[2],us[3]);
        }
    }
    std::printf("SUMMARY {\"phase\":\"%s\",\"checks\":%d,\"banks\":8}\n",phase.c_str(),checks);
    q.wait_and_throw();sycl::free(y,q);sycl::free(xf,q);sycl::free(x,q);sycl::free(original_w,q);
}
}

int main(int argc,char** argv)try {
    std::setvbuf(stdout,nullptr,_IONBF,0);
    u4s4::require(argc==6||argc==7,"usage: raw.bf16 radix16.u4 N K prefill|decode [--bench]");
    size_t used=0;const int n=std::stoi(argv[3],&used);u4s4::require(!argv[3][used],"invalid N");
    const int k=std::stoi(argv[4],&used);u4s4::require(!argv[4][used],"invalid K");
    const std::string phase=argv[5];
    u4s4::require(n>0&&n<=16384&&n%8==0&&k>0&&k<=16384&&k%128==0&&int64_t(n)*k<=16777216,"invalid dimensions");
    u4s4::require((phase=="prefill"||phase=="decode")&&(argc==6||std::string(argv[6])=="--bench"),"invalid phase/option");
    sycl::queue q(sycl::gpu_selector_v,{sycl::property::queue::in_order{},sycl::property::queue::enable_profiling{}});
    const auto name=q.get_device().get_info<sycl::info::device::name>();
    std::printf("DEVICE %s DRIVER %s\n",name.c_str(),q.get_device().get_info<sycl::info::device::driver_version>().c_str());
    if(name.find("A770")==std::string::npos)return 77;
    phase_check::run(q,argv[1],argv[2],n,k,phase,argc==7);
}catch(const std::exception& e){std::fprintf(stderr,"FAIL %s\n",e.what());return 1;}
