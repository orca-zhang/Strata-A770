"""Make a private diagnostic: compare the actual engine's FP16 GEMM inputs before continuing."""
from pathlib import Path

H = Path(__file__).resolve().parent
R = H.parent
source = (R/'sycl/src/prefill/gemm.dp.cpp').read_text()
source = source.replace('#include <climits>', '#include <vector>\n#include <cmath>\n#include <climits>', 1)
before = '''    float* reference = nullptr;
    static int compared = 0;
    const bool diagnose = std::getenv("STRATA_PREFILL_GEMM_COMPARE") && compared < 32;
    if (diagnose) {
        reference = sycl::malloc_device<float>((size_t) T * ldy, *q);
        q->memcpy(reference, Y, (size_t) T * ldy * sizeof(float)).wait_and_throw();
    }
'''
needle = '    auto* wf = static_cast<float*>(workspace);'
assert source.count(needle) == 1
source = source.replace(needle, before+needle, 1)
after = '''    if (diagnose) {
        const float alpha = 1;
        dpct::blas::descriptor desc;
        desc.set_queue(q);
        dpct::blas::gemm(&desc, oneapi::mkl::transpose::trans, oneapi::mkl::transpose::nontrans,
            N, T, K, &alpha, W, dpct::library_data_t::real_half, K,
            X, dpct::library_data_t::real_half, ldx, &beta, reference,
            dpct::library_data_t::real_float, ldy, dpct::compute_type::f32);
        std::vector<float> actual((size_t)T*ldy), expected(actual.size());
        q->memcpy(actual.data(),Y,actual.size()*4);
        q->memcpy(expected.data(),reference,expected.size()*4).wait_and_throw();
        size_t different=0, bad_actual=0, bad_expected=0;
        for(int64_t row=0;row<T;++row) for(int64_t col=0;col<N;++col) {
            size_t i=row*ldy+col;
            different += std::memcmp(&actual[i],&expected[i],4)!=0;
            bad_actual += !std::isfinite(actual[i]);
            bad_expected += !std::isfinite(expected[i]);
        }
        std::fprintf(stderr,"GEMM_COMPARE call=%d T=%lld N=%lld K=%lld ldy=%lld beta=%g different=%zu bad_actual=%zu bad_expected=%zu\\n",
            ++compared,(long long)T,(long long)N,(long long)K,(long long)ldy,beta,different,bad_actual,bad_expected);
        if(different || bad_actual || bad_expected) {
            std::vector<uint16_t> x((size_t)T*ldx),w((size_t)N*K);
            q->memcpy(x.data(),X,x.size()*2); q->memcpy(w.data(),W,w.size()*2).wait_and_throw();
            size_t bad_x=0,bad_w=0;
            for(auto v:x) bad_x += !std::isfinite(float(sycl::bit_cast<sycl::half>(v)));
            for(auto v:w) bad_w += !std::isfinite(float(sycl::bit_cast<sycl::half>(v)));
            std::fprintf(stderr,"GEMM_COMPARE inputs bad_x=%zu bad_w=%zu first_actual=%g first_expected=%g\\n",bad_x,bad_w,actual[0],expected[0]);
            std::exit(86);
        }
        sycl::free(reference,*q);
        if(compared==32) std::exit(87);
    }
'''
needle = '    return true;\n}\n}  // namespace\n\nvoid Gemm::bf16'
assert source.count(needle) == 1
source = source.replace(needle, after+needle, 1)
target = H/'gemm_compare.cpp'
target.write_text(source)
print(target)
