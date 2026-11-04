// A single module shares initialized/zero-valued globals and constant memory
// across two kernels, changing scalar captures and independent queue replays.
__global volatile uint module_values[8] = {3, 5, 7, 11};
__constant volatile uint module_coefficients[4] = {2, 13, 29, 61};

__attribute__((reqd_work_group_size(4, 1, 1)))
__kernel void module_update(uint delta) {
    const uint i = get_global_id(0);
    module_values[i] += delta * module_coefficients[i];
    module_values[4 + i] += 1;
}

__attribute__((reqd_work_group_size(8, 1, 1)))
__kernel void module_read(__global uint* output) {
    const uint i = get_global_id(0);
    output[i] = module_values[i];
}
