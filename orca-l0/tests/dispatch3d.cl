// Distinct local/group coordinates also exercise hidden local/global/group sizes.
__kernel void grid3d(__global uint* output) {
    const size_t x = get_global_id(0), y = get_global_id(1), z = get_global_id(2);
    const size_t local_index = get_local_id(0) + get_local_size(0) *
        (get_local_id(1) + get_local_size(1) * get_local_id(2));
    const size_t group = get_group_id(0) + get_num_groups(0) *
        (get_group_id(1) + get_num_groups(1) * get_group_id(2));
    output[x + get_global_size(0) * (y + get_global_size(1) * z)] = (uint)(local_index + 1024 * group);
}
