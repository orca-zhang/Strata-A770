// One check/benchmark for the decode KV writes: scalar, batched and norm/RoPE/Q8-fused.
#include <sycl/sycl.hpp>
#include <dpct/dpct.hpp>
#include <sycl/ext/oneapi/experimental/graph.hpp>
#include "strata/kernels/native_qsa.hpp"
#include "strata/kernels/native_rope.hpp"
#include "strata/kernels/mrope.hpp"
#include "strata/kernels/rope.hpp"
#include "strata/kernels/native_qsa_indexer.hpp"
#include "strata/kernels/kv_q8.hpp"
#include "strata/kernels/kv_q4.hpp"
#include "strata/kernels/kv_stream.hpp"
#include <algorithm>
#include <chrono>
#include <cstdio>
#include <cstring>
#include <functional>
#include <stdexcept>
#include <vector>

namespace k = strata::kernels;
template<class T> struct Dev {
    sycl::queue& q; T* p; size_t n;
    Dev(sycl::queue& q_, size_t n_, bool host=false) : q(q_),
        p(host ? sycl::malloc_host<T>(n_,q) : sycl::malloc_device<T>(n_,q)), n(n_) {
        if (!p) throw std::bad_alloc();
    }
    ~Dev() { sycl::free(p,q); }
    void put(const std::vector<T>& v) { if (v.size()!=n) throw std::runtime_error("upload size"); q.memcpy(p,v.data(),n*sizeof(T)).wait_and_throw(); }
    std::vector<T> get() { std::vector<T> v(n);q.memcpy(v.data(),p,n*sizeof(T)).wait_and_throw();return v; }
};

