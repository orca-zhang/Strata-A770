// Real QSA projection weights; changing synthetic activations, no KV cache or attention.
// ponytail: fixed deployed geometry; compare complete projection/normalization windows.
#include <dpct/dpct.hpp>
#include <strata/kernels/native_mmvq.hpp>
#include <strata/kernels/bf16_gemv.hpp>
#include <strata/kernels/native_qsa.hpp>
#include <strata/kernels/native_rope.hpp>
#include <strata/kernels/elementwise.hpp>
#include "runtime_dependencies.hpp"
#include <array>
#include <bit>
#include <chrono>
#include <cmath>
#include <cstring>

namespace {
void require(bool value, const char* message) { if (!value) throw std::runtime_error(message); }
constexpr int K=2560, NH=24, NK=2, HD=256, IH=4, ID=128, G=16;
struct Buffer { float* base; float* data; size_t size; };
}
int main(int argc, char** argv) try {
    if (argc<4 || argc>5 || (argc==5 && std::string(argv[4])!="--bench"))
        throw std::invalid_argument("usage: qsa-schedule-check images fixtures new-output.bin [--bench]");
    require(!std::filesystem::exists(argv[3]), "output already exists");
    std::setvbuf(stdout,nullptr,_IONBF,0); check_runtime_dependencies();
    namespace k=strata::kernels;
    orca::l0::Runtime rt;
    orca::l0::compiler::Program program(rt,std::filesystem::path(argv[1])/"native_mmvq");
    for (const char* name:{"native_bf16","native_qsa","native_rope","elementwise"})
        program.add(std::filesystem::path(argv[1])/name);
    strata::l0::Queue q(rt,program); strata::l0::bind_default(q);
    auto alloc=[&](size_t n) { return static_cast<float*>(rt.allocate(n*4)); };
    auto load=[&](const char* file,size_t size) {
        const auto path=std::filesystem::path(argv[2])/file;
        require(std::filesystem::file_size(path)==size,"weight fixture size mismatch");
        std::vector<char> host(size); std::ifstream in(path,std::ios::binary);
        in.read(host.data(),std::streamsize(size)); require(bool(in),"weight fixture read failed");
        void* result=rt.allocate(size); q.memcpy(result,host.data(),size).wait(); return result;
    };
    void* wq=load("q.iq4_xs",k::native_mmvq_weight_bytes(23,K,NH*2*HD));
    void* wk=load("k.q6_k",k::native_mmvq_weight_bytes(14,K,NK*HD));
    void* wv=load("v.iq4_xs",k::native_mmvq_weight_bytes(23,K,NK*HD));
    auto* wik=static_cast<uint16_t*>(load("ik.bf16",size_t(K)*ID*2));
    auto* wiq=static_cast<uint16_t*>(load("iq.bf16",size_t(K)*IH*ID*2));
    auto* gq=static_cast<float*>(load("q.norm",HD*4));
    auto* gk=static_cast<float*>(load("k.norm",HD*4));
    auto* giq=static_cast<float*>(load("iq.norm",ID*4));
    auto* gik=static_cast<float*>(load("ik.norm",ID*4));
    auto* progress=static_cast<volatile uint32_t*>(rt.allocate(3*4,orca::l0::Memory::host));
    progress[0]=17;progress[1]=0;progress[2]=29;uint32_t epoch=0;
    std::ofstream binary(argv[3],std::ios::binary);require(bool(binary),"cannot create output");
    int checks=0;
    for(int t:{1,4,8}) {
        float* x=alloc(size_t(t)*K);
        q.fill(x,0.25f,size_t(t)*K);
        const size_t qb=k::native_q8_1_bytes(K,t);
        std::array<size_t,8> sizes{size_t(t)*NH*2*HD,size_t(t)*NK*HD,size_t(t)*NK*HD,
            size_t(t)*ID,size_t(t)*IH*ID,size_t(t)*NH*HD,size_t(t)*ID,qb/4};
        std::array<Buffer,8> v;
        for(int i=0;i<8;++i) { auto* base=alloc(sizes[i]+2*G);v[i]={base,base+G,sizes[i]}; }
        void* xq=v[7].data;
        auto positions=[&](int heads) {
            std::vector<int> h(size_t(t)*heads);
            for(size_t i=0;i<h.size();++i)h[i]=1023+int(i/heads)*17;
            auto* p=static_cast<int*>(rt.allocate(h.size()*sizeof(int)));
            q.memcpy(p,h.data(),h.size()*sizeof(int)).wait();return p;
        };
        auto* pq=positions(NH);auto* pk=positions(NK);auto* pi=positions(IH);
        const k::RopeScaling scaling;
        auto ik=[&] {k::bf16_gemv_fp32_mmvf_multi(x,K,wik,v[3].data,ID,K,ID,t,&q);};
        auto key=[&] {k::native_mmvq(14,wk,xq,v[1].data,K,NK*HD,t,&q);};
        auto value=[&] {k::native_mmvq(23,wv,xq,v[2].data,K,NK*HD,t,&q);};
        auto query=[&] {k::native_mmvq(23,wq,xq,v[0].data,K,NH*2*HD,t,&q);};
        auto iq=[&] {k::bf16_gemv_fp32_mmvf_multi(x,K,wiq,v[4].data,IH*ID,K,IH*ID,t,&q);};
        auto normk=[&] {k::native_qsa_rms_norm_rope(v[1].data,HD,gk,v[1].data,t*NK,HD,64,1e-6f,scaling,pk,&q);};
        auto normq=[&] {k::native_qsa_rms_norm_rope(v[0].data,2*HD,gq,v[5].data,t*NH,HD,64,1e-6f,scaling,pq,&q);};
        auto normiq=[&] {k::native_qsa_rms_norm_rope(v[4].data,ID,giq,v[4].data,t*IH,ID,64,1e-6f,scaling,pi,&q);};
        auto finish=[&] {
            k::native_qsa_rms_norm_weighted(v[3].data,gik,v[6].data,ID,t,1e-6f,&q);
            q.ext_oneapi_submit_barrier();k::doorbell_ring(const_cast<uint32_t*>(progress+1),&q);
        };
        auto serial_body=[&] {
            k::native_quantize_q8_1(x,xq,K,t,&q);
            ik();key();value();normk();query();normq();iq();normiq();finish();
        };
        // Materialize first-use constant tables outside the independent phase.
        serial_body();q.wait_and_throw();require(progress[1]==++epoch,"warmup GPU tail mismatch");
        q.begin_recording();serial_body();auto serial=q.end_recording();
        q.begin_recording(true);k::native_quantize_q8_1(x,xq,K,t,&q);
        q.independent([&]{ik();key();value();query();iq();});
        normk();normq();normiq();finish();auto phased=q.end_recording();
        auto run=[&](int mode) {
            if(mode==0)q.replay(serial);else if(mode==1)q.replay(phased);else serial_body();
            q.wait_and_throw();require(progress[0]==17 && progress[2]==29 && progress[1]==++epoch,"GPU tail epoch mismatch");
        };
        std::vector<float> hx(size_t(t)*K);
        auto snapshot=[&] {
            std::vector<uint32_t> out;
            for(int i=0;i<8;++i) {
                size_t at=out.size();out.resize(at+v[i].size+2*G);
                q.memcpy(out.data()+at,v[i].base,(v[i].size+2*G)*4).wait();
                for(int j=0;j<G;++j)require(out[at+j]==std::bit_cast<uint32_t>(-12345.f) && out[out.size()-1-j]==std::bit_cast<uint32_t>(-12345.f),"QSA output guard");
                if(i!=7)for(size_t j=0;j<v[i].size;++j) {
                    float value=std::bit_cast<float>(out[at+G+j]);
                    require(std::isfinite(value) && value!=-12345.f,"non-finite or unwritten QSA output");
                }
            }
            std::vector<float> read_x(hx.size());q.memcpy(read_x.data(),x,read_x.size()*4).wait();
            require(read_x==hx,"projection changed its input");return out;
        };
        auto clear=[&] {for(auto& b:v)q.fill(b.base,-12345.f,b.size+2*G);q.wait_and_throw();};
        std::vector<uint32_t> reference;
        for(int profile=0;profile<3;++profile) {
            for(size_t i=0;i<hx.size();++i)hx[i]=float(int((i*7919+profile*101)%1000)-500)/1024.f;
            q.memcpy(x,hx.data(),hx.size()*4).wait();clear();run(0);reference=snapshot();
            for(int mode:{1,2}) {clear();run(mode);require(snapshot()==reference,"QSA schedule changed output bits or Q8 input");++checks;}
            binary.write(reinterpret_cast<const char*>(reference.data()),std::streamsize(reference.size()*4));require(bool(binary),"output write failed");
        }
        if(argc==5) {
            for(int i=0;i<8;++i){run(0);run(1);}
            for(int repeat=0;repeat<7;++repeat)for(int mode:{0,1,1,0}) {
                auto start=std::chrono::steady_clock::now();for(int i=0;i<8;++i)run(mode);
                double us=std::chrono::duration<double,std::micro>(std::chrono::steady_clock::now()-start).count()/8;
                std::printf("SAMPLE {\"T\":%d,\"repetition\":%d,\"mode\":%d,\"window_us\":%.6f}\n",t,repeat,mode,us);
            }
            for(int mode:{0,1}){run(mode);require(snapshot()==reference,"timed QSA output changed");++checks;}
        }
        std::printf("PASS QSA T=%d: five projections, joined norms/RoPE, bitwise outputs, Q8 input, guards\n",t);
    }
    binary.close();require(bool(binary),"output close failed");
    std::printf("PASS QSA schedule: %d comparisons, %u checked GPU tails\n",checks,epoch);
    check_runtime_dependencies();return 0;
} catch(const std::exception& e) {std::fprintf(stderr,"%s\n",e.what());return 1;}
