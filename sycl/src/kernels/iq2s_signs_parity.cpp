#include "cuda/iq_kernels.dp.cpp"
#include <cstring>
#include <stdexcept>
#include <vector>

int main() try {
    constexpr int N = 1024 * 256;
    auto& q = dpct::get_in_order_queue();
    std::vector<block_iq2_s> blocks(N);
    for (int i=0; i<N; ++i) {
        auto& b=blocks[i]; b.d=sycl::half(1.f);
        for(int c=0;c<8;++c) {
            b.scales[c]=uint8_t(i*17+c*23+i/256);
            for(int j=0;j<4;++j) {
                int index=(i/256+131*j+37*c)&1023;
                b.qs[4*c+j]=uint8_t(index);
                b.qh[c]|=uint8_t((index>>8)<<(2*j));
                b.qs[32+4*c+j]=uint8_t(i+29*j);
            }
        }
    }
    for(auto grid:iq2s_grid) for(int p=0;p<8;++p)
        if(((grid>>(8*p))&255)==0)throw std::runtime_error("zero grid byte invalidates addition");
    auto* weights=sycl::malloc_device<block_iq2_s>(N,q);
    auto* result=sycl::malloc_device<float>(N,q);
    auto* activation=sycl::malloc_device<block_q8_1>(8,q);
    if(!weights||!result||!activation)throw std::bad_alloc();
    q.memcpy(weights,blocks.data(),N*sizeof(block_iq2_s));
    std::vector<float> actual(N);
    for(int pos=0;pos<32;++pos) {
        block_q8_1 x[8]{};
        for(auto& chunk:x) { chunk.ds[0]=sycl::half(1.f);chunk.qs[pos]=8; }
        q.memcpy(activation,x,sizeof(x));
        q.parallel_for(sycl::range<1>(N),[=](sycl::id<1> id) {
            int i=int(id[0]), c=i%8;
            strata::kernels::MultiW w;
            strata::kernels::Multi<22>::prep(weights,i,c*2,w);
            int u[8]; float ds;
            strata::kernels::Multi<22>::acts(activation,c*2,u,ds);
            int a=0,b=0;
            for(int j=0;j<4;++j)a=strata::dp4a(w.w[j],u[j],a);
            for(int j=4;j<8;++j)b=strata::dp4a(w.w[j],u[j],b);
            result[i]=strata::kernels::Multi<22>::finish(a,b,w,ds);
        });
        q.memcpy(actual.data(),result,N*sizeof(float)).wait_and_throw();
        for(int i=0;i<N;++i) {
            int c=i%8,j=pos/8,index=(i/256+131*j+37*c)&1023;
            auto* bytes=reinterpret_cast<const uint8_t*>(iq2s_grid+index);
            int sign=(uint8_t(i+29*j)>>(pos%8))&1;
            int scale=(uint8_t(i*17+c*23+i/256)>>(pos<16?0:4))&15;
            float expected=(sign?-1:1)*int(bytes[pos%8])*(2*scale+1);
            if(actual[i]!=expected) { std::fprintf(stderr,"i=%d pos=%d actual=%g expected=%g\n",i,pos,actual[i],expected); throw std::runtime_error("IQ2_S sign/scale differs from byte reference"); }
        }
    }
    sycl::free(weights,q);sycl::free(result,q);sycl::free(activation,q);
    std::printf("PASS IQ2_S: %d exact dots, 1024 grid codes, 256 signs, 32 positions, eight chunks\n",N*32);
    return 0;
} catch(const std::exception& e) { std::fprintf(stderr,"FAIL %s\n",e.what());return 1; }
