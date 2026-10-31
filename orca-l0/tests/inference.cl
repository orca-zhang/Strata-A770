// Explicit, stable OpenCL C ABI. ocloc is a build tool; no OpenCL runtime is used.
typedef struct { uint width; uint rows; float scale; float epsilon; } Parameters;

// ponytail: reference FP32 kernels validate the execution contract; port the measured Strata kernels for speed.
__kernel void rms_norm(__global const float* input, __global const float* weight,
                       __global float* output, __global const Parameters* params) {
    const uint lane = get_local_id(0);
    __local float sums[64];
    float sum = 0.0f;
    for (uint i = lane; i < params->width; i += 64) sum += input[i] * input[i];
    sums[lane] = sum;
    barrier(CLK_LOCAL_MEM_FENCE);
    for (uint stride = 32; stride; stride >>= 1) {
        if (lane < stride) sums[lane] += sums[lane + stride];
        barrier(CLK_LOCAL_MEM_FENCE);
    }
    const float norm = 1.0f / sqrt(sums[0] / params->width + params->epsilon);
    for (uint i = lane; i < params->width; i += 64)
        output[i] = input[i] * norm * weight[i] * params->scale;
}

__kernel void gate_up(__global const float* matrix, __global const float* input,
                      __global float* output, __global const Parameters* params) {
    const uint row = get_global_id(0);
    if (row >= 2 * params->rows) return;
    float value = 0.0f;
    for (uint i = 0; i < params->width; ++i)
        value += matrix[row * params->width + i] * input[i];
    output[row] = value;
}

__kernel void swiglu(__global const float* gate_up_values, __global float* output,
                     __global const Parameters* params) {
    const uint row = get_global_id(0);
    if (row >= params->rows) return;
    const float gate = gate_up_values[row];
    output[row] = gate / (1.0f + exp(-gate)) * gate_up_values[params->rows + row];
}

__kernel void advance(__global float* values, uint delta) {
    values[get_global_id(0)] += (float)delta;
}
