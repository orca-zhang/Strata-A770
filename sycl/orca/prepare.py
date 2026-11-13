"""Keep Strata's numerical sources; replace their host submission types for Orca.

Generated files live only in the build tree. The SYCL baseline and its source
files remain available for same-source comparisons.
"""
import argparse
import hashlib
from pathlib import Path
import re


def transform(text):
    text = re.sub(r"sycl::ext::oneapi::experimental::command_graph\s*<\s*"
                  r"sycl::ext::oneapi::experimental::graph_state::\s*executable\s*>",
                  "dpct::experimental::Graph", text)
    text = text.replace("__trap()", "__builtin_trap()")
    text = re.sub(r"(?<![\w:])fmaf\s*\(", "sycl::fma(", text)
    for original, native in {
        "sycl::handler": "strata::l0::Queue",
        "sycl::queue": "strata::l0::Queue",
        "sycl::local_accessor": "orca::l0::compiler::local_accessor",
        "sycl::exception": "std::exception",
        "sycl::event": "strata::l0::Event",
        "sycl::malloc_device": "strata::l0::malloc_device",
        "sycl::aligned_alloc_device": "strata::l0::aligned_alloc_device",
        "sycl::malloc_host": "strata::l0::malloc_host",
        "sycl::malloc_shared": "strata::l0::malloc_shared",
        "sycl::free": "strata::l0::free",
    }.items():
        text = re.sub(re.escape(original) + r"\b", native, text)
    text = re.sub(r"(?<![\w:])float4\b", "sycl::float4", text)
    return text


