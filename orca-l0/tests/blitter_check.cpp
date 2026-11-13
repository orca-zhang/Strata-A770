// CPU-only check of the actual shared BLT recording bodies; no driver required.
#include <algorithm>
#include <array>
#include <cstdint>
#include <cstdio>
#include <cstring>
#include <stdexcept>
#include <vector>
#include "../src/drm/blitter.hpp"

namespace orca::l0::dg2 {
namespace detail {
template<class T> void emit(std::vector<uint32_t>& out, const T& packet) {
    static_assert(sizeof(T) % 4 == 0);
    const auto start = out.size();
    out.resize(start + sizeof(T) / 4);
    std::memcpy(out.data() + start, &packet, sizeof(T));
}
}
struct Recorder {
    struct {
        std::vector<uint32_t> commands;
        struct { bool in_order = false, valid = false; } encoder_state;
    } recording;
#include "../src/drm/blitter_operations.inl"
};
}
using namespace orca::l0::dg2;
using P = blitter_packet::Packets;
constexpr uint64_t DST = 0x1000000000ull, SRC = 0x2000000000ull;
void check(bool ok, const char* message) { if (!ok) throw std::runtime_error(message); }
template<class T> T packet(const std::vector<uint32_t>& words, size_t& cursor) {
    check(cursor + sizeof(T) / 4 <= words.size(), "truncated packet");
    T result;
    std::memcpy(&result, words.data() + cursor, sizeof(T));
    cursor += sizeof(T) / 4;
    return result;
}
void decode(const Recorder& r, std::vector<uint8_t>& destination, const std::vector<uint8_t>& source) {
    unsigned blits = 0, flushes = 0, arbs = 0;
    for (size_t cursor = 0; cursor < r.recording.commands.size();) {
        const auto head = r.recording.commands[cursor];
        if (head >> 29 == 2 && ((head >> 22) & 127) == 0x44) {
            const auto p = packet<P::XY_FAST_COLOR_BLT>(r.recording.commands, cursor);
            const unsigned depth = p.getColorDepth();
            check(depth <= 3 || depth == 5, "invalid fill depth");
            const size_t pixel = depth == 5 ? 16 : size_t{1} << depth;
            const size_t width = p.getDestinationX2CoordinateRight(), height = p.getDestinationY2CoordinateBottom();
            check(width && height && width <= 16384 && height <= 16384, "invalid fill rectangle");
            check(p.getDestinationMOCS() == 2, "fill MOCS changed");
            check(!p.getDestinationX1CoordinateLeft() && !p.getDestinationY1CoordinateTop(), "unexpected fill origin");
            check(p.getDestinationSurfaceWidth() == width && p.getDestinationSurfaceHeight() == height, "fill surface mismatch");
            const auto* color = reinterpret_cast<const uint8_t*>(p.TheStructure.Common.FillColor);
            for (size_t y = 0; y < height; ++y) {
                const auto address = p.getDestinationBaseAddress() + y * p.getDestinationPitch();
                check(address >= DST && address - DST <= destination.size(), "fill address out of range");
                const auto offset = size_t(address - DST);
                check(width * pixel <= destination.size() - offset, "fill exceeds destination");
                for (size_t x = 0; x < width * pixel; ++x) destination[offset + x] = color[x % pixel];
            }
            ++blits;
        } else if (head >> 29 == 2 && ((head >> 22) & 127) == 0x41) {
            const auto p = packet<P::XY_BLOCK_COPY_BLT>(r.recording.commands, cursor);
            const size_t width = p.getDestinationX2CoordinateRight(), height = p.getDestinationY2CoordinateBottom();
            check(width && height && width <= 16384 && height <= 16384, "invalid copy rectangle");
            check(p.getColorDepth() == P::XY_BLOCK_COPY_BLT::COLOR_DEPTH_8_BIT_COLOR, "copy depth changed");
            check(p.getSourceMOCS() == 2 && p.getDestinationMOCS() == 2, "copy MOCS changed");
            check(!p.getSourceX1CoordinateLeft() && !p.getSourceY1CoordinateTop() &&
                  !p.getDestinationX1CoordinateLeft() && !p.getDestinationY1CoordinateTop(), "unexpected copy origin");
            check(p.getSourcePitch() == width && p.getDestinationPitch() == width, "copy pitch mismatch");
            check(p.getSourceSurfaceWidth() == width && p.getDestinationSurfaceWidth() == width &&
                  p.getSourceSurfaceHeight() == height && p.getDestinationSurfaceHeight() == height, "copy surface mismatch");
            for (size_t y = 0; y < height; ++y) {
                const auto d = p.getDestinationBaseAddress() + y * p.getDestinationPitch();
                const auto s = p.getSourceBaseAddress() + y * p.getSourcePitch();
                check(d >= DST && d - DST <= destination.size() && s >= SRC && s - SRC <= source.size(), "copy address out of range");
                check(width <= destination.size() - size_t(d - DST) && width <= source.size() - size_t(s - SRC), "copy exceeds buffer");
                std::memcpy(destination.data() + size_t(d - DST), source.data() + size_t(s - SRC), width);
            }
            ++blits;
        } else if (head >> 29 == 0 && ((head >> 23) & 63) == 38) {
            const auto p = packet<P::MI_FLUSH_DW>(r.recording.commands, cursor);
            check(p.getFlushLlc() && p.getFlushCcs(), "incomplete BLT flush");
            check(p.getPostSyncOperation() == P::MI_FLUSH_DW::POST_SYNC_OPERATION_NO_WRITE, "unexpected flush write");
            ++flushes;
        } else if (head >> 29 == 0 && ((head >> 23) & 63) == 5) {
            packet<P::MI_ARB_CHECK>(r.recording.commands, cursor);
            ++arbs;
        } else throw std::runtime_error("unknown BLT packet");
    }
    check(arbs == blits && flushes == 1 + (r.recording.encoder_state.in_order ? blits : 0), "flush/arbitration count mismatch");
}
int main() try {
    unsigned cases = 0;
    const std::array<size_t, 21> lengths{0,1,2,3,7,15,16,17,31,32,127,128,129,257,4097,16383,16384,16385,32769,65539,262145};
    for (bool ordered : {false, true}) for (size_t offset : std::array<size_t,6>{64,65,67,79,80,95})
        for (auto bytes : lengths) for (size_t pattern : std::array<size_t,9>{0,1,2,4,8,16,32,64,128}) {
            std::array<uint8_t,128> value{};
            for (size_t i = 0; i < value.size(); ++i) value[i] = uint8_t(i * 37 + 11);
            std::vector<uint8_t> source(offset + bytes + 64), destination(source.size(), 0xa5), expected(destination);
            for (size_t i = 0; i < source.size(); ++i) source[i] = uint8_t(i * 29 + 7);
            const auto saved_source = source;
            Recorder recorder;
            recorder.recording.encoder_state.in_order = ordered;
            if (pattern) recorder.record_color_fill(DST + offset, value.data(), pattern, bytes);
            else recorder.record_blit(DST + offset, SRC + offset, bytes);
            detail::emit(recorder.recording.commands, blitter_flush());
            decode(recorder, destination, source);
            for (size_t i = 0; i < bytes; ++i) expected[offset + i] = pattern ? value[i % pattern] : source[offset + i];
            check(destination == expected, "complete output/guard mismatch");
            check(source == saved_source, "source changed");
            ++cases;
        }
    // Check the 16K-by-16K split without allocating 256 MiB per test.
    Recorder large;
    large.record_blit(DST, SRC, size_t{16384} * 16384 + 17);
    size_t cursor = 0;
    auto first = packet<P::XY_BLOCK_COPY_BLT>(large.recording.commands, cursor);
    check(first.getDestinationX2CoordinateRight() == 16384 && first.getDestinationY2CoordinateBottom() == 16384, "large rectangle changed");
    packet<P::MI_ARB_CHECK>(large.recording.commands, cursor);
    auto tail = packet<P::XY_BLOCK_COPY_BLT>(large.recording.commands, cursor);
    check(tail.getDestinationBaseAddress() == DST + size_t{16384} * 16384 && tail.getSourceBaseAddress() == SRC + size_t{16384} * 16384 &&
          tail.getDestinationX2CoordinateRight() == 17 && tail.getDestinationY2CoordinateBottom() == 1, "large copy tail mismatch");
    const auto flush = blitter_flush(DST, 0x123456789abcdef0ull);
    check(flush.getDestinationAddress() == DST && flush.getImmediateData() == 0x123456789abcdef0ull &&
          flush.getPostSyncOperation() == P::MI_FLUSH_DW::POST_SYNC_OPERATION_WRITE_IMMEDIATE_DATA_QWORD, "post-sync flush mismatch");
    std::printf("PASS %u shared BLT CPU output/source/guard cases;16K geometry;flushes;no GPU claim\n", cases);
    return 0;
} catch (const std::exception& e) { std::fprintf(stderr, "FAIL %s\n", e.what()); return 1; }