int main(int argc, char** argv) try {
    auto& q = dpct::get_in_order_queue();
    const bool bench = argc == 3 && std::strcmp(argv[1],"--bench")==0;
    const int mode = bench ? std::stoi(argv[2]) : -1;
    if ((argc != 1 && !bench) || (bench && (mode < 0 || mode > 5)))
        throw std::invalid_argument("usage: kv_steps_parity [--bench 0|1|2|3|4|5]");
    auto s = k::qsa_real_shapes(); s.page_size=bench?512:4;
    auto time_call=[&](const std::function<void()>& call, int fmt, int n) {
        using namespace sycl::ext::oneapi::experimental;
        command_graph graph(q.get_context(), q.get_device());
        graph.begin_recording(q); call(); graph.end_recording(q);
        auto executable = graph.finalize();
        const auto replay = [&] { q.ext_oneapi_graph(executable); };
        for(int i=0;i<512;++i)replay();q.wait_and_throw();
        std::vector<double> times;
        for(int batch=0;batch<9;++batch) {
            auto begin=std::chrono::steady_clock::now();
            for(int i=0;i<2048;++i)replay();q.wait_and_throw();
            times.push_back(std::chrono::duration<double,std::micro>(std::chrono::steady_clock::now()-begin).count()/2048);
        }
        auto ordered=times;std::sort(ordered.begin(),ordered.end());
        std::printf("RESULT {\"mode\":%d,\"format\":%d,\"T\":%d,\"us\":%.9f,\"samples_us\":[",mode,fmt,n,ordered[4]);
        for(size_t i=0;i<times.size();++i)std::printf("%s%.9f",i?",":"",times[i]);
        std::puts("]}");
    };
    if(bench && mode>=4) {
        constexpr int ID=128, MC=64;
        Dev<float> data(q,6*ID), weight(q,ID), tail(q,3*ID), dead(q,ID), pooled(q,(MC/4+1)*ID);
        Dev<int32_t> block(q,1), positions(q,6*7);
        data.put(std::vector<float>(data.n,.75f));weight.put(std::vector<float>(ID,1.f));
        q.memset(tail.p,0,tail.n*4);q.memset(dead.p,0,dead.n*4);q.memset(pooled.p,0,pooled.n*4);block.put({-1});
        std::vector<int32_t> p(positions.n);for(int t=0;t<6;++t)p[t*7]=3+t;positions.put(p);
        const k::QsaIndexerBuffers buffers{tail.p,dead.p,pooled.p,block.p};
        for(int n : {1,4,6}) time_call([&] {
            if(mode==5)k::native_qsa_indexer_append_steps(data.p,positions.p,7,n,0,weight.p,1e-6f,buffers,s,MC,{},&q);
            else for(int t=0;t<n;++t)k::native_qsa_indexer_append(data.p+t*ID,positions.p+t*7,0,weight.p,1e-6f,buffers,s,MC,{},&q);
        },4,n);
        std::puts("PASS indexer benchmark");return 0;
    }
    constexpr int H=2,D=256,STRIDE=7,MAXT=6;
    const int CELLS=bench?1024:64;
    const size_t BYTES=CELLS*H*D*2, SECTOR=BYTES+128, BANK=4*SECTOR;
    Dev<uint8_t> pools(q,2*BANK), mirrors(q,2*BANK,true);
    Dev<float> raw(q,MAXT*H*D), values(q,MAXT*H*D), norm(q,MAXT*H*D), gamma(q,D);
    Dev<int32_t> steps(q,MAXT*STRIDE), pos(q,MAXT*H), table(q,CELLS/s.page_size);
    Dev<float> cos_tab(q,128*32),sin_tab(q,128*32);
    std::vector<float> x(raw.n),v(values.n),g(D);
    for (size_t i=0;i<x.size();++i) {
        x[i]=std::sin(float(i)*.73f)*(1+i%23);
        v[i]=i%64<16 ? 0.f : std::cos(float(i)*.21f)*(i%3==0 ? .00001f : 37.f);
    }
    for(int i=0;i<D;++i)g[i]=.5f+float(i%17)/16;
    raw.put(x);values.put(v);gamma.put(g);
    if(!bench) {
        k::kv_append_steps(nullptr,nullptr,nullptr,nullptr,0,0,nullptr,nullptr,s,nullptr,nullptr);
        k::kv_append_q8_steps(nullptr,nullptr,nullptr,nullptr,nullptr,nullptr,0,nullptr,nullptr,0,0,s,nullptr,nullptr);
        k::kv_append_q4_steps(nullptr,nullptr,nullptr,nullptr,0,0,nullptr,nullptr,s,nullptr,nullptr);
        k::native_qsa_norm_rope_store_q8_steps(nullptr,nullptr,nullptr,nullptr,0,nullptr,0,0,
            nullptr,nullptr,nullptr,nullptr,nullptr,s,0,{},nullptr,nullptr);
        int rejected=0;
        try { k::kv_append_steps(nullptr,nullptr,nullptr,nullptr,0,1,nullptr,nullptr,s,&q,nullptr); }
        catch(const std::invalid_argument&) { ++rejected; }
        try { k::kv_append_q8_steps(nullptr,nullptr,nullptr,nullptr,nullptr,nullptr,0,nullptr,nullptr,0,1,s,&q,nullptr); }
        catch(const std::invalid_argument&) { ++rejected; }
        try { k::kv_append_q4_steps(nullptr,nullptr,nullptr,nullptr,0,1,nullptr,nullptr,s,&q,nullptr); }
        catch(const std::invalid_argument&) { ++rejected; }
        try { k::native_qsa_norm_rope_store_q8_steps(nullptr,nullptr,nullptr,nullptr,0,nullptr,0,1,
            nullptr,nullptr,nullptr,nullptr,nullptr,s,0,{},&q,nullptr); }
        catch(const std::invalid_argument&) { ++rejected; }
        if(rejected!=4)throw std::runtime_error("invalid argument guard");
    }
    auto view = [&](int bank,int fmt) {
        k::KvHostPools p;
        auto* base=bank<2 ? pools.p+bank*BANK : mirrors.p+(bank-2)*BANK;
        auto ptr=[&](int i){return base+i*SECTOR+64;};
        if(fmt==0){p.k_pool=(uint16_t*)ptr(0);p.v_pool=(uint16_t*)ptr(1);}
        if(fmt==1||fmt==3){p.k_q=(int8_t*)ptr(0);p.k_scale=(uint16_t*)ptr(2);}
        if(fmt==1){p.v_q=(int8_t*)ptr(1);p.v_scale=(uint16_t*)ptr(3);}
        if(fmt==2){p.k_q4=ptr(0);p.v_q4=ptr(1);}
        if(fmt==3)p.v_q4=ptr(1);
        return p;
    };
    k::RopeScaling scaling;
    int checks=0;
    for(int fmt=0;fmt<4;++fmt) for(int n : {1,4,6}) {
        if(bench && fmt!=1)continue;
        const int variants=bench?1:3;
        for(int variant=0;variant<variants;++variant) {
            std::vector<int32_t> st(steps.n,12345),ps(pos.n),pt(table.n);
            for(int i=0;i<(int)pt.size();++i)pt[i]=variant==2 ? i%4 : (i*5+3)%pt.size();
            if(variant==1)pt[1]=-1;
            for(int i=0;i<MAXT;++i) {
                st[i*STRIDE+k::kStepPos]=3+i;
                for(int h=0;h<H;++h)ps[i*H+h]=9+i+h;
            }
            steps.put(st);pos.put(ps);table.put(pt);
            const auto a=view(0,fmt), b=view(1,fmt), ha=view(2,fmt), hb=view(3,fmt);
            auto append=[&](bool batch, k::KvHostPools d, k::KvHostPools host, const float* keys) {
                const int count=batch?1:n;
                for(int t=0;t<count;++t) {
                    const auto* stp=steps.p+t*STRIDE;
                    const float* kp=keys+t*H*D;const float* vp=values.p+t*H*D;
                    if(fmt==0) {
                        if(batch)k::kv_append_steps(d.k_pool,d.v_pool,table.p,steps.p,STRIDE,n,keys,values.p,s,&q,&host);
                        else k::kv_append_step(d.k_pool,d.v_pool,table.p,stp,kp,vp,s,&q,&host);
                    } else if(fmt==1) {
                        if(batch)k::kv_append_q8_steps(d.k_q,d.v_q,d.k_scale,d.v_scale,table.p,steps.p,STRIDE,keys,values.p,H*D,n,s,&q,&host);
                        else k::kv_append_q8_step(d.k_q,d.v_q,d.k_scale,d.v_scale,table.p,stp,kp,vp,s,&q,&host);
                    } else if(fmt==2) {
                        if(batch)k::kv_append_q4_steps(d.k_q4,d.v_q4,table.p,steps.p,STRIDE,n,keys,values.p,s,&q,&host);
                        else k::kv_append_q4_step(d.k_q4,d.v_q4,table.p,stp,kp,vp,s,&q,&host);
                    } else {
                        const auto hk=k::kv_hybrid_k_half(host),hv=k::kv_hybrid_v_half(host);
                        if(batch) {
                            k::kv_append_q8_steps(d.k_q,d.k_q,d.k_scale,d.k_scale,table.p,steps.p,STRIDE,keys,keys,H*D,n,s,&q,&hk);
                            k::kv_append_q4_steps(d.v_q4,d.v_q4,table.p,steps.p,STRIDE,n,values.p,values.p,s,&q,&hv);
                        } else {
                            k::kv_append_q8_step(d.k_q,d.k_q,d.k_scale,d.k_scale,table.p,stp,kp,kp,s,&q,&hk);
                            k::kv_append_q4_step(d.v_q4,d.v_q4,table.p,stp,vp,vp,s,&q,&hv);
                        }
                    }
                }
            };
            auto fused=[&]() {
                k::native_qsa_norm_rope_store_q8_steps(raw.p,values.p,gamma.p,pos.p,H,steps.p,STRIDE,n,
                    b.k_q,b.v_q,b.k_scale,b.v_scale,table.p,s,1e-6f,scaling,&q,&hb);
            };
            auto separate=[&]() {
                k::native_qsa_rms_norm_rope(raw.p,D,gamma.p,norm.p,n*H,D,64,1e-6f,scaling,pos.p,&q);
                append(true,a,ha,norm.p);
            };
            auto compare=[&]() {
                auto data=pools.get();const auto host_data=mirrors.get();
                data.insert(data.end(),host_data.begin(),host_data.end());
                if(std::memcmp(data.data(),data.data()+BANK,BANK) ||
                   std::memcmp(data.data()+2*BANK,data.data()+3*BANK,BANK))
                    throw std::runtime_error("KV bytes/scales/unused storage differ: fmt="+std::to_string(fmt)+" T="+std::to_string(n));
                for(int bank=0;bank<4;++bank)for(int part=0;part<4;++part)for(int j=0;j<64;++j)
                    if(data[bank*BANK+part*SECTOR+j]!=0x5a || data[bank*BANK+part*SECTOR+64+BYTES+j]!=0x5a)
                        throw std::runtime_error("KV canary changed");
                ++checks;
            };
            auto reset=[&](){q.memset(pools.p,0x5a,pools.n);q.memset(mirrors.p,0x5a,mirrors.n).wait_and_throw();};
            reset();
            append(false,a,ha,raw.p);append(true,b,hb,raw.p);compare();
            if(fmt==1)for(int scale=0;scale<3;++scale) {
                scaling.type=scale==0?k::RopeScalingType::None:scale==1?k::RopeScalingType::Linear:k::RopeScalingType::YaRN;
                scaling.factor=4;scaling.ext_factor=scale==2?1:0;
                std::vector<float> hc(cos_tab.n),hs(sin_tab.n);
                k::build_rope_table(64,scaling,128,hc.data(),hs.data());cos_tab.put(hc);sin_tab.put(hs);
                k::rope_table_set(cos_tab.p,sin_tab.p,128,scaling);
                reset();separate();fused();compare();
            }
            using namespace sycl::ext::oneapi::experimental;
            // Exercise the batched Q8 graph itself as well as the independent fused reference.
            if(!bench) {
                for(int fused_replay=0; fused_replay<(fmt==1?2:1); ++fused_replay) {
                    command_graph graph(q.get_context(),q.get_device());
                    graph.begin_recording(q);
                    if(fmt==1 && fused_replay)fused();else append(true,b,hb,raw.p);
                    graph.end_recording(q);auto executable=graph.finalize();
                    for(int replay=0;replay<3;++replay) {
                        for(int i=0;i<MAXT;++i){st[i*STRIDE]=15+replay*7+i;for(int h=0;h<H;++h)ps[i*H+h]=31+replay*7+i+h;}
                        steps.put(st);pos.put(ps);reset();
                        if(fmt==1 && fused_replay)separate();else append(false,a,ha,raw.p);
                        q.ext_oneapi_graph(executable).wait_and_throw();compare();
                    }
                }
            } else {
                std::function<void()> call = mode==0 ? std::function<void()>([&]{append(false,a,{},raw.p);}) :
                    mode==1 ? std::function<void()>([&]{append(true,b,{},raw.p);}) :
                    mode==2 ? std::function<void()>(separate) : std::function<void()>(fused);
                time_call(call,fmt,n);
            }
            k::rope_table_release(cos_tab.p);
        }
    }
    if(raw.get()!=x || values.get()!=v || gamma.get()!=g)throw std::runtime_error("read-only input changed");
    std::printf("PASS kv_steps: %d byte/scales/mirror/canary/graph checks\n",checks);
    return 0;
} catch(const std::exception& e) { std::fprintf(stderr,"FAIL %s\n",e.what());return 1; }
