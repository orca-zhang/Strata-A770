"""Keep Strata's numerical sources; replace their host submission types for Orca.

Generated files live only in the build tree. The SYCL baseline and its source
files remain available for same-source comparisons.
"""
import argparse
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
    for target in ("m.copy", "copy_"):
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
        text = text.replace("cudaHostRegister PORTABLE ok", "Level Zero external host import ok")
        text = text.replace("cudaHostRegister FAILED", "Level Zero external host import FAILED")
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
        std::fprintf(stderr, "strata generate: GPU %d: %s, native Orca Level Zero\\n", dev, name);
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
    end = "bool native_xmx_gemm("
    if text.count(begin) != 1 or text.count(end) != 1:
        raise ValueError("Strata native XMX section changed; review the compiler adapter")
    kernel = text[text.index(begin):text.index(end)]
    kernel = transform(kernel)
    # ponytail: duplicate only the generated FP16 tile, retaining the checked
    # BF16 exports and original SYCL source. Wider tile selection needs measurements.
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
    return '''#include <dpct/dpct.hpp>
#include <climits>
namespace strata::prefill {
''' + kernel + '''
''' + small + '''
void orca_xmx_gemm(const uint16_t* x, const uint16_t* w, float* y,
                   int64_t t, int64_t n, int64_t k, int64_t stride,
                   const uint16_t* tail, bool bf16, void* stream) {
    if (!x || !w || !y || t <= 0 || n <= 0 || k <= 0 || stride < k ||
        t > INT_MAX || n > INT_MAX || k > INT_MAX || stride > INT_MAX ||
        n % 8 || k % 16 || (t % 8 && !tail))
        throw std::invalid_argument("invalid native XMX dimensions/pointers/tail");
    auto* queue = &strata::l0::stream_of(stream);
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
    check_runtime_dependencies();
    orca::l0::Runtime runtime;
    const std::filesystem::path images(argv[1]);
    orca::l0::compiler::Program program(runtime, images / "elementwise");
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
        '    dpct::get_current_device().destroy_queue(s);\n    check_runtime_dependencies();\n' + finish)
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
    return '#include "runtime_dependencies.hpp"\n' + text


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