def engine_source(text):
    # These are runtime interoperability sites, rather than numerical kernels.
    if 'const float g_env_mtp_cum_min_p = [] {' in text:
        before = '    const char* v = std::getenv("STRATA_MTP_CUM_MIN_P");'
        if text.count(before) != 1:
            raise ValueError('MTP cumulative switch changed; review the Orca L0 default')
        # Reuse the existing draft/verify early stop; explicit zero retains the old window.
        text = text.replace(before, before + '''
#ifndef STRATA_ORCA_DG2
    if (v == nullptr) v = "0.5";
#endif''')
        for marker, condition, remaining in (
            ('                    drafted = !use_mtp || last ||',
             'use_mtp && !last', 'max_new - produced_n'),
            ('            const bool drafted = !use_mtp || (int64_t) produced.size() >= o.max_new ||',
             'use_mtp && (int64_t) produced.size() < o.max_new', 'o.max_new - (int64_t) produced.size()'),
        ):
            if text.count(marker) != 1:
                raise ValueError('MTP draft site changed; review the Orca L0 output budget')
            indent = marker[:len(marker) - len(marker.lstrip())]
            text = text.replace(marker, '#ifndef STRATA_ORCA_DG2\n' +
                indent + '// ponytail: the existing cap trims unused tail steps; the first draft also maintains KV.\n' +
                indent + f'if ({condition})\n' +
                indent + f'    mtp.set_max_drafts((int) std::min<int64_t>(S_mtp - 1, {remaining} - 1));\n' +
                '#endif\n' + marker)
    if "void mark_event(dpct::event_ptr ev, dpct::queue_ptr q)" in text:
        # ponytail: these prefill sites discard the returned event; retain exported markers.
        text, count = re.subn(r"ext_oneapi_submit_barrier\((\s*\{)", r"depends_on(\1", text)
        if count != 16:
            raise ValueError("prefill dependency sites changed; review discarded completion events")
    # Native waits use signed nanoseconds; normalize the legacy teardown sentinel.
    text = text.replace("wait_verify_queue(cs_, l0_sync_, UINT64_MAX)",
                        "wait_verify_queue(cs_, l0_sync_, uint64_t(INT64_MAX))")
    text = re.sub(r"(const uint8_t\* (?:Gguf|File|Arena)ExpertSource::device_alias\([^)]*\) const \{)(.*?)(^\})",
        lambda match: match[1] + re.sub(r"\breturn (?!nullptr;)([^;\n]+);",
            r"return strata::l0::device_pointer(\1, dpct::get_in_order_queue());", match[2]) + match[3],
        text, flags=re.S | re.M)
    for target in ("m.copy", "copy_", "adapt_stream"):
        text = text.replace(target + " = dpct::get_current_device().create_queue(true)",
                            target + " = strata::l0::create_copy_queue()")
    if "strata::l0::create_copy_queue()" in text:
        text = '#include "strata/orca_engine.hpp"\n' + text
    if "PinnedArena::~PinnedArena()" in text:
        text = '#include "strata/orca_engine.hpp"\n' + text
        text, replacements = re.subn(r"capped \? 0 :\s*/\*.*?\*/\s*0;",
            "capped ? 0 : strata::l0::import_host(base, size_t(bytes));", text, flags=re.S)
        if replacements != 1 or text.count("if (0 != 0)") != 1:
            raise ValueError("pinned registration sites changed; review native host import")
        text = text.replace("if (0 != 0)",
            "if (strata::l0::import_host((uint8_t*)base + off, size_t(n)) != 0)")
        begin = text.index("PinnedArena::~PinnedArena()")
        end = text.index("LoadStats load_experts(", begin)
        text = text[:begin] + '''PinnedArena::~PinnedArena() {
    if (!base) return;
    try {
        if (slice_bytes) {
            for (uint64_t off : slice_starts) strata::l0::release_host((uint8_t*)base + off);
        } else if (registered_bytes) strata::l0::release_host(base);
    } catch (const std::exception& e) {
        // Do not unmap memory that could still be imported or in GPU use.
        std::fprintf(stderr, "strata: native arena release failed: %s\\n", e.what());
        return;
    }
    if (locked_bytes) strata::platform::unlock_resident(
        (uint8_t*)base + (slice_bytes ? registered_bytes : 0), locked_bytes);
    release(mapping_base ? mapping_base : base, mapping_bytes ? mapping_bytes : capacity);
}

''' + text[end:]
        text = text.replace("cudaHostRegister PORTABLE ok", "Orca external host import ok")
        text = text.replace("cudaHostRegister FAILED", "Orca external host import FAILED")
    context = '''    const sycl::context context = cs_->get_context();
    const sycl::device device = cs_->get_device();
    const ze_context_handle_t ze_context =
        sycl::get_native<sycl::backend::ext_oneapi_level_zero>(context);
    ze_device_handle_t ze_device =
        sycl::get_native<sycl::backend::ext_oneapi_level_zero>(device);'''
    text = text.replace(context, '''    const auto ze_context = cs_->runtime().native_context();
    auto ze_device = cs_->runtime().native_device();''')
    imported = '''    const auto input = sycl::backend_input_t<
        sycl::backend::ext_oneapi_level_zero, sycl::event>{
            (ze_event_handle_t) l0_native_events_[i], sycl::ext::oneapi::level_zero::ownership::keep};
    l0_gate_events_.push_back(
        sycl::make_event<sycl::backend::ext_oneapi_level_zero>(input, queue->get_context()));'''
    text = text.replace(imported, '''    l0_gate_events_.push_back(strata::l0::Event::borrow(
        (ze_event_handle_t) l0_native_events_[i]));''')
    if "bool Verifier::init_l0_events(" in text:
        begin = text.index("bool Verifier::init_l0_events(")
        end = text.index("bool Verifier::release_gpu_waits(", begin)
        native = '''#ifdef STRATA_ORCA_DG2
using NativeHostEvent = strata::l0::dg2_backend::HostEvent;
bool Verifier::init_l0_events(std::string& err) {
    if (!skip_words_ || skip_words_ > UINT32_MAX / 3) {
        err = "verify: invalid native event count"; return false;
    }
    l0_events_ = true; return true;
}
bool Verifier::reset_l0_events(size_t count, std::string& err) try {
    if (!count || count > skip_words_) {
        err = "verify: invalid active native event count"; return false;
    }
    l0_event_error_.store(0, std::memory_order_release);
    l0_gate_events_.clear();
    const size_t total = skip_words_ * 3;
    if (!l0_native_events_.empty()) {
        if (l0_native_events_.size() != total) {
            err = "verify: incomplete native event allocation"; return false;
        }
        for (void* raw : l0_native_events_) static_cast<NativeHostEvent*>(raw)->reset();
    } else {
        l0_native_events_.reserve(total); l0_gate_events_.reserve(total);
        for (size_t i = 0; i < total; ++i) {
            auto event = std::make_unique<NativeHostEvent>(cs_->runtime());
            l0_native_events_.push_back(event.release());
        }
    }
    return true;
} catch (const std::exception& error) {
    err = std::string("verify: native event reset/create failed: ") + error.what(); return false;
}
void Verifier::wait_l0_event(int phase, uint32_t ring, dpct::queue_ptr queue) {
    const size_t i = size_t(phase) * skip_words_ + ring - 1;
    auto* event = static_cast<NativeHostEvent*>(l0_native_events_.at(i));
    l0_gate_events_.push_back(strata::l0::Event::borrow(event->handle()));
    queue->ext_oneapi_submit_barrier({l0_gate_events_.back()});
}
void Verifier::signal_l0_event(int phase, uint32_t ring) {
    if (!l0_events_ || !ring || ring > skip_words_) return;
    try { static_cast<NativeHostEvent*>(l0_native_events_.at(size_t(phase) * skip_words_ + ring - 1))->signal(); }
    catch (...) { l0_event_error_.store(1, std::memory_order_release); }
}
void Verifier::signal_all_l0_events() {
    if (!l0_events_) return;
    for (void* raw : l0_native_events_) try { static_cast<NativeHostEvent*>(raw)->signal(); } catch (...) {}
}
void Verifier::destroy_l0_events() {
    l0_gate_events_.clear();
    for (void* raw : l0_native_events_) delete static_cast<NativeHostEvent*>(raw);
    l0_native_events_.clear(); l0_event_pool_ = nullptr; l0_events_ = false;
}
#else
'''
        text = text[:begin] + native + text[begin:end] + '#endif\n\n' + text[end:]
        text = text.replace('#if defined(STRATA_HAS_LEVEL_ZERO_EVENTS)\n#include <level_zero/',
                            '#if defined(STRATA_HAS_LEVEL_ZERO_EVENTS) && !defined(STRATA_ORCA_DG2)\n#include <level_zero/', 1)
        text = text.replace('#if defined(STRATA_ORCA_NATIVE)\n        const auto& clock =',
                            '#if defined(STRATA_ORCA_DG2)\n'
                            '        err = "verify: native DG2 GPU timestamp profiling is not implemented"; return false;\n'
                            '#elif defined(STRATA_ORCA_NATIVE)\n        const auto& clock =')
    profiled = r"new sycl::queue\(dev.get_context\(\), dev, dpct::exception_handler,\s*" \
               r"sycl::property_list\{sycl::property::queue::in_order\{\},\s*" \
               r"sycl::property::queue::enable_profiling\{\}\}\)"
    text = re.sub(profiled,
                  '(throw std::runtime_error("native GPU event profiling is not implemented"), '
                  'static_cast<strata::l0::Queue*>(nullptr))', text)
    main = "int main(int argc, char **argv) try {"
    if main in text:
        text = '#include "strata/orca_engine.hpp"\n' + text.replace(main,
            main + "\n    strata::l0::install_engine();")
        begin = text.index('#else\n        std::fprintf(\n            stderr, "strata generate: GPU %d: %s, compute capability')
        end = text.index('#endif\n        const std::string e =', begin)
        text = text[:begin] + '''#else
        std::fprintf(stderr, "strata generate: GPU %d: %s, %s\\n", dev, name, strata::core::compiled_gpu_archs());
''' + text[end:]
    if "sycl::get_native" in text or "sycl::make_event" in text:
        raise ValueError("unmapped native engine interoperability site")
    return transform(text)


