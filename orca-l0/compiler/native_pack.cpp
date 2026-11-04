// Bake the installed driver's code generation and ABI into a reusable native pack.
#include <orca_l0/compiler/program.hpp>
#include <chrono>
#include <cstdio>
#include <limits>
#include <map>

namespace fs = std::filesystem;
using Clock = std::chrono::steady_clock;
using Selection = std::set<std::pair<fs::path, std::string>>;

static void write(const fs::path& path, const void* data, std::size_t size) {
    std::ofstream out(path, std::ios::binary);
    if (!out || !out.write(static_cast<const char*>(data), static_cast<std::streamsize>(size)) || !out.flush())
        throw std::runtime_error("cannot write native pack file: " + path.string());
}
static void text(const fs::path& path, const std::string& value) { write(path, value.data(), value.size()); }

// Inspect declared requirements without changing any arithmetic. A subgroup
// size alone does not imply matrix instructions (ordinary SG16 works on DG2).
enum : uint32_t { needs_fp64 = 1, needs_matrix_sg16 = 2 };
static uint32_t requirements(const std::vector<uint32_t>& words) {
    if (words.size() < 5 || words[0] != 0x07230203 || words[4] != 0)
        throw std::runtime_error("invalid SPIR-V header");
    uint32_t required = 0;
    bool matrix = false, sg16 = false;
    for (size_t i = 5; i < words.size();) {
        const uint32_t size = words[i] >> 16, opcode = words[i] & 0xffff;
        if (!size || size > words.size() - i) throw std::runtime_error("invalid SPIR-V instruction length");
        if (opcode == 17) { // OpCapability
            if (size != 2) throw std::runtime_error("invalid SPIR-V capability");
            if (words[i + 1] == 10) required |= needs_fp64; // Float64
            matrix |= words[i + 1] == 6022; // CooperativeMatrixKHR
        }
        if (opcode == 16 && size >= 3 && words[i + 2] == 35) { // OpExecutionMode SubgroupSize
            if (size != 4) throw std::runtime_error("invalid SPIR-V subgroup size");
            sg16 |= words[i + 3] == 16;
        }
        i += size;
    }
    return required | (matrix && sg16 ? needs_matrix_sg16 : 0);
}
static uint32_t requirements(const fs::path& path) {
    const auto size = fs::file_size(path);
    if (size % 4 || size > SIZE_MAX || size > uintmax_t(std::numeric_limits<std::streamsize>::max()))
        throw std::runtime_error("invalid SPIR-V file size");
    std::vector<uint32_t> words(static_cast<size_t>(size / 4));
    std::ifstream in(path, std::ios::binary);
    if (!in || !in.read(reinterpret_cast<char*>(words.data()), static_cast<std::streamsize>(size)))
        throw std::runtime_error("cannot read SPIR-V image");
    return requirements(words);
}
static void self_check() {
    const std::vector<uint32_t> header{0x07230203, 0x00010500, 0, 2, 0};
    auto words = header; words.insert(words.end(), {0x00020011, 6});
    if (requirements(words)) throw std::runtime_error("false FP64 requirement");
    words.insert(words.end(), {0x00020011, 10});
    if (requirements(words) != needs_fp64) throw std::runtime_error("missing FP64 requirement");
    words = header; words.insert(words.end(), {0x00040010, 1, 35, 16});
    if (requirements(words)) throw std::runtime_error("ordinary SG16 was treated as a matrix requirement");
    words.insert(words.end(), {0x00020011, 6022});
    if (requirements(words) != needs_matrix_sg16) throw std::runtime_error("missing SG16 matrix requirement");
    words = header; words.insert(words.end(), {0x00040010, 1, 35, 8, 0x00020011, 6022});
    if (requirements(words)) throw std::runtime_error("SG8 matrix was treated as SG16");
    for (auto bad : {std::vector<uint32_t>{}, std::vector<uint32_t>{0, 0, 0, 0, 0},
                     std::vector<uint32_t>{0x07230203, 0x00010500, 0, 2, 1}}) {
        bool rejected = false; try { requirements(bad); } catch (const std::runtime_error&) { rejected = true; }
        if (!rejected) throw std::runtime_error("accepted invalid SPIR-V header");
    }
    for (uint32_t instruction : {0u, 0x00010011u, 0x00030011u, 0xffff0011u}) {
        auto bad = header; bad.push_back(instruction);
        bool rejected = false; try { requirements(bad); } catch (const std::runtime_error&) { rejected = true; }
        if (!rejected) throw std::runtime_error("accepted invalid SPIR-V instruction");
    }
    std::puts("PASS SPIR-V FP64/SG16 matrix requirements, ordinary SG16 and malformed instruction rejection");
}

