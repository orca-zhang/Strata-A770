// Real shared-expert weights; compare the complete joined chain, including scratch reuse.
// ponytail: fixed deployed geometry, existing numerical kernels and no test framework.
#include <dpct/dpct.hpp>
#include <strata/kernels/shared_expert.hpp>
#include <strata/kernels/native_mmvq.hpp>
#include <strata/kernels/elementwise.hpp>
#include "runtime_dependencies.hpp"
#include <array>
#include <bit>
#include <chrono>
#include <cmath>

namespace {
void require(bool value, const char* message) { if (!value) throw std::runtime_error(message); }
constexpr int K=2560, F=640, G=16;
struct Buffer { float* base; float* data; size_t size; };
}
int main(int argc, char** argv) try {
    if (argc<4 || argc>5 || (argc==5 && std::string(argv[4])!="--bench"))
        throw std::invalid_argument("usage: shared-schedule-check images fixtures new-output.bin [--bench]");
    require(!std::filesystem::exists(argv[3]),"output already exists");
    std::setvbuf(stdout,nullptr,_IONBF,0); check_runtime_dependencies();
    namespace k=strata::kernels;
    orca::l0::Runtime rt;
    orca::l0::compiler::Program program(rt,std::filesystem::path(argv[1])/"native_mmvq");
    for (const char* name:{"iq_kernels","native_bf16","shared_expert","elementwise"})
        program.add(std::filesystem::path(argv[1])/name);
    strata::l0::Queue q(rt,program); strata::l0::bind_default(q);
    auto load=[&](const char* file,size_t size) {
        auto path=std::filesystem::path(argv[2])/file;
        require(std::filesystem::file_size(path)==size,"weight fixture size mismatch");
        std::vector<char> host(size);std::ifstream in(path,std::ios::binary);
        in.read(host.data(),std::streamsize(size));require(bool(in),"weight fixture read failed");
        auto* p=rt.allocate(size);q.memcpy(p,host.data(),size).wait();return p;
    };
    k::NativeSharedWeights nw;
    nw.gate_type=21;nw.gate_data=load("gate.iq3_s",k::native_mmvq_weight_bytes(21,K,F));
    nw.up_type=23;nw.up_data=load("up.iq4_xs",k::native_mmvq_weight_bytes(23,K,F));
    nw.down_type=20;nw.down_data=load("down.iq4_nl",k::native_mmvq_weight_bytes(20,F,K));
    auto* scalar=static_cast<uint16_t*>(load("scalar.bf16",K*2));
    k::shared_expert_set_native_bf16(true);
    auto* progress=static_cast<volatile uint32_t*>(rt.allocate(3*4,orca::l0::Memory::host));
    progress[0]=17;progress[1]=0;progress[2]=29;uint32_t epoch=0;
    std::ofstream binary(argv[3],std::ios::binary);require(bool(binary),"cannot create output");
    int checks=0;
    for (int t:{1,4,8}) for (bool ready:{false,true}) {
        auto* x=static_cast<float*>(rt.allocate(size_t(t)*K*4));q.fill(x,0.25f,size_t(t)*K);
        const size_t qb=k::native_q8_1_bytes(K,t);
        std::array<size_t,6> sizes{size_t(t)*F,size_t(t)*F,size_t(t),size_t(t)*K,qb/4,qb/4};
        std::array<Buffer,6> v;
        for (int i=0;i<6;++i) {
            auto* p=static_cast<float*>(rt.allocate((sizes[i]+2*G)*4));v[i]={p,p+G,sizes[i]};
        }
        nw.q8_1=v[4].data;
        auto body=[&] {
            if (ready) k::native_quantize_q8_1(x,v[5].data,K,t,&q);
            k::shared_expert_multi(t,x,nullptr,nw,scalar,v[0].data,v[1].data,v[2].data,v[3].data,K,F,&q,
                                  ready?v[5].data:nullptr);
            q.ext_oneapi_submit_barrier();k::doorbell_ring(const_cast<uint32_t*>(progress+1),&q);
        };
        // Materialize constant tables outside the independent phase.
        body();q.wait_and_throw();require(progress[1]==++epoch,"warmup GPU tail mismatch");
        q.begin_recording();body();auto serial=q.end_recording();
        q.begin_recording(true);body();auto phased=q.end_recording();
        auto run=[&](int mode) {
            if(mode==0)q.replay(serial);else if(mode==1)q.replay(phased);else body();
            q.wait_and_throw();require(progress[0]==17 && progress[2]==29 && progress[1]==++epoch,"GPU tail epoch mismatch");
        };
        std::vector<float> hx(size_t(t)*K);
        auto snapshot=[&] {
            std::vector<uint32_t> out;
            for (int i=0;i<6;++i) {
                size_t at=out.size();out.resize(at+v[i].size+2*G);
                q.memcpy(out.data()+at,v[i].base,(v[i].size+2*G)*4).wait();
                for(int j=0;j<G;++j)require(out[at+j]==std::bit_cast<uint32_t>(-12345.f) &&
                    out[out.size()-1-j]==std::bit_cast<uint32_t>(-12345.f),"shared output guard");
                if(i<4)for(size_t j=0;j<v[i].size;++j) {
                    float value=std::bit_cast<float>(out[at+G+j]);
                    require(std::isfinite(value) && value!=-12345.f,"non-finite or unwritten shared output");
                }
            }
            std::vector<float> read_x(hx.size());q.memcpy(read_x.data(),x,read_x.size()*4).wait();
            require(read_x==hx,"shared expert changed its input");return out;
        };
        auto clear=[&] {for(auto& b:v)q.fill(b.base,-12345.f,b.size+2*G);q.wait_and_throw();};
        std::vector<uint32_t> reference;
        for(int profile=0;profile<3;++profile) {
            for(size_t i=0;i<hx.size();++i)hx[i]=float(int((i*7919+profile*101)%1000)-500)/1024.f;
            q.memcpy(x,hx.data(),hx.size()*4).wait();clear();run(0);reference=snapshot();
            for(int mode:{1,2}){clear();run(mode);require(snapshot()==reference,"shared schedule changed output or scratch bits");++checks;}
            binary.write(reinterpret_cast<const char*>(reference.data()),std::streamsize(reference.size()*4));
            require(bool(binary),"output write failed");
        }
        if(argc==5) {
            for(int i=0;i<8;++i){run(0);run(1);}
            for(int repeat=0;repeat<7;++repeat)for(int mode:{0,1,1,0}) {
                auto start=std::chrono::steady_clock::now();for(int i=0;i<8;++i)run(mode);
                double us=std::chrono::duration<double,std::micro>(std::chrono::steady_clock::now()-start).count()/8;
                std::printf("SAMPLE {\"T\":%d,\"ready\":%d,\"repetition\":%d,\"mode\":%d,\"window_us\":%.6f}\n",t,int(ready),repeat,mode,us);
            }
            for(int mode:{0,1}){run(mode);require(snapshot()==reference,"timed shared output changed");++checks;}
        }
        std::printf("PASS shared T=%d ready=%d: gate/up/scalar, joined SwiGLU/down/scale, outputs and scratch\n",t,int(ready));
    }
    binary.close();require(bool(binary),"output close failed");
    std::printf("PASS shared schedule: %d comparisons, %u checked GPU tails\n",checks,epoch);
    check_runtime_dependencies();return 0;
} catch(const std::exception& e) {std::fprintf(stderr,"%s\n",e.what());return 1;}
