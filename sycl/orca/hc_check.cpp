// HC eager/replay check: independent CPU FP64 reference, separate Q8 quantizer and full guards.
#include <dpct/dpct.hpp>
#include <strata/kernels/fused_gr.hpp>
#include <strata/kernels/native_mmvq.hpp>
#include <algorithm>
#include <cmath>
#include <cstdio>
#include <cstring>
#include <limits>
#include <vector>

int main(int argc,char** argv) try {
    if (argc!=2) throw std::invalid_argument("usage: hc1-replay image-directory");
    namespace k=strata::kernels;
    constexpr int N=2560,HC=4,D=N*HC,LR=320,G=64,REPEAT=1;
    constexpr uint32_t poison=0x7fc055aa;
    auto bf16=[](float f) { uint32_t u;std::memcpy(&u,&f,4);return uint16_t(u>>16); };
    auto decode=[](uint16_t v) { uint32_t u=uint32_t(v)<<16;float f;std::memcpy(&f,&u,4);return double(f); };
    uint32_t rng=12345;
    auto rnd=[&] { rng=rng*1664525u+1013904223u;return float((rng>>8)&0xffff)/65536.f-0.5f; };
    std::vector<uint16_t> wd(size_t(LR)*D+2*G,0x5555),wu(size_t(D)*LR+2*G,0x5555),wi(size_t(HC)*D+2*G,0x5555);
    for (auto* h:{&wd,&wu,&wi}) for (size_t i=G;i<h->size()-G;++i) (*h)[i]=bf16(rnd()*0.05f);
    std::vector<float> hn(D+2*G,-12345.f);
    for (int i=0;i<D;++i) hn[G+i]=1.f+0.1f*rnd();
    strata::l0::Runtime runtime;
    const std::filesystem::path images(argv[1]);
    strata::l0::Program program(runtime,images/"elementwise");
    program.add(images/"fused_gr");program.add(images/"native_mmvq");
    strata::l0::Queue q(runtime,program);strata::l0::bind_default(q);
    auto upload=[&](const auto& h) {
        auto* p=runtime.allocate(h.size()*sizeof(h[0]),strata::l0::Memory::device);
        q.memcpy(p,h.data(),h.size()*sizeof(h[0])).wait_and_throw();return p;
    };
    auto unchanged=[&](const auto& h,void* p) {
        std::vector<unsigned char> bytes(h.size()*sizeof(h[0]));
        q.memcpy(bytes.data(),p,bytes.size()).wait_and_throw();
        if (std::memcmp(bytes.data(),h.data(),bytes.size())) throw std::runtime_error("HC1 source/guard modified");
    };
    auto* dd=static_cast<uint16_t*>(upload(wd));auto* du=static_cast<uint16_t*>(upload(wu));
    auto* di=static_cast<uint16_t*>(upload(wi));auto* dn=static_cast<float*>(upload(hn));
    int checks=0;
    for (int T:{1,2,3,4,5,6,7,8}) {
        std::vector<float> hr(size_t(T)*D+2*G,-12345.f),hb(size_t(T)*N+2*G,-12345.f),hip(size_t(T)*HC+2*G,-12345.f);
        auto* dr=static_cast<float*>(upload(hr));auto* db=static_cast<float*>(upload(hb));auto* dip=static_cast<float*>(upload(hip));
        size_t words=G;
        auto take=[&](size_t n) { const size_t pos=words;words+=n+2*G;return pos; };
        const size_t oR=take(size_t(T)*D),olo=take(size_t(T)*LR),ors=take(size_t(T)*HC),oi=take(size_t(T)*HC),
                     om=take(size_t(T)*N),oxn=take(size_t(T)*D),nq=size_t(T)*N/32*9,oq=take(nq);
        std::vector<uint32_t> init(words,poison),actual(words),expected(words);
        auto* out=static_cast<uint32_t*>(runtime.allocate(words*4,strata::l0::Memory::device));
        auto f=[&](size_t offset) { return reinterpret_cast<float*>(out+offset); };
        auto* dq=static_cast<uint32_t*>(runtime.allocate(nq*4,strata::l0::Memory::device));
        for (int mode=0;mode<4;++mode) {
            k::FusedGrArgs a[k::kFusedGrMaxT];
            for (int t=0;t<T;++t) {
                a[t].R=dr+G+size_t(t)*D;a[t].R_out=f(oR)+size_t(t)*D;a[t].apply=(mode==1 || mode==2);
                a[t].bo_prev=db+G+size_t(t)*N;a[t].inj_prev=dip+G+size_t(t)*HC;
                a[t].w_norm=dn+G;a[t].w_down=dd+G;a[t].w_up=du+G;a[t].w_inject=di+G;
                a[t].lo=f(olo)+size_t(t)*LR;a[t].rs=f(ors)+size_t(t)*HC;
                a[t].inject_out=f(oi)+size_t(t)*HC;a[t].mixed=f(om)+size_t(t)*N;
                if (mode>=2) a[t].q8_mixed=reinterpret_cast<uint8_t*>(out+oq)+size_t(t)*N/32*36;
            }
            auto body=[&] { if (k::fused_gr_read_multi(a,T,f(oxn),&q)!=(mode>=2)) throw std::runtime_error("unexpected HC1 Q8 mode"); };
            // Resolve scratch allocations and kernel state outside recording/timing.
            for (auto* h:{&hr,&hb,&hip}) for (size_t i=G;i<h->size()-G;++i) (*h)[i]=rnd();
            q.memcpy(dr,hr.data(),hr.size()*4);q.memcpy(db,hb.data(),hb.size()*4);q.memcpy(dip,hip.data(),hip.size()*4).wait_and_throw();
            body();q.wait_and_throw();q.begin_recording();
            for (int i=0;i<REPEAT;++i) body();
            auto graph=q.end_recording();
            for (int payload=0;payload<2;++payload) {
                rng=67890+payload*100+T;
                for (auto* h:{&hr,&hb,&hip}) for (size_t i=G;i<h->size()-G;++i) (*h)[i]=rnd();
                q.memcpy(dr,hr.data(),hr.size()*4);q.memcpy(db,hb.data(),hb.size()*4);q.memcpy(dip,hip.data(),hip.size()*4);
                q.memcpy(out,init.data(),init.size()*4).wait_and_throw();body();q.wait_and_throw();
                q.memcpy(expected.data(),out,expected.size()*4).wait_and_throw();
                std::vector<double> gold(words,std::numeric_limits<double>::quiet_NaN());
                for (int t=0;t<T;++t) {
                    std::vector<double> x(D),lo(LR);
                    for (int c=0;c<HC;++c) {
                        double ss=0;
                        for (int d=0;d<N;++d) {
                            const int i=c*N+d;
                            const double v=hr[G+size_t(t)*D+i]+(a[t].apply?hb[G+size_t(t)*N+d]*2/(1+std::exp(-double(hip[G+t*HC+c])/HC)):0);
                            if (a[t].apply) gold[oR+size_t(t)*D+i]=v;
                            x[i]=v;ss+=v*v;
                        }
                        const double rs=1/std::sqrt(ss/N+1e-6);gold[ors+t*HC+c]=rs;
                        for (int d=0;d<N;++d) { const int i=c*N+d;x[i]*=hn[G+i]*rs;gold[oxn+size_t(t)*D+i]=x[i]; }
                    }
                    for (int r=0;r<LR;++r) {
                        double v=0;for (int i=0;i<D;++i) v+=decode(wd[G+size_t(r)*D+i])*x[i];
                        v/=HC;lo[r]=v/(1+std::exp(-v));gold[olo+t*LR+r]=lo[r];
                    }
                    for (int c=0;c<HC;++c) {
                        double v=0;for (int i=0;i<D;++i) v+=decode(wi[G+size_t(c)*D+i])*x[i];
                        gold[oi+t*HC+c]=v;
                    }
                    for (int d=0;d<N;++d) {
                        double mixed=0;
                        for (int c=0;c<HC;++c) {
                            double v=0;for (int j=0;j<LR;++j) v+=decode(wu[G+size_t(c*N+d)*LR+j])*lo[j];
                            mixed+=x[c*N+d]/(1+std::exp(-v));
                        }
                        gold[om+size_t(t)*N+d]=mixed/HC;
                    }
                }
                double worst=0;
                for (size_t i=0;i<words;++i) {
                    if (std::isfinite(gold[i])) {
                        float v;std::memcpy(&v,&expected[i],4);const double error=std::abs(v-gold[i]);
                        if (!std::isfinite(v)||error>2e-5+2e-4*std::abs(gold[i])) throw std::runtime_error("HC1 CPU reference mismatch at word "+std::to_string(i));
                        worst=std::max(worst,error);
                    } else if (!(mode>=2&&i>=oq&&i<oq+nq)&&expected[i]!=poison) throw std::runtime_error("HC1 eager output guard overwritten");
                }
                if (mode>=2) {
                    k::native_quantize_q8_1(f(om),dq,N,T,&q);q.wait_and_throw();
                    std::vector<uint32_t> raw(nq);q.memcpy(raw.data(),dq,nq*4).wait_and_throw();
                    if (std::memcmp(raw.data(),expected.data()+oq,nq*4)) throw std::runtime_error("HC1 fused versus separate Q8 mismatch");
                }
                q.memcpy(out,init.data(),init.size()*4).wait_and_throw();
                q.replay(graph);q.wait_and_throw();
                q.memcpy(actual.data(),out,actual.size()*4).wait_and_throw();
                if (actual!=expected) throw std::runtime_error("HC replay full-output/guard mismatch");
                ++checks;
                uint64_t hash=1469598103934665603ull;for (uint32_t v:expected) { hash^=v;hash*=1099511628211ull; }
                std::printf("HC1_OUTPUT T=%d mode=%d payload=%d hash=%016llx words=%zu\n",T,mode,payload,static_cast<unsigned long long>(hash),words);
                std::printf("HC1_CPU T=%d mode=%d payload=%d max_abs_error=%.12g\n",T,mode,payload,worst);
                unchanged(hr,dr);unchanged(hb,db);unchanged(hip,dip);
            }
        }
        runtime.deallocate(dr);runtime.deallocate(db);runtime.deallocate(dip);runtime.deallocate(out);runtime.deallocate(dq);
    }
    unchanged(wd,dd);unchanged(wu,du);unchanged(wi,di);unchanged(hn,dn);
    runtime.deallocate(dd);runtime.deallocate(du);runtime.deallocate(di);runtime.deallocate(dn);
    if (checks!=64) throw std::runtime_error("missing HC1 checks");
    std::puts("PASS HC:64 eager/replay output/source/guard checks;T1..8,CPU FP64 reference,fused/separate Q8,pending write on/off,changed inputs");
    return 0;
} catch (const std::exception& e) { std::fprintf(stderr,"%s\n",e.what());return 1; }
