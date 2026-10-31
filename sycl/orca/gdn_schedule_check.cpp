// Real IQ4_XS/IQ3_S GDN projections, synthetic activations/state and BF16 alpha/beta.
// ponytail: fixed deployed geometry; this checks scheduling and scratch reuse, not model quality.
#include <dpct/dpct.hpp>
#include <strata/kernels/native_mmvq.hpp>
#include <strata/kernels/verify_kernels.hpp>
#include <strata/kernels/elementwise.hpp>
#include <strata/kernels/bf16_bits.hpp>
#include "runtime_dependencies.hpp"
#include <array>
#include <bit>
#include <chrono>
#include <cmath>
#include <cstring>

namespace {
void require(bool value, const char* message) { if (!value) throw std::runtime_error(message); }
constexpr int K=2560, C=10240, HK=16, HV=48, S=128, Z=HV*S, G=16;
struct Buffer { float* base; float* data; size_t size; };
}
int main(int argc,char** argv) try {
    if (argc<5 || argc>6 || (argc==6 && std::string(argv[5])!="--bench"))
        throw std::invalid_argument("usage: gdn-schedule-check images qkv.iq4_xs gate.iq3_s new-output.bin [--bench]");
    require(!std::filesystem::exists(argv[4]), "output already exists");
    std::setvbuf(stdout,nullptr,_IONBF,0); check_runtime_dependencies();
    namespace k=strata::kernels;
    orca::l0::Runtime rt; orca::l0::compiler::Program program(rt,std::filesystem::path(argv[1])/"native_mmvq");
    for (const char* name:{"iq_kernels","verify_kernels","elementwise"}) program.add(std::filesystem::path(argv[1])/name);
    strata::l0::Queue q(rt,program); strata::l0::bind_default(q);
    auto alloc=[&](size_t n) { return static_cast<float*>(rt.allocate(n*4)); };
    auto weights=[&](const char* file,int type,int rows) {
        const size_t size=k::native_mmvq_weight_bytes(type,K,rows);
        require(std::filesystem::file_size(file)==size,"weight fixture size mismatch");
        std::vector<char> host(size); std::ifstream in(file,std::ios::binary);
        in.read(host.data(),std::streamsize(size)); require(bool(in),"weight fixture read failed");
        void* result=rt.allocate(size); q.memcpy(result,host.data(),size).wait(); return result;
    };
    void* wq=weights(argv[2],23,C); void* wz=weights(argv[3],21,Z);
    float* state=alloc(S*HV*S),*history=alloc(3*C),*conv=alloc(4*C),*dt=alloc(HV),*a=alloc(HV),*gamma=alloc(Z);
    q.fill(state,0.001f,S*HV*S);q.fill(history,0.01f,3*C);q.fill(conv,0.25f,4*C);
    q.fill(dt,-2.f,HV);q.fill(a,0.f,HV);q.fill(gamma,1.f,Z);
    std::vector<uint16_t> hw(size_t(2)*HV*K);
    for(size_t i=0;i<hw.size();++i) hw[i]=k::bf16_from_f32(float(int(i*7919%31)-15)/128.f);
    auto* wa=static_cast<uint16_t*>(rt.allocate(hw.size()*2));q.memcpy(wa,hw.data(),hw.size()*2).wait();
    auto* progress=static_cast<volatile uint32_t*>(rt.allocate(3*4,orca::l0::Memory::host));
    progress[0]=17;progress[1]=0;progress[2]=29;uint32_t epoch=0;
    std::ofstream binary(argv[4],std::ios::binary);require(bool(binary),"cannot create output");
    int checks=0;
    for(int t:{1,4,8}) for(bool tail:{false,true}) {
        float* x=alloc(size_t(t)*K);
        const size_t qb=k::native_q8_1_bytes(Z,t);
        std::array<size_t,7> sizes{size_t(t)*C,size_t(t)*C,size_t(t)*HV,size_t(t)*HV,size_t(t)*Z,size_t(t)*Z,qb/4};
        std::array<Buffer,7> v;
        for(int i=0;i<7;++i) { auto* base=alloc(sizes[i]+2*G);v[i]={base,base+G,sizes[i]}; }
        void* xq=v[6].data;
        auto products=[&] {
            k::native_mmvq(23,wq,xq,v[0].data,K,C,t,&q);
            k::gdn_ab_multi(x,wa,wa+size_t(HV)*K,dt,a,v[2].data,v[3].data,K,HV,t,&q);
            k::native_mmvq(21,wz,xq,v[4].data,K,Z,t,&q);
        };
        auto convolution=[&] { k::gdn_conv_l2_multi(history,v[0].data,conv,v[1].data,C,2*HK,1e-6f,t,&q); };
        auto finish=[&] {
            k::gdn_step_norm_multi(state,v[1].data,C,v[2].data,v[3].data,v[4].data,gamma,1e-6f,v[5].data,HK,HV,t,nullptr,&q,0,tail?xq:nullptr);
            if(!tail) k::native_quantize_q8_1(v[5].data,xq,Z,t,&q);
            q.ext_oneapi_submit_barrier();k::doorbell_ring(const_cast<uint32_t*>(progress+1),&q);
        };
        auto serial_body=[&] {
            k::native_quantize_q8_1(x,xq,K,t,&q);
            k::native_mmvq(23,wq,xq,v[0].data,K,C,t,&q);convolution();
            k::gdn_ab_multi(x,wa,wa+size_t(HV)*K,dt,a,v[2].data,v[3].data,K,HV,t,&q);
            k::native_mmvq(21,wz,xq,v[4].data,K,Z,t,&q);finish();
        };
        q.begin_recording();serial_body();auto serial=q.end_recording();
        q.begin_recording(true);k::native_quantize_q8_1(x,xq,K,t,&q);
        q.independent(products);convolution();finish();auto phased=q.end_recording();
        auto run=[&](int mode) {
            if(mode==0)q.replay(serial);else if(mode==1)q.replay(phased);else serial_body();
            q.wait_and_throw();require(progress[0]==17 && progress[2]==29 && progress[1]==++epoch,"GPU tail epoch mismatch");
        };
        auto snapshot=[&] {
            std::vector<uint32_t> out;
            for(int i=0;i<7;++i) {
                size_t at=out.size();out.resize(at+v[i].size+2*G);
                q.memcpy(out.data()+at,v[i].base,(v[i].size+2*G)*4).wait();
                for(int j=0;j<G;++j)require(out[at+j]==std::bit_cast<uint32_t>(-12345.f) && out[out.size()-1-j]==std::bit_cast<uint32_t>(-12345.f),"GDN output guard");
                if(i!=6)for(size_t j=0;j<v[i].size;++j) {
                    const float value=std::bit_cast<float>(out[at+G+j]);
                    require(std::isfinite(value) && value!=-12345.f,"non-finite or unwritten GDN output");
                }
            }
            std::vector<float> state_check(S*HV*S),history_check(3*C);
            q.memcpy(state_check.data(),state,state_check.size()*4);q.memcpy(history_check.data(),history,history_check.size()*4);q.wait_and_throw();
            for(float value:state_check)require(value==0.001f,"verify modified recurrence state");
            for(float value:history_check)require(value==0.01f,"verify modified convolution history");
            return out;
        };
        auto clear=[&] { for(auto& b:v)q.fill(b.base,-12345.f,b.size+2*G);q.wait_and_throw(); };
        std::vector<uint32_t> reference;
        for(int profile=0;profile<3;++profile) {
            std::vector<float> hx(size_t(t)*K);
            for(size_t i=0;i<hx.size();++i)hx[i]=float(int((i*7919+profile*101)%1000)-500)/1024.f;
            q.memcpy(x,hx.data(),hx.size()*4).wait();clear();run(0);reference=snapshot();
            for(int mode:{1,2}) { clear();run(mode);require(snapshot()==reference,"GDN schedule changed output bits");++checks; }
            binary.write(reinterpret_cast<const char*>(reference.data()),std::streamsize(reference.size()*4));require(bool(binary),"output write failed");
        }
        if(argc==6) {
            for(int i=0;i<8;++i){run(0);run(1);}
            for(int repeat=0;repeat<7;++repeat)for(int mode:{0,1,1,0}) {
                auto start=std::chrono::steady_clock::now();for(int i=0;i<8;++i)run(mode);
                double us=std::chrono::duration<double,std::micro>(std::chrono::steady_clock::now()-start).count()/8;
                std::printf("SAMPLE {\"T\":%d,\"tail\":%d,\"repetition\":%d,\"mode\":%d,\"window_us\":%.6f}\n",t,int(tail),repeat,mode,us);
            }
            for(int mode:{0,1}){run(mode);require(snapshot()==reference,"timed GDN output changed");++checks;}
        }
        std::printf("PASS GDN T=%d tail=%d: complete bitwise outputs, Q8 scratch, guards, immutable state/history\n",t,int(tail));
    }
    std::printf("PASS GDN schedule: %d comparisons, %u checked GPU tails\n",checks,epoch);
    check_runtime_dependencies();return 0;
} catch(const std::exception& e) { std::fprintf(stderr,"%s\n",e.what());return 1; }