def device_util(text):
    # The remaining DPCT file is numerical helpers (shuffle, packing, etc.).
    # Host-runtime sections are not part of the native kernel adapter.
    for begin, end in [
        ("template <typename T> struct [[deprecated]] DataType", "/// Cast the high or low"),
        ("// Returns the SYCL event status", "namespace experimental {"),
        ("// The original source of the functions calculate_max_active_wg_per_xecore",
         "/// Supported group type during migration."),
    ]:
        if text.count(begin) != 1 or end not in text.split(begin)[1]:
            raise ValueError("DPCT utility sections changed; review the native adapter")
        start = text.index(begin)
        stop = text.index(end, start)
        text = text[:start] + text[stop:]
    return text.replace("throw sycl::exception(sycl::errc::runtime,", "throw std::runtime_error(")


def xmx_kernel(text):
    begin = "template <class Input, int NSG = 8>"
    paired = "template <class Input, int NSG = 8, int ROWS = 8>"
    if paired in text:
        # SYCL now shares B across two M4 accumulators. Preserve Orca's separately
        # measured M4 tile and BF16 kernel, including the generated source order.
        start = text.index("                jm::joint_matrix<sycl::sub_group, float, jm::use::accumulator, ROWS, TILE>")
        stop = text.index("                    sycl::group_barrier(sg);", start)
        if hashlib.sha256(text[start:stop].encode()).hexdigest() != "85651206517423fb4fe6c3ddf2e9c5672921a9cb2ff9c1d9c39ec642f7a86967":
            raise ValueError("SYCL paired XMX body changed; review the Orca adapter")
        text = text[:start] + '''                jm::joint_matrix<sycl::sub_group, float, jm::use::accumulator, TILE, TILE> c;
                jm::joint_matrix_fill(sg, c, 0.f);
                for (int64_t k = 0; k < K; k += 16) {
                    jm::joint_matrix<sycl::sub_group, Input, jm::use::a, TILE, 16, jm::layout::row_major> a;
                    jm::joint_matrix<sycl::sub_group, Input, jm::use::b, 16, TILE, jm::layout::col_major> b;
                    jm::joint_matrix_load(sg, a, gptr(source + k), stride);
                    jm::joint_matrix_load(sg, b, gptr(w + col * K + k), size_t(K));
                    jm::joint_matrix_mad(sg, c, a, b, c);
                }
                if (row + TILE <= T) {
                    jm::joint_matrix_store(sg, c, gptr(Y + row * N + col), size_t(N), jm::layout::row_major);
                } else {
                    auto* tmp = tail.get_multi_ptr<sycl::access::decorated::no>().get() + subgroup * TILE * TILE;
                    jm::joint_matrix_store(sg, c, sycl::address_space_cast<sycl::access::address_space::local_space,
                        sycl::access::decorated::no>(tmp), TILE, jm::layout::row_major);
''' + text[stop:]
        text = text.replace(paired, begin)
    end = "bool native_xmx_gemm("
    if text.count(begin) != 1 or text.count(end) != 1:
        raise ValueError("Strata native XMX section changed; review the compiler adapter")
    kernel = text[text.index(begin):text.index(end)]
    kernel = transform(kernel)
    # Retain the checked M4 tails and BF16 exports alongside the FP16 register tile.
    small = kernel
    for original, native in {
        "native_xmx_launch(": "native_xmx_launch_m4(",
        "constexpr int SG = 8, TILE = 8;": "constexpr int SG = 8, TILE = 8, ROWS = 4;",
        "NSG * TILE * TILE": "NSG * ROWS * TILE",
        "(T + TILE - 1) / TILE": "(T + ROWS - 1) / ROWS",
        "int64_t(it.get_group(0)) * TILE": "int64_t(it.get_group(0)) * ROWS",
        "row + TILE": "row + ROWS",
        "partial ? last :": "partial ? last + (row % TILE) * K :",
        "jm::use::accumulator, TILE, TILE": "jm::use::accumulator, ROWS, TILE",
        "jm::use::a, TILE, 16": "jm::use::a, ROWS, 16",
        "subgroup * TILE * TILE": "subgroup * ROWS * TILE",
        "i < TILE * TILE": "i < ROWS * TILE",
    }.items():
        if original not in small:
            raise ValueError("Strata FP16 tile changed; review the checked M4 adapter: " + original)
        small = small.replace(original, native)
    # Preserve the tested candidate's generated source, including this comment.
    small = "// Diagnostic only: use repeat-count 4 DPAS, retaining K accumulation order.\n" + small
    return '''#include <cstdlib>
#include <cstring>
#include <sycl/ext/intel/experimental/grf_size_properties.hpp>
#include <dpct/dpct.hpp>
#include <climits>
namespace strata::prefill {
''' + kernel + '''
''' + small + '''

class OrcaFp16ProjectionWide;
void native_xmx_launch_m16(const uint16_t* input, const uint16_t* weight, float* y,
                           int64_t t, int64_t n, int64_t k, int64_t stride, strata::l0::Queue* queue) {
    auto& q = *queue;
    if (!t) return;
    namespace jm = sycl::ext::oneapi::experimental::matrix;
    constexpr int SG = 8, NSG = 8;
    q.parallel_for<OrcaFp16ProjectionWide>(sycl::nd_range<3>(
        sycl::range<3>(size_t(t / 16), size_t(n / 64), SG * NSG),
        sycl::range<3>(1, 1, SG * NSG)),
        sycl::ext::oneapi::experimental::properties{sycl::ext::intel::experimental::grf_size<256>},
        [=](sycl::nd_item<3> it) [[sycl::reqd_sub_group_size(SG)]] {
        const auto sg = it.get_sub_group();
        const int sid = int(sg.get_group_id()[0]);
        const int row = int(it.get_group(0)) * 16 + sid / 4 * 8;
        const int col = int(it.get_group(1)) * 64 + sid % 4 * 16;
        const auto* x = reinterpret_cast<const sycl::half*>(input);
        const auto* w = reinterpret_cast<const sycl::half*>(weight);
        auto gp = [](auto* p) { return sycl::address_space_cast<sycl::access::address_space::global_space,
            sycl::access::decorated::no>(p); };
        using A = jm::joint_matrix<sycl::sub_group, sycl::half, jm::use::a, 4, 16, jm::layout::row_major>;
        using B = jm::joint_matrix<sycl::sub_group, sycl::half, jm::use::b, 16, 8, jm::layout::col_major>;
        jm::joint_matrix<sycl::sub_group, float, jm::use::accumulator, 4, 8> acc[2][2];
        #pragma unroll
        for (int r = 0; r < 2; ++r)
            #pragma unroll
            for (int c = 0; c < 2; ++c) jm::joint_matrix_fill(sg, acc[r][c], 0.f);
        for (int z = 0; z < k; z += 64) {
            #pragma unroll
            for (int j = 0; j < 64; j += 16) {
                A aa[2]; B bb[2];
                #pragma unroll
                for (int r = 0; r < 2; ++r) {
                    jm::joint_matrix_load(sg, aa[r], gp(x + size_t(row + r * 4) * stride + z + j), size_t(stride));
                    jm::joint_matrix_load(sg, bb[r], gp(w + size_t(col + r * 8) * k + z + j), size_t(k));
                }
                #pragma unroll
                for (int r = 0; r < 2; ++r)
                    #pragma unroll
                    for (int c = 0; c < 2; ++c) jm::joint_matrix_mad(sg, acc[r][c], aa[r], bb[c], acc[r][c]);
            }
        }
        #pragma unroll
        for (int r = 0; r < 2; ++r)
            #pragma unroll
            for (int c = 0; c < 2; ++c)
                jm::joint_matrix_store(sg, acc[r][c], gp(y + size_t(row + r * 4) * n + col + c * 8), size_t(n), jm::layout::row_major);
    });
}

void orca_xmx_gemm(const uint16_t* x, const uint16_t* w, float* y,
                   int64_t t, int64_t n, int64_t k, int64_t stride,
                   const uint16_t* tail, bool bf16, void* stream) {
    if (!x || !w || !y || t <= 0 || n <= 0 || k <= 0 || stride < k ||
        t > INT_MAX || n > INT_MAX || k > INT_MAX || stride > INT_MAX ||
        n % 8 || k % 16 || (t % 8 && !tail))
        throw std::invalid_argument("invalid native XMX dimensions/pointers/tail");
    auto* queue = &strata::l0::stream_of(stream);
    // ponytail: calibrated full tiles only; retain the checked M4 tails and BF16.
    static const bool wide = [] {
        const char* value = std::getenv("STRATA_ORCA_XMX_WIDE");
        if (!value || !std::strcmp(value, "1")) return true;
        if (!std::strcmp(value, "0")) return false;
        throw std::invalid_argument("STRATA_ORCA_XMX_WIDE must be 0 or 1");
    }();
    const int64_t full = wide && !bf16 && t >= 128 && n % 64 == 0 && k % 64 == 0 ? t / 16 * 16 : 0;
    if (full) {
        native_xmx_launch_m16(x, w, y, full, n, k, stride, queue);
        x += full * stride; y += full * n; t -= full;
        if (!t) return;
    }
    // Keep the same measured A770 small-projection choice as native_xmx_gemm.
    if (bf16 && n == 8 && k == 10240 && (t == 1024 || t == 2069) &&
        queue->get_device().get_info<sycl::info::device::name>().find("A770") != std::string::npos)
        native_xmx_launch<sycl::ext::oneapi::bfloat16, 1>(x, w, y, t, n, k, stride, queue, tail);
    else if (bf16) native_xmx_launch<sycl::ext::oneapi::bfloat16>(x, w, y, t, n, k, stride, queue, tail);
    else native_xmx_launch_m4<sycl::half>(x, w, y, t, n, k, stride, queue, tail);
}
}
'''