static void bake(const fs::path& input, const fs::path& output,
                 const Selection* selected, Selection& remaining, bool compatible) {
    orca::l0::Runtime runtime;
    orca::l0::compiler::Program validation(runtime, input); // Same filename, symbol and flag validation as execution.
    fs::create_directories(output);
    std::ifstream index(input / "index.txt");
    std::map<std::pair<std::string, std::string>, std::string> images;
    std::string line, native_index, retained, manifest = "native_file\tspirv_file\tbuild_flags\tcompile_ms\tbytes\n";
    ze_device_module_properties_t capabilities{};
    capabilities.stype = ZE_STRUCTURE_TYPE_DEVICE_MODULE_PROPERTIES;
    if (compatible) orca::l0::check(zeDeviceGetModuleProperties(runtime.native_device(), &capabilities), "native pack capabilities");
    // DG2 uses SG8 DPAS. This is the same PCI family bound as Orca::DG2;
    // do not infer matrix support from the ordinary subgroup-size list.
    const auto& device = runtime.properties();
    const bool sg8_only = device.vendorId == 0x8086 && device.deviceId >= 0x5690 && device.deviceId <= 0x56bf;
    while (std::getline(index, line)) {
        if (!line.empty() && line.back() == '\r') line.pop_back();
        const auto first = line.find('\t'), second = line.find('\t', first + 1);
        const auto file = line.substr(0, first);
        const auto name = line.substr(first + 1, second == std::string::npos ? second : second - first - 1);
        const auto flags = second == std::string::npos ? std::string{} : line.substr(second + 1);
        if (fs::path(file).extension() != ".spv") throw std::runtime_error("bake input must contain SPIR-V images");
        const auto key = std::make_pair(fs::canonical(input / file), flags);
        const auto required = compatible ? requirements(input / file) : 0;
        const char* reason = (required & needs_fp64) && !(capabilities.flags & ZE_DEVICE_MODULE_FLAG_FP64) ? "device lacks FP64" :
            (required & needs_matrix_sg16) && sg8_only ? "device advertises SG8 DPAS only; SG16 cooperative matrix retained" : nullptr;
        const bool unsupported = reason != nullptr;
        if ((selected && !selected->count(key)) || unsupported) {
            // Preserve lazy dispatch for unexercised variants, without claiming
            // their native compilation succeeded. Selected failures remain fatal.
            if (!fs::exists(output / file)) fs::copy_file(input / file, output / file);
            native_index += line + '\n';
            if (unsupported) {
                retained += file + '\t' + name + '\t' + reason + '\n';
                std::printf("RETAIN %s %s: %s\n", input.filename().string().c_str(), file.c_str(), reason);
            }
            continue;
        }
        remaining.erase(key);
        auto& native = images[{file, flags}];
        if (native.empty()) {
            native = "image_" + std::to_string(images.size() - 1) + ".zebin";
            const auto start = Clock::now();
            auto binary = runtime.compile_native(input / file, flags.c_str());
            const auto ms = std::chrono::duration<double, std::milli>(Clock::now() - start).count();
            write(output / native, binary.data(), binary.size());
            manifest += native + '\t' + file + '\t' + flags + '\t' + std::to_string(ms) + '\t' + std::to_string(binary.size()) + '\n';
            std::printf("BAKE %s %s %.3f ms %zu bytes\n", input.filename().string().c_str(), native.c_str(), ms, binary.size());
            std::fflush(stdout);
        }
        native_index += native + '\t' + name + '\n';
    }
    if (!index.eof()) throw std::runtime_error("cannot read complete source index");
    if (fs::exists(input / "abi.hpp")) fs::copy_file(input / "abi.hpp", output / "abi.hpp");
    fs::copy_file(input / "index.txt", output / "source-index.txt");
    text(output / "native-device.txt", runtime.binary_identity() + '\n');
    text(output / "native-manifest.tsv", manifest);
    if (!retained.empty()) text(output / "retained-spirv.tsv", retained);
    // ponytail: index is the completion marker; an interrupted bake cannot be loaded.
    text(output / "index.txt", native_index);
}

