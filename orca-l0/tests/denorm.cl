// Bake twice: default denorm retention and -cl-denorms-are-zero.
__attribute__((reqd_work_group_size(32, 1, 1)))
__attribute__((intel_reqd_sub_group_size(32)))
__kernel void denorm_mul(__global const uint2* input, __global uint* output) {
    const uint i = get_global_id(0);
    output[i] = as_uint(as_float(input[i].x) * as_float(input[i].y));
}
