// Full-buffer equality for real shared weights and changing routed plans.
// ponytail: fixed A770 geometry, two deployed expert formats, no test framework.
#include <dpct/dpct.hpp>
#include <strata/kernels/shared_expert.hpp>
#include <strata/kernels/iq_kernels.hpp>
#include <strata/kernels/native_mmvq.hpp>
#include <strata/kernels/elementwise.hpp>
#include <runtime_dependencies.hpp>
#include <array>
#include <bit>
#include <cmath>
#include <cstdio>
#include <cstring>
#include <filesystem>
#include <fstream>
#include <numeric>
#include <random>
#include <stdexcept>

namespace k = strata::kernels;
void require(bool yes,const char* why) { if(!yes)throw std::runtime_error(why); }
constexpr int K=2560,F=640,GUARD=16;
struct Buffer { float* base;float* data;size_t count; };
int main(int argc,char** argv) try {
    if(argc!=3)throw std::invalid_argument("usage: expert-overlap-check modules shared-weight-fixtures");
#ifdef STRATA_ORCA_DG2
    check_runtime_dependencies(true);
#else
    check_runtime_dependencies(false);
#endif
    std::setvbuf(stdout,nullptr,_IONBF,0);
    strata::l0::Runtime rt;
    strata::l0::Program program(rt,std::filesystem::path(argv[1])/"native_mmvq");
    for(const char* n:{"iq_kernels","native_bf16","shared_expert","elementwise"})program.add(std::filesystem::path(argv[1])/n);
    strata::l0::Queue q(rt,program);strata::l0::bind_default(q);
    auto upload=[&](const void* p,size_t n) { auto* d=rt.allocate(n);q.memcpy(d,p,n).wait();return d; };
    auto load=[&](const char* file,size_t n) {
        auto path=std::filesystem::path(argv[2])/file;require(std::filesystem::file_size(path)==n,"fixture size");
        std::vector<char> host(n);std::ifstream in(path,std::ios::binary);in.read(host.data(),std::streamsize(n));
        require(bool(in),"fixture read");return upload(host.data(),n);
    };
    k::NativeSharedWeights nw;
    nw.gate_type=21;nw.gate_data=load("gate.iq3_s",k::native_mmvq_weight_bytes(21,K,F));
    nw.up_type=23;nw.up_data=load("up.iq4_xs",k::native_mmvq_weight_bytes(23,K,F));
    nw.down_type=20;nw.down_data=load("down.iq4_nl",k::native_mmvq_weight_bytes(20,F,K));
    auto* scalar=static_cast<uint16_t*>(load("scalar.bf16",K*2));
    k::shared_expert_set_native_bf16(true);k::native_grouped_set_v1(false);
    auto* epoch=static_cast<uint32_t*>(rt.allocate(4,strata::l0::Memory::host));*epoch=0;uint32_t expected=0;
    std::mt19937 rng(47159);int checks=0;
    for(int gu:{22,42})for(int t:{1,2,3,4,5,6,7,8})for(bool ready:{false,true}) {
        const int cap=t*10;
        auto layout=k::native_expert_layout(gu,42,K,F);
        std::vector<uint8_t> blob(3*layout.bytes);
        for(auto& v:blob)v=uint8_t(rng());
        for(int b=0;b<3;++b)for(int stage=0;stage<3;++stage) {
            const int type=stage==2?42:gu,values=type==42?64:256;
            const size_t bytes=k::iq_row_bytes(type,values);
            const size_t at=size_t(b)*layout.bytes+(stage==0?0:stage==1?layout.up_off:layout.down_off);
            const size_t end=size_t(b)*layout.bytes+(stage==2?layout.bytes:stage==0?layout.up_off:layout.down_off);
            for(size_t i=at;i<end;i+=bytes) { uint16_t scale=uint16_t(0x1400+(rng()&1023));std::memcpy(blob.data()+i,&scale,2); }
        }
        auto* weights=static_cast<uint8_t*>(upload(blob.data(),blob.size()));
        std::vector<unsigned long long> hp{(unsigned long long)weights,(unsigned long long)(weights+layout.bytes),(unsigned long long)(weights+2*layout.bytes)};
        auto* ptr=static_cast<unsigned long long*>(upload(hp.data(),hp.size()*8));
        auto* starts=static_cast<int32_t*>(rt.allocate((cap+1)*4));
        auto* counts=static_cast<int32_t*>(rt.allocate(4));
        auto* dst=static_cast<int32_t*>(rt.allocate(cap*4));auto* tok=static_cast<int32_t*>(rt.allocate(cap*4));
        std::vector<int32_t> hs(cap+1),hd(cap),ht(cap);
        const size_t qb=k::native_q8_1_bytes(K,t),scratch=k::native_expert_scratch_bytes(cap,F);
        std::array<size_t,9> sizes{size_t(t)*F,size_t(t)*F,size_t(t),size_t(t)*K,qb/4,qb/4,scratch/4,size_t(cap)*K,size_t(t)*K};
        std::array<Buffer,9> buffers;
        for(size_t i=0;i<buffers.size();++i) { auto* p=static_cast<float*>(rt.allocate((sizes[i]+2*GUARD)*4));buffers[i]={p,p+GUARD,sizes[i]}; }
        auto* x=buffers[8].data;nw.q8_1=buffers[4].data;
        const bool staged=!ready||t<=4;
        auto shared=[&](int phase, bool raw = false) { k::shared_expert_multi(t,x,nullptr,nw,scalar,buffers[0].data,buffers[1].data,buffers[2].data,
            buffers[3].data,K,F,&q,ready?buffers[5].data:raw?buffers[4].data:nullptr,0,phase); };
        auto routed=[&](int phase) { k::native_expert_grouped(layout,ptr,starts,counts,dst,tok,cap,cap,buffers[5].data,
            buffers[6].data,buffers[7].data,&q,0,phase); };
        // Rejection must happen before recording an unsupported phase.
        for(int phase:{-1,6}) {
            bool rejected=false;try { shared(phase); }catch(const std::invalid_argument&) { rejected=true; }
            require(rejected,"unsupported shared phase accepted");
        }
        for(int phase:{-1,4}) {
            bool rejected=false;try { routed(phase); }catch(const std::invalid_argument&) { rejected=true; }
            require(rejected,"unsupported routed phase accepted");
        }
        k::native_grouped_set_v1(true);
        bool rejected_v1=false;try { routed(2); }catch(const std::invalid_argument&) { rejected_v1=true; }
        k::native_grouped_set_v1(false);require(rejected_v1,"dependent grouped v1 stage accepted");
        auto body=[&](int mode) {
            k::native_quantize_q8_1(x,buffers[5].data,K,t,&q);
            if(!staged||mode==0) { shared(0);routed(0); }
            else if(mode==3 && t<=4) {
                if(!ready)k::native_quantize_q8_1(x,buffers[4].data,K,t,&q);
                q.independent([&]{shared(5,true);routed(1);});
                q.independent([&]{shared(2,true);routed(2);});
                q.independent([&]{shared(3,true);routed(3);});
                shared(4,true);
            } else {
                shared(1);
                if(mode==1) { shared(2);routed(1);shared(3);routed(2);shared(4);routed(3); }
                else {
                    q.independent([&]{shared(2);routed(1);});
                    q.independent([&]{shared(3);routed(2);});
                    q.independent([&]{shared(4);routed(3);});
                }
            }
            q.ext_oneapi_submit_barrier();k::doorbell_ring(epoch,&q);
        };
        // Warm constant-table uploads before a stage can enter independent().
        q.fill(x,0.25f,size_t(t)*K);q.fill(counts,0,1);q.fill(starts,0,size_t(cap)+1);
        body(0);q.wait_and_throw();require(*epoch==++expected,"warmup tail");
        std::array<std::shared_ptr<strata::l0::NativeGraph>,4> graphs;
        for(int m=0;m<4;++m) { q.begin_recording(true);body(m);graphs[m]=q.end_recording(); }
        std::vector<float> hx(size_t(t)*K);int active_entries=0;
        auto clear=[&] {
            for(auto& b:buffers)q.fill(b.base,-12345.f,b.count+2*GUARD);
            q.memcpy(x,hx.data(),hx.size()*4).wait();
        };
        auto snapshot=[&] {
            std::vector<uint32_t> words;
            for(size_t i=0;i<buffers.size();++i) {
                auto& b=buffers[i];size_t at=words.size();words.resize(at+b.count+2*GUARD);
                q.memcpy(words.data()+at,b.base,(b.count+2*GUARD)*4).wait();
                for(int j=0;j<GUARD;++j)require(words[at+j]==std::bit_cast<uint32_t>(-12345.f)&&
                    words[words.size()-1-j]==std::bit_cast<uint32_t>(-12345.f),"output or scratch guard");
                if(i<4)for(size_t j=0;j<b.count;++j) {float v=std::bit_cast<float>(words[at+GUARD+j]);require(std::isfinite(v)&&v!=-12345.f,"shared nonfinite/unwritten");}
                if(i==7)for(int e=0;e<cap;++e)for(int j=0;j<K;++j) {
                    float v=std::bit_cast<float>(words[at+GUARD+size_t(cap-1-e)*K+j]);
                    require(e<active_entries?(std::isfinite(v)&&v!=-12345.f):v==-12345.f,"routed active/inactive rows");
                }
                if(i==8)require(std::memcmp(words.data()+at+GUARD,hx.data(),hx.size()*4)==0,"changed input");
            }
            return words;
        };
        for(int pattern=0;pattern<6;++pattern) {
            for(size_t i=0;i<hx.size();++i)hx[i]=float(int((i*7919+pattern*101)%1000)-500)/2048.f;
            const int groups=pattern==0?0:pattern%3+1,entries=pattern==0?0:pattern==5?cap:cap-pattern%3;
            active_entries=entries;
            for(int i=0;i<=cap;++i)hs[i]=i<=groups?(groups?i*entries/groups:0):entries;
            for(int i=0;i<cap;++i) {hd[i]=cap-1-i;ht[i]=(i*3+pattern)%t;}
            q.memcpy(starts,hs.data(),hs.size()*4);q.memcpy(dst,hd.data(),hd.size()*4);q.memcpy(tok,ht.data(),ht.size()*4);q.memcpy(counts,&groups,4).wait();
            clear();q.replay(graphs[0]);q.wait_and_throw();require(*epoch==++expected,"serial tail");auto reference=snapshot();
            for(int mode:{1,2,3})for(int replay=0;replay<2;++replay) {
                clear();q.replay(graphs[mode]);q.wait_and_throw();require(*epoch==++expected,"staged tail");
                require(snapshot()==reference,"stage schedule changed full output/scratch bits");++checks;
            }
        }
        graphs={};q.wait_and_throw();
        for(auto& b:buffers)rt.deallocate(b.base);
        for(void* p:{static_cast<void*>(weights),static_cast<void*>(ptr),static_cast<void*>(starts),static_cast<void*>(counts),static_cast<void*>(dst),static_cast<void*>(tok)})rt.deallocate(p);
        std::printf("PASS gu=%d T=%d ready=%d staged=%d changing-plans=6 full-buffer-checks=36\n",gu,t,int(ready),int(staged));
    }
    require(checks==1152,"check count");std::printf("PASS %d exact full-buffer/guard checks and %u GPU tail epochs\n",checks,expected);
    return 0;
} catch(const std::exception& e) { std::fprintf(stderr,"FAIL %s\n",e.what());return 1; }
