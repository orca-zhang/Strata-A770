// Offline build input. Execution uses only the resulting DG2 zebin, without OpenCL or a compiler.
__attribute__((reqd_work_group_size(256, 1, 1)))
__kernel void orca_copy(__global uchar* destination, __global const uchar* source, ulong bytes) {
    const ulong lane = get_global_id(0), stride = get_global_size(0);
    ulong tail = 0;
    if ((((ulong)destination | (ulong)source) & 15) == 0) {
        const ulong vectors = bytes / 16;
        for (ulong i = lane; i < vectors; i += stride)
            ((__global uint4*)destination)[i] = ((__global const uint4*)source)[i];
        tail = vectors * 16;
    }
    for (ulong i = tail + lane; i < bytes; i += stride) destination[i] = source[i];
}

__attribute__((reqd_work_group_size(256, 1, 1)))
__kernel void orca_fill(__global uchar* destination, ulong bytes, uint4 pattern) {
    const ulong lane = get_global_id(0), stride = get_global_size(0);
    ulong tail = 0;
    if (((ulong)destination & 15) == 0) {
        const ulong vectors = bytes / 16;
        for (ulong i = lane; i < vectors; i += stride)
            ((__global uint4*)destination)[i] = pattern;
        tail = vectors * 16;
    }
    for (ulong i = tail + lane; i < bytes; i += stride) {
        const uint component = (i & 15) >> 2;
        const uint word = component == 0 ? pattern.s0 : component == 1 ? pattern.s1 : component == 2 ? pattern.s2 : pattern.s3;
        destination[i] = (uchar)(word >> ((i & 3) * 8));
    }
}

__attribute__((reqd_work_group_size(256, 1, 1)))
__kernel void orca_fill_wide(__global uchar* destination, ulong bytes, __global const uchar* pattern, uint mask) {
    const ulong lane = get_global_id(0), stride = get_global_size(0);
    ulong tail = 0;
    if (((ulong)destination & 15) == 0) {
        const ulong vectors = bytes / 16;
        for (ulong i = lane; i < vectors; i += stride)
            ((__global uint4*)destination)[i] = ((__global const uint4*)pattern)[i & (mask >> 4)];
        tail = vectors * 16;
    }
    for (ulong i = tail + lane; i < bytes; i += stride) destination[i] = pattern[i & mask];
}

// One dispatch for independent rows. Vectorize aligned rows, retaining all byte tails.
__attribute__((reqd_work_group_size(256, 1, 1)))
__kernel void orca_copy_2d(__global uchar* destination, __global const uchar* source,
                         ulong destination_pitch, ulong source_pitch, ulong width, ulong height) {
    const ulong lane = get_global_id(0), stride = get_global_size(0);
    ulong tail = 0;
    if ((((ulong)destination | (ulong)source | destination_pitch | source_pitch) & 15) == 0) {
        const ulong columns = width / 16;
        for (ulong i = lane; i < columns * height; i += stride) {
            const ulong row = i / columns, column = i - row * columns;
            ((__global uint4*)(destination + row * destination_pitch))[column] =
                ((__global const uint4*)(source + row * source_pitch))[column];
        }
        tail = columns * 16;
    }
    const ulong remaining = width - tail;
    for (ulong i = lane; i < remaining * height; i += stride) {
        const ulong row = i / remaining, column = tail + i - row * remaining;
        destination[row * destination_pitch + column] = source[row * source_pitch + column];
    }
}

// Co-aligned byte offsets: peel at most 15 bytes, keeping orca_copy's aligned path intact.
__attribute__((reqd_work_group_size(256, 1, 1)))
__kernel void orca_copy_coaligned(__global uchar* destination, __global const uchar* source, ulong bytes) {
    const ulong lane = get_global_id(0), stride = get_global_size(0);
    const ulong head = min(bytes, (16ul - ((ulong)destination & 15)) & 15ul);
    if (lane < head) destination[lane] = source[lane];
    const ulong vectors = (bytes - head) / 16;
    for (ulong i = lane; i < vectors; i += stride)
        ((__global uint4*)(destination + head))[i] = ((__global const uint4*)(source + head))[i];
    for (ulong i = head + vectors * 16 + lane; i < bytes; i += stride)
        destination[i] = source[i];
}