def iq_check(text):
    begin = '    const std::string dir = argc > 1 ? argv[1] : "logs/iq_fixture";'
    finish = '    std::printf("iq_parity: %d failures\\n", failures);'
    if text.count(begin) != 1 or text.count(finish) != 1:
        raise ValueError("IQ parity harness changed; review native setup")
    text = transform(text)
    setup = '''    if (argc != 3) throw std::invalid_argument("usage: strata-orca-iq-check images fixtures");
    check_iq_dependencies();
    strata::l0::Runtime runtime;
    const std::filesystem::path images(argv[1]);
    strata::l0::Program program(runtime, images / "elementwise");
    std::ifstream manifest(images / "packs.txt");
    if (!manifest) throw std::runtime_error("missing native pack manifest");
    std::string pack;
    while (std::getline(manifest, pack)) {
        if (pack == "elementwise") continue;
        if (pack.empty() || std::filesystem::path(pack).has_parent_path())
            throw std::runtime_error("invalid native pack name");
        program.add(images / pack);
    }
    strata::l0::Queue default_queue(runtime, program);
    strata::l0::bind_default(default_queue);
    const std::string dir = argv[2];'''
    text = text.replace(begin, setup).replace(finish,
        '    dpct::get_current_device().destroy_queue(s);\n    check_iq_dependencies();\n' + finish)
    # Reuse the existing CPU oracle and comparisons, exercising native capture
    # including first-use table initialization and every multi-column shape.
    one_column = '''            for (int c = 0; c < MC; ++c)
                strata::kernels::native_mmvq'''
    first_wait = '''            s->wait();
            (dpct::get_current_device()'''
    multi = '''                strata::kernels::native_mmvq(type, dw, xq, dy, cols, rows, nc, s);
                s->wait();'''
    for marker in (one_column, first_wait, multi):
        if text.count(marker) != 1:
            raise ValueError("IQ parity submission sequence changed; review native capture")
    text = text.replace(one_column, '            s->begin_recording();\n' + one_column)
    text = text.replace(first_wait, '''            auto singles = s->end_recording();
            s->replay(singles);
''' + first_wait)
    text = text.replace(multi, '''                s->begin_recording();
                strata::kernels::native_mmvq(type, dw, xq, dy, cols, rows, nc, s);
                auto multiple = s->end_recording();
                s->replay(multiple);
                s->wait();''')
    return '''#include "runtime_dependencies.hpp"
namespace {
void check_iq_dependencies() {
#ifdef STRATA_ORCA_DG2
    check_runtime_dependencies(true);
#else
    check_runtime_dependencies();
#endif
}
}
''' + text


if __name__ == "__main__":
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("source", type=Path)
    parser.add_argument("output", type=Path)
    parser.add_argument("--device-util", action="store_true")
    parser.add_argument("--xmx-kernel", action="store_true")
    parser.add_argument("--iq-check", action="store_true")
    parser.add_argument("--engine", action="store_true")
    args = parser.parse_args()
    text = args.source.read_text(encoding="utf-8")
    text = (device_util(text) if args.device_util else xmx_kernel(text) if args.xmx_kernel
            else iq_check(text) if args.iq_check else engine_source(text) if args.engine else transform(text))
    args.output.parent.mkdir(parents=True, exist_ok=True)
    if not args.output.exists() or args.output.read_text(encoding="utf-8") != text:
        args.output.write_text(text, encoding="utf-8")