int main(int argc, char** argv) try {
    if (argc == 2 && std::string(argv[1]) == "--self-check") { self_check(); return 0; }
    const bool compatible = argc == 4 && std::string(argv[3]) == "--compatible";
    if (argc != 3 && !compatible && !(argc == 5 && std::string(argv[3]) == "--modules"))
        throw std::invalid_argument("usage: orca-l0-native-pack <source-pack-or-collection> <new-output-directory> [--modules trace.tsv | --compatible]");
    const auto input = fs::canonical(argv[1]);
    const auto output = fs::absolute(argv[2]);
    if (!fs::is_directory(input) || fs::exists(output))
        throw std::invalid_argument("source must be a directory and output must not exist");
    Selection selected;
    if (argc == 5) {
        std::ifstream trace(argv[4]);
        if (!trace) throw std::runtime_error("cannot open module trace");
        std::string row;
        while (std::getline(trace, row)) {
            if (!row.empty() && row.back() == '\r') row.pop_back();
            const auto tab = row.find('\t');
            if (tab == std::string::npos || !tab) throw std::runtime_error("invalid module trace row");
            selected.emplace(fs::canonical(row.substr(0, tab)), row.substr(tab + 1));
        }
        if (!trace.eof() || selected.empty()) throw std::runtime_error("incomplete module trace");
    }
    Selection remaining = selected;
    if (fs::exists(input / "packs.txt")) {
        std::ifstream packs(input / "packs.txt");
        std::string name, complete;
        while (std::getline(packs, name)) {
            if (!name.empty() && name.back() == '\r') name.pop_back();
            const fs::path file(name);
            if (name.empty() || file.has_parent_path() || file.is_absolute() || name == "." || name == "..")
                throw std::runtime_error("invalid collection pack name");
            bake(input / file, output / file, argc == 5 ? &selected : nullptr, remaining, compatible);
            complete += name + '\n';
        }
        if (!packs.eof() || complete.empty()) throw std::runtime_error("incomplete collection index");
        if (!remaining.empty()) throw std::runtime_error("module trace contains images/flags absent from collection");
        text(output / "packs.txt", complete);
    } else {
        bake(input, output, argc == 5 ? &selected : nullptr, remaining, compatible);
        if (!remaining.empty()) {
            fs::remove(output / "index.txt");
            throw std::runtime_error("module trace contains images/flags absent from pack");
        }
    }
    std::puts(compatible ? "PASS compatible bake complete; device-incompatible variants explicitly retain SPIR-V" :
        argc == 5 ? "PASS traced modules baked; unselected variants retain SPIR-V" :
        "PASS native pack complete; kernels use ZE_MODULE_FORMAT_NATIVE with baked build flags");
    return 0;
} catch (const std::exception& e) {
    std::fprintf(stderr, "%s\n", e.what());
    return 1;
}
