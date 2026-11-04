// Only the DDI domains used by Orca are initialized.
ze_command_list_dditable_t command_list{};
ze_command_list_exp_dditable_t command_list_exp{};
ze_command_queue_dditable_t command_queue{};
ze_context_dditable_t context{};
ze_device_dditable_t device{};
ze_driver_dditable_t driver{};
ze_event_dditable_t event{};
ze_event_pool_dditable_t event_pool{};
ze_fence_dditable_t fence{};
ze_global_dditable_t global{};
ze_kernel_dditable_t kernel{};
ze_mem_dditable_t mem{};
ze_module_dditable_t module{};
ze_module_build_log_dditable_t module_build_log{};
zes_global_dditable_t sysman_global{};
zes_driver_dditable_t sysman_driver{};
zes_device_dditable_t sysman_device{};
zes_memory_dditable_t sysman_memory{};
void load_tables() {
    table("zeGetCommandListProcAddrTable", command_list);
    table("zeGetCommandListExpProcAddrTable", command_list_exp);
    table("zeGetCommandQueueProcAddrTable", command_queue);
    table("zeGetContextProcAddrTable", context);
    table("zeGetDeviceProcAddrTable", device);
    table("zeGetDriverProcAddrTable", driver);
    table("zeGetEventProcAddrTable", event);
    table("zeGetEventPoolProcAddrTable", event_pool);
    table("zeGetFenceProcAddrTable", fence);
    table("zeGetGlobalProcAddrTable", global);
    table("zeGetKernelProcAddrTable", kernel);
    table("zeGetMemProcAddrTable", mem);
    table("zeGetModuleProcAddrTable", module);
    table("zeGetModuleBuildLogProcAddrTable", module_build_log);
    // Sysman is optional for standalone compute; Strata uses it for the VRAM budget.
    if (symbol("zesGetGlobalProcAddrTable")) {
        table("zesGetGlobalProcAddrTable", sysman_global);
        table("zesGetDriverProcAddrTable", sysman_driver);
        table("zesGetDeviceProcAddrTable", sysman_device);
        table("zesGetMemoryProcAddrTable", sysman_memory);
    }
}
